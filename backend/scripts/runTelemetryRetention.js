const { pool, testConnection } = require("../src/db/pool");
const { runTelemetryRetention } = require("../src/services/telemetryRetentionService");

function readArgument(name) {
  const prefix = `--${name}=`;
  const entry = process.argv.slice(2).find((argument) => argument.startsWith(prefix));
  return entry ? entry.slice(prefix.length) : null;
}

function buildInput() {
  return {
    before: readArgument("before") || process.env.TELEMETRY_RETENTION_BEFORE,
    batchSize: readArgument("batch-size") || process.env.TELEMETRY_RETENTION_BATCH_SIZE,
    maxBatches: readArgument("max-batches") || process.env.TELEMETRY_RETENTION_MAX_BATCHES,
    apply: process.argv.includes("--apply"),
  };
}

async function main() {
  await testConnection();
  const result = await runTelemetryRetention(buildInput());
  console.log(JSON.stringify({
    timestamp: new Date().toISOString(),
    level: "info",
    component: "telemetry_retention",
    event: "telemetry_retention_completed",
    ...result,
  }));
}

if (require.main === module) {
  main()
    .catch((error) => {
      console.error(JSON.stringify({
        timestamp: new Date().toISOString(),
        level: "error",
        component: "telemetry_retention",
        event: "telemetry_retention_failed",
        code: error.code || "TELEMETRY_RETENTION_FAILED",
        message: error.message,
      }));
      process.exitCode = 1;
    })
    .finally(async () => {
      await pool.end();
    });
}

module.exports = {
  buildInput,
  main,
  readArgument,
};
