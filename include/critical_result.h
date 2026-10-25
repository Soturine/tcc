#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Validacao de critical-event-result-v1 (ADR-026, F-29) recebido no topico
// <base>/<device_id>/critical-event-results. A autoridade de origem e a ACL do
// broker (somente o principal do backend publica nesse topico); o payload ainda
// precisa corresponder ao proprio device_id e, na outbox, ao hash do evento.
namespace CriticalResult {

enum class Status { Committed, Duplicate, Retryable, RejectedPermanent };

enum class ParseStatus {
  Ok,
  WrongTopic,
  InvalidJson,
  UnsupportedSchema,
  WrongDevice,
  InvalidUuid,
  InvalidHash,
  InvalidStatus,
  InconsistentFields
};

struct Result {
  std::string deviceId;
  std::string eventUuid;
  std::string payloadSha256;
  Status status = Status::Retryable;
  std::string reasonCode;  // vazio para committed/duplicate
  int64_t eventId = 0;     // > 0 somente para committed/duplicate
  uint32_t retryAfterMs = 0;
  bool hasRetryAfter = false;
};

constexpr uint32_t kMaxRetryAfterMs = 3600000U;

ParseStatus parse(const std::string& expectedTopic,
                  const std::string& receivedTopic,
                  const std::string& expectedDeviceId,
                  const char* payload,
                  std::size_t length,
                  Result& result);

bool confirmsPersistence(Status status);
const char* describe(ParseStatus status);
const char* describe(Status status);

}  // namespace CriticalResult
