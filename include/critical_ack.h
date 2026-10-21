#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Validacao do ACK de aplicacao pos-commit (critical-event-ack-v1).
// O device so remove um item da outbox com ACK valido, recebido no SEU topico.
namespace CriticalAck {

enum class ParseStatus {
  Ok,
  WrongTopic,
  InvalidJson,
  UnsupportedSchema,
  InvalidUuid,
  InvalidEventId,
  InvalidStatus,
  InvalidCommittedAt
};

struct Ack {
  std::string eventUuid;
  int64_t eventId = 0;
  std::string status;  // "committed" | "duplicate"
  std::string committedAt;
};

ParseStatus parse(const std::string& expectedTopic,
                  const std::string& receivedTopic,
                  const char* payload,
                  std::size_t length,
                  Ack& ack);

const char* describe(ParseStatus status);

}  // namespace CriticalAck
