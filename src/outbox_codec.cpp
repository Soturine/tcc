#include "outbox_codec.h"

#include <cstdlib>

#include "event_identity.h"

namespace OutboxCodec {

namespace {
// CEO1: formato original (2026-10-21), somente leitura para migracao sem formatar.
// CEO2: acrescenta estado (pendente/quarentena), prioridade e motivo de quarentena.
constexpr char kMagicV1[] = "CEO1\n";
constexpr char kMagicV2[] = "CEO2\n";
constexpr std::size_t kMagicLength = sizeof(kMagicV2) - 1;
constexpr std::size_t kMaxReasonLength = 64;

bool readLine(const std::string& data, std::size_t& cursor, const std::string& key, std::string& value) {
  const std::string prefix = key + "=";
  if (data.compare(cursor, prefix.size(), prefix) != 0) {
    return false;
  }
  const std::size_t start = cursor + prefix.size();
  const std::size_t end = data.find('\n', start);
  if (end == std::string::npos) {
    return false;
  }
  value = data.substr(start, end - start);
  cursor = end + 1;
  return true;
}

bool readUnsigned(const std::string& data, std::size_t& cursor, const std::string& key, uint64_t& value) {
  std::string text;
  if (!readLine(data, cursor, key, text) || text.empty() || text.size() > 20) {
    return false;
  }
  for (char character : text) {
    if (character < '0' || character > '9') {
      return false;
    }
  }
  value = std::strtoull(text.c_str(), nullptr, 10);
  return true;
}

bool readBlob(const std::string& data, std::size_t& cursor, const std::string& key, std::string& value) {
  uint64_t length = 0;
  if (!readUnsigned(data, cursor, key, length) || cursor + length + 1 > data.size()) {
    return false;
  }
  value = data.substr(cursor, static_cast<std::size_t>(length));
  cursor += static_cast<std::size_t>(length);
  if (data[cursor] != '\n') {
    return false;
  }
  ++cursor;
  return true;
}
}  // namespace

uint32_t crc32(const std::string& data) {
  uint32_t crc = 0xFFFFFFFFU;
  for (unsigned char byte : data) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

std::string fileStemFor(const std::string& uuid) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (unsigned char byte : uuid) {
    hash ^= byte;
    hash *= 0x100000001b3ULL;
  }
  return "e" + EventIdentity::hex8(static_cast<uint32_t>(hash >> 32U)) +
         EventIdentity::hex8(static_cast<uint32_t>(hash));
}

std::string encode(const OutboxEntry& entry) {
  std::string body = kMagicV2;
  body += "order=" + std::to_string(entry.order) + "\n";
  body += "created=" + std::to_string(entry.createdUptimeMs) + "\n";
  body += std::string("state=") + (entry.state == EntryState::Quarantined ? "q" : "p") + "\n";
  body += "prio=" + std::to_string(static_cast<unsigned>(entry.priority)) + "\n";
  const std::string reason = entry.quarantineReason.substr(0, kMaxReasonLength);
  body += "reason=" + std::to_string(reason.size()) + "\n" + reason + "\n";
  body += "uuid=" + std::to_string(entry.uuid.size()) + "\n" + entry.uuid + "\n";
  body += "topic=" + std::to_string(entry.topic.size()) + "\n" + entry.topic + "\n";
  body += "payload=" + std::to_string(entry.payload.size()) + "\n" + entry.payload + "\n";
  body += "crc=" + EventIdentity::hex8(crc32(body)) + "\n";
  return body;
}

bool decode(const std::string& data, OutboxEntry& entry) {
  const std::size_t crcPosition = data.rfind("crc=");
  const bool v1 = data.compare(0, kMagicLength, kMagicV1) == 0;
  const bool v2 = data.compare(0, kMagicLength, kMagicV2) == 0;
  if ((!v1 && !v2) || crcPosition == std::string::npos) {
    return false;
  }
  const std::string body = data.substr(0, crcPosition);
  if (data.substr(crcPosition) != "crc=" + EventIdentity::hex8(crc32(body)) + "\n") {
    return false;
  }

  std::size_t cursor = kMagicLength;
  uint64_t order = 0;
  uint64_t created = 0;
  OutboxEntry decoded;
  if (!readUnsigned(body, cursor, "order", order) ||
      !readUnsigned(body, cursor, "created", created) || created > UINT32_MAX) {
    return false;
  }

  if (v2) {
    std::string state;
    uint64_t priority = 0;
    if (!readLine(body, cursor, "state", state) || (state != "p" && state != "q") ||
        !readUnsigned(body, cursor, "prio", priority) || priority > 2U ||
        !readBlob(body, cursor, "reason", decoded.quarantineReason) ||
        decoded.quarantineReason.size() > kMaxReasonLength) {
      return false;
    }
    decoded.state = state == "q" ? EntryState::Quarantined : EntryState::Pending;
    decoded.priority = static_cast<EventPriority>(priority);
  } else {
    // CEO1 nao registrava prioridade; rotulo conservador apenas para diagnostico
    // (a admissao conta ocupacao total, nao a prioridade dos itens ja aceitos).
    decoded.state = EntryState::Pending;
    decoded.priority = EventPriority::Fall;
  }

  if (!readBlob(body, cursor, "uuid", decoded.uuid) ||
      !readBlob(body, cursor, "topic", decoded.topic) ||
      !readBlob(body, cursor, "payload", decoded.payload) || cursor != body.size() ||
      decoded.uuid.empty() || decoded.topic.empty() || decoded.payload.empty()) {
    return false;
  }

  decoded.order = order;
  decoded.createdUptimeMs = static_cast<uint32_t>(created);
  decoded.persisted = true;
  entry = decoded;
  return true;
}

TmpRecovery decideTmpRecovery(bool finalExists, const std::string& tmpContent) {
  if (finalExists) {
    return TmpRecovery::DiscardTmp;
  }
  OutboxEntry probe;
  return decode(tmpContent, probe) ? TmpRecovery::PromoteTmp : TmpRecovery::DiscardTmp;
}

}  // namespace OutboxCodec
