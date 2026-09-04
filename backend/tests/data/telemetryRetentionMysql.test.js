const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const mysql = require("mysql2/promise");

const { removeDatabaseStatements } = require("../../scripts/sqlUtils");
const { runTelemetryRetention } = require("../../src/services/telemetryRetentionService");

const integrationTest = process.env.MYSQL_INTEGRATION === "1" ? test : test.skip;
const databasePrefix = `queda_retention_${process.pid}`;

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
  if (!/^queda_retention_\d+_[a-z]+$/.test(identifier)) {
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
    const schemaPath = path.resolve(__dirname, "../../../database/schema.sql");
    await pool.query(removeDatabaseStatements(fs.readFileSync(schemaPath, "utf8")));
    await work(pool);
  } finally {
    await pool.end();
    await admin.query(`DROP DATABASE ${quoteIdentifier(database)}`);
    await admin.end();
  }
}

async function insertTelemetry(pool, deviceId, createdAt) {
  const [result] = await pool.execute(
    `INSERT INTO telemetry_logs (device_id, accel_magnitude, created_at)
     VALUES (?, 1.0, ?)`,
    [deviceId, createdAt],
  );
  return Number(result.insertId);
}

async function countByIds(pool, ids) {
  const placeholders = ids.map(() => "?").join(", ");
  const [[row]] = await pool.execute(
    `SELECT COUNT(*) AS total FROM telemetry_logs WHERE id IN (${placeholders})`,
    ids,
  );
  return Number(row.total);
}

integrationTest("retention protege evidence, audit e timestamps nulos em batches reexecutaveis", async () => {
  await withDatabase("safety", async (pool) => {
    await pool.query("ALTER TABLE telemetry_logs MODIFY created_at DATETIME NULL");
    const [deviceResult] = await pool.execute(
      `INSERT INTO devices (device_uid, device_identifier, name)
       VALUES ('test:retention', 'retention_test', 'Retention test')`,
    );
    const deviceId = Number(deviceResult.insertId);
    const oldOne = await insertTelemetry(pool, deviceId, "2026-01-01T00:00:00Z");
    const oldTwo = await insertTelemetry(pool, deviceId, "2026-01-01T00:01:00Z");
    const directEvidence = await insertTelemetry(pool, deviceId, "2026-01-01T00:02:00Z");
    const linkedEvidence = await insertTelemetry(pool, deviceId, "2026-01-01T00:03:00Z");
    const recent = await insertTelemetry(pool, deviceId, "2026-01-03T00:00:00Z");
    const nullTimestamp = await insertTelemetry(pool, deviceId, null);

    const [eventResult] = await pool.execute(
      `INSERT INTO events (
         device_id, event_uuid, event_type, evidence_status, evidence_telemetry_id, event_time
       ) VALUES (?, 'retention-event-001', 'fall_detected', 'linked', ?, '2026-01-01 00:02:00')`,
      [deviceId, directEvidence],
    );
    await pool.execute(
      `INSERT INTO event_telemetry_evidence (event_id, telemetry_log_id, relative_ms, role)
       VALUES (?, ?, 1000, 'after_peak')`,
      [eventResult.insertId, linkedEvidence],
    );
    await pool.execute(
      `INSERT INTO audit_logs (action, entity_type, entity_id)
       VALUES ('retention.test', 'event', ?)`,
      [eventResult.insertId],
    );

    const baseInput = {
      before: "2026-01-02T00:00:00Z",
      batchSize: 1,
      maxBatches: 1,
    };
    const options = {
      databasePool: pool,
      log: {},
      now: new Date("2026-01-10T00:00:00Z"),
    };

    const dryRun = await runTelemetryRetention(baseInput, options);
    assert.equal(dryRun.mode, "dry_run");
    assert.equal(dryRun.candidateRows, 2);
    assert.equal(dryRun.protectedEvidenceRows, 2);
    assert.equal(dryRun.legacyNullTimestampRows, 1);
    assert.equal(await countByIds(pool, [oldOne, oldTwo]), 2);

    const firstApply = await runTelemetryRetention({ ...baseInput, apply: true }, options);
    assert.equal(firstApply.deletedRows, 1);
    assert.equal(firstApply.batchesCompleted, 1);
    assert.equal(firstApply.remainingCandidateRows, 1);
    assert.equal(await countByIds(pool, [oldOne, oldTwo]), 1);

    const secondApply = await runTelemetryRetention({ ...baseInput, apply: true }, options);
    assert.equal(secondApply.deletedRows, 1);
    assert.equal(secondApply.remainingCandidateRows, 0);

    const safeRerun = await runTelemetryRetention({ ...baseInput, apply: true }, options);
    assert.equal(safeRerun.deletedRows, 0);
    assert.equal(safeRerun.batchesCompleted, 0);
    assert.equal(
      await countByIds(pool, [directEvidence, linkedEvidence, recent, nullTimestamp]),
      4,
    );
    const [[auditCount]] = await pool.query("SELECT COUNT(*) AS total FROM audit_logs");
    assert.equal(Number(auditCount.total), 1);

    const failureOne = await insertTelemetry(pool, deviceId, "2026-01-01T01:00:00Z");
    const failureTwo = await insertTelemetry(pool, deviceId, "2026-01-01T01:01:00Z");
    await pool.query(`
      CREATE TRIGGER reject_retention_delete
      BEFORE DELETE ON telemetry_logs
      FOR EACH ROW
      BEGIN
        IF OLD.id = ${failureTwo} THEN
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'injected retention failure';
        END IF;
      END
    `);

    await assert.rejects(
      runTelemetryRetention({
        ...baseInput,
        batchSize: 2,
        apply: true,
      }, options),
      /injected retention failure/,
    );
    assert.equal(await countByIds(pool, [failureOne, failureTwo]), 2);
    await pool.query("DROP TRIGGER reject_retention_delete");

    const recovered = await runTelemetryRetention({
      ...baseInput,
      batchSize: 2,
      apply: true,
    }, options);
    assert.equal(recovered.deletedRows, 2);
    assert.equal(await countByIds(pool, [failureOne, failureTwo]), 0);
  });
});
