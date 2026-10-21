#pragma once

#include <map>
#include <string>
#include <vector>

#include "critical_outbox.h"
#include "outbox_codec.h"

// Armazenamento em memoria que passa pelo codec real: simula o conteudo de
// flash sobrevivendo a reboots (a instancia continua viva entre "boots").
class MemoryOutboxStore : public OutboxStore {
 public:
  bool isAvailable = true;
  bool failWrites = false;
  bool failRemoves = false;
  int writes = 0;
  std::map<uint64_t, std::string> files;

  bool available() const override {
    return isAvailable;
  }

  bool write(const OutboxEntry& entry) override {
    ++writes;
    if (failWrites || !isAvailable) {
      return false;
    }
    files[entry.order] = OutboxCodec::encode(entry);
    return true;
  }

  bool remove(const OutboxEntry& entry) override {
    if (failRemoves || !isAvailable) {
      return false;
    }
    files.erase(entry.order);
    return true;
  }

  bool loadAll(std::vector<OutboxEntry>& out, StoreLoadReport& report) override {
    for (const auto& item : files) {
      OutboxEntry entry;
      if (OutboxCodec::decode(item.second, entry)) {
        out.push_back(entry);
        ++report.loaded;
      } else {
        ++report.corrupted;
      }
    }
    return true;
  }
};
