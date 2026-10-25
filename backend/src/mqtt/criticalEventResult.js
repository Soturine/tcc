const crypto = require("crypto");

/*
 * critical-event-result-v1 (ADR-026, F-29).
 *
 * Resultado de ingestao publicado no topico <base>/<device>/critical-event-results
 * somente para devices que declaram `ingest_result_version: 1` no evento.
 * Devices antigos continuam recebendo apenas critical-event-ack-v1.
 *
 * - committed / duplicate: o evento esta persistido (publicado SOMENTE apos COMMIT);
 * - retryable: nada foi confirmado; o device mantem o evento e retransmite;
 * - rejected_permanent: o backend nunca aceitara ESTES bytes. NAO e confirmacao de
 *   persistencia: o device move o evento para quarentena local, sem apaga-lo.
 *
 * `payload_sha256` e calculado sobre os bytes recebidos: o device so aplica o
 * resultado ao evento cujo payload armazenado tem o mesmo hash.
 */
const CRITICAL_EVENT_RESULT_SCHEMA_VERSION = 1;
const SUPPORTED_INGEST_RESULT_VERSIONS = new Set([1]);

const RESULT_STATUSES = Object.freeze({
  COMMITTED: "committed",
  DUPLICATE: "duplicate",
  RETRYABLE: "retryable",
  REJECTED_PERMANENT: "rejected_permanent",
});

// Codigos deliberadamente genericos: nao revelam a qual device/tenant pertence
// um device_uid ou event_uuid em conflito.
const REASON_CODES = Object.freeze({
  EVENT_UUID_CONFLICT: "event_uuid_conflict",
  INVALID_EVENT_UUID: "invalid_event_uuid",
  IDENTITY_REJECTED: "identity_rejected",
  STORAGE_UNAVAILABLE: "storage_unavailable",
  INTERNAL_ERROR: "internal_error",
});

const PERMANENT_ERROR_REASONS = new Map([
  ["EVENT_UUID_CONFLICT", REASON_CODES.EVENT_UUID_CONFLICT],
  ["INVALID_EVENT_UUID", REASON_CODES.INVALID_EVENT_UUID],
  ["DEVICE_IDENTITY_MISMATCH", REASON_CODES.IDENTITY_REJECTED],
]);

// Falhas de conectividade/disponibilidade do MySQL: o evento pode ser aceito depois.
const STORAGE_ERROR_CODES = new Set([
  "ECONNREFUSED",
  "ECONNRESET",
  "ETIMEDOUT",
  "EPIPE",
  "EHOSTUNREACH",
  "PROTOCOL_CONNECTION_LOST",
  "PROTOCOL_SEQUENCE_TIMEOUT",
  "PROTOCOL_ENQUEUE_AFTER_FATAL_ERROR",
  "ER_LOCK_WAIT_TIMEOUT",
  "ER_LOCK_DEADLOCK",
  "ER_CON_COUNT_ERROR",
  "ER_SERVER_SHUTDOWN",
  "POOL_CLOSED",
]);

const STORAGE_RETRY_AFTER_MS = 5000;

function sha256Hex(value) {
  return crypto.createHash("sha256").update(value).digest("hex");
}

function requestedIngestResultVersion(payload) {
  if (!payload || typeof payload !== "object" || Array.isArray(payload)) {
    return null;
  }
  const version = payload.ingest_result_version;
  return SUPPORTED_INGEST_RESULT_VERSIONS.has(version) ? version : null;
}

/**
 * Classifica uma falha de ingestao. Desconhecido => retryable: na duvida o device
 * mantem o evento (fail-safe), nunca o descarta.
 */
function classifyIngestError(error) {
  const code = error?.code ? String(error.code) : null;
  if (code && PERMANENT_ERROR_REASONS.has(code)) {
    return { status: RESULT_STATUSES.REJECTED_PERMANENT, reasonCode: PERMANENT_ERROR_REASONS.get(code) };
  }
  if (code && STORAGE_ERROR_CODES.has(code)) {
    return {
      status: RESULT_STATUSES.RETRYABLE,
      reasonCode: REASON_CODES.STORAGE_UNAVAILABLE,
      retryAfterMs: STORAGE_RETRY_AFTER_MS,
    };
  }
  return { status: RESULT_STATUSES.RETRYABLE, reasonCode: REASON_CODES.INTERNAL_ERROR };
}

function buildCriticalEventResult({
  deviceIdentifier,
  eventUuid,
  payloadSha256,
  status,
  reasonCode = null,
  event = null,
  retryAfterMs = null,
  issuedAt = new Date(),
}) {
  const persisted = status === RESULT_STATUSES.COMMITTED || status === RESULT_STATUSES.DUPLICATE;
  return {
    schema_version: CRITICAL_EVENT_RESULT_SCHEMA_VERSION,
    device_id: String(deviceIdentifier),
    event_uuid: String(eventUuid),
    payload_sha256: payloadSha256,
    status,
    reason_code: persisted ? null : reasonCode,
    event_id: persisted ? Number(event.id) : null,
    committed_at: persisted ? (event.persistedAt || issuedAt.toISOString()) : null,
    retry_after_ms: status === RESULT_STATUSES.RETRYABLE ? retryAfterMs : null,
    issued_at: issuedAt.toISOString(),
  };
}

function buildPersistedResult({ deviceIdentifier, event, payloadSha256 }) {
  return buildCriticalEventResult({
    deviceIdentifier,
    eventUuid: event.eventUuid,
    payloadSha256,
    status: event.deduplicated ? RESULT_STATUSES.DUPLICATE : RESULT_STATUSES.COMMITTED,
    event,
  });
}

module.exports = {
  CRITICAL_EVENT_RESULT_SCHEMA_VERSION,
  REASON_CODES,
  RESULT_STATUSES,
  STORAGE_RETRY_AFTER_MS,
  buildCriticalEventResult,
  buildPersistedResult,
  classifyIngestError,
  requestedIngestResultVersion,
  sha256Hex,
};
