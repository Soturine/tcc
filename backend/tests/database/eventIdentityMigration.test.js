const assert = require("node:assert/strict");
const test = require("node:test");

const migration = require("../../../database/migrations/001_event_identity");
const { loadWithMocks } = require("../helpers/moduleSandbox");

function audit(overrides = {}) {
  return {
    totalEvents: 4,
    rawUuidMissing: 1,
    rawUuidInvalid: 1,
    rawUuidRecoverable: 2,
    rawDuplicateGroups: [],
    hasEventUuidColumn: false,
    structured: {
      missingRecoverable: 0,
      divergent: 0,
      duplicateGroups: [],
    },
    blockingIssues: 0,
    ...overrides,
  };
}

function migrationContext(options = {}) {
  const calls = [];
  const existingColumns = new Set(options.existingColumns || []);
  let auditCall = 0;
  const audits = options.audits || [audit(), audit({ hasEventUuidColumn: true })];

  return {
    calls,
    context: {
      auditEventIdentity: async () => audits[Math.min(auditCall++, audits.length - 1)],
      columnExists: async (_tableName, columnName) => existingColumns.has(columnName),
      connection: { id: "test" },
      execute: async (_executor, sql, params = []) => {
        calls.push({ sql, params });
        const addedColumn = sql.match(/ALTER TABLE events ADD COLUMN ([a-z_]+)/i)?.[1];
        if (addedColumn) {
          existingColumns.add(addedColumn);
        }
        const droppedColumn = sql.match(/ALTER TABLE events DROP COLUMN ([a-z_]+)/i)?.[1];
        if (droppedColumn) {
          existingColumns.delete(droppedColumn);
        }
        return [];
      },
      indexExists: async () => Boolean(options.indexExists),
      log: () => undefined,
    },
  };
}

test("auditoria classifica UUID ausente, invalido, recuperavel e duplicado", async () => {
  const responses = [
    [{ found: 1 }],
    [],
    [{
      total_events: 7,
      raw_uuid_missing: 2,
      raw_uuid_invalid: 1,
      raw_uuid_recoverable: 4,
    }],
    [{ event_uuid: "evt-duplicate", event_count: 2, device_count: 2 }],
  ];
  const fakePool = {
    execute: async () => responses.shift(),
  };
  const { module: auditModule, restore } = loadWithMocks(
    "src/db/eventIdentityAudit.js",
    { "src/db/pool.js": fakePool },
  );

  try {
    const result = await auditModule.auditEventIdentity();
    assert.equal(result.totalEvents, 7);
    assert.equal(result.rawUuidMissing, 2);
    assert.equal(result.rawUuidInvalid, 1);
    assert.equal(result.rawUuidRecoverable, 4);
    assert.deepEqual(result.rawDuplicateGroups, [{
      eventUuid: "evt-duplicate",
      eventCount: 2,
      deviceCount: 2,
    }]);
    assert.equal(result.blockingIssues, 1);
  } finally {
    restore();
  }
});

test("migration bloqueia UUID duplicado antes de alterar schema", async () => {
  const duplicateAudit = audit({
    rawDuplicateGroups: [{ eventUuid: "evt-duplicate", eventCount: 2, deviceCount: 1 }],
    blockingIssues: 1,
  });
  const harness = migrationContext({ audits: [duplicateAudit] });

  await assert.rejects(
    migration.up(harness.context),
    (error) => error.code === "EVENT_UUID_DUPLICATES_FOUND" && error.audit === duplicateAudit,
  );
  assert.equal(harness.calls.length, 0);
});

test("migration promove somente identidade recuperavel e cria UNIQUE global", async () => {
  const harness = migrationContext();
  const result = await migration.up(harness.context);
  const sql = harness.calls.map((call) => call.sql).join("\n");

  assert.equal(result.rawUuidMissing, 1);
  assert.equal(result.rawUuidInvalid, 1);
  assert.match(sql, /ADD COLUMN event_uuid VARCHAR\(160\) NULL/);
  assert.match(sql, /SET event_uuid = TRIM\(JSON_UNQUOTE/);
  assert.match(sql, /SET persisted_at = created_at/);
  assert.match(sql, /ADD UNIQUE KEY uq_events_event_uuid \(event_uuid\)/);
  assert.match(sql, /MODIFY persisted_at DATETIME\(3\) NOT NULL/);
  assert.doesNotMatch(sql, /\bDROP\b|\bTRUNCATE\b|\bDELETE FROM events\b/i);
});

test("migration falha se validacao posterior divergir do JSON", async () => {
  const invalidAfter = audit({
    hasEventUuidColumn: true,
    structured: {
      missingRecoverable: 1,
      divergent: 0,
      duplicateGroups: [],
    },
    blockingIssues: 1,
  });
  const harness = migrationContext({ audits: [audit(), invalidAfter] });

  await assert.rejects(
    migration.up(harness.context),
    (error) => error.code === "EVENT_UUID_BACKFILL_VALIDATION_FAILED",
  );
  assert.equal(
    harness.calls.some((call) => /ADD UNIQUE KEY/.test(call.sql)),
    false,
  );
});

test("rollback explicito remove somente indice e colunas da migration", async () => {
  const harness = migrationContext({
    existingColumns: [
      "event_uuid",
      "occurred_at_device",
      "received_at",
      "persisted_at",
      "boot_id",
      "device_uptime_ms",
      "clock_quality",
    ],
    indexExists: true,
  });

  await migration.down(harness.context);
  const sql = harness.calls.map((call) => call.sql).join("\n");

  assert.match(sql, /DROP INDEX uq_events_event_uuid/);
  assert.match(sql, /DROP COLUMN event_uuid/);
  assert.doesNotMatch(sql, /DROP TABLE|TRUNCATE|DELETE FROM events/i);
});
