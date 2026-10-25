#include "critical_delivery.h"

namespace CriticalDelivery {

ServiceReport serviceOnce(CriticalEventOutbox& outbox, CriticalTransport& transport, uint32_t nowMs) {
  ServiceReport report;
  if (!transport.ready()) {
    report.outcome = ServiceOutcome::NotReady;
    return report;
  }
  const OutboxEntry* due = outbox.nextDue(nowMs);
  if (due == nullptr) {
    report.outcome = ServiceOutcome::Idle;
    return report;
  }
  // Copias: onPublishAttempt pode reordenar o vetor interno.
  report.uuid = due->uuid;
  const std::string topic = due->topic;
  const std::string payload = due->payload;
  const bool accepted = transport.publish(topic, payload);
  // Transmitir NAO remove: o item permanece ate a confirmacao pos-commit.
  outbox.onPublishAttempt(report.uuid, nowMs, accepted);
  report.outcome = accepted ? ServiceOutcome::Sent : ServiceOutcome::PublishFailed;
  return report;
}

InboundReport handleInbound(CriticalEventOutbox& outbox,
                            const InboundConfig& config,
                            const std::string& topic,
                            const char* payload,
                            std::size_t length,
                            uint32_t nowMs) {
  InboundReport report;
  if (!config.resultTopic.empty() && topic == config.resultTopic) {
    report.resultParse =
        CriticalResult::parse(config.resultTopic, topic, config.deviceId, payload, length, report.result);
    if (report.resultParse != CriticalResult::ParseStatus::Ok) {
      report.kind = InboundKind::InvalidResult;
      return report;
    }
    const IngestDisposition disposition =
        CriticalResult::confirmsPersistence(report.result.status) ? IngestDisposition::Persisted
        : report.result.status == CriticalResult::Status::Retryable ? IngestDisposition::Retryable
                                                                    : IngestDisposition::RejectedPermanent;
    report.kind = InboundKind::Result;
    report.resultOutcome = outbox.onIngestResult(report.result.eventUuid,
                                                 report.result.payloadSha256,
                                                 disposition,
                                                 report.result.reasonCode,
                                                 report.result.retryAfterMs,
                                                 nowMs);
    return report;
  }

  if (!config.acceptLegacyAck) {
    report.kind = InboundKind::LegacyAckDisabled;
    return report;
  }
  report.ackParse = CriticalAck::parse(config.ackTopic, topic, payload, length, report.ack);
  if (report.ackParse != CriticalAck::ParseStatus::Ok) {
    report.kind = InboundKind::InvalidAck;
    return report;
  }
  report.kind = InboundKind::LegacyAck;
  report.ackOutcome = outbox.onAck(report.ack.eventUuid, nowMs);
  return report;
}

const char* describe(ResultOutcome outcome) {
  switch (outcome) {
    case ResultOutcome::Removed:
      return "removed";
    case ResultOutcome::RemovedStoreError:
      return "removed_store_error";
    case ResultOutcome::Quarantined:
      return "quarantined";
    case ResultOutcome::KeptQuarantineFull:
      return "kept_quarantine_full";
    case ResultOutcome::KeptStoreError:
      return "kept_store_error";
    case ResultOutcome::RetryScheduled:
      return "retry_scheduled";
    case ResultOutcome::DuplicateResult:
      return "duplicate_result";
    case ResultOutcome::UnknownUuid:
      return "unknown_uuid";
    case ResultOutcome::HashMismatch:
      return "hash_mismatch";
  }
  return "unknown";
}

const char* describe(AckResult outcome) {
  switch (outcome) {
    case AckResult::Removed:
      return "removed";
    case AckResult::RemovedStoreError:
      return "removed_store_error";
    case AckResult::DuplicateAck:
      return "duplicate_ack";
    case AckResult::UnknownUuid:
      return "unknown_uuid";
  }
  return "unknown";
}

}  // namespace CriticalDelivery
