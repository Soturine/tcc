const assert = require("node:assert/strict");
const test = require("node:test");

const {
  buildProportions,
  deriveConfiguredRowsPerDay,
  deriveTelemetryGrowth,
} = require("../../src/services/dataLifecycleMeasurementService");

test("cadencia configura crescimento derivado sem chamar de medido", () => {
  assert.equal(deriveConfiguredRowsPerDay(2000), 43200);
  assert.equal(deriveConfiguredRowsPerDay(500), 172800);
  assert.equal(deriveConfiguredRowsPerDay(null), null);
  assert.equal(deriveConfiguredRowsPerDay(0), null);
});

test("crescimento por device/dia explicita derivacao e estimativa", () => {
  const result = deriveTelemetryGrowth(100, 3000, 2000, [{
    intervalMs: 2000,
    detectorMode: "normal",
    configuredRowsPerDeviceDay: 43200,
    condition: "continuous valid persistence at the configured interval",
  }]);

  assert.deepEqual(result.currentAllocation, {
    exactRowCount: 100,
    allocatedDataBytes: 3000,
    allocatedIndexBytes: 2000,
    allocatedBytesPerExactRow: 50,
    classification: "derived_from_measured_engine_allocation",
    caveat: "Allocated bytes include table/index overhead and are not logical payload size.",
  });
  assert.equal(result.byConfiguredCadence[0].approximateAllocatedBytesPerDeviceDay, 2160000);
  assert.equal(
    result.byConfiguredCadence[0].classification,
    "estimated_from_derived_cadence_and_current_allocation",
  );

  const empty = deriveTelemetryGrowth(0, 16384, 16384, []);
  assert.equal(empty.currentAllocation.allocatedBytesPerExactRow, null);
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
