const VERSION = "001";
const NAME = "event_identity";
const UNIQUE_INDEX = "uq_events_event_uuid";

const EVENT_COLUMNS = [
  ["event_uuid", "event_uuid VARCHAR(160) NULL AFTER device_assignment_history_id"],
  ["occurred_at_device", "occurred_at_device DATETIME(3) NULL AFTER event_time"],
  ["received_at", "received_at DATETIME(3) NULL AFTER occurred_at_device"],
  ["persisted_at", "persisted_at DATETIME(3) NULL AFTER received_at"],
  ["boot_id", "boot_id VARCHAR(128) NULL AFTER persisted_at"],
  ["device_uptime_ms", "device_uptime_ms BIGINT UNSIGNED NULL AFTER boot_id"],
  [
    "clock_quality",
    "clock_quality ENUM('synced', 'unsynced', 'unknown') NOT NULL DEFAULT 'unknown' AFTER device_uptime_ms",
  ],
];

function migrationError(code, message, audit = null) {
  const error = new Error(message);
  error.code = code;
  error.audit = audit;
  return error;
}

async function ensureColumn(context, columnName, definition) {
  if (!(await context.columnExists("events", columnName, context.connection))) {
    await context.execute(context.connection, `ALTER TABLE events ADD COLUMN ${definition}`);
    context.log("info", "migration_column_added", { table: "events", column: columnName });
  }
}

async function up(context) {
  const before = await context.auditEventIdentity(context.connection);

  if (before.rawDuplicateGroups.length > 0) {
    throw migrationError(
      "EVENT_UUID_DUPLICATES_FOUND",
      "Backfill bloqueado: existem event_uuid duplicados no JSON historico.",
      before,
    );
  }

  for (const [columnName, definition] of EVENT_COLUMNS) {
    await ensureColumn(context, columnName, definition);
  }

  await context.execute(
    context.connection,
    `
      UPDATE events
      SET event_uuid = TRIM(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.event_uuid')))
      WHERE event_uuid IS NULL
        AND JSON_TYPE(JSON_EXTRACT(raw_payload_json, '$.event_uuid')) = 'STRING'
        AND CHAR_LENGTH(TRIM(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.event_uuid'))))
          BETWEEN 1 AND 160
    `,
  );
  await context.execute(
    context.connection,
    `
      UPDATE events
      SET occurred_at_device = TIMESTAMPADD(
        SECOND,
        CAST(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.timestamp')) AS UNSIGNED),
        '1970-01-01 00:00:00'
      )
      WHERE occurred_at_device IS NULL
        AND JSON_TYPE(JSON_EXTRACT(raw_payload_json, '$.timestamp')) = 'INTEGER'
        AND CAST(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.timestamp')) AS UNSIGNED)
          BETWEEN 1700000000 AND UNIX_TIMESTAMP(UTC_TIMESTAMP()) + 604800
    `,
  );
  await context.execute(
    context.connection,
    `
      UPDATE events
      SET boot_id = TRIM(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.boot_id')))
      WHERE boot_id IS NULL
        AND JSON_TYPE(JSON_EXTRACT(raw_payload_json, '$.boot_id')) = 'STRING'
        AND CHAR_LENGTH(TRIM(JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.boot_id'))))
          BETWEEN 1 AND 128
    `,
  );
  await context.execute(
    context.connection,
    `
      UPDATE events
      SET device_uptime_ms = CAST(
        COALESCE(
          JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.device_uptime_ms')),
          JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.event_uptime_ms'))
        ) AS UNSIGNED
      )
      WHERE device_uptime_ms IS NULL
        AND (
          JSON_TYPE(JSON_EXTRACT(raw_payload_json, '$.device_uptime_ms')) = 'INTEGER'
          OR JSON_TYPE(JSON_EXTRACT(raw_payload_json, '$.event_uptime_ms')) = 'INTEGER'
        )
    `,
  );
  await context.execute(
    context.connection,
    `
      UPDATE events
      SET clock_quality = JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.clock_quality'))
      WHERE JSON_UNQUOTE(JSON_EXTRACT(raw_payload_json, '$.clock_quality'))
        IN ('synced', 'unsynced', 'unknown')
    `,
  );
  await context.execute(
    context.connection,
    "UPDATE events SET persisted_at = created_at WHERE persisted_at IS NULL",
  );

  const afterBackfill = await context.auditEventIdentity(context.connection);
  if (
    afterBackfill.structured.missingRecoverable > 0 ||
    afterBackfill.structured.divergent > 0 ||
    afterBackfill.structured.duplicateGroups.length > 0
  ) {
    throw migrationError(
      "EVENT_UUID_BACKFILL_VALIDATION_FAILED",
      "Backfill de event_uuid nao passou na validacao estrutural.",
      afterBackfill,
    );
  }

  if (!(await context.indexExists("events", UNIQUE_INDEX, context.connection))) {
    await context.execute(
      context.connection,
      `ALTER TABLE events ADD UNIQUE KEY ${UNIQUE_INDEX} (event_uuid)`,
    );
  }
  await context.execute(
    context.connection,
    `
      ALTER TABLE events
      MODIFY persisted_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
    `,
  );

  return afterBackfill;
}

async function down(context) {
  if (await context.indexExists("events", UNIQUE_INDEX, context.connection)) {
    await context.execute(context.connection, `ALTER TABLE events DROP INDEX ${UNIQUE_INDEX}`);
  }

  for (const [columnName] of [...EVENT_COLUMNS].reverse()) {
    if (await context.columnExists("events", columnName, context.connection)) {
      await context.execute(context.connection, `ALTER TABLE events DROP COLUMN ${columnName}`);
    }
  }
}

module.exports = {
  NAME,
  UNIQUE_INDEX,
  VERSION,
  down,
  up,
};
