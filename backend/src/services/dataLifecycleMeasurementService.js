const { pool } = require("../db/pool");

const TABLE_CLASSIFICATIONS = Object.freeze({
  device_status: "operational",
  telemetry_logs: "operational",
  battery_calibrations: "operational",
  events: "critical_evidence",
  event_telemetry_evidence: "critical_evidence",
  alerts: "human_response",
  alert_actions: "human_response",
  audit_logs: "audit",
  device_pairing_sessions: "audit",
  device_assignment_history: "audit",
  users: "identity_configuration",
  organizations: "identity_configuration",
  organization_members: "identity_configuration",
  caregiver_assignments: "identity_configuration",
  patients: "identity_configuration",
  devices: "identity_configuration",
});

const TRACKED_TABLES = Object.keys(TABLE_CLASSIFICATIONS);
const PROPORTION_TABLES = ["telemetry_logs", "events", "alerts", "audit_logs"];

function toNumber(value) {
  return Number(value || 0);
}

async function exactTableCounts(executor) {
  const entries = await Promise.all(TRACKED_TABLES.map(async (tableName) => {
    const [[row]] = await executor.query(`SELECT COUNT(*) AS total FROM \`${tableName}\``);
    return [tableName, toNumber(row.total)];
  }));
  return Object.fromEntries(entries);
}

async function storageMetadata(executor) {
  const placeholders = TRACKED_TABLES.map(() => "?").join(", ");
  const [rows] = await executor.execute(
    `
      SELECT
        TABLE_NAME AS table_name,
        TABLE_ROWS AS estimated_rows,
        DATA_LENGTH AS data_length_bytes,
        INDEX_LENGTH AS index_length_bytes,
        AVG_ROW_LENGTH AS estimated_avg_row_length_bytes
      FROM information_schema.TABLES
      WHERE TABLE_SCHEMA = DATABASE()
        AND TABLE_NAME IN (${placeholders})
      ORDER BY TABLE_NAME
    `,
    TRACKED_TABLES,
  );
  return new Map(rows.map((row) => [row.table_name, row]));
}

function deriveConfiguredRowsPerDay(intervalMs) {
  const parsed = Number(intervalMs);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    return null;
  }
  return Number((86_400_000 / parsed).toFixed(3));
}

async function telemetryObservations(executor) {
  const [[range]] = await executor.query(`
    SELECT
      COUNT(*) AS row_count,
      COUNT(DISTINCT device_id) AS device_count,
      MIN(created_at) AS first_persisted_at,
      MAX(created_at) AS last_persisted_at
    FROM telemetry_logs
    WHERE created_at IS NOT NULL
  `);
  const [dailyRows] = await executor.query(`
    SELECT
      DATE(created_at) AS observed_date,
      COUNT(*) AS row_count,
      COUNT(DISTINCT device_id) AS device_count
    FROM telemetry_logs
    WHERE created_at IS NOT NULL
    GROUP BY DATE(created_at)
    ORDER BY observed_date
  `);
  const [configuredRows] = await executor.query(`
    SELECT
      telemetry_interval_ms,
      COALESCE(detector_mode, 'unknown') AS detector_mode,
      COUNT(*) AS device_count
    FROM device_status
    WHERE telemetry_interval_ms IS NOT NULL
      AND telemetry_interval_ms > 0
    GROUP BY telemetry_interval_ms, COALESCE(detector_mode, 'unknown')
    ORDER BY telemetry_interval_ms, detector_mode
  `);

  return {
    persistedRange: {
      classification: "measured",
      rowCount: toNumber(range.row_count),
      deviceCount: toNumber(range.device_count),
      firstPersistedAt: range.first_persisted_at || null,
      lastPersistedAt: range.last_persisted_at || null,
    },
    persistedByUtcDate: dailyRows.map((row) => ({
      classification: "measured",
      date: row.observed_date,
      rowCount: toNumber(row.row_count),
      deviceCount: toNumber(row.device_count),
    })),
    configuredCadences: configuredRows.map((row) => ({
      intervalMs: toNumber(row.telemetry_interval_ms),
      detectorMode: row.detector_mode,
      deviceCount: toNumber(row.device_count),
      configuredRowsPerDeviceDay: deriveConfiguredRowsPerDay(row.telemetry_interval_ms),
      classification: "derived",
      condition: "continuous valid persistence at the configured interval",
    })),
  };
}

function buildProportions(counts) {
  const total = PROPORTION_TABLES.reduce((sum, tableName) => sum + counts[tableName], 0);
  return {
    classification: "derived",
    denominatorRows: total,
    tables: PROPORTION_TABLES.map((tableName) => ({
      table: tableName,
      rowCount: counts[tableName],
      percentOfSelectedRows: total === 0
        ? 0
        : Number(((counts[tableName] / total) * 100).toFixed(3)),
    })),
  };
}

async function measureDataLifecycle(options = {}) {
  const executor = options.databasePool || pool;
  const [counts, metadata, telemetry] = await Promise.all([
    exactTableCounts(executor),
    storageMetadata(executor),
    telemetryObservations(executor),
  ]);

  return {
    measuredAt: new Date().toISOString(),
    tables: TRACKED_TABLES.map((tableName) => {
      const row = metadata.get(tableName) || {};
      return {
        table: tableName,
        dataClass: TABLE_CLASSIFICATIONS[tableName],
        exactRowCount: counts[tableName],
        exactRowCountClassification: "measured",
        dataLengthBytes: toNumber(row.data_length_bytes),
        indexLengthBytes: toNumber(row.index_length_bytes),
        allocationClassification: "measured_engine_metadata",
        estimatedRows: toNumber(row.estimated_rows),
        estimatedAvgRowLengthBytes: toNumber(row.estimated_avg_row_length_bytes),
        rowEstimateClassification: "estimated_by_storage_engine",
      };
    }),
    telemetry,
    selectedRowProportions: buildProportions(counts),
    caveats: [
      "InnoDB TABLE_ROWS and AVG_ROW_LENGTH are storage-engine estimates.",
      "DATA_LENGTH and INDEX_LENGTH are allocated bytes, not logical payload bytes.",
      "Configured cadence derivation assumes continuous connectivity and valid samples; it is not observed persistence.",
      "This read-only measurement does not define a retention duration.",
    ],
  };
}

module.exports = {
  PROPORTION_TABLES,
  TABLE_CLASSIFICATIONS,
  TRACKED_TABLES,
  buildProportions,
  deriveConfiguredRowsPerDay,
  measureDataLifecycle,
};
