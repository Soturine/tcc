const VERSION = "003";
const NAME = "event_evidence_source";
const COLUMN = "evidence_source";

// Origem da evidencia que sustentou a decisao de alerta de um evento critico
// (ADR-016). NULL em registros anteriores: nao fabricamos origem historica.
async function up(context) {
  if (!(await context.columnExists("events", COLUMN, context.connection))) {
    await context.execute(
      context.connection,
      `ALTER TABLE events
         ADD COLUMN ${COLUMN} ENUM('device', 'server_telemetry', 'both', 'none') NULL`,
    );
    context.log("info", "migration_column_added", { table: "events", column: COLUMN });
  }

  return { table: "events", column: COLUMN, backfill: "none" };
}

async function down(context) {
  if (await context.columnExists("events", COLUMN, context.connection)) {
    await context.execute(context.connection, `ALTER TABLE events DROP COLUMN ${COLUMN}`);
  }
}

module.exports = {
  COLUMN,
  NAME,
  VERSION,
  down,
  up,
};
