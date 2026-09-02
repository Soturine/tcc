const { pool, testConnection } = require("../src/db/pool");
const { runMigrations } = require("./migrationRunner");

async function main() {
  const direction = process.argv.includes("--down") ? "down" : "up";
  await testConnection();
  const result = await runMigrations({ direction });
  console.log(JSON.stringify({
    timestamp: new Date().toISOString(),
    level: "info",
    component: "migration_runner",
    event: "migration_run_completed",
    direction,
    ...result,
  }));
}

if (require.main === module) {
  main()
    .catch(() => {
      process.exitCode = 1;
    })
    .finally(async () => {
      await pool.end();
    });
}

module.exports = { main };
