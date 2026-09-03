const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

const { auditEventIdentity, columnExists, indexExists } = require("../src/db/eventIdentityAudit");
const { execute, pool } = require("../src/db/pool");

const MIGRATIONS_DIRECTORY = path.resolve(__dirname, "../../database/migrations");
const MIGRATION_LOCK = "tcc_schema_migrations";

function log(level, event, details = {}) {
  console.log(JSON.stringify({
    timestamp: new Date().toISOString(),
    level,
    component: "migration_runner",
    event,
    ...details,
  }));
}

function migrationChecksum(filePath) {
  return crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
}

function loadMigrations(directory = MIGRATIONS_DIRECTORY) {
  return fs.readdirSync(directory)
    .filter((name) => /^\d{3}_[a-z0-9_]+\.js$/.test(name))
    .sort()
    .map((fileName) => {
      const filePath = path.join(directory, fileName);
      delete require.cache[require.resolve(filePath)];
      return {
        ...require(filePath),
        checksum: migrationChecksum(filePath),
        fileName,
      };
    });
}

async function ensureMigrationHistory(connection) {
  await execute(
    connection,
    `
      CREATE TABLE IF NOT EXISTS schema_migrations (
        version VARCHAR(64) NOT NULL,
        name VARCHAR(255) NOT NULL,
        checksum CHAR(64) NOT NULL,
        applied_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
        PRIMARY KEY (version)
      )
    `,
  );
}

async function getAppliedMigrations(connection) {
  const rows = await execute(
    connection,
    "SELECT version, name, checksum, applied_at FROM schema_migrations ORDER BY version",
  );
  return new Map(rows.map((row) => [String(row.version), row]));
}

function buildContext(connection) {
  return {
    auditEventIdentity,
    columnExists,
    connection,
    execute,
    indexExists,
    log,
  };
}

async function acquireLock(connection) {
  const [row] = await execute(connection, "SELECT GET_LOCK(?, 30) AS acquired", [MIGRATION_LOCK]);
  if (Number(row?.acquired) !== 1) {
    throw new Error("Nao foi possivel adquirir o lock exclusivo de migrations.");
  }
}

async function releaseLock(connection) {
  await execute(connection, "SELECT RELEASE_LOCK(?) AS released", [MIGRATION_LOCK]);
}

async function runMigrations({
  direction = "up",
  migrations = loadMigrations(),
  databasePool = pool,
} = {}) {
  const connection = await databasePool.getConnection();

  try {
    await acquireLock(connection);
    await ensureMigrationHistory(connection);
    const applied = await getAppliedMigrations(connection);

    if (direction === "down") {
      const appliedVersions = [...applied.keys()].sort().reverse();
      const latestVersion = appliedVersions[0];
      const migration = migrations.find((candidate) => candidate.VERSION === latestVersion);

      if (!migration) {
        throw new Error(`Migration aplicada ${latestVersion || "nenhuma"} nao esta disponivel.`);
      }
      if (applied.get(latestVersion).checksum !== migration.checksum) {
        throw new Error(`Checksum divergente para migration aplicada ${latestVersion}.`);
      }

      await migration.down(buildContext(connection));
      await execute(connection, "DELETE FROM schema_migrations WHERE version = ?", [latestVersion]);
      log("info", "migration_reverted", { version: latestVersion, name: migration.NAME });
      return { reverted: [latestVersion], applied: [] };
    }

    const newlyApplied = [];
    for (const migration of migrations) {
      const existing = applied.get(migration.VERSION);
      if (existing) {
        if (existing.checksum !== migration.checksum) {
          throw new Error(`Checksum divergente para migration aplicada ${migration.VERSION}.`);
        }
        continue;
      }

      log("info", "migration_started", { version: migration.VERSION, name: migration.NAME });
      const audit = await migration.up(buildContext(connection));
      await execute(
        connection,
        "INSERT INTO schema_migrations (version, name, checksum) VALUES (?, ?, ?)",
        [migration.VERSION, migration.NAME, migration.checksum],
      );
      newlyApplied.push(migration.VERSION);
      log("info", "migration_applied", {
        version: migration.VERSION,
        name: migration.NAME,
        audit,
      });
    }

    return { applied: newlyApplied, reverted: [] };
  } catch (error) {
    log("error", "migration_failed", {
      code: error.code || "MIGRATION_FAILED",
      message: error.message,
      audit: error.audit || null,
    });
    throw error;
  } finally {
    try {
      await releaseLock(connection);
    } finally {
      connection.release();
    }
  }
}

module.exports = {
  MIGRATIONS_DIRECTORY,
  ensureMigrationHistory,
  loadMigrations,
  migrationChecksum,
  runMigrations,
};
