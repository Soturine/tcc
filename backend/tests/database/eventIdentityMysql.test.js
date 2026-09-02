const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const mysql = require("mysql2/promise");

const { removeDatabaseStatements } = require("../../scripts/sqlUtils");

const integrationEnabled = process.env.MYSQL_INTEGRATION === "1";
const integrationTest = integrationEnabled ? test : test.skip;
const databasePrefix = `queda_event_identity_${process.pid}`;

function connectionOptions(database = undefined) {
  return {
    host: process.env.MYSQL_HOST || "127.0.0.1",
    port: Number(process.env.MYSQL_PORT || 3306),
    user: process.env.MYSQL_USER || "root",
    password: process.env.MYSQL_PASSWORD || "",
    database,
    multipleStatements: true,
    timezone: "Z",
  };
}

function quoteIdentifier(identifier) {
  if (!/^queda_event_identity_\d+_[a-z]+$/.test(identifier)) {
    throw new Error(`Nome de banco temporario inesperado: ${identifier}`);
  }
  return `\`${identifier}\``;
}

async function withDatabase(suffix, work) {
  const database = `${databasePrefix}_${suffix}`;
  const admin = await mysql.createConnection(connectionOptions());
  await admin.query(`CREATE DATABASE ${quoteIdentifier(database)} CHARACTER SET utf8mb4`);
  const pool = mysql.createPool({ ...connectionOptions(database), connectionLimit: 4 });

  try {
    await work({ database, pool });
  } finally {
    await pool.end();
    await admin.query(`DROP DATABASE ${quoteIdentifier(database)}`);
    await admin.end();
  }
}

async function createBaselineEventsTable(pool) {
  await pool.query(`
    CREATE TABLE events (
      id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
      device_id BIGINT UNSIGNED NOT NULL,
      device_assignment_history_id BIGINT UNSIGNED NULL,
      event_type VARCHAR(80) NOT NULL,
      event_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
      raw_payload_json JSON NULL,
      created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
      PRIMARY KEY (id)
    )
  `);
}

integrationTest("runner migra baseline, registra historico, reexecuta e reverte", async () => {
  await withDatabase("upgrade", async ({ pool }) => {
    await createBaselineEventsTable(pool);
    await pool.execute(
      `INSERT INTO events (device_id, event_type, raw_payload_json, created_at)
       VALUES
       (1, 'sos_pressed', ?, '2026-05-13 14:38:15'),
       (1, 'device_event', ?, '2026-05-13 14:39:15'),
       (1, 'device_event', ?, '2026-05-13 14:40:15'),
       (1, 'device_event', ?, '2026-05-13 14:41:15')`,
      [
        JSON.stringify({
          event_uuid: "evt-upgrade-001",
          timestamp: 1_768_000_000,
          event_uptime_ms: 4500,
          boot_id: "boot-upgrade-001",
        }),
        JSON.stringify({ event_type: "device_event" }),
        JSON.stringify({ event_uuid: "   " }),
        JSON.stringify({ event_uuid: 42 }),
      ],
    );

    const { runMigrations } = require("../../scripts/migrationRunner");
    const first = await runMigrations({ databasePool: pool });
    const second = await runMigrations({ databasePool: pool });
    const [rows] = await pool.query(
      `SELECT event_uuid, occurred_at_device, received_at, persisted_at,
              boot_id, device_uptime_ms, clock_quality
       FROM events ORDER BY id`,
    );
    const [indexes] = await pool.query(
      "SHOW INDEX FROM events WHERE Key_name = 'uq_events_event_uuid'",
    );
    const [history] = await pool.query("SELECT version, name FROM schema_migrations");

    assert.deepEqual(first.applied, ["001"]);
    assert.deepEqual(second.applied, []);
    assert.equal(rows[0].event_uuid, "evt-upgrade-001");
    assert.equal(rows[0].boot_id, "boot-upgrade-001");
    assert.equal(Number(rows[0].device_uptime_ms), 4500);
    assert.equal(rows[0].received_at, null);
    assert.ok(rows[0].occurred_at_device instanceof Date);
    assert.ok(rows[0].persisted_at instanceof Date);
    assert.equal(rows[0].clock_quality, "unknown");
    assert.equal(rows[1].event_uuid, null);
    assert.equal(rows[2].event_uuid, null);
    assert.equal(rows[3].event_uuid, null);
    assert.equal(indexes[0].Non_unique, 0);
    assert.deepEqual(history.map((row) => [row.version, row.name]), [["001", "event_identity"]]);

    const reverted = await runMigrations({ direction: "down", databasePool: pool });
    const [columnsAfterDown] = await pool.query(
      "SHOW COLUMNS FROM events WHERE Field = 'event_uuid'",
    );
    const [historyAfterDown] = await pool.query("SELECT version FROM schema_migrations");
    assert.deepEqual(reverted.reverted, ["001"]);
    assert.equal(columnsAfterDown.length, 0);
    assert.equal(historyAfterDown.length, 0);
  });
});

integrationTest("migration recusa duplicatas historicas sem fabricar identidade", async () => {
  await withDatabase("duplicates", async ({ pool }) => {
    await createBaselineEventsTable(pool);
    const raw = JSON.stringify({ event_uuid: "evt-existing-duplicate" });
    await pool.execute(
      `INSERT INTO events (device_id, event_type, raw_payload_json)
       VALUES (1, 'sos_pressed', ?), (2, 'sos_pressed', ?)`,
      [raw, raw],
    );

    const { runMigrations } = require("../../scripts/migrationRunner");
    await assert.rejects(
      runMigrations({ databasePool: pool }),
      (error) => error.code === "EVENT_UUID_DUPLICATES_FOUND"
        && error.audit.rawDuplicateGroups[0].eventCount === 2,
    );
    const [columns] = await pool.query("SHOW COLUMNS FROM events WHERE Field = 'event_uuid'");
    const [history] = await pool.query("SELECT version FROM schema_migrations");
    assert.equal(columns.length, 0);
    assert.equal(history.length, 0);
  });
});

integrationTest("constraint resolve concorrencia e impede alerta duplicado", async () => {
  await withDatabase("concurrency", async ({ pool }) => {
    const schemaPath = path.resolve(__dirname, "../../../database/schema.sql");
    const seedPath = path.resolve(__dirname, "../../../database/seed.sql");
    await pool.query(removeDatabaseStatements(fs.readFileSync(schemaPath, "utf8")));
    await pool.query(removeDatabaseStatements(fs.readFileSync(seedPath, "utf8")));
    const [deviceInsert] = await pool.execute(
      `INSERT INTO devices (device_uid, device_identifier, name)
       VALUES ('test:concurrency', 'test_concurrency', 'Concurrency test')`,
    );
    const device = {
      id: deviceInsert.insertId,
      deviceUid: "test:concurrency",
      deviceIdentifier: "test_concurrency",
      organization: null,
      currentPatient: null,
      currentAssignmentHistoryId: null,
    };
    const payload = {
      event_uuid: "evt-concurrency-001",
      event_type: "sos_pressed",
      event_sequence: 1,
      event_uptime_ms: 9000,
      timestamp: 1_768_000_000,
    };
    const { recordEventFromMqtt } = require("../../src/services/eventService");
    const { createAlertForEvent } = require("../../src/services/alertService");

    async function ingest() {
      const connection = await pool.getConnection();
      try {
        await connection.beginTransaction();
        const event = await recordEventFromMqtt({
          device,
          payload,
          receivedAt: new Date("2026-09-02T20:00:00.000Z"),
        }, connection);
        if (!event.deduplicated) {
          await createAlertForEvent(event, connection);
        }
        await connection.commit();
        return event;
      } catch (error) {
        await connection.rollback();
        throw error;
      } finally {
        connection.release();
      }
    }

    const results = await Promise.all([ingest(), ingest()]);
    const [eventCount] = await pool.query(
      "SELECT COUNT(*) AS total FROM events WHERE event_uuid = 'evt-concurrency-001'",
    );
    const [alertCount] = await pool.query(
      `SELECT COUNT(*) AS total
       FROM alerts a INNER JOIN events e ON e.id = a.event_id
       WHERE e.event_uuid = 'evt-concurrency-001'`,
    );

    assert.equal(eventCount[0].total, 1);
    assert.equal(alertCount[0].total, 1);
    assert.equal(results.filter((event) => event.deduplicated).length, 1);

    const connection = await pool.getConnection();
    try {
      await connection.beginTransaction();
      await assert.rejects(
        recordEventFromMqtt({
          device,
          payload: { ...payload, event_type: "fall_detected" },
          receivedAt: new Date("2026-09-02T20:01:00.000Z"),
        }, connection),
        (error) => error.code === "EVENT_UUID_CONFLICT",
      );
      await connection.rollback();
    } finally {
      connection.release();
    }
  });
});
