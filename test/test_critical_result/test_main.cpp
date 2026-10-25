#include <unity.h>

#include <string>

#include <cstring>

#include "app_config.h"
#include "critical_outbox.h"
#include "critical_result.h"
#include "event_identity.h"
#include "outbox_codec.h"
#include "sha256.h"
#include "support/memory_outbox_store.h"

// critical-event-result-v1, quarentena e reservas da outbox (ADR-026, F-29).
namespace {

const std::string kTopic = "queda/devices/esp32_01/events";
const std::string kResultTopic = "queda/devices/esp32_01/critical-event-results";
const std::string kDevice = "esp32_01";

std::string payloadFor(const std::string& uuid, const std::string& type = "fall_detected") {
  return "{\"event_type\":\"" + type + "\",\"event_uuid\":\"" + uuid + "\"}";
}

std::string hashOf(const std::string& uuid, const std::string& type = "fall_detected") {
  return Sha256::hex(payloadFor(uuid, type));
}

OutboxConfig config(std::size_t capacity = 4,
                    std::size_t sosReserve = 0,
                    std::size_t fallReserve = 0,
                    std::size_t quarantine = 2) {
  OutboxConfig value;
  value.capacity = capacity;
  value.maxPayloadBytes = 512;
  value.retryBaseMs = 1000;
  value.retryMaxMs = 8000;
  value.sosReserve = sosReserve;
  value.fallReserve = fallReserve;
  value.quarantineCapacity = quarantine;
  return value;
}

std::string resultJson(const std::string& status,
                       const std::string& uuid,
                       const std::string& hash,
                       const std::string& extra) {
  return "{\"schema_version\":1,\"device_id\":\"esp32_01\",\"event_uuid\":\"" + uuid +
         "\",\"payload_sha256\":\"" + hash + "\",\"status\":\"" + status + "\"," + extra +
         ",\"issued_at\":\"2026-10-25T00:00:00Z\"}";
}

const std::string kPersistedExtra =
    "\"reason_code\":null,\"event_id\":7,\"committed_at\":\"2026-10-25T00:00:00Z\",\"retry_after_ms\":null";
const std::string kRejectedExtra =
    "\"reason_code\":\"event_uuid_conflict\",\"event_id\":null,\"committed_at\":null,\"retry_after_ms\":null";
const std::string kRetryExtra =
    "\"reason_code\":\"storage_unavailable\",\"event_id\":null,\"committed_at\":null,\"retry_after_ms\":5000";

CriticalResult::ParseStatus parse(const std::string& json, CriticalResult::Result& out,
                                  const std::string& topic = kResultTopic,
                                  const std::string& device = kDevice) {
  return CriticalResult::parse(kResultTopic, topic, device, json.c_str(), json.size(), out);
}

// ---------------------------------------------------------------- sha256

void testSha256KnownVectors() {
  TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                           Sha256::hex(std::string()).c_str());
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                           Sha256::hex(std::string("abc")).c_str());
  TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                           Sha256::hex(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")).c_str());
  // Fronteiras de bloco (55/56/64 bytes) exercitam o padding em dois blocos.
  TEST_ASSERT_EQUAL_STRING("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
                           Sha256::hex(std::string(55, 'a')).c_str());
  TEST_ASSERT_EQUAL_STRING("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
                           Sha256::hex(std::string(56, 'a')).c_str());
  TEST_ASSERT_EQUAL_STRING("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
                           Sha256::hex(std::string(64, 'a')).c_str());
}

// ---------------------------------------------------------------- parser

void testParserAcceptsValidResults() {
  CriticalResult::Result result;
  const std::string hash(64, 'a');
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalResult::ParseStatus::Ok),
                    static_cast<int>(parse(resultJson("committed", "u1", hash, kPersistedExtra), result)));
  TEST_ASSERT_TRUE(CriticalResult::confirmsPersistence(result.status));
  TEST_ASSERT_EQUAL_INT64(7, result.eventId);

  TEST_ASSERT_EQUAL(static_cast<int>(CriticalResult::ParseStatus::Ok),
                    static_cast<int>(parse(resultJson("rejected_permanent", "u1", hash, kRejectedExtra), result)));
  TEST_ASSERT_FALSE(CriticalResult::confirmsPersistence(result.status));
  TEST_ASSERT_EQUAL_STRING("event_uuid_conflict", result.reasonCode.c_str());

  TEST_ASSERT_EQUAL(static_cast<int>(CriticalResult::ParseStatus::Ok),
                    static_cast<int>(parse(resultJson("retryable", "u1", hash, kRetryExtra), result)));
  TEST_ASSERT_TRUE(result.hasRetryAfter);
  TEST_ASSERT_EQUAL_UINT32(5000, result.retryAfterMs);
}

void testParserRejectsForgedOrInconsistentResults() {
  CriticalResult::Result result;
  const std::string hash(64, 'a');
  using PS = CriticalResult::ParseStatus;
  // Cross-device: topico de outro device ou device_id diferente do proprio.
  TEST_ASSERT_EQUAL(static_cast<int>(PS::WrongTopic),
                    static_cast<int>(parse(resultJson("committed", "u1", hash, kPersistedExtra), result,
                                           "queda/devices/esp32_02/critical-event-results")));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::WrongDevice),
                    static_cast<int>(parse(resultJson("committed", "u1", hash, kPersistedExtra), result,
                                           kResultTopic, "esp32_02")));
  // Rejeicao com identidade de persistencia e incoerente (nao pode remover nada).
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InconsistentFields),
                    static_cast<int>(parse(resultJson("rejected_permanent", "u1", hash,
                                                      "\"reason_code\":\"event_uuid_conflict\",\"event_id\":7,"
                                                      "\"committed_at\":null,\"retry_after_ms\":null"),
                                           result)));
  // Persistido sem event_id, motivo invalido, status desconhecido, hash invalido.
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InconsistentFields),
                    static_cast<int>(parse(resultJson("committed", "u1", hash,
                                                      "\"reason_code\":null,\"event_id\":null,"
                                                      "\"committed_at\":\"x\",\"retry_after_ms\":null"),
                                           result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InconsistentFields),
                    static_cast<int>(parse(resultJson("retryable", "u1", hash,
                                                      "\"reason_code\":\"event_uuid_conflict\",\"event_id\":null,"
                                                      "\"committed_at\":null,\"retry_after_ms\":null"),
                                           result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InvalidStatus),
                    static_cast<int>(parse(resultJson("accepted", "u1", hash, kPersistedExtra), result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InvalidHash),
                    static_cast<int>(parse(resultJson("committed", "u1", std::string(64, 'A'), kPersistedExtra), result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InvalidHash),
                    static_cast<int>(parse(resultJson("committed", "u1", "abc", kPersistedExtra), result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InvalidUuid),
                    static_cast<int>(parse(resultJson("committed", "", hash, kPersistedExtra), result)));
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InvalidJson),
                    static_cast<int>(parse("{not json", result)));
  const std::string v2 = "{\"schema_version\":2}";
  TEST_ASSERT_EQUAL(static_cast<int>(PS::UnsupportedSchema), static_cast<int>(parse(v2, result)));
  // retry_after_ms fora do limite.
  TEST_ASSERT_EQUAL(static_cast<int>(PS::InconsistentFields),
                    static_cast<int>(parse(resultJson("retryable", "u1", hash,
                                                      "\"reason_code\":\"storage_unavailable\",\"event_id\":null,"
                                                      "\"committed_at\":null,\"retry_after_ms\":3600001"),
                                           result)));
}

// ---------------------------------------------------------------- outbox x resultado

void testPersistedResultRemovesOnlyWithMatchingHash() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);

  // Replay de resultado de outro conteudo com o mesmo UUID: nao remove.
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::HashMismatch),
                    static_cast<int>(outbox.onIngestResult("e1", hashOf("e1", "sos_pressed"),
                                                           IngestDisposition::Persisted, "", 0, 10)));
  TEST_ASSERT_TRUE(outbox.contains("e1"));
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().hashMismatchResults);

  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Removed),
                    static_cast<int>(outbox.onIngestResult("e1", hashOf("e1"),
                                                           IngestDisposition::Persisted, "", 0, 20)));
  TEST_ASSERT_FALSE(outbox.contains("e1"));
  TEST_ASSERT_EQUAL_UINT32(0, store.files.size());
  // Resultado duplicado depois da remocao (ex.: retransmissao + reboot do backend).
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::DuplicateResult),
                    static_cast<int>(outbox.onIngestResult("e1", hashOf("e1"),
                                                           IngestDisposition::Persisted, "", 0, 30)));
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::UnknownUuid),
                    static_cast<int>(outbox.onIngestResult("ghost", hashOf("ghost"),
                                                           IngestDisposition::Persisted, "", 0, 30)));
}

void testRejectedPermanentQuarantinesWithoutCountingAsAck() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("bad", kTopic, payloadFor("bad"), 1, 0, EventPriority::Fall);
  outbox.onPublishAttempt("bad", 0, true);

  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Quarantined),
                    static_cast<int>(outbox.onIngestResult("bad", hashOf("bad"),
                                                           IngestDisposition::RejectedPermanent,
                                                           "event_uuid_conflict", 0, 50)));
  const OutboxStats stats = outbox.stats();
  TEST_ASSERT_FALSE(outbox.contains("bad"));
  TEST_ASSERT_TRUE(outbox.isQuarantined("bad"));
  TEST_ASSERT_EQUAL_UINT32(0, stats.pending);
  TEST_ASSERT_EQUAL_UINT32(1, stats.quarantined);
  TEST_ASSERT_EQUAL_UINT32(0, stats.acked);  // rejeicao NUNCA e confirmacao
  TEST_ASSERT_FALSE(stats.hasLastAck);
  TEST_ASSERT_EQUAL_UINT32(1, stats.rejectedPermanentResults);
  TEST_ASSERT_EQUAL_STRING("event_uuid_conflict", stats.lastQuarantineReason.c_str());
  // Evidencia preservada na flash com o novo estado.
  TEST_ASSERT_EQUAL_UINT32(1, store.files.size());
  OutboxEntry stored;
  TEST_ASSERT_TRUE(OutboxCodec::decode(store.files.begin()->second, stored));
  TEST_ASSERT_EQUAL(static_cast<int>(EntryState::Quarantined), static_cast<int>(stored.state));
  TEST_ASSERT_EQUAL_STRING("event_uuid_conflict", stored.quarantineReason.c_str());
  TEST_ASSERT_EQUAL_STRING(payloadFor("bad").c_str(), stored.payload.c_str());
  // Nao e reenviado.
  TEST_ASSERT_NULL(outbox.nextDue(1000000));
}

void testQuarantineSurvivesRebootAndDoesNotOccupyActiveSlots() {
  MemoryOutboxStore store;
  {
    CriticalEventOutbox before(config(2));
    before.begin(&store, 0);
    before.enqueue("bad", kTopic, payloadFor("bad"), 1, 0, EventPriority::Fall);
    before.onIngestResult("bad", hashOf("bad"), IngestDisposition::RejectedPermanent, "invalid_event_uuid", 0, 1);
    before.enqueue("ok", kTopic, payloadFor("ok"), 2, 2, EventPriority::Fall);
  }
  CriticalEventOutbox after(config(2));
  after.begin(&store, 100);
  TEST_ASSERT_TRUE(after.isQuarantined("bad"));
  TEST_ASSERT_TRUE(after.contains("ok"));
  TEST_ASSERT_EQUAL_UINT32(1, after.stats().pending);
  TEST_ASSERT_EQUAL_UINT32(1, after.stats().quarantined);
  // F-29: a rejeicao nao consome capacidade ativa; ainda ha vaga para um novo evento.
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                    static_cast<int>(after.enqueue("new", kTopic, payloadFor("new"), 3, 101, EventPriority::Fall)));
  const OutboxEntry* due = after.nextDue(101);
  TEST_ASSERT_NOT_NULL(due);
  TEST_ASSERT_EQUAL_STRING("ok", due->uuid.c_str());
}

void testQuarantineFullKeepsEventActiveWithSlowRetry() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config(4, 0, 0, 1));
  outbox.begin(&store, 0);
  outbox.enqueue("a", kTopic, payloadFor("a"), 1, 0, EventPriority::Fall);
  outbox.enqueue("b", kTopic, payloadFor("b"), 2, 0, EventPriority::Fall);
  outbox.onIngestResult("a", hashOf("a"), IngestDisposition::RejectedPermanent, "event_uuid_conflict", 0, 10);
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::KeptQuarantineFull),
                    static_cast<int>(outbox.onIngestResult("b", hashOf("b"), IngestDisposition::RejectedPermanent,
                                                           "event_uuid_conflict", 0, 10)));
  TEST_ASSERT_TRUE(outbox.contains("b"));  // nao apagado
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().quarantineFullCount);
  TEST_ASSERT_NULL(outbox.nextDue(10 + 7999));
  TEST_ASSERT_NOT_NULL(outbox.nextDue(10 + 8000));
}

void testQuarantineStoreWriteFailureKeepsEventActive() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("a", kTopic, payloadFor("a"), 1, 0, EventPriority::Fall);
  store.failWrites = true;
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::KeptStoreError),
                    static_cast<int>(outbox.onIngestResult("a", hashOf("a"), IngestDisposition::RejectedPermanent,
                                                           "event_uuid_conflict", 0, 10)));
  TEST_ASSERT_TRUE(outbox.contains("a"));
  OutboxEntry stored;
  TEST_ASSERT_TRUE(OutboxCodec::decode(store.files.begin()->second, stored));
  TEST_ASSERT_EQUAL(static_cast<int>(EntryState::Pending), static_cast<int>(stored.state));
}

void testLateCommitReleasesQuarantineAndExplicitClear() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("a", kTopic, payloadFor("a"), 1, 0, EventPriority::Fall);
  outbox.enqueue("b", kTopic, payloadFor("b"), 2, 0, EventPriority::Fall);
  outbox.onIngestResult("a", hashOf("a"), IngestDisposition::RejectedPermanent, "event_uuid_conflict", 0, 1);
  outbox.onIngestResult("b", hashOf("b"), IngestDisposition::RejectedPermanent, "event_uuid_conflict", 0, 1);

  // Hash divergente nao libera a quarentena.
  outbox.onIngestResult("a", hashOf("b"), IngestDisposition::Persisted, "", 0, 2);
  TEST_ASSERT_TRUE(outbox.isQuarantined("a"));
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Removed),
                    static_cast<int>(outbox.onIngestResult("a", hashOf("a"), IngestDisposition::Persisted, "", 0, 3)));
  TEST_ASSERT_FALSE(outbox.isQuarantined("a"));

  TEST_ASSERT_FALSE(outbox.clearQuarantined("ghost"));
  TEST_ASSERT_TRUE(outbox.clearQuarantined("b"));
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().quarantineCleared);
  TEST_ASSERT_EQUAL_UINT32(0, store.files.size());
}

void testRetryableKeepsEventAndHonorsBoundedRetryAfter() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("a", kTopic, payloadFor("a"), 1, 0, EventPriority::Fall);
  outbox.onPublishAttempt("a", 0, true);
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::RetryScheduled),
                    static_cast<int>(outbox.onIngestResult("a", hashOf("a"), IngestDisposition::Retryable,
                                                           "storage_unavailable", 5000, 100)));
  TEST_ASSERT_TRUE(outbox.contains("a"));
  TEST_ASSERT_NULL(outbox.nextDue(100 + 4999));
  TEST_ASSERT_NOT_NULL(outbox.nextDue(100 + 5000));
  // retry_after acima do teto e limitado a retryMaxMs.
  outbox.onIngestResult("a", hashOf("a"), IngestDisposition::Retryable, "storage_unavailable", 3600000, 200);
  TEST_ASSERT_NOT_NULL(outbox.nextDue(200 + 8000));
  TEST_ASSERT_EQUAL_UINT32(2, outbox.stats().retryableResults);
  TEST_ASSERT_EQUAL_UINT32(0, outbox.stats().acked);
}

// ---------------------------------------------------------------- reservas / capacidade

void testReservesKeepSlotsForSosAndFall() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config(4, 1, 1));
  outbox.begin(&store, 0);
  TEST_ASSERT_EQUAL_UINT32(2, outbox.admissionLimit(EventPriority::Normal));
  TEST_ASSERT_EQUAL_UINT32(3, outbox.admissionLimit(EventPriority::Fall));
  TEST_ASSERT_EQUAL_UINT32(4, outbox.admissionLimit(EventPriority::Sos));

  outbox.enqueue("n1", kTopic, payloadFor("n1", "fall_suspected"), 1, 0, EventPriority::Normal);
  outbox.enqueue("n2", kTopic, payloadFor("n2", "fall_suspected"), 2, 0, EventPriority::Normal);
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull),
                    static_cast<int>(outbox.enqueue("n3", kTopic, payloadFor("n3"), 3, 0, EventPriority::Normal)));
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                    static_cast<int>(outbox.enqueue("f1", kTopic, payloadFor("f1"), 4, 0, EventPriority::Fall)));
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull),
                    static_cast<int>(outbox.enqueue("f2", kTopic, payloadFor("f2"), 5, 0, EventPriority::Fall)));
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                    static_cast<int>(outbox.enqueue("s1", kTopic, payloadFor("s1", "sos_pressed"), 6, 0, EventPriority::Sos)));
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull),
                    static_cast<int>(outbox.enqueue("s2", kTopic, payloadFor("s2", "sos_pressed"), 7, 0, EventPriority::Sos)));

  const OutboxStats stats = outbox.stats();
  TEST_ASSERT_EQUAL_UINT32(4, stats.pending);
  TEST_ASSERT_EQUAL_UINT32(3, stats.overflowCount);
  TEST_ASSERT_EQUAL_UINT32(1, stats.fallOverflowCount);
  TEST_ASSERT_EQUAL_UINT32(1, stats.sosOverflowCount);
  TEST_ASSERT_EQUAL_STRING("s2", stats.lastOverflowUuid.c_str());
  // Nenhum evento ja aceito foi sacrificado.
  for (const char* uuid : {"n1", "n2", "f1", "s1"}) {
    TEST_ASSERT_TRUE(outbox.contains(uuid));
  }
}

void testReserveConfigurationIsClamped() {
  CriticalEventOutbox outbox(config(3, 5, 5));
  TEST_ASSERT_EQUAL_UINT32(3, outbox.admissionLimit(EventPriority::Sos));
  TEST_ASSERT_EQUAL_UINT32(0, outbox.admissionLimit(EventPriority::Fall));
  TEST_ASSERT_EQUAL_UINT32(0, outbox.admissionLimit(EventPriority::Normal));
}

// 16 (capacidade de produto), 17 (overflow) e 100 eventos com dois reboots.
void testCapacity16_17_100WithTwoRebootsNeverDropsAccepted() {
  MemoryOutboxStore store;
  const OutboxConfig product = config(16, 4, 4, 8);
  std::size_t accepted = 0;
  std::size_t rejected = 0;
  {
    CriticalEventOutbox boot1(product);
    boot1.begin(&store, 0);
    for (int i = 0; i < 100; ++i) {
      const std::string uuid = "fall-" + std::to_string(i);
      const EnqueueResult result =
          boot1.enqueue(uuid, kTopic, payloadFor(uuid), static_cast<uint64_t>(i), 0, EventPriority::Fall);
      if (result == EnqueueResult::Persisted) {
        ++accepted;
      } else {
        TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull), static_cast<int>(result));
        ++rejected;
      }
    }
    TEST_ASSERT_EQUAL_UINT32(12, accepted);  // 16 - reserva SOS(4)
    TEST_ASSERT_EQUAL_UINT32(88, rejected);
    // Com a fila de quedas lotada, o SOS ainda e aceito (reserva).
    for (int i = 0; i < 4; ++i) {
      const std::string uuid = "sos-" + std::to_string(i);
      TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                        static_cast<int>(boot1.enqueue(uuid, kTopic, payloadFor(uuid, "sos_pressed"),
                                                       static_cast<uint64_t>(1000 + i), 0, EventPriority::Sos)));
    }
    TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull),
                      static_cast<int>(boot1.enqueue("sos-17", kTopic, payloadFor("sos-17", "sos_pressed"),
                                                     2000, 0, EventPriority::Sos)));
    TEST_ASSERT_EQUAL_UINT32(16, store.files.size());
  }
  {
    CriticalEventOutbox boot2(product);
    boot2.begin(&store, 10);
    TEST_ASSERT_EQUAL_UINT32(16, boot2.stats().pending);
    TEST_ASSERT_EQUAL_UINT32(16, boot2.stats().restored);
    boot2.onAck("fall-0", 11);
    boot2.onIngestResult("fall-1", hashOf("fall-1"), IngestDisposition::RejectedPermanent, "event_uuid_conflict", 0, 12);
  }
  CriticalEventOutbox boot3(product);
  boot3.begin(&store, 20);
  TEST_ASSERT_EQUAL_UINT32(14, boot3.stats().pending);
  TEST_ASSERT_EQUAL_UINT32(1, boot3.stats().quarantined);
  TEST_ASSERT_FALSE(boot3.contains("fall-0"));
  TEST_ASSERT_TRUE(boot3.isQuarantined("fall-1"));
  for (int i = 2; i < 12; ++i) {
    TEST_ASSERT_TRUE(boot3.contains("fall-" + std::to_string(i)));
  }
  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_TRUE(boot3.contains("sos-" + std::to_string(i)));
  }
  TEST_ASSERT_EQUAL_UINT32(15, store.files.size());
}

// ---------------------------------------------------------------- codec / migracao

void testCodecV2RoundTripsStateAndPriority() {
  OutboxEntry entry;
  entry.uuid = "u:b1-2:00000003";
  entry.topic = kTopic;
  entry.payload = payloadFor("u");
  entry.order = 9;
  entry.priority = EventPriority::Sos;
  entry.state = EntryState::Quarantined;
  entry.quarantineReason = "identity_rejected";
  const std::string encoded = OutboxCodec::encode(entry);
  TEST_ASSERT_EQUAL(0, encoded.compare(0, 5, "CEO2\n"));
  OutboxEntry decoded;
  TEST_ASSERT_TRUE(OutboxCodec::decode(encoded, decoded));
  TEST_ASSERT_EQUAL(static_cast<int>(EventPriority::Sos), static_cast<int>(decoded.priority));
  TEST_ASSERT_EQUAL(static_cast<int>(EntryState::Quarantined), static_cast<int>(decoded.state));
  TEST_ASSERT_EQUAL_STRING("identity_rejected", decoded.quarantineReason.c_str());
}

std::string legacyCeo1(const std::string& uuid, const std::string& payload) {
  std::string body = "CEO1\n";
  body += "order=5\n";
  body += "created=77\n";
  body += "uuid=" + std::to_string(uuid.size()) + "\n" + uuid + "\n";
  body += "topic=" + std::to_string(kTopic.size()) + "\n" + kTopic + "\n";
  body += "payload=" + std::to_string(payload.size()) + "\n" + payload + "\n";
  body += "crc=" + EventIdentity::hex8(OutboxCodec::crc32(body)) + "\n";
  return body;
}

void testLegacyCeo1FilesAreRestoredAfterUpgrade() {
  MemoryOutboxStore store;
  store.files[OutboxCodec::fileStemFor("old")] = legacyCeo1("old", payloadFor("old"));
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  TEST_ASSERT_TRUE(outbox.contains("old"));
  TEST_ASSERT_EQUAL_UINT32(0, outbox.stats().corruptedOnLoad);
  // Hash recalculado no boot: resultado v1 continua correlacionavel apos upgrade.
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Removed),
                    static_cast<int>(outbox.onIngestResult("old", hashOf("old"), IngestDisposition::Persisted, "", 0, 1)));
  // CEO1 corrompido continua detectado.
  std::string broken = legacyCeo1("x", payloadFor("x"));
  broken[8] = static_cast<char>(broken[8] ^ 0x01);
  OutboxEntry decoded;
  TEST_ASSERT_FALSE(OutboxCodec::decode(broken, decoded));
}

void testTmpRecoveryNeverLosesRewrittenEvent() {
  OutboxEntry entry;
  entry.uuid = "u1";
  entry.topic = kTopic;
  entry.payload = payloadFor("u1");
  const std::string encoded = OutboxCodec::encode(entry);
  using R = OutboxCodec::TmpRecovery;
  // Corte depois de gravar o .tmp e antes de remover o .evt: .evt antigo vale.
  TEST_ASSERT_EQUAL(static_cast<int>(R::DiscardTmp), static_cast<int>(OutboxCodec::decideTmpRecovery(true, encoded)));
  // Corte entre remove(.evt) e rename(.tmp): promover, senao o evento sumiria.
  TEST_ASSERT_EQUAL(static_cast<int>(R::PromoteTmp), static_cast<int>(OutboxCodec::decideTmpRecovery(false, encoded)));
  // Primeira escrita truncada (nunca aceita): descartar.
  TEST_ASSERT_EQUAL(static_cast<int>(R::DiscardTmp),
                    static_cast<int>(OutboxCodec::decideTmpRecovery(false, encoded.substr(0, encoded.size() / 2))));
  TEST_ASSERT_EQUAL(static_cast<int>(R::DiscardTmp), static_cast<int>(OutboxCodec::decideTmpRecovery(false, "")));
}

// ---------------------------------------------------------------- configuracao de produto

void testFactoryDefaultsDoNotUsePublicBrokerOrInsecureTls() {
  // F-17: nenhum broker de fabrica (publico ou nao) e TLS nunca inseguro por padrao.
  TEST_ASSERT_EQUAL_UINT32(0, std::strlen(AppConfig::DEFAULT_MQTT_HOST));
  TEST_ASSERT_FALSE(AppConfig::DEFAULT_MQTT_TLS_INSECURE);
}

void testProductOutboxBudgetIsCoherent() {
  // Reservas deixam espaco para eventos normais e cabem na capacidade.
  TEST_ASSERT_TRUE(AppConfig::CRITICAL_OUTBOX_SOS_RESERVE + AppConfig::CRITICAL_OUTBOX_FALL_RESERVE <
                   AppConfig::CRITICAL_OUTBOX_CAPACITY);
  TEST_ASSERT_TRUE(AppConfig::CRITICAL_OUTBOX_SOS_RESERVE >= 1U);
  // Payload maximo + topico + cabecalho cabem no buffer MQTT.
  TEST_ASSERT_TRUE(AppConfig::CRITICAL_OUTBOX_MAX_PAYLOAD_BYTES + 128U <= AppConfig::MQTT_PACKET_BUFFER_SIZE);
  // Pior caso da outbox+quarentena (payload maximo + ~512 B de cabecalho do codec)
  // abaixo de 10% da particao de dados do default.csv do esp32dev (spiffs 0x160000).
  const std::size_t worstCaseBytes =
      (AppConfig::CRITICAL_OUTBOX_CAPACITY + AppConfig::CRITICAL_OUTBOX_QUARANTINE_CAPACITY) *
      (AppConfig::CRITICAL_OUTBOX_MAX_PAYLOAD_BYTES + 512U);
  TEST_ASSERT_TRUE(worstCaseBytes <= 0x160000U / 10U);
  TEST_ASSERT_EQUAL_INT(1, AppConfig::CRITICAL_INGEST_RESULT_VERSION);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testSha256KnownVectors);
  RUN_TEST(testParserAcceptsValidResults);
  RUN_TEST(testParserRejectsForgedOrInconsistentResults);
  RUN_TEST(testPersistedResultRemovesOnlyWithMatchingHash);
  RUN_TEST(testRejectedPermanentQuarantinesWithoutCountingAsAck);
  RUN_TEST(testQuarantineSurvivesRebootAndDoesNotOccupyActiveSlots);
  RUN_TEST(testQuarantineFullKeepsEventActiveWithSlowRetry);
  RUN_TEST(testQuarantineStoreWriteFailureKeepsEventActive);
  RUN_TEST(testLateCommitReleasesQuarantineAndExplicitClear);
  RUN_TEST(testRetryableKeepsEventAndHonorsBoundedRetryAfter);
  RUN_TEST(testReservesKeepSlotsForSosAndFall);
  RUN_TEST(testReserveConfigurationIsClamped);
  RUN_TEST(testCapacity16_17_100WithTwoRebootsNeverDropsAccepted);
  RUN_TEST(testCodecV2RoundTripsStateAndPriority);
  RUN_TEST(testLegacyCeo1FilesAreRestoredAfterUpgrade);
  RUN_TEST(testTmpRecoveryNeverLosesRewrittenEvent);
  RUN_TEST(testFactoryDefaultsDoNotUsePublicBrokerOrInsecureTls);
  RUN_TEST(testProductOutboxBudgetIsCoherent);
  return UNITY_END();
}
