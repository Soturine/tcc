#include <unity.h>

#include <set>
#include <string>

#include "critical_outbox.h"
#include "event_identity.h"
#include "outbox_codec.h"
#include "support/memory_outbox_store.h"

namespace {

const std::string kTopic = "queda/devices/esp32_01/events";

std::string payloadFor(const std::string& uuid) {
  return "{\"event_type\":\"fall_detected\",\"event_uuid\":\"" + uuid + "\"}";
}

OutboxConfig smallConfig(std::size_t capacity = 3) {
  OutboxConfig config;
  config.capacity = capacity;
  config.maxPayloadBytes = 256;
  config.retryBaseMs = 1000;
  config.retryMaxMs = 8000;
  return config;
}

// ---------------------------------------------------------------- identidade

void testUuidIsStableFormatAndIncrementsWithinBoot() {
  EventIdentity::Generator generator;
  generator.begin("esp32-aabbccddeeff", 7, 0x1234abcdU, true);
  const auto first = generator.next();
  const auto second = generator.next();

  TEST_ASSERT_EQUAL_STRING("b00000007-1234abcd", generator.bootId().c_str());
  TEST_ASSERT_EQUAL_STRING("esp32-aabbccddeeff:b00000007-1234abcd:00000001", first.uuid.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, second.sequence);
  TEST_ASSERT_TRUE(first.uuid != second.uuid);
  TEST_ASSERT_TRUE(generator.orderFor(first.sequence) < generator.orderFor(second.sequence));
}

void testRebootWithoutClockDoesNotCollide() {
  // Antes: uid-tipo-segundos(millis/1000)-millis-seq colidia em boots deterministicos.
  std::set<std::string> seen;
  for (uint32_t boot = 1; boot <= 50; ++boot) {
    EventIdentity::Generator generator;
    generator.begin("esp32-aabbccddeeff", boot, 0xdeadbeefU, true);
    TEST_ASSERT_TRUE(seen.insert(generator.next().uuid).second);
  }
}

void testCounterPersistenceFailureIsCoveredByNonce() {
  // Contador nao gravado: dois boots com o mesmo contador e sequencia, nonces distintos.
  EventIdentity::Generator first;
  EventIdentity::Generator second;
  first.begin("esp32-aabbccddeeff", 0, 0x00000001U, false);
  second.begin("esp32-aabbccddeeff", 0, 0x00000002U, false);
  TEST_ASSERT_FALSE(first.bootCounterPersisted());
  TEST_ASSERT_TRUE(first.next().uuid != second.next().uuid);
}

void testUuidRespectsContractLength() {
  EventIdentity::Generator generator;
  generator.begin(std::string(400, 'x'), UINT32_MAX, UINT32_MAX, true);
  TEST_ASSERT_TRUE(generator.next().uuid.size() <= EventIdentity::kMaxUuidLength);

  EventIdentity::Generator empty;
  empty.begin("", 1, 1, true);
  TEST_ASSERT_TRUE(empty.next().uuid.rfind("unknown-device:", 0) == 0);
}

// ---------------------------------------------------------------- codec

void testCodecRoundTripAndCorruptionDetection() {
  OutboxEntry entry;
  entry.uuid = "u:b1-2:00000003";
  entry.topic = kTopic;
  entry.payload = "{\"a\":\"line1\\nline2\",\"crc=\":1}\n";
  entry.order = (5ULL << 32U) | 3U;
  entry.createdUptimeMs = 4294967295U;

  const std::string encoded = OutboxCodec::encode(entry);
  OutboxEntry decoded;
  TEST_ASSERT_TRUE(OutboxCodec::decode(encoded, decoded));
  TEST_ASSERT_EQUAL_STRING(entry.payload.c_str(), decoded.payload.c_str());
  TEST_ASSERT_TRUE(decoded.order == entry.order);
  TEST_ASSERT_EQUAL_UINT32(entry.createdUptimeMs, decoded.createdUptimeMs);

  std::string flipped = encoded;
  flipped[20] = static_cast<char>(flipped[20] ^ 0x01);
  TEST_ASSERT_FALSE(OutboxCodec::decode(flipped, decoded));
  TEST_ASSERT_FALSE(OutboxCodec::decode(encoded.substr(0, encoded.size() / 2), decoded));
  TEST_ASSERT_FALSE(OutboxCodec::decode("", decoded));
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, OutboxCodec::crc32("123456789"));
}

// ---------------------------------------------------------------- outbox

void testPublishDoesNotRemoveOnlyAckDoes() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);

  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                    static_cast<int>(outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 100)));
  TEST_ASSERT_EQUAL_UINT32(1, store.files.size());

  const OutboxEntry* due = outbox.nextDue(100);
  TEST_ASSERT_NOT_NULL(due);
  outbox.onPublishAttempt("e1", 100, true);
  TEST_ASSERT_TRUE(outbox.contains("e1"));
  TEST_ASSERT_NULL(outbox.nextDue(500));
  TEST_ASSERT_NOT_NULL(outbox.nextDue(1100));  // ACK perdido -> retry com o mesmo UUID

  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::Removed), static_cast<int>(outbox.onAck("e1", 1200)));
  TEST_ASSERT_FALSE(outbox.contains("e1"));
  TEST_ASSERT_EQUAL_UINT32(0, store.files.size());
  TEST_ASSERT_TRUE(outbox.stats().hasLastAck);
}

void testDuplicateAndUnknownAcksAreIdempotent() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
  outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0);

  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::Removed), static_cast<int>(outbox.onAck("e1", 10)));
  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::DuplicateAck), static_cast<int>(outbox.onAck("e1", 11)));
  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::UnknownUuid), static_cast<int>(outbox.onAck("ghost", 12)));
  TEST_ASSERT_TRUE(outbox.contains("e2"));

  const OutboxStats stats = outbox.stats();
  TEST_ASSERT_EQUAL_UINT32(1, stats.acked);
  TEST_ASSERT_EQUAL_UINT32(1, stats.duplicateAcks);
  TEST_ASSERT_EQUAL_UINT32(1, stats.unknownAcks);
}

void testRebootRestoresPendingInOrderAndAllowsImmediateRetry() {
  MemoryOutboxStore store;
  {
    CriticalEventOutbox beforeReboot(smallConfig());
    beforeReboot.begin(&store, 0);
    beforeReboot.enqueue("late", kTopic, payloadFor("late"), (2ULL << 32U) | 1U, 50);
    beforeReboot.enqueue("early", kTopic, payloadFor("early"), (1ULL << 32U) | 9U, 60);
    beforeReboot.onPublishAttempt("early", 70, true);
  }

  CriticalEventOutbox afterReboot(smallConfig());
  afterReboot.begin(&store, 5);
  const OutboxStats stats = afterReboot.stats();
  TEST_ASSERT_EQUAL_UINT32(2, stats.pending);
  TEST_ASSERT_EQUAL_UINT32(2, stats.restored);
  const OutboxEntry* due = afterReboot.nextDue(5);
  TEST_ASSERT_NOT_NULL(due);
  TEST_ASSERT_EQUAL_STRING("early", due->uuid.c_str());
  TEST_ASSERT_EQUAL_STRING(payloadFor("early").c_str(), due->payload.c_str());
}

void testOverflowRejectsNewEventWithoutDroppingAccepted() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig(2));
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
  outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0);

  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedFull),
                    static_cast<int>(outbox.enqueue("e3", kTopic, payloadFor("e3"), 3, 0)));
  TEST_ASSERT_TRUE(outbox.contains("e1"));
  TEST_ASSERT_TRUE(outbox.contains("e2"));
  const OutboxStats stats = outbox.stats();
  TEST_ASSERT_EQUAL_UINT32(1, stats.overflowCount);
  TEST_ASSERT_EQUAL_STRING("e3", stats.lastOverflowUuid.c_str());

  outbox.onAck("e1", 1);
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::Persisted),
                    static_cast<int>(outbox.enqueue("e4", kTopic, payloadFor("e4"), 4, 0)));
}

void testInvalidOversizedAndDuplicateEnqueue() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedInvalid),
                    static_cast<int>(outbox.enqueue("", kTopic, "{}", 1, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::RejectedTooLarge),
                    static_cast<int>(outbox.enqueue("big", kTopic, std::string(300, 'x'), 1, 0)));
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::DuplicateUuid),
                    static_cast<int>(outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0)));
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().pending);
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().tooLargeCount);
}

void testPersistenceFailureIsObservableAndRetried() {
  MemoryOutboxStore store;
  store.failWrites = true;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);

  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::AcceptedVolatile),
                    static_cast<int>(outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0)));
  TEST_ASSERT_TRUE(outbox.contains("e1"));
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().notPersisted);
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().persistFailures);

  store.failWrites = false;
  TEST_ASSERT_EQUAL_UINT32(1, outbox.retryPersistence());
  TEST_ASSERT_EQUAL_UINT32(0, outbox.stats().notPersisted);
  TEST_ASSERT_EQUAL_UINT32(1, store.files.size());
}

void testStoreUnavailableStillKeepsEventsInRam() {
  MemoryOutboxStore store;
  store.isAvailable = false;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  TEST_ASSERT_FALSE(outbox.stats().storeAvailable);
  TEST_ASSERT_EQUAL(static_cast<int>(EnqueueResult::AcceptedVolatile),
                    static_cast<int>(outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0)));
  TEST_ASSERT_NOT_NULL(outbox.nextDue(0));
}

void testStoreRemoveFailureStillAcknowledgesAndReappearsSafely() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
  store.failRemoves = true;
  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::RemovedStoreError), static_cast<int>(outbox.onAck("e1", 5)));
  TEST_ASSERT_FALSE(outbox.contains("e1"));

  // Apos reboot o item reaparece e sera reenviado com o MESMO UUID; o backend deduplica.
  CriticalEventOutbox afterReboot(smallConfig());
  afterReboot.begin(&store, 0);
  TEST_ASSERT_TRUE(afterReboot.contains("e1"));
}

void testCorruptedFileIsCountedNotLoaded() {
  MemoryOutboxStore store;
  {
    CriticalEventOutbox outbox(smallConfig());
    outbox.begin(&store, 0);
    outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
    outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0);
  }
  std::string& file = store.files[OutboxCodec::fileStemFor("e2")];
  file.resize(file.size() - 7);  // escrita interrompida

  CriticalEventOutbox afterReboot(smallConfig());
  afterReboot.begin(&store, 0);
  TEST_ASSERT_EQUAL_UINT32(1, afterReboot.stats().pending);
  TEST_ASSERT_EQUAL_UINT32(1, afterReboot.stats().corruptedOnLoad);
  TEST_ASSERT_EQUAL_UINT32(2, store.files.size());  // nao apagado silenciosamente
}

void testSameOrderAcrossBootsNeverOverwritesPendingEvent() {
  // Contador de boot nao persistido: dois boots geram a mesma ordem (0<<32 | 1).
  MemoryOutboxStore store;
  {
    CriticalEventOutbox firstBoot(smallConfig());
    firstBoot.begin(&store, 0);
    firstBoot.enqueue("dev:b00000000-0000000a:00000001", kTopic, payloadFor("a"), 1, 0);
  }
  CriticalEventOutbox secondBoot(smallConfig());
  secondBoot.begin(&store, 0);
  secondBoot.enqueue("dev:b00000000-0000000b:00000001", kTopic, payloadFor("b"), 1, 0);

  TEST_ASSERT_EQUAL_UINT32(2, store.files.size());
  CriticalEventOutbox thirdBoot(smallConfig());
  thirdBoot.begin(&store, 0);
  TEST_ASSERT_TRUE(thirdBoot.contains("dev:b00000000-0000000a:00000001"));
  TEST_ASSERT_TRUE(thirdBoot.contains("dev:b00000000-0000000b:00000001"));
  TEST_ASSERT_TRUE(OutboxCodec::fileStemFor("a") != OutboxCodec::fileStemFor("b"));
  TEST_ASSERT_EQUAL_UINT32(17, OutboxCodec::fileStemFor("x").size());
}

void testRestoredItemAgeCountsFromRestoration() {
  MemoryOutboxStore store;
  {
    CriticalEventOutbox beforeReboot(smallConfig());
    beforeReboot.begin(&store, 0);
    beforeReboot.enqueue("e1", kTopic, payloadFor("e1"), 1, 900000);
  }
  CriticalEventOutbox afterReboot(smallConfig());
  afterReboot.begin(&store, 1500);
  TEST_ASSERT_EQUAL_UINT32(1500, afterReboot.stats().oldestCreatedUptimeMs);
}

void testBackoffIsExponentialCappedAndJittered() {
  CriticalEventOutbox deterministic(smallConfig());
  TEST_ASSERT_EQUAL_UINT32(0, deterministic.backoffForAttempt(0));
  TEST_ASSERT_EQUAL_UINT32(1000, deterministic.backoffForAttempt(1));
  TEST_ASSERT_EQUAL_UINT32(2000, deterministic.backoffForAttempt(2));
  TEST_ASSERT_EQUAL_UINT32(8000, deterministic.backoffForAttempt(4));
  TEST_ASSERT_EQUAL_UINT32(8000, deterministic.backoffForAttempt(60000));

  CriticalEventOutbox jittered(smallConfig(), []() { return 0xFFFFFFFFU; });
  const uint32_t delay = jittered.backoffForAttempt(1);
  TEST_ASSERT_TRUE(delay >= 1000 && delay <= 1250);
}

void testRetrySchedulingSurvivesMillisWraparound() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  const uint32_t nearWrap = 0xFFFFFF00U;
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, nearWrap);
  outbox.onPublishAttempt("e1", nearWrap, false);
  TEST_ASSERT_NULL(outbox.nextDue(nearWrap + 10U));
  TEST_ASSERT_NOT_NULL(outbox.nextDue(nearWrap + 1000U));  // ja passou de 0
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().publishFailures);
}

void testBackoffOfOneEntryDoesNotBlockOthers() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(smallConfig());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0);
  outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0);
  outbox.onPublishAttempt("e1", 0, true);
  const OutboxEntry* due = outbox.nextDue(1);
  TEST_ASSERT_NOT_NULL(due);
  TEST_ASSERT_EQUAL_STRING("e2", due->uuid.c_str());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testUuidIsStableFormatAndIncrementsWithinBoot);
  RUN_TEST(testRebootWithoutClockDoesNotCollide);
  RUN_TEST(testCounterPersistenceFailureIsCoveredByNonce);
  RUN_TEST(testUuidRespectsContractLength);
  RUN_TEST(testCodecRoundTripAndCorruptionDetection);
  RUN_TEST(testPublishDoesNotRemoveOnlyAckDoes);
  RUN_TEST(testDuplicateAndUnknownAcksAreIdempotent);
  RUN_TEST(testRebootRestoresPendingInOrderAndAllowsImmediateRetry);
  RUN_TEST(testOverflowRejectsNewEventWithoutDroppingAccepted);
  RUN_TEST(testInvalidOversizedAndDuplicateEnqueue);
  RUN_TEST(testPersistenceFailureIsObservableAndRetried);
  RUN_TEST(testStoreUnavailableStillKeepsEventsInRam);
  RUN_TEST(testStoreRemoveFailureStillAcknowledgesAndReappearsSafely);
  RUN_TEST(testCorruptedFileIsCountedNotLoaded);
  RUN_TEST(testSameOrderAcrossBootsNeverOverwritesPendingEvent);
  RUN_TEST(testRestoredItemAgeCountsFromRestoration);
  RUN_TEST(testBackoffIsExponentialCappedAndJittered);
  RUN_TEST(testRetrySchedulingSurvivesMillisWraparound);
  RUN_TEST(testBackoffOfOneEntryDoesNotBlockOthers);
  return UNITY_END();
}
