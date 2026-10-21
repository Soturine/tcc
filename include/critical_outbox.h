#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Outbox de eventos criticos do device (ADR-005, auditoria F-05/F-06).
//
// Garantia alvo: persistencia local -> publish -> commit no backend -> ACK de
// aplicacao(event_uuid) -> remocao. Um item NUNCA sai da outbox porque o publish
// local foi aceito; somente onAck() remove. Telemetria nao usa esta outbox.
//
// Capacidade e finita: quando cheia, o NOVO evento e recusado de forma observavel
// (overflowCount/lastOverflowUuid) e nenhum evento ja aceito e descartado.

struct OutboxEntry {
  std::string uuid;
  std::string topic;
  std::string payload;
  uint64_t order = 0;
  uint32_t createdUptimeMs = 0;
  uint16_t attempts = 0;
  uint32_t lastAttemptAtMs = 0;
  uint32_t nextAttemptAtMs = 0;
  bool persisted = false;
};

struct StoreLoadReport {
  std::size_t loaded = 0;
  std::size_t corrupted = 0;
};

class OutboxStore {
 public:
  virtual ~OutboxStore() = default;
  virtual bool available() const = 0;
  virtual bool write(const OutboxEntry& entry) = 0;
  virtual bool remove(const OutboxEntry& entry) = 0;
  virtual bool loadAll(std::vector<OutboxEntry>& out, StoreLoadReport& report) = 0;
};

struct OutboxConfig {
  std::size_t capacity = 8;
  std::size_t maxPayloadBytes = 3584;
  uint32_t retryBaseMs = 2000;
  uint32_t retryMaxMs = 60000;
};

enum class EnqueueResult {
  Persisted,
  AcceptedVolatile,  // aceito em RAM; persistencia falhou e sera re-tentada
  DuplicateUuid,
  RejectedFull,
  RejectedTooLarge,
  RejectedInvalid
};

enum class AckResult {
  Removed,
  RemovedStoreError,  // removido da RAM; arquivo pode reaparecer no reboot (reenvio idempotente)
  DuplicateAck,       // UUID ja confirmado recentemente
  UnknownUuid
};

struct OutboxStats {
  std::size_t pending = 0;
  std::size_t notPersisted = 0;
  std::size_t capacity = 0;
  uint32_t oldestCreatedUptimeMs = 0;
  bool hasOldest = false;
  uint32_t overflowCount = 0;
  uint32_t tooLargeCount = 0;
  uint32_t persistFailures = 0;
  uint32_t storeRemoveFailures = 0;
  uint32_t publishAttempts = 0;
  uint32_t publishFailures = 0;
  uint32_t acked = 0;
  uint32_t duplicateAcks = 0;
  uint32_t unknownAcks = 0;
  uint32_t restored = 0;
  uint32_t corruptedOnLoad = 0;
  bool storeAvailable = false;
  std::string lastOverflowUuid;
  std::string lastAckedUuid;
  uint32_t lastAckAtMs = 0;
  bool hasLastAck = false;
};

class CriticalEventOutbox {
 public:
  using RandomSource = std::function<uint32_t()>;

  explicit CriticalEventOutbox(OutboxConfig config = OutboxConfig{},
                               RandomSource random = nullptr);

  // Restaura itens persistidos; todos ficam elegiveis para envio imediato.
  void begin(OutboxStore* store, uint32_t nowMs);

  EnqueueResult enqueue(const std::string& uuid,
                        const std::string& topic,
                        const std::string& payload,
                        uint64_t order,
                        uint32_t nowMs);

  // Proximo item cujo backoff venceu, em ordem de criacao; nullptr se nenhum.
  const OutboxEntry* nextDue(uint32_t nowMs) const;

  // Registra tentativa (aceita ou nao pelo cliente MQTT) e agenda o retry. Mesmo
  // publish aceito so evita retry ate o timeout do ACK; nao remove o item.
  void onPublishAttempt(const std::string& uuid, uint32_t nowMs, bool publishAccepted);

  AckResult onAck(const std::string& uuid, uint32_t nowMs);

  // Re-tenta gravar itens aceitos apenas em RAM. Retorna quantos foram gravados.
  std::size_t retryPersistence();

  bool contains(const std::string& uuid) const;
  OutboxStats stats() const;
  uint32_t backoffForAttempt(uint16_t attempts) const;

 private:
  OutboxEntry* find(const std::string& uuid);
  const OutboxEntry* find(const std::string& uuid) const;
  void rememberAcked(const std::string& uuid);
  bool recentlyAcked(const std::string& uuid) const;

  OutboxConfig config_;
  RandomSource random_;
  OutboxStore* store_ = nullptr;
  std::vector<OutboxEntry> entries_;
  std::vector<std::string> recentAcks_;
  std::size_t recentAckCursor_ = 0;
  OutboxStats counters_;
};
