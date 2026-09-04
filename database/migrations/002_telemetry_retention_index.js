const VERSION = "002";
const NAME = "telemetry_retention_index";
const INDEX_NAME = "idx_telemetry_created_id";

async function up(context) {
  if (!(await context.indexExists("telemetry_logs", INDEX_NAME, context.connection))) {
    await context.execute(
      context.connection,
      `ALTER TABLE telemetry_logs ADD KEY ${INDEX_NAME} (created_at, id)`,
    );
  }

  return {
    table: "telemetry_logs",
    index: INDEX_NAME,
    columns: ["created_at", "id"],
  };
}

async function down(context) {
  if (await context.indexExists("telemetry_logs", INDEX_NAME, context.connection)) {
    await context.execute(
      context.connection,
      `ALTER TABLE telemetry_logs DROP INDEX ${INDEX_NAME}`,
    );
  }
}

module.exports = {
  INDEX_NAME,
  NAME,
  VERSION,
  down,
  up,
};
