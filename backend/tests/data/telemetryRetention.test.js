const assert = require("node:assert/strict");
const test = require("node:test");

const {
  DEFAULT_BATCH_SIZE,
  DEFAULT_MAX_BATCHES,
  normalizeRetentionConfig,
  runTelemetryRetention,
} = require("../../src/services/telemetryRetentionService");

const NOW = new Date("2026-09-04T12:00:00.000Z");

test("config exige cutoff passado com timezone e falha fechada", () => {
  for (const before of [undefined, "2026-09-01", "invalid", "2026-09-05T00:00:00Z"]) {
    assert.throws(
      () => normalizeRetentionConfig({ before }, { now: NOW }),
      (error) => error.code === "INVALID_RETENTION_CONFIG",
    );
  }

  assert.throws(
    () => normalizeRetentionConfig({ before: "2026-09-01T00:00:00Z", batchSize: 0 }, { now: NOW }),
    (error) => error.code === "INVALID_RETENTION_CONFIG",
  );
  assert.throws(
    () => normalizeRetentionConfig({ before: "2026-09-01T00:00:00Z", maxBatches: 1001 }, { now: NOW }),
    (error) => error.code === "INVALID_RETENTION_CONFIG",
  );
});

test("dry-run e default e nao abre transacao destrutiva", async () => {
  const responses = [
    [[{
      candidate_count: 3,
      oldest_candidate_at: new Date("2026-08-01T00:00:00Z"),
      newest_candidate_at: new Date("2026-08-03T00:00:00Z"),
    }], []],
    [[{ protected_count: 2 }], []],
    [[{ null_timestamp_count: 1 }], []],
  ];
  const calls = [];
  const fakePool = {
    execute: async (sql, params = []) => {
      calls.push({ sql, params });
      return responses.shift();
    },
    getConnection: async () => {
      throw new Error("dry-run nao deve abrir transacao");
    },
  };

  const result = await runTelemetryRetention(
    { before: "2026-09-01T00:00:00Z" },
    { databasePool: fakePool, log: {}, now: NOW },
  );

  assert.equal(result.mode, "dry_run");
  assert.equal(result.batchSize, DEFAULT_BATCH_SIZE);
  assert.equal(result.maxBatches, DEFAULT_MAX_BATCHES);
  assert.equal(result.candidateRows, 3);
  assert.equal(result.protectedEvidenceRows, 2);
  assert.equal(result.legacyNullTimestampRows, 1);
  assert.equal(result.deletedRows, 0);
  assert.equal(calls.length, 3);
  assert.ok(calls.every((call) => !/\bDELETE\b/i.test(call.sql)));
});

test("sem --apply explicito, valores falsy continuam em dry-run", () => {
  for (const apply of [undefined, false, "true", 1]) {
    const config = normalizeRetentionConfig(
      { before: "2026-09-01T00:00:00Z", apply },
      { now: NOW },
    );
    assert.equal(config.dryRun, true);
  }

  assert.equal(normalizeRetentionConfig({
    before: "2026-09-01T00:00:00Z",
    apply: true,
  }, { now: NOW }).dryRun, false);
});
