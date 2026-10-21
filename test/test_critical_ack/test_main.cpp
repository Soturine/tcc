#include <unity.h>

#include <cstring>
#include <string>

#include "critical_ack.h"

namespace {

const std::string kAckTopic = "queda/devices/esp32_01/critical-event-acks";

CriticalAck::ParseStatus parse(const std::string& topic, const char* json, CriticalAck::Ack& ack) {
  return CriticalAck::parse(kAckTopic, topic, json, std::strlen(json), ack);
}

void testValidCommittedAndDuplicateAcks() {
  CriticalAck::Ack ack;
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalAck::ParseStatus::Ok),
                    static_cast<int>(parse(kAckTopic,
                                           "{\"schema_version\":1,\"event_uuid\":\"u:b1-2:00000001\","
                                           "\"event_id\":123,\"status\":\"committed\","
                                           "\"committed_at\":\"2026-10-21T00:00:00.250Z\"}",
                                           ack)));
  TEST_ASSERT_EQUAL_STRING("u:b1-2:00000001", ack.eventUuid.c_str());
  TEST_ASSERT_EQUAL_INT64(123, ack.eventId);

  TEST_ASSERT_EQUAL(static_cast<int>(CriticalAck::ParseStatus::Ok),
                    static_cast<int>(parse(kAckTopic,
                                           "{\"schema_version\":1,\"event_uuid\":\"x\",\"event_id\":9,"
                                           "\"status\":\"duplicate\",\"committed_at\":\"t\"}",
                                           ack)));
  TEST_ASSERT_EQUAL_STRING("duplicate", ack.status.c_str());
}

void testAckFromAnotherDeviceTopicIsRejected() {
  CriticalAck::Ack ack;
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalAck::ParseStatus::WrongTopic),
                    static_cast<int>(parse("queda/devices/other/critical-event-acks",
                                           "{\"schema_version\":1,\"event_uuid\":\"x\",\"event_id\":1,"
                                           "\"status\":\"committed\",\"committed_at\":\"t\"}",
                                           ack)));
}

void testMalformedAcksNeverValidate() {
  struct Case {
    const char* json;
    CriticalAck::ParseStatus expected;
  };
  const Case cases[] = {
      {"not json", CriticalAck::ParseStatus::InvalidJson},
      {"[1,2]", CriticalAck::ParseStatus::InvalidJson},
      {"{\"schema_version\":2,\"event_uuid\":\"x\",\"event_id\":1,\"status\":\"committed\",\"committed_at\":\"t\"}",
       CriticalAck::ParseStatus::UnsupportedSchema},
      {"{\"schema_version\":1,\"event_uuid\":\"\",\"event_id\":1,\"status\":\"committed\",\"committed_at\":\"t\"}",
       CriticalAck::ParseStatus::InvalidUuid},
      {"{\"schema_version\":1,\"event_uuid\":5,\"event_id\":1,\"status\":\"committed\",\"committed_at\":\"t\"}",
       CriticalAck::ParseStatus::InvalidUuid},
      {"{\"schema_version\":1,\"event_uuid\":\"x\",\"event_id\":0,\"status\":\"committed\",\"committed_at\":\"t\"}",
       CriticalAck::ParseStatus::InvalidEventId},
      {"{\"schema_version\":1,\"event_uuid\":\"x\",\"event_id\":1,\"status\":\"rejected\",\"committed_at\":\"t\"}",
       CriticalAck::ParseStatus::InvalidStatus},
      {"{\"schema_version\":1,\"event_uuid\":\"x\",\"event_id\":1,\"status\":\"committed\"}",
       CriticalAck::ParseStatus::InvalidCommittedAt},
  };
  for (const Case& item : cases) {
    CriticalAck::Ack ack;
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(item.expected),
                              static_cast<int>(parse(kAckTopic, item.json, ack)),
                              item.json);
    TEST_ASSERT_TRUE(ack.eventUuid.empty());
  }
}

void testOversizedPayloadIsRejectedBeforeParsing() {
  const std::string huge(600, ' ');
  CriticalAck::Ack ack;
  TEST_ASSERT_EQUAL(static_cast<int>(CriticalAck::ParseStatus::InvalidJson),
                    static_cast<int>(CriticalAck::parse(kAckTopic, kAckTopic, huge.c_str(), huge.size(), ack)));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testValidCommittedAndDuplicateAcks);
  RUN_TEST(testAckFromAnotherDeviceTopicIsRejected);
  RUN_TEST(testMalformedAcksNeverValidate);
  RUN_TEST(testOversizedPayloadIsRejectedBeforeParsing);
  return UNITY_END();
}
