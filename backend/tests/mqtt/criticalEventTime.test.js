const assert = require("node:assert/strict");
const test = require("node:test");

const { resolveCriticalEventOccurrence } = require("../../src/utils/time");

const receivedAt = new Date("2026-10-21T12:00:00.000Z");

test("occurred_at_device sincronizado e plausivel define o instante de ocorrencia", () => {
  const result = resolveCriticalEventOccurrence(
    { clock_quality: "synced", occurred_at_device: "2026-10-21T10:30:00.000Z" },
    receivedAt,
  );
  assert.equal(result.source, "device_occurred_at");
  assert.equal(result.date.toISOString(), "2026-10-21T10:30:00.000Z");
  assert.equal(result.delaySeconds, 5400);
});

test("relogio nao sincronizado nao e usado como instante de ocorrencia", () => {
  const result = resolveCriticalEventOccurrence(
    { clock_quality: "unsynced", occurred_at_device: "2026-10-21T10:30:00.000Z", timestamp: 1234 },
    receivedAt,
  );
  assert.equal(result.source, "backend_received_at");
  assert.equal(result.date.getTime(), receivedAt.getTime());
});

test("occurred_at_device no futuro ou antigo demais cai para a regra realtime", () => {
  for (const occurredAt of ["2026-10-21T12:05:00.000Z", "2026-09-01T00:00:00.000Z", "not-a-date"]) {
    const result = resolveCriticalEventOccurrence(
      { clock_quality: "synced", occurred_at_device: occurredAt },
      receivedAt,
    );
    assert.equal(result.source, "backend_received_at");
    assert.equal(result.reason, "implausible_occurred_at_device");
  }
});

test("pequeno adiantamento do relogio do device e tolerado", () => {
  const result = resolveCriticalEventOccurrence(
    { clock_quality: "synced", occurred_at_device: "2026-10-21T12:00:30.000Z" },
    receivedAt,
  );
  assert.equal(result.source, "device_occurred_at");
  assert.equal(result.delaySeconds, 0);
});
