#include "critical_outbox_fs_store.h"

#include <LittleFS.h>
#include <Preferences.h>

#include "app_logging.h"
#include "outbox_codec.h"

namespace {
constexpr char kBasePath[] = "/littlefs";
constexpr char kPartitionLabel[] = "spiffs";
constexpr char kDirectory[] = "/ceo";
constexpr char kBootNamespace[] = "tcc_boot";
constexpr char kBootCounterKey[] = "count";

String pathFor(const OutboxEntry& entry, const char* suffix) {
  return String(kDirectory) + "/" + OutboxCodec::fileStemFor(entry.uuid).c_str() + suffix;
}

bool endsWith(const String& value, const char* suffix) {
  return value.endsWith(suffix);
}
}  // namespace

bool CriticalOutboxFsStore::begin() {
  mounted_ = LittleFS.begin(true, kBasePath, 4, kPartitionLabel);
  if (!mounted_) {
    AppLog::error("[outbox] littlefs_mount_failed; outbox critica em RAM apenas");
    return false;
  }
  if (!LittleFS.exists(kDirectory) && !LittleFS.mkdir(kDirectory)) {
    AppLog::error("[outbox] mkdir_failed; outbox critica em RAM apenas");
    mounted_ = false;
  }
  return mounted_;
}

bool CriticalOutboxFsStore::available() const {
  return mounted_;
}

bool CriticalOutboxFsStore::write(const OutboxEntry& entry) {
  if (!mounted_) {
    return false;
  }
  const std::string encoded = OutboxCodec::encode(entry);
  const String tmpPath = pathFor(entry, ".tmp");
  const String finalPath = pathFor(entry, ".evt");

  File file = LittleFS.open(tmpPath, FILE_WRITE);
  if (!file) {
    return false;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
  file.flush();
  file.close();
  if (written != encoded.size()) {
    LittleFS.remove(tmpPath);
    return false;
  }
  if (LittleFS.exists(finalPath) && !LittleFS.remove(finalPath)) {
    LittleFS.remove(tmpPath);
    return false;
  }
  return LittleFS.rename(tmpPath, finalPath);
}

bool CriticalOutboxFsStore::remove(const OutboxEntry& entry) {
  if (!mounted_) {
    return false;
  }
  const String finalPath = pathFor(entry, ".evt");
  return !LittleFS.exists(finalPath) || LittleFS.remove(finalPath);
}

bool CriticalOutboxFsStore::loadAll(std::vector<OutboxEntry>& out, StoreLoadReport& report) {
  if (!mounted_) {
    return false;
  }
  File directory = LittleFS.open(kDirectory);
  if (!directory || !directory.isDirectory()) {
    return false;
  }

  std::vector<String> temporaryFiles;
  for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
    const String path = String(kDirectory) + "/" + String(file.name()).substring(
                                                       String(file.name()).lastIndexOf('/') + 1);
    if (endsWith(path, ".tmp")) {
      // Escrita interrompida antes do rename: o evento nao havia sido aceito como persistido.
      temporaryFiles.push_back(path);
      file.close();
      continue;
    }
    if (!endsWith(path, ".evt")) {
      file.close();
      continue;
    }

    std::string content;
    content.reserve(file.size());
    while (file.available()) {
      content.push_back(static_cast<char>(file.read()));
    }
    file.close();

    OutboxEntry entry;
    if (OutboxCodec::decode(content, entry)) {
      out.push_back(entry);
      ++report.loaded;
    } else {
      ++report.corrupted;
      const String quarantined = path.substring(0, path.length() - 4) + ".bad";
      LittleFS.rename(path, quarantined);
      AppLog::errorf("[outbox] corrupted_file quarantined=%s\n", quarantined.c_str());
    }
  }
  directory.close();

  for (const String& path : temporaryFiles) {
    LittleFS.remove(path);
  }
  return true;
}

bool incrementPersistentBootCounter(uint32_t& counter) {
  Preferences preferences;
  if (!preferences.begin(kBootNamespace, false)) {
    counter = 0;
    return false;
  }
  counter = preferences.getULong(kBootCounterKey, 0) + 1U;
  const bool saved = preferences.putULong(kBootCounterKey, counter) == sizeof(uint32_t);
  preferences.end();
  return saved;
}
