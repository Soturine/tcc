#include "critical_result.h"

#include <ArduinoJson.h>

#include <cstring>

namespace CriticalResult {

namespace {
constexpr std::size_t kMaxResultPayloadBytes = 768;
constexpr std::size_t kMaxUuidLength = 160;
constexpr std::size_t kHashLength = 64;

bool isLowerHex64(const char* value) {
  if (value == nullptr || std::strlen(value) != kHashLength) {
    return false;
  }
  for (std::size_t i = 0; i < kHashLength; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

bool parseStatus(const char* value, Status& status) {
  if (value == nullptr) {
    return false;
  }
  const std::string text(value);
  if (text == "committed") {
    status = Status::Committed;
  } else if (text == "duplicate") {
    status = Status::Duplicate;
  } else if (text == "retryable") {
    status = Status::Retryable;
  } else if (text == "rejected_permanent") {
    status = Status::RejectedPermanent;
  } else {
    return false;
  }
  return true;
}

bool reasonAllowed(Status status, const std::string& reason) {
  if (status == Status::RejectedPermanent) {
    return reason == "event_uuid_conflict" || reason == "invalid_event_uuid" ||
           reason == "identity_rejected";
  }
  if (status == Status::Retryable) {
    return reason == "storage_unavailable" || reason == "internal_error";
  }
  return false;
}
}  // namespace

bool confirmsPersistence(Status status) {
  return status == Status::Committed || status == Status::Duplicate;
}

ParseStatus parse(const std::string& expectedTopic,
                  const std::string& receivedTopic,
                  const std::string& expectedDeviceId,
                  const char* payload,
                  std::size_t length,
                  Result& result) {
  if (expectedTopic.empty() || receivedTopic != expectedTopic) {
    return ParseStatus::WrongTopic;
  }
  if (payload == nullptr || length == 0U || length > kMaxResultPayloadBytes) {
    return ParseStatus::InvalidJson;
  }

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok || !doc.is<JsonObject>()) {
    return ParseStatus::InvalidJson;
  }
  if (!doc["schema_version"].is<int>() || doc["schema_version"].as<int>() != 1) {
    return ParseStatus::UnsupportedSchema;
  }

  const char* deviceId = doc["device_id"].as<const char*>();
  if (deviceId == nullptr || expectedDeviceId.empty() || expectedDeviceId != deviceId) {
    return ParseStatus::WrongDevice;
  }

  const char* uuid = doc["event_uuid"].as<const char*>();
  if (uuid == nullptr || std::strlen(uuid) == 0U || std::strlen(uuid) > kMaxUuidLength) {
    return ParseStatus::InvalidUuid;
  }

  const char* hash = doc["payload_sha256"].as<const char*>();
  if (!isLowerHex64(hash)) {
    return ParseStatus::InvalidHash;
  }

  Status status;
  if (!parseStatus(doc["status"].as<const char*>(), status)) {
    return ParseStatus::InvalidStatus;
  }

  Result parsed;
  parsed.deviceId = deviceId;
  parsed.eventUuid = uuid;
  parsed.payloadSha256 = hash;
  parsed.status = status;

  if (confirmsPersistence(status)) {
    const char* committedAt = doc["committed_at"].as<const char*>();
    if (!doc["event_id"].is<int64_t>() || doc["event_id"].as<int64_t>() < 1 ||
        committedAt == nullptr || committedAt[0] == '\0' || !doc["reason_code"].isNull()) {
      return ParseStatus::InconsistentFields;
    }
    parsed.eventId = doc["event_id"].as<int64_t>();
  } else {
    // Resultado negativo nunca carrega identidade de persistencia.
    const char* reason = doc["reason_code"].as<const char*>();
    if (!doc["event_id"].isNull() || !doc["committed_at"].isNull() || reason == nullptr ||
        !reasonAllowed(status, reason)) {
      return ParseStatus::InconsistentFields;
    }
    parsed.reasonCode = reason;
  }

  const JsonVariantConst retryAfter = doc["retry_after_ms"];
  if (!retryAfter.isNull()) {
    if (status != Status::Retryable || !retryAfter.is<int64_t>() || retryAfter.as<int64_t>() < 0 ||
        retryAfter.as<int64_t>() > static_cast<int64_t>(kMaxRetryAfterMs)) {
      return ParseStatus::InconsistentFields;
    }
    parsed.retryAfterMs = static_cast<uint32_t>(retryAfter.as<int64_t>());
    parsed.hasRetryAfter = true;
  }

  result = parsed;
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
    case ParseStatus::WrongDevice:
      return "wrong_device";
    case ParseStatus::InvalidUuid:
      return "invalid_uuid";
    case ParseStatus::InvalidHash:
      return "invalid_hash";
    case ParseStatus::InvalidStatus:
      return "invalid_status";
    case ParseStatus::InconsistentFields:
      return "inconsistent_fields";
  }
  return "unknown";
}

const char* describe(Status status) {
  switch (status) {
    case Status::Committed:
      return "committed";
    case Status::Duplicate:
      return "duplicate";
    case Status::Retryable:
      return "retryable";
    case Status::RejectedPermanent:
      return "rejected_permanent";
  }
  return "unknown";
}

}  // namespace CriticalResult
