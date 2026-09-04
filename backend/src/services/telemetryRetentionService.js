const { pool } = require("../db/pool");
const { logger } = require("../utils/logger");

const DEFAULT_BATCH_SIZE = 500;
const DEFAULT_MAX_BATCHES = 1;
const MAX_BATCH_SIZE = 5000;
const MAX_BATCHES = 1000;
const RETENTION_TABLE = "telemetry_logs";

const ELIGIBLE_WHERE = `
  tl.created_at IS NOT NULL
  AND tl.created_at < ?
  AND NOT EXISTS (
    SELECT 1
    FROM event_telemetry_evidence ete
    WHERE ete.telemetry_log_id = tl.id
  )
  AND NOT EXISTS (
    SELECT 1
    FROM events e
    WHERE e.evidence_telemetry_id = tl.id
  )
`;

function configError(message) {
  const error = new Error(message);
  error.code = "INVALID_RETENTION_CONFIG";
  return error;
}

function parsePositiveInteger(value, label, defaultValue, maximum) {
  const candidate = value == null || value === "" ? defaultValue : Number(value);
  if (!Number.isInteger(candidate) || candidate < 1 || candidate > maximum) {
    throw configError(`${label} deve ser inteiro entre 1 e ${maximum}.`);
  }
  return candidate;
}

function parseCutoff(value) {
  if (value instanceof Date && !Number.isNaN(value.getTime())) {
    return new Date(value.getTime());
  }

  if (
    typeof value !== "string"
    || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?(?:Z|[+-]\d{2}:\d{2})$/.test(value)
  ) {
    throw configError("before deve ser um date-time ISO 8601 com timezone explicito.");
  }

  const parsed = new Date(value);
  if (Number.isNaN(parsed.getTime())) {
    throw configError("before deve ser um date-time ISO 8601 valido.");
  }
  return parsed;
}

function normalizeRetentionConfig(input = {}, options = {}) {
  const now = options.now instanceof Date ? options.now : new Date();
  const before = parseCutoff(input.before);
  if (before.getTime() >= now.getTime()) {
    throw configError("before deve representar um instante anterior ao momento da execucao.");
  }

  return {
    table: RETENTION_TABLE,
    before,
    batchSize: parsePositiveInteger(
      input.batchSize,
      "batchSize",
      DEFAULT_BATCH_SIZE,
      MAX_BATCH_SIZE,
    ),
    maxBatches: parsePositiveInteger(
      input.maxBatches,
      "maxBatches",
      DEFAULT_MAX_BATCHES,
      MAX_BATCHES,
    ),
    dryRun: input.apply !== true,
  };
}

function toIso(value) {
  if (!value) {
    return null;
  }
  const parsed = value instanceof Date ? value : new Date(value);
  return Number.isNaN(parsed.getTime()) ? null : parsed.toISOString();
}

function toCount(value) {
  return Number(value || 0);
}

async function inspectTelemetryRetention(before, executor) {
  const [[candidate], [protectedEvidence], [legacyNull]] = await Promise.all([
    executor.execute(
      `
        SELECT
          COUNT(*) AS candidate_count,
          MIN(tl.created_at) AS oldest_candidate_at,
          MAX(tl.created_at) AS newest_candidate_at
        FROM telemetry_logs tl
        WHERE ${ELIGIBLE_WHERE}
      `,
      [before],
    ).then(([rows]) => rows),
    executor.execute(
      `
        SELECT COUNT(DISTINCT tl.id) AS protected_count
        FROM telemetry_logs tl
        WHERE tl.created_at IS NOT NULL
          AND tl.created_at < ?
          AND (
            EXISTS (
              SELECT 1
              FROM event_telemetry_evidence ete
              WHERE ete.telemetry_log_id = tl.id
            )
            OR EXISTS (
              SELECT 1
              FROM events e
              WHERE e.evidence_telemetry_id = tl.id
            )
          )
      `,
      [before],
    ).then(([rows]) => rows),
    executor.execute(
      "SELECT COUNT(*) AS null_timestamp_count FROM telemetry_logs WHERE created_at IS NULL",
    ).then(([rows]) => rows),
  ]);

  return {
    candidateRows: toCount(candidate.candidate_count),
    candidateRange: {
      oldest: toIso(candidate.oldest_candidate_at),
      newest: toIso(candidate.newest_candidate_at),
    },
    protectedEvidenceRows: toCount(protectedEvidence.protected_count),
    legacyNullTimestampRows: toCount(legacyNull.null_timestamp_count),
  };
}

async function withRetriedTransaction(databasePool, work, maxAttempts = 3) {
  for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
    const connection = await databasePool.getConnection();
    try {
      await connection.beginTransaction();
      const result = await work(connection);
      await connection.commit();
      return result;
    } catch (error) {
      await connection.rollback();
      if (error.code !== "ER_LOCK_DEADLOCK" || attempt === maxAttempts) {
        throw error;
      }
    } finally {
      connection.release();
    }
  }
  throw new Error("Retention transaction terminou sem resultado.");
}

async function deleteTelemetryBatch(config, databasePool) {
  return withRetriedTransaction(databasePool, async (connection) => {
    const [rows] = await connection.execute(
      `
        SELECT tl.id, tl.created_at
        FROM telemetry_logs tl
        WHERE ${ELIGIBLE_WHERE}
        ORDER BY tl.created_at, tl.id
        LIMIT ${config.batchSize}
        FOR UPDATE
      `,
      [config.before],
    );

    if (!rows.length) {
      return null;
    }

    const ids = rows.map((row) => Number(row.id));
    const placeholders = ids.map(() => "?").join(", ");
    const [result] = await connection.execute(
      `
        DELETE tl
        FROM telemetry_logs tl
        LEFT JOIN event_telemetry_evidence ete ON ete.telemetry_log_id = tl.id
        LEFT JOIN events e ON e.evidence_telemetry_id = tl.id
        WHERE tl.id IN (${placeholders})
          AND tl.created_at IS NOT NULL
          AND tl.created_at < ?
          AND ete.id IS NULL
          AND e.id IS NULL
      `,
      [...ids, config.before],
    );

    return {
      selectedRows: rows.length,
      deletedRows: toCount(result.affectedRows),
      range: {
        oldest: toIso(rows[0].created_at),
        newest: toIso(rows[rows.length - 1].created_at),
      },
    };
  });
}

function emitLog(log, level, message, metadata) {
  if (typeof log?.[level] === "function") {
    log[level](message, metadata);
  }
}

async function runTelemetryRetention(input, options = {}) {
  const databasePool = options.databasePool || pool;
  const log = options.log || logger;
  const config = normalizeRetentionConfig(input, { now: options.now });
  const initial = await inspectTelemetryRetention(config.before, databasePool);
  const baseResult = {
    table: RETENTION_TABLE,
    mode: config.dryRun ? "dry_run" : "apply",
    criterion: "created_at before cutoff, timestamp non-null, not referenced as event evidence",
    before: config.before.toISOString(),
    batchSize: config.batchSize,
    maxBatches: config.maxBatches,
    ...initial,
  };

  if (config.dryRun) {
    const result = {
      ...baseResult,
      batchesCompleted: 0,
      deletedRows: 0,
      remainingCandidateRows: initial.candidateRows,
    };
    emitLog(log, "info", "Telemetry retention dry-run concluido.", result);
    return result;
  }

  let batchesCompleted = 0;
  let deletedRows = 0;
  try {
    while (batchesCompleted < config.maxBatches) {
      const batch = await deleteTelemetryBatch(config, databasePool);
      if (!batch) {
        break;
      }
      batchesCompleted += 1;
      deletedRows += batch.deletedRows;
      emitLog(log, "info", "Batch de telemetry retention concluido.", {
        table: RETENTION_TABLE,
        before: config.before.toISOString(),
        batchNumber: batchesCompleted,
        ...batch,
      });
    }
  } catch (error) {
    emitLog(log, "error", "Telemetry retention falhou.", {
      table: RETENTION_TABLE,
      before: config.before.toISOString(),
      batchesCompleted,
      deletedRows,
      code: error.code || "TELEMETRY_RETENTION_FAILED",
    });
    throw error;
  }

  const remaining = await inspectTelemetryRetention(config.before, databasePool);
  const result = {
    ...baseResult,
    batchesCompleted,
    deletedRows,
    remainingCandidateRows: remaining.candidateRows,
    remainingCandidateRange: remaining.candidateRange,
  };
  emitLog(log, "info", "Telemetry retention apply concluido.", result);
  return result;
}

module.exports = {
  DEFAULT_BATCH_SIZE,
  DEFAULT_MAX_BATCHES,
  MAX_BATCH_SIZE,
  MAX_BATCHES,
  deleteTelemetryBatch,
  inspectTelemetryRetention,
  normalizeRetentionConfig,
  runTelemetryRetention,
  withRetriedTransaction,
};
