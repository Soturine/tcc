const assert = require("node:assert/strict");
const test = require("node:test");

const migration = require("../../../database/migrations/002_telemetry_retention_index");

function migrationContext(indexPresent = false) {
  const calls = [];
  let exists = indexPresent;

  return {
    calls,
    context: {
      connection: { id: "test" },
      execute: async (_executor, sql) => {
        calls.push(sql);
        if (/ADD KEY/.test(sql)) {
          exists = true;
        }
        if (/DROP INDEX/.test(sql)) {
          exists = false;
        }
        return [];
      },
      indexExists: async () => exists,
    },
  };
}

test("migration adiciona indice de cutoff sem alterar dados", async () => {
  const harness = migrationContext();
  const result = await migration.up(harness.context);
  const sql = harness.calls.join("\n");

  assert.deepEqual(result.columns, ["created_at", "id"]);
  assert.match(sql, /ADD KEY idx_telemetry_created_id \(created_at, id\)/);
  assert.doesNotMatch(sql, /DELETE|TRUNCATE|DROP TABLE/i);
});

test("migration e rollback sao idempotentes e limitados ao indice", async () => {
  const harness = migrationContext(true);

  await migration.up(harness.context);
  assert.equal(harness.calls.length, 0);

  await migration.down(harness.context);
  await migration.down(harness.context);
  assert.deepEqual(harness.calls, [
    "ALTER TABLE telemetry_logs DROP INDEX idx_telemetry_created_id",
  ]);
});
