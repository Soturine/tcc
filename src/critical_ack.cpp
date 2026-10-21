#include "critical_ack.h"

#include <ArduinoJson.h>

namespace CriticalAck {

namespace {
constexpr std::size_t kMaxAckPayloadBytes = 512;
constexpr std::size_t kMaxUuidLength = 160;
}  // namespace

ParseStatus parse(const std::string& expectedTopic,
                  const std::string& receivedTopic,
                  const char* payload,
                  std::size_t length,
                  Ack& ack) {
  if (expectedTopic.empty() || receivedTopic != expectedTopic) {
    return ParseStatus::WrongTopic;
  }
  if (payload == nullptr || length == 0U || length > kMaxAckPayloadBytes) {
    return ParseStatus::InvalidJson;
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok || !doc.is<JsonObject>()) {
    return ParseStatus::InvalidJson;
  }
  if (!doc["schema_version"].is<int>() || doc["schema_version"].as<int>() != 1) {
    return ParseStatus::UnsupportedSchema;
  }

  const char* uuid = doc["event_uuid"].as<const char*>();
  if (uuid == nullptr) {
    return ParseStatus::InvalidUuid;
  }
  const std::string uuidValue(uuid);
  if (uuidValue.empty() || uuidValue.size() > kMaxUuidLength) {
    return ParseStatus::InvalidUuid;
  }

  if (!doc["event_id"].is<int64_t>() || doc["event_id"].as<int64_t>() < 1) {
    return ParseStatus::InvalidEventId;
  }

  const char* status = doc["status"].as<const char*>();
  if (status == nullptr ||
      (std::string(status) != "committed" && std::string(status) != "duplicate")) {
    return ParseStatus::InvalidStatus;
  }

  const char* committedAt = doc["committed_at"].as<const char*>();
  if (committedAt == nullptr || std::string(committedAt).empty()) {
    return ParseStatus::InvalidCommittedAt;
  }

  ack.eventUuid = uuidValue;
  ack.eventId = doc["event_id"].as<int64_t>();
  ack.status = status;
  ack.committedAt = committedAt;
  return ParseStatus::Ok;
}

const char* describe(ParseStatus status) {
  switch (status) {
    case ParseStatus::Ok:
      return "ok";
    case ParseStatus::WrongTopic:
      return "wrong_topic";
    case ParseStatus::InvalidJson:
      return "invalid_json";
    case ParseStatus::UnsupportedSchema:
      return "unsupported_schema";
    case ParseStatus::InvalidUuid:
      return "invalid_uuid";
    case ParseStatus::InvalidEventId:
      return "invalid_event_id";
    case ParseStatus::InvalidStatus:
      return "invalid_status";
    case ParseStatus::InvalidCommittedAt:
      return "invalid_committed_at";
  }
  return "unknown";
}

}  // namespace CriticalAck
