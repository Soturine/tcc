const { auditEventIdentity } = require("../src/db/eventIdentityAudit");
const { pool, testConnection } = require("../src/db/pool");

async function main() {
  await testConnection();
  const audit = await auditEventIdentity();
  console.log(JSON.stringify({
    timestamp: new Date().toISOString(),
    level: audit.blockingIssues > 0 ? "warn" : "info",
    component: "event_identity_audit",
    event: "event_identity_audit_completed",
    ...audit,
  }));
  return audit;
}

if (require.main === module) {
  main()
    .catch((error) => {
      console.error(JSON.stringify({
        timestamp: new Date().toISOString(),
        level: "error",
        component: "event_identity_audit",
        event: "event_identity_audit_failed",
        code: error.code || "AUDIT_FAILED",
        message: error.message,
      }));
      process.exitCode = 1;
    })
    .finally(async () => {
      await pool.end();
    });
}

module.exports = { main };
