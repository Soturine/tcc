#include "critical_outbox.h"

#include <algorithm>

#include "sha256.h"

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
  // Reservas maiores que a capacidade bloqueariam toda admissao; limita-se para que
  // ao menos SOS sempre tenha a capacidade total.
  if (config_.sosReserve > config_.capacity) {
    config_.sosReserve = config_.capacity;
  }
  if (config_.sosReserve + config_.fallReserve > config_.capacity) {
    config_.fallReserve = config_.capacity - config_.sosReserve;
  }
  recentAcks_.resize(kRecentAckMemory);
}

void CriticalEventOutbox::begin(OutboxStore* store, uint32_t nowMs) {
  store_ = store;
  entries_.clear();
  quarantine_.clear();
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
    if (find(entry.uuid) != nullptr || findQuarantined(entry.uuid) != nullptr) {
      continue;
    }
    entry.payloadSha256 = Sha256::hex(entry.payload);
    entry.persisted = true;
    if (entry.state == EntryState::Quarantined) {
      if (quarantine_.size() >= config_.quarantineCapacity) {
        // Continua no armazenamento; nao e apagado.
        ++counters_.quarantineNotLoaded;
        continue;
      }
      quarantine_.push_back(entry);
      continue;
    }
    if (entries_.size() >= config_.capacity) {
      // Itens alem da capacidade continuam no armazenamento e nao sao apagados.
      continue;
    }
    entry.attempts = 0;
    entry.lastAttemptAtMs = 0;
    entry.nextAttemptAtMs = nowMs;
    // O uptime gravado pertence a outro boot; a idade passa a contar da restauracao.
    entry.createdUptimeMs = nowMs;
    entries_.push_back(entry);
    ++counters_.restored;
  }
}

std::size_t CriticalEventOutbox::admissionLimit(EventPriority priority) const {
  switch (priority) {
    case EventPriority::Sos:
      return config_.capacity;
    case EventPriority::Fall:
      return config_.capacity - config_.sosReserve;
    case EventPriority::Normal:
      break;
  }
  return config_.capacity - config_.sosReserve - config_.fallReserve;
}

EnqueueResult CriticalEventOutbox::enqueue(const std::string& uuid,
                                           const std::string& topic,
                                           const std::string& payload,
                                           uint64_t order,
                                           uint32_t nowMs,
                                           EventPriority priority) {
  if (uuid.empty() || uuid.size() > kMaxUuidLength || topic.empty() || payload.empty()) {
    return EnqueueResult::RejectedInvalid;
  }
  if (find(uuid) != nullptr || findQuarantined(uuid) != nullptr) {
    return EnqueueResult::DuplicateUuid;
  }
  if (payload.size() > config_.maxPayloadBytes) {
    ++counters_.tooLargeCount;
    return EnqueueResult::RejectedTooLarge;
  }
  if (entries_.size() >= admissionLimit(priority)) {
    ++counters_.overflowCount;
    if (priority == EventPriority::Sos) {
      ++counters_.sosOverflowCount;
    } else if (priority == EventPriority::Fall) {
      ++counters_.fallOverflowCount;
    }
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
  entry.priority = priority;
  entry.payloadSha256 = Sha256::hex(payload);
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

bool CriticalEventOutbox::removeActive(std::vector<OutboxEntry>::iterator position, uint32_t nowMs) {
  bool storeOk = true;
  if (position->persisted && store_ != nullptr) {
    storeOk = store_->remove(*position);
  }
  if (!storeOk) {
    ++counters_.storeRemoveFailures;
  }
  const std::string uuid = position->uuid;
  entries_.erase(position);
  rememberAcked(uuid);
  ++counters_.acked;
  counters_.lastAckedUuid = uuid;
  counters_.lastAckAtMs = nowMs;
  counters_.hasLastAck = true;
  return storeOk;
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

  return removeActive(position, nowMs) ? AckResult::Removed : AckResult::RemovedStoreError;
}

ResultOutcome CriticalEventOutbox::onIngestResult(const std::string& uuid,
                                                  const std::string& payloadSha256,
                                                  IngestDisposition disposition,
                                                  const std::string& reasonCode,
                                                  uint32_t retryAfterMs,
                                                  uint32_t nowMs) {
  auto position = std::find_if(entries_.begin(), entries_.end(), [&uuid](const OutboxEntry& entry) {
    return entry.uuid == uuid;
  });

  if (position == entries_.end()) {
    auto quarantined = std::find_if(quarantine_.begin(), quarantine_.end(),
                                    [&uuid](const OutboxEntry& entry) { return entry.uuid == uuid; });
    if (quarantined != quarantine_.end() && quarantined->payloadSha256 == payloadSha256 &&
        disposition == IngestDisposition::Persisted) {
      // Confirmacao tardia (ex.: backend corrigido aceitou o evento): sai da quarentena.
      bool storeOk = !quarantined->persisted || store_ == nullptr || store_->remove(*quarantined);
      if (!storeOk) {
        ++counters_.storeRemoveFailures;
      }
      quarantine_.erase(quarantined);
      rememberAcked(uuid);
      ++counters_.acked;
      counters_.lastAckedUuid = uuid;
      counters_.lastAckAtMs = nowMs;
      counters_.hasLastAck = true;
      return storeOk ? ResultOutcome::Removed : ResultOutcome::RemovedStoreError;
    }
    if (quarantined != quarantine_.end()) {
      return ResultOutcome::DuplicateResult;
    }
    if (recentlyAcked(uuid)) {
      ++counters_.duplicateAcks;
      return ResultOutcome::DuplicateResult;
    }
    ++counters_.unknownAcks;
    return ResultOutcome::UnknownUuid;
  }

  if (position->payloadSha256 != payloadSha256) {
    ++counters_.hashMismatchResults;
    return ResultOutcome::HashMismatch;
  }

  switch (disposition) {
    case IngestDisposition::Persisted:
      return removeActive(position, nowMs) ? ResultOutcome::Removed : ResultOutcome::RemovedStoreError;

    case IngestDisposition::Retryable: {
      ++counters_.retryableResults;
      uint32_t delay = backoffForAttempt(position->attempts == 0U ? 1U : position->attempts);
      const uint32_t hinted = std::min(retryAfterMs, config_.retryMaxMs);
      if (hinted > delay) {
        delay = hinted;
      }
      position->nextAttemptAtMs = nowMs + delay;
      return ResultOutcome::RetryScheduled;
    }

    case IngestDisposition::RejectedPermanent:
      break;
  }

  ++counters_.rejectedPermanentResults;
  counters_.lastQuarantineReason = reasonCode;
  if (quarantine_.size() >= config_.quarantineCapacity) {
    // Sem espaco de quarentena: nao apagar. Mantem ativo com retry lento e sinaliza.
    ++counters_.quarantineFullCount;
    position->quarantineReason = reasonCode;
    position->nextAttemptAtMs = nowMs + config_.retryMaxMs;
    return ResultOutcome::KeptQuarantineFull;
  }

  OutboxEntry moved = *position;
  moved.state = EntryState::Quarantined;
  moved.quarantineReason = reasonCode;
  if (moved.persisted && store_ != nullptr) {
    // Mesmo arquivo (nome derivado do UUID), regravado de forma atomica com o novo estado.
    if (!store_->available() || !store_->write(moved)) {
      ++counters_.persistFailures;
      position->quarantineReason = reasonCode;
      position->nextAttemptAtMs = nowMs + config_.retryMaxMs;
      return ResultOutcome::KeptStoreError;
    }
  }
  entries_.erase(position);
  quarantine_.push_back(moved);
  counters_.lastQuarantinedUuid = uuid;
  return ResultOutcome::Quarantined;
}

bool CriticalEventOutbox::clearQuarantined(const std::string& uuid) {
  auto position = std::find_if(quarantine_.begin(), quarantine_.end(),
                               [&uuid](const OutboxEntry& entry) { return entry.uuid == uuid; });
  if (position == quarantine_.end()) {
    return false;
  }
  if (position->persisted && store_ != nullptr && !store_->remove(*position)) {
    ++counters_.storeRemoveFailures;
    return false;
  }
  quarantine_.erase(position);
  ++counters_.quarantineCleared;
  return true;
}

std::size_t CriticalEventOutbox::retryPersistence() {
  if (store_ == nullptr || !store_->available()) {
    return 0U;
  }
  std::size_t written = 0;
  for (std::vector<OutboxEntry>* list : {&entries_, &quarantine_}) {
    for (OutboxEntry& entry : *list) {
      if (!entry.persisted && store_->write(entry)) {
        entry.persisted = true;
        ++written;
      }
    }
  }
  return written;
}

bool CriticalEventOutbox::contains(const std::string& uuid) const {
  return find(uuid) != nullptr;
}

bool CriticalEventOutbox::isQuarantined(const std::string& uuid) const {
  return findQuarantined(uuid) != nullptr;
}

OutboxStats CriticalEventOutbox::stats() const {
  OutboxStats snapshot = counters_;
  snapshot.pending = entries_.size();
  snapshot.capacity = config_.capacity;
  snapshot.sosReserve = config_.sosReserve;
  snapshot.fallReserve = config_.fallReserve;
  snapshot.quarantined = quarantine_.size();
  snapshot.quarantineCapacity = config_.quarantineCapacity;
  const auto volatileCount = [](const std::vector<OutboxEntry>& list) {
    return static_cast<std::size_t>(std::count_if(
        list.begin(), list.end(), [](const OutboxEntry& entry) { return !entry.persisted; }));
  };
  snapshot.notPersisted = volatileCount(entries_) + volatileCount(quarantine_);
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

const OutboxEntry* CriticalEventOutbox::findQuarantined(const std::string& uuid) const {
  for (const OutboxEntry& entry : quarantine_) {
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
