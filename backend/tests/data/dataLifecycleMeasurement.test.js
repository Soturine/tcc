const assert = require("node:assert/strict");
const test = require("node:test");

const {
  buildProportions,
  deriveConfiguredRowsPerDay,
} = require("../../src/services/dataLifecycleMeasurementService");

test("cadencia configura crescimento derivado sem chamar de medido", () => {
  assert.equal(deriveConfiguredRowsPerDay(2000), 43200);
  assert.equal(deriveConfiguredRowsPerDay(500), 172800);
  assert.equal(deriveConfiguredRowsPerDay(null), null);
  assert.equal(deriveConfiguredRowsPerDay(0), null);
});

test("proporcao usa apenas contagens observadas selecionadas", () => {
  const result = buildProportions({
    telemetry_logs: 90,
    events: 5,
    alerts: 4,
    audit_logs: 1,
  });

  assert.equal(result.classification, "derived");
  assert.equal(result.denominatorRows, 100);
  assert.deepEqual(
    result.tables.map((entry) => [entry.table, entry.percentOfSelectedRows]),
    [
      ["telemetry_logs", 90],
      ["events", 5],
      ["alerts", 4],
      ["audit_logs", 1],
    ],
  );
});
