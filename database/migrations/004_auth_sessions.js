const VERSION = "004";
const NAME = "auth_sessions";
const TABLE = "auth_sessions";

// Sessoes moveis revogaveis (ADR-009, CP4 Android A1/A2). Somente o hash SHA-256 do
// refresh token e armazenado. Cada rotacao cria uma linha nova na mesma familia; um
// refresh token ja rotacionado reutilizado revoga a familia inteira (deteccao de reuso).
// Tabela nova: nao altera users nem o login web legado.
const CREATE_SQL = `
  CREATE TABLE IF NOT EXISTS ${TABLE} (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    session_uuid CHAR(36) NOT NULL,
    family_uuid CHAR(36) NOT NULL,
    user_id BIGINT UNSIGNED NOT NULL,
    client_type ENUM('mobile') NOT NULL DEFAULT 'mobile',
    device_label VARCHAR(80) NULL,
    refresh_token_hash CHAR(64) NOT NULL,
    status ENUM('active', 'rotated', 'revoked') NOT NULL DEFAULT 'active',
    revoked_reason VARCHAR(40) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_used_at DATETIME NULL,
    expires_at DATETIME NOT NULL,
    rotated_at DATETIME NULL,
    revoked_at DATETIME NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_auth_sessions_session_uuid (session_uuid),
    UNIQUE KEY uq_auth_sessions_refresh_hash (refresh_token_hash),
    KEY idx_auth_sessions_family_status (family_uuid, status),
    KEY idx_auth_sessions_user_status (user_id, status),
    CONSTRAINT fk_auth_sessions_user
      FOREIGN KEY (user_id) REFERENCES users (id)
      ON DELETE CASCADE
  )
`;

async function up(context) {
  await context.execute(context.connection, CREATE_SQL);
  context.log("info", "migration_table_ensured", { table: TABLE });
  return { table: TABLE, backfill: "none" };
}

async function down(context) {
  await context.execute(context.connection, `DROP TABLE IF EXISTS ${TABLE}`);
}

module.exports = {
  CREATE_SQL,
  NAME,
  TABLE,
  VERSION,
  down,
  up,
};
