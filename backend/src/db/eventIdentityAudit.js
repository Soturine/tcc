const { execute } = require("./pool");

const RAW_EVENT_UUID = "JSON_EXTRACT(raw_payload_json, '$.event_uuid')";
const NORMALIZED_RAW_EVENT_UUID = `TRIM(JSON_UNQUOTE(${RAW_EVENT_UUID}))`;
const VALID_RAW_EVENT_UUID = `
  JSON_TYPE(${RAW_EVENT_UUID}) = 'STRING'
  AND CHAR_LENGTH(${NORMALIZED_RAW_EVENT_UUID}) BETWEEN 1 AND 160
`;

function toCount(value) {
  return Number(value || 0);
}

async function columnExists(tableName, columnName, executor = null) {
  const rows = await execute(
    executor,
    `
      SELECT 1 AS found
      FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA = DATABASE()
        AND TABLE_NAME = ?
        AND COLUMN_NAME = ?
      LIMIT 1
    `,
    [tableName, columnName],
  );

  return rows.length > 0;
}

async function indexExists(tableName, indexName, executor = null) {
  const rows = await execute(
    executor,
    `
      SELECT 1 AS found
      FROM information_schema.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE()
        AND TABLE_NAME = ?
        AND INDEX_NAME = ?
      LIMIT 1
    `,
    [tableName, indexName],
  );

  return rows.length > 0;
}

async function tableExists(tableName, executor = null) {
  const rows = await execute(
    executor,
    `
      SELECT 1 AS found
      FROM information_schema.TABLES
      WHERE TABLE_SCHEMA = DATABASE()
        AND TABLE_NAME = ?
      LIMIT 1
    `,
    [tableName],
  );

  return rows.length > 0;
}

async function auditEventIdentity(executor = null) {
  if (!(await tableExists("events", executor))) {
    const error = new Error("Tabela events nao existe no banco selecionado.");
    error.code = "EVENTS_TABLE_MISSING";
    throw error;
  }

  const hasEventUuidColumn = await columnExists("events", "event_uuid", executor);
  const [rawCounts] = await execute(
    executor,
    `
      SELECT
        COUNT(*) AS total_events,
        COALESCE(SUM(${RAW_EVENT_UUID} IS NULL), 0) AS raw_uuid_missing,
        COALESCE(SUM(${RAW_EVENT_UUID} IS NOT NULL AND NOT (${VALID_RAW_EVENT_UUID})), 0)
          AS raw_uuid_invalid,
        COALESCE(SUM(${VALID_RAW_EVENT_UUID}), 0) AS raw_uuid_recoverable
      FROM events
    `,
  );
  const rawDuplicateGroups = await execute(
    executor,
    `
      SELECT
        ${NORMALIZED_RAW_EVENT_UUID} AS event_uuid,
        COUNT(*) AS event_count,
        COUNT(DISTINCT device_id) AS device_count
      FROM events
      WHERE ${VALID_RAW_EVENT_UUID}
      GROUP BY ${NORMALIZED_RAW_EVENT_UUID}
      HAVING COUNT(*) > 1
      ORDER BY event_count DESC, event_uuid ASC
    `,
  );

  let structured = {
    missingRecoverable: 0,
    divergent: 0,
    duplicateGroups: [],
  };

  if (hasEventUuidColumn) {
    const [structuredCounts] = await execute(
      executor,
      `
        SELECT
          COALESCE(SUM(event_uuid IS NULL AND (${VALID_RAW_EVENT_UUID})), 0)
            AS missing_recoverable,
          COALESCE(SUM(
            event_uuid IS NOT NULL
            AND (
              NOT (${VALID_RAW_EVENT_UUID})
              OR event_uuid <> ${NORMALIZED_RAW_EVENT_UUID}
            )
          ), 0) AS divergent
        FROM events
      `,
    );
    const duplicateGroups = await execute(
      executor,
      `
        SELECT event_uuid, COUNT(*) AS event_count
        FROM events
        WHERE event_uuid IS NOT NULL
        GROUP BY event_uuid
        HAVING COUNT(*) > 1
        ORDER BY event_count DESC, event_uuid ASC
      `,
    );

    structured = {
      missingRecoverable: toCount(structuredCounts.missing_recoverable),
      divergent: toCount(structuredCounts.divergent),
      duplicateGroups: duplicateGroups.map((row) => ({
        eventUuid: row.event_uuid,
        eventCount: toCount(row.event_count),
      })),
    };
  }

  const result = {
    totalEvents: toCount(rawCounts.total_events),
    rawUuidMissing: toCount(rawCounts.raw_uuid_missing),
    rawUuidInvalid: toCount(rawCounts.raw_uuid_invalid),
    rawUuidRecoverable: toCount(rawCounts.raw_uuid_recoverable),
    rawDuplicateGroups: rawDuplicateGroups.map((row) => ({
      eventUuid: row.event_uuid,
      eventCount: toCount(row.event_count),
      deviceCount: toCount(row.device_count),
    })),
    hasEventUuidColumn,
    structured,
  };

  result.blockingIssues =
    result.rawDuplicateGroups.length +
    result.structured.duplicateGroups.length +
    result.structured.missingRecoverable +
    result.structured.divergent;

  return result;
}

module.exports = {
  VALID_RAW_EVENT_UUID,
  auditEventIdentity,
  columnExists,
  indexExists,
  tableExists,
};
