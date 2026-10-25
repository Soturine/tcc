#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Outbox de eventos criticos do device (ADR-005, ADR-020, ADR-026; auditoria F-05/F-06/F-29).
//
// Garantia alvo: persistencia local -> publish -> commit no backend -> confirmacao de
// aplicacao(event_uuid) -> remocao. Um item NUNCA sai da outbox porque o publish
// local foi aceito; somente uma confirmacao de persistencia (critical-event-ack-v1
// ou critical-event-result-v1 committed/duplicate com hash correspondente) remove.
// Telemetria nao usa esta outbox.
//
// Rejeicao permanente autenticada (critical-event-result-v1) NAO e confirmacao: o
// item sai da fila ativa para uma quarentena persistente e limitada, preservando
// payload, motivo e hash. Nada e apagado sem confirmacao de persistencia ou acao
// local explicita (clearQuarantined).
//
// Capacidade e finita: quando cheia, o NOVO evento e recusado de forma observavel e
// nenhum evento ja aceito e descartado. Reservas garantem slots que eventos de menor
// prioridade nao podem ocupar (SOS > queda > demais).

enum class EventPriority : uint8_t { Normal = 0, Fall = 1, Sos = 2 };

enum class EntryState : uint8_t { Pending = 0, Quarantined = 1 };

struct OutboxEntry {
  std::string uuid;
  std::string topic;
  std::string payload;
  uint64_t order = 0;
  // Uptime de criacao neste boot; itens restaurados usam o instante da restauracao.
  uint32_t createdUptimeMs = 0;
  uint16_t attempts = 0;
  uint32_t lastAttemptAtMs = 0;
  uint32_t nextAttemptAtMs = 0;
  bool persisted = false;
  EventPriority priority = EventPriority::Normal;
  EntryState state = EntryState::Pending;
  std::string quarantineReason;
  // Derivado do payload (nao gravado): correlaciona critical-event-result-v1.
  std::string payloadSha256;
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
  // Slots ativos que somente SOS pode ocupar.
  std::size_t sosReserve = 0;
  // Slots ativos adicionais que somente SOS ou queda podem ocupar.
  std::size_t fallReserve = 0;
  // Rejeitados permanentes guardados fora da fila ativa.
  std::size_t quarantineCapacity = 4;
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

// Disposicao de um critical-event-result-v1 ja validado (topico, schema, device).
enum class IngestDisposition { Persisted, Retryable, RejectedPermanent };

enum class ResultOutcome {
  Removed,
  RemovedStoreError,
  Quarantined,
  KeptQuarantineFull,  // quarentena cheia: item continua ativo com retry lento
  KeptStoreError,      // nao foi possivel gravar o estado de quarentena: continua ativo
  RetryScheduled,
  DuplicateResult,
  UnknownUuid,
  HashMismatch  // resultado nao corresponde aos bytes armazenados: ignorado
};

struct OutboxStats {
  std::size_t pending = 0;
  std::size_t notPersisted = 0;
  std::size_t capacity = 0;
  std::size_t sosReserve = 0;
  std::size_t fallReserve = 0;
  uint32_t oldestCreatedUptimeMs = 0;
  bool hasOldest = false;
  uint32_t overflowCount = 0;
  uint32_t sosOverflowCount = 0;
  uint32_t fallOverflowCount = 0;
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
  // critical-event-result-v1 / quarentena
  std::size_t quarantined = 0;
  std::size_t quarantineCapacity = 0;
  uint32_t retryableResults = 0;
  uint32_t rejectedPermanentResults = 0;
  uint32_t hashMismatchResults = 0;
  uint32_t quarantineFullCount = 0;
  uint32_t quarantineCleared = 0;
  uint32_t quarantineNotLoaded = 0;
  std::string lastQuarantinedUuid;
  std::string lastQuarantineReason;
};

class CriticalEventOutbox {
 public:
  using RandomSource = std::function<uint32_t()>;

  explicit CriticalEventOutbox(OutboxConfig config = OutboxConfig{},
                               RandomSource random = nullptr);

  // Restaura itens persistidos; pendentes ficam elegiveis para envio imediato e
  // quarentenados voltam para a quarentena (sem reenvio).
  void begin(OutboxStore* store, uint32_t nowMs);

  EnqueueResult enqueue(const std::string& uuid,
                        const std::string& topic,
                        const std::string& payload,
                        uint64_t order,
                        uint32_t nowMs,
                        EventPriority priority = EventPriority::Normal);

  // Proximo item cujo backoff venceu, em ordem de criacao; nullptr se nenhum.
  const OutboxEntry* nextDue(uint32_t nowMs) const;

  // Registra tentativa (aceita ou nao pelo cliente MQTT) e agenda o retry. Mesmo
  // publish aceito so evita retry ate o timeout do ACK; nao remove o item.
  void onPublishAttempt(const std::string& uuid, uint32_t nowMs, bool publishAccepted);

  // critical-event-ack-v1 (legado, sem hash): confirmacao de persistencia.
  AckResult onAck(const std::string& uuid, uint32_t nowMs);

  // critical-event-result-v1: so tem efeito quando o hash corresponde ao payload.
  ResultOutcome onIngestResult(const std::string& uuid,
                               const std::string& payloadSha256,
                               IngestDisposition disposition,
                               const std::string& reasonCode,
                               uint32_t retryAfterMs,
                               uint32_t nowMs);

  // Acao local explicita (operador): descarta um item quarentenado.
  bool clearQuarantined(const std::string& uuid);

  // Re-tenta gravar itens aceitos apenas em RAM. Retorna quantos foram gravados.
  std::size_t retryPersistence();

  bool contains(const std::string& uuid) const;
  bool isQuarantined(const std::string& uuid) const;
  OutboxStats stats() const;
  uint32_t backoffForAttempt(uint16_t attempts) const;
  std::size_t admissionLimit(EventPriority priority) const;

 private:
  OutboxEntry* find(const std::string& uuid);
  const OutboxEntry* find(const std::string& uuid) const;
  const OutboxEntry* findQuarantined(const std::string& uuid) const;
  void rememberAcked(const std::string& uuid);
  bool recentlyAcked(const std::string& uuid) const;
  bool removeActive(std::vector<OutboxEntry>::iterator position, uint32_t nowMs);

  OutboxConfig config_;
  RandomSource random_;
  OutboxStore* store_ = nullptr;
  std::vector<OutboxEntry> entries_;
  std::vector<OutboxEntry> quarantine_;
  std::vector<std::string> recentAcks_;
  std::size_t recentAckCursor_ = 0;
  OutboxStats counters_;
};
