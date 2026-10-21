#include "critical_outbox.h"

#include <algorithm>

namespace {
constexpr std::size_t kRecentAckMemory = 16;
constexpr std::size_t kMaxUuidLength = 160;

bool byOrder(const OutboxEntry& left, const OutboxEntry& right) {
  return left.order < right.order;
}

// Comparacao segura com overflow de millis(): "a ja chegou em b".
bool reached(uint32_t nowMs, uint32_t targetMs) {
  return static_cast<int32_t>(nowMs - targetMs) >= 0;
}
}  // namespace

CriticalEventOutbox::CriticalEventOutbox(OutboxConfig config, RandomSource random)
    : config_(config), random_(std::move(random)) {
  recentAcks_.resize(kRecentAckMemory);
}

void CriticalEventOutbox::begin(OutboxStore* store, uint32_t nowMs) {
  store_ = store;
  entries_.clear();
  counters_.storeAvailable = store_ != nullptr && store_->available();
  if (!counters_.storeAvailable) {
    return;
  }

  std::vector<OutboxEntry> loaded;
  StoreLoadReport report;
  store_->loadAll(loaded, report);
  counters_.corruptedOnLoad += static_cast<uint32_t>(report.corrupted);

  std::sort(loaded.begin(), loaded.end(), byOrder);
  for (OutboxEntry& entry : loaded) {
    if (entries_.size() >= config_.capacity || find(entry.uuid) != nullptr) {
      // Itens alem da capacidade continuam no armazenamento e nao sao apagados.
      continue;
    }
    entry.attempts = 0;
    entry.lastAttemptAtMs = 0;
    entry.nextAttemptAtMs = nowMs;
    entry.persisted = true;
    entries_.push_back(entry);
    ++counters_.restored;
  }
}

EnqueueResult CriticalEventOutbox::enqueue(const std::string& uuid,
                                           const std::string& topic,
                                           const std::string& payload,
                                           uint64_t order,
                                           uint32_t nowMs) {
  if (uuid.empty() || uuid.size() > kMaxUuidLength || topic.empty() || payload.empty()) {
    return EnqueueResult::RejectedInvalid;
  }
  if (find(uuid) != nullptr) {
    return EnqueueResult::DuplicateUuid;
  }
  if (payload.size() > config_.maxPayloadBytes) {
    ++counters_.tooLargeCount;
    return EnqueueResult::RejectedTooLarge;
  }
  if (entries_.size() >= config_.capacity) {
    ++counters_.overflowCount;
    counters_.lastOverflowUuid = uuid;
    return EnqueueResult::RejectedFull;
  }

  OutboxEntry entry;
  entry.uuid = uuid;
  entry.topic = topic;
  entry.payload = payload;
  entry.order = order;
  entry.createdUptimeMs = nowMs;
  entry.nextAttemptAtMs = nowMs;
  entry.persisted = store_ != nullptr && store_->available() && store_->write(entry);
  if (!entry.persisted) {
    ++counters_.persistFailures;
  }

  entries_.insert(std::upper_bound(entries_.begin(), entries_.end(), entry, byOrder), entry);
  return entry.persisted ? EnqueueResult::Persisted : EnqueueResult::AcceptedVolatile;
}

const OutboxEntry* CriticalEventOutbox::nextDue(uint32_t nowMs) const {
  for (const OutboxEntry& entry : entries_) {
    if (reached(nowMs, entry.nextAttemptAtMs)) {
      return &entry;
    }
  }
  return nullptr;
}

uint32_t CriticalEventOutbox::backoffForAttempt(uint16_t attempts) const {
  if (attempts == 0U) {
    return 0U;
  }
  uint64_t delay = config_.retryBaseMs;
  for (uint16_t step = 1; step < attempts && delay < config_.retryMaxMs; ++step) {
    delay *= 2U;
  }
  if (delay > config_.retryMaxMs) {
    delay = config_.retryMaxMs;
  }
  uint32_t jitter = 0;
  if (random_) {
    jitter = random_() % static_cast<uint32_t>(delay / 4U + 1U);
  }
  return static_cast<uint32_t>(delay) + jitter;
}

void CriticalEventOutbox::onPublishAttempt(const std::string& uuid,
                                           uint32_t nowMs,
                                           bool publishAccepted) {
  OutboxEntry* entry = find(uuid);
  if (entry == nullptr) {
    return;
  }
  ++counters_.publishAttempts;
  if (!publishAccepted) {
    ++counters_.publishFailures;
  }
  if (entry->attempts < UINT16_MAX) {
    ++entry->attempts;
  }
  entry->lastAttemptAtMs = nowMs;
  entry->nextAttemptAtMs = nowMs + backoffForAttempt(entry->attempts);
}

AckResult CriticalEventOutbox::onAck(const std::string& uuid, uint32_t nowMs) {
  auto position = std::find_if(entries_.begin(), entries_.end(), [&uuid](const OutboxEntry& entry) {
    return entry.uuid == uuid;
  });

  if (position == entries_.end()) {
    if (recentlyAcked(uuid)) {
      ++counters_.duplicateAcks;
      return AckResult::DuplicateAck;
    }
    ++counters_.unknownAcks;
    return AckResult::UnknownUuid;
  }

  bool storeOk = true;
  if (position->persisted && store_ != nullptr) {
    storeOk = store_->remove(*position);
  }
  if (!storeOk) {
    ++counters_.storeRemoveFailures;
  }

  entries_.erase(position);
  rememberAcked(uuid);
  ++counters_.acked;
  counters_.lastAckedUuid = uuid;
  counters_.lastAckAtMs = nowMs;
  counters_.hasLastAck = true;
  return storeOk ? AckResult::Removed : AckResult::RemovedStoreError;
}

std::size_t CriticalEventOutbox::retryPersistence() {
  if (store_ == nullptr || !store_->available()) {
    return 0U;
  }
  std::size_t written = 0;
  for (OutboxEntry& entry : entries_) {
    if (!entry.persisted && store_->write(entry)) {
      entry.persisted = true;
      ++written;
    }
  }
  return written;
}

bool CriticalEventOutbox::contains(const std::string& uuid) const {
  return find(uuid) != nullptr;
}

OutboxStats CriticalEventOutbox::stats() const {
  OutboxStats snapshot = counters_;
  snapshot.pending = entries_.size();
  snapshot.capacity = config_.capacity;
  snapshot.notPersisted = static_cast<std::size_t>(
      std::count_if(entries_.begin(), entries_.end(), [](const OutboxEntry& entry) {
        return !entry.persisted;
      }));
  snapshot.storeAvailable = store_ != nullptr && store_->available();
  if (!entries_.empty()) {
    snapshot.hasOldest = true;
    snapshot.oldestCreatedUptimeMs = entries_.front().createdUptimeMs;
  }
  return snapshot;
}

OutboxEntry* CriticalEventOutbox::find(const std::string& uuid) {
  for (OutboxEntry& entry : entries_) {
    if (entry.uuid == uuid) {
      return &entry;
    }
  }
  return nullptr;
}

const OutboxEntry* CriticalEventOutbox::find(const std::string& uuid) const {
  for (const OutboxEntry& entry : entries_) {
    if (entry.uuid == uuid) {
      return &entry;
    }
  }
  return nullptr;
}

void CriticalEventOutbox::rememberAcked(const std::string& uuid) {
  recentAcks_[recentAckCursor_] = uuid;
  recentAckCursor_ = (recentAckCursor_ + 1U) % recentAcks_.size();
}

bool CriticalEventOutbox::recentlyAcked(const std::string& uuid) const {
  return std::find(recentAcks_.begin(), recentAcks_.end(), uuid) != recentAcks_.end();
}
