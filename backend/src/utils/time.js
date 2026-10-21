const MIN_PLAUSIBLE_DEVICE_UNIX_SECONDS = 1_700_000_000;
const MAX_DEVICE_CLOCK_SKEW_SECONDS = 7 * 24 * 60 * 60;
const MAX_REALTIME_MQTT_CLOCK_SKEW_SECONDS = 10 * 60;

function toDateFromUnixSeconds(value) {
  const numericValue = Number(value);

  if (!Number.isFinite(numericValue)) {
    return new Date();
  }

  return new Date(numericValue * 1000);
}

function isPlausibleDeviceUnixSeconds(value) {
  const numericValue = Number(value);

  if (!Number.isFinite(numericValue)) {
    return false;
  }

  const maxFutureSeconds = Math.floor(Date.now() / 1000) + MAX_DEVICE_CLOCK_SKEW_SECONDS;
  return numericValue >= MIN_PLAUSIBLE_DEVICE_UNIX_SECONDS &&
    numericValue <= maxFutureSeconds;
}

function toDateFromDeviceTimestamp(value) {
  if (!isPlausibleDeviceUnixSeconds(value)) {
    return new Date();
  }

  return toDateFromUnixSeconds(value);
}

function resolveRealtimeMqttTimestamp(value, receivedAt = new Date()) {
  const receivedDate = receivedAt instanceof Date ? receivedAt : new Date(receivedAt);
  const safeReceivedAt = Number.isNaN(receivedDate.getTime()) ? new Date() : receivedDate;

  if (value == null || value === "") {
    return {
      date: safeReceivedAt,
      source: "backend_received_at",
      reason: "missing_device_timestamp",
      skewSeconds: 0,
    };
  }

  if (!isPlausibleDeviceUnixSeconds(value)) {
    return {
      date: safeReceivedAt,
      source: "backend_received_at",
      reason: "implausible_device_timestamp",
      skewSeconds: null,
    };
  }

  const deviceDate = toDateFromUnixSeconds(value);
  const skewSeconds = Math.round(
    (deviceDate.getTime() - safeReceivedAt.getTime()) / 1000,
  );

  if (Math.abs(skewSeconds) > MAX_REALTIME_MQTT_CLOCK_SKEW_SECONDS) {
    return {
      date: safeReceivedAt,
      source: "backend_received_at",
      reason: "device_clock_skew_exceeded",
      skewSeconds,
    };
  }

  return {
    date: deviceDate,
    source: "device_timestamp",
    reason: null,
    skewSeconds,
  };
}

// Tolerancia para relogio do device a frente do backend (drift/latencia).
const MAX_DEVICE_FUTURE_SKEW_SECONDS = 60;

/**
 * Instante em que um evento critico OCORREU, para correlacao (evidencia,
 * deduplicacao de alerta). Diferente da telemetria realtime, um evento entregue
 * apos periodo offline pode ser legitimamente antigo: usa `occurred_at_device`
 * somente quando o device declara relogio sincronizado e o valor e plausivel.
 * Sem isso, usa a regra realtime (que cai para `received_at`) e sinaliza a fonte.
 */
function resolveCriticalEventOccurrence(payload = {}, receivedAt = new Date()) {
  const receivedDate = receivedAt instanceof Date ? receivedAt : new Date(receivedAt);
  const safeReceivedAt = Number.isNaN(receivedDate.getTime()) ? new Date() : receivedDate;

  if (payload.clock_quality === "synced" && typeof payload.occurred_at_device === "string") {
    const occurred = new Date(payload.occurred_at_device);
    const deltaSeconds = (occurred.getTime() - safeReceivedAt.getTime()) / 1000;

    if (
      !Number.isNaN(occurred.getTime()) &&
      deltaSeconds <= MAX_DEVICE_FUTURE_SKEW_SECONDS &&
      deltaSeconds >= -MAX_DEVICE_CLOCK_SKEW_SECONDS
    ) {
      return {
        date: occurred,
        source: "device_occurred_at",
        reason: null,
        delaySeconds: Math.max(0, Math.round(-deltaSeconds)),
      };
    }

    return {
      ...resolveRealtimeMqttTimestamp(payload.timestamp, safeReceivedAt),
      reason: "implausible_occurred_at_device",
    };
  }

  return resolveRealtimeMqttTimestamp(payload.timestamp, safeReceivedAt);
}

function toIsoDate(value) {
  if (!value) {
    return null;
  }

  if (value instanceof Date) {
    return value.toISOString();
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? null : parsed.toISOString();
}

function parseDateBoundary(value, endOfDay = false) {
  if (!value) {
    return null;
  }

  const suffix = endOfDay ? "T23:59:59.999Z" : "T00:00:00.000Z";
  const parsed = new Date(`${value}${suffix}`);

  return Number.isNaN(parsed.getTime()) ? null : parsed;
}

module.exports = {
  isPlausibleDeviceUnixSeconds,
  parseDateBoundary,
  resolveCriticalEventOccurrence,
  resolveRealtimeMqttTimestamp,
  toDateFromDeviceTimestamp,
  toDateFromUnixSeconds,
  toIsoDate,
};
