#include <unity.h>

#include <string>
#include <vector>

#include "critical_delivery.h"
#include "sha256.h"
#include "support/memory_outbox_store.h"

// Laco de entrega e roteamento de confirmacoes extraidos de main.cpp (ADR-020).
namespace {

const std::string kTopic = "queda/devices/esp32_01/events";
const std::string kAckTopic = "queda/devices/esp32_01/critical-event-acks";
const std::string kResultTopic = "queda/devices/esp32_01/critical-event-results";

class FakeTransport : public CriticalTransport {
 public:
  bool isReady = true;
  bool accept = true;
  std::vector<std::string> published;
  bool ready() override {
    return isReady;
  }
  bool publish(const std::string& topic, const std::string& payload) override {
    published.push_back(topic + "|" + payload);
    return accept;
  }
};

std::string payloadFor(const std::string& uuid) {
  return "{\"event_type\":\"fall_detected\",\"event_uuid\":\"" + uuid + "\"}";
}

OutboxConfig config() {
  OutboxConfig value;
  value.capacity = 4;
  value.maxPayloadBytes = 512;
  value.retryBaseMs = 1000;
  value.retryMaxMs = 8000;
  return value;
}

CriticalDelivery::InboundConfig inbound(bool legacy = true) {
  CriticalDelivery::InboundConfig value;
  value.ackTopic = kAckTopic;
  value.resultTopic = kResultTopic;
  value.deviceId = "esp32_01";
  value.acceptLegacyAck = legacy;
  return value;
}

std::string result(const std::string& status, const std::string& uuid, const std::string& hash,
                   const std::string& device = "esp32_01") {
  const bool persisted = status == "committed" || status == "duplicate";
  const std::string tail =
      persisted ? "\"reason_code\":null,\"event_id\":3,\"committed_at\":\"2026-10-25T00:00:00Z\",\"retry_after_ms\":null"
      : status == "retryable"
          ? "\"reason_code\":\"storage_unavailable\",\"event_id\":null,\"committed_at\":null,\"retry_after_ms\":2000"
          : "\"reason_code\":\"event_uuid_conflict\",\"event_id\":null,\"committed_at\":null,\"retry_after_ms\":null";
  return "{\"schema_version\":1,\"device_id\":\"" + device + "\",\"event_uuid\":\"" + uuid +
         "\",\"payload_sha256\":\"" + hash + "\",\"status\":\"" + status + "\"," + tail +
         ",\"issued_at\":\"2026-10-25T00:00:00Z\"}";
}

CriticalDelivery::InboundReport deliver(CriticalEventOutbox& outbox, const std::string& topic,
                                        const std::string& json, bool legacy = true) {
  return CriticalDelivery::handleInbound(outbox, inbound(legacy), topic, json.c_str(), json.size(), 100);
}

void testNotReadyTransportNeverPublishes() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);
  FakeTransport transport;
  transport.isReady = false;  // conectado sem assinatura => sem envio cego
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::NotReady),
                    static_cast<int>(CriticalDelivery::serviceOnce(outbox, transport, 0).outcome));
  TEST_ASSERT_EQUAL_UINT32(0, transport.published.size());
  TEST_ASSERT_EQUAL_UINT32(0, outbox.stats().publishAttempts);
}

void testServiceSendsOneDueItemPerCallAndKeepsIt() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);
  outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0, EventPriority::Fall);
  FakeTransport transport;
  const auto first = CriticalDelivery::serviceOnce(outbox, transport, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::Sent), static_cast<int>(first.outcome));
  TEST_ASSERT_EQUAL_STRING("e1", first.uuid.c_str());
  TEST_ASSERT_EQUAL_UINT32(1, transport.published.size());
  TEST_ASSERT_EQUAL_STRING((kTopic + "|" + payloadFor("e1")).c_str(), transport.published[0].c_str());
  TEST_ASSERT_TRUE(outbox.contains("e1"));  // publish aceito nao remove
  TEST_ASSERT_EQUAL_STRING("e2", CriticalDelivery::serviceOnce(outbox, transport, 0).uuid.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::Idle),
                    static_cast<int>(CriticalDelivery::serviceOnce(outbox, transport, 1).outcome));
}

void testPublishFailureIsCountedAndRetried() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);
  FakeTransport transport;
  transport.accept = false;
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::PublishFailed),
                    static_cast<int>(CriticalDelivery::serviceOnce(outbox, transport, 0).outcome));
  TEST_ASSERT_EQUAL_UINT32(1, outbox.stats().publishFailures);
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::Idle),
                    static_cast<int>(CriticalDelivery::serviceOnce(outbox, transport, 500).outcome));
  transport.accept = true;
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::ServiceOutcome::Sent),
                    static_cast<int>(CriticalDelivery::serviceOnce(outbox, transport, 1000).outcome));
}

void testInboundRoutesResultsAndLegacyAcks() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);
  outbox.enqueue("e2", kTopic, payloadFor("e2"), 2, 0, EventPriority::Fall);
  outbox.enqueue("e3", kTopic, payloadFor("e3"), 3, 0, EventPriority::Fall);

  auto report = deliver(outbox, kResultTopic, result("committed", "e1", Sha256::hex(payloadFor("e1"))));
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::InboundKind::Result), static_cast<int>(report.kind));
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Removed), static_cast<int>(report.resultOutcome));
  TEST_ASSERT_FALSE(outbox.contains("e1"));

  report = deliver(outbox, kResultTopic, result("rejected_permanent", "e2", Sha256::hex(payloadFor("e2"))));
  TEST_ASSERT_EQUAL(static_cast<int>(ResultOutcome::Quarantined), static_cast<int>(report.resultOutcome));
  TEST_ASSERT_TRUE(outbox.isQuarantined("e2"));

  const std::string ack =
      "{\"schema_version\":1,\"event_uuid\":\"e3\",\"event_id\":9,\"status\":\"committed\",\"committed_at\":\"x\"}";
  report = deliver(outbox, kAckTopic, ack);
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::InboundKind::LegacyAck), static_cast<int>(report.kind));
  TEST_ASSERT_EQUAL(static_cast<int>(AckResult::Removed), static_cast<int>(report.ackOutcome));
  TEST_ASSERT_EQUAL_UINT32(0, outbox.stats().pending);
}

void testCrossDeviceResultAndDisabledLegacyAckAreIgnored() {
  MemoryOutboxStore store;
  CriticalEventOutbox outbox(config());
  outbox.begin(&store, 0);
  outbox.enqueue("e1", kTopic, payloadFor("e1"), 1, 0, EventPriority::Fall);
  const std::string hash = Sha256::hex(payloadFor("e1"));

  auto report = deliver(outbox, kResultTopic, result("committed", "e1", hash, "esp32_02"));
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::InboundKind::InvalidResult), static_cast<int>(report.kind));
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalResult::ParseStatus::WrongDevice), static_cast<int>(report.resultParse));
  // Resultado valido em topico de outro device cai no caminho legado e e recusado pelo topico.
  report = deliver(outbox, "queda/devices/esp32_02/critical-event-results", result("committed", "e1", hash));
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::InboundKind::InvalidAck), static_cast<int>(report.kind));
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalAck::ParseStatus::WrongTopic), static_cast<int>(report.ackParse));

  const std::string ack =
      "{\"schema_version\":1,\"event_uuid\":\"e1\",\"event_id\":9,\"status\":\"committed\",\"committed_at\":\"x\"}";
  report = deliver(outbox, kAckTopic, ack, /*legacy=*/false);
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalDelivery::InboundKind::LegacyAckDisabled), static_cast<int>(report.kind));
  TEST_ASSERT_TRUE(outbox.contains("e1"));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testNotReadyTransportNeverPublishes);
  RUN_TEST(testServiceSendsOneDueItemPerCallAndKeepsIt);
  RUN_TEST(testPublishFailureIsCountedAndRetried);
  RUN_TEST(testInboundRoutesResultsAndLegacyAcks);
  RUN_TEST(testCrossDeviceResultAndDisabledLegacyAckAreIgnored);
  return UNITY_END();
}
