#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "critical_ack.h"
#include "critical_outbox.h"
#include "critical_result.h"

// Laco de entrega critica independente do cliente MQTT (ADR-020: interface de
// transporte). PubSubClient hoje; ESP-MQTT/QoS 1 apenas apos HIL. Qualquer
// transporte so precisa dizer se esta pronto e tentar publicar: a garantia de
// entrega continua na outbox + confirmacao de aplicacao pos-COMMIT.

class CriticalTransport {
 public:
  virtual ~CriticalTransport() = default;
  // Conectado E com as assinaturas de confirmacao aceitas: sem elas, publicar
  // geraria retries cegos.
  virtual bool ready() = 0;
  // true = aceito pelo cliente local. NAO significa entrega nem persistencia.
  virtual bool publish(const std::string& topic, const std::string& payload) = 0;
};

namespace CriticalDelivery {

enum class ServiceOutcome { NotReady, Idle, Sent, PublishFailed };

struct ServiceReport {
  ServiceOutcome outcome = ServiceOutcome::Idle;
  std::string uuid;
};

// Tenta transmitir no maximo UM item vencido (nao bloqueia o laco principal).
ServiceReport serviceOnce(CriticalEventOutbox& outbox, CriticalTransport& transport, uint32_t nowMs);

struct InboundConfig {
  std::string ackTopic;     // critical-event-acks (legado)
  std::string resultTopic;  // critical-event-results
  std::string deviceId;
  bool acceptLegacyAck = true;
};

enum class InboundKind { Result, LegacyAck, LegacyAckDisabled, InvalidResult, InvalidAck };

struct InboundReport {
  InboundKind kind = InboundKind::InvalidAck;
  CriticalResult::ParseStatus resultParse = CriticalResult::ParseStatus::Ok;
  CriticalAck::ParseStatus ackParse = CriticalAck::ParseStatus::Ok;
  CriticalResult::Result result;
  CriticalAck::Ack ack;
  ResultOutcome resultOutcome = ResultOutcome::UnknownUuid;
  AckResult ackOutcome = AckResult::UnknownUuid;
};

InboundReport handleInbound(CriticalEventOutbox& outbox,
                            const InboundConfig& config,
                            const std::string& topic,
                            const char* payload,
                            std::size_t length,
                            uint32_t nowMs);

const char* describe(ResultOutcome outcome);
const char* describe(AckResult outcome);

}  // namespace CriticalDelivery
