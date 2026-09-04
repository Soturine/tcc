const { pool, testConnection } = require("../src/db/pool");
const { measureDataLifecycle } = require("../src/services/dataLifecycleMeasurementService");

async function main() {
  await testConnection();
  const result = await measureDataLifecycle();
  console.log(JSON.stringify({
    level: "info",
    component: "data_lifecycle_measurement",
    event: "data_lifecycle_measured",
    ...result,
  }));
}

if (require.main === module) {
  main()
    .catch((error) => {
      console.error(JSON.stringify({
        timestamp: new Date().toISOString(),
        level: "error",
        component: "data_lifecycle_measurement",
        event: "data_lifecycle_measurement_failed",
        code: error.code || "DATA_LIFECYCLE_MEASUREMENT_FAILED",
        message: error.message,
      }));
      process.exitCode = 1;
    })
    .finally(async () => {
      await pool.end();
    });
}

module.exports = { main };
