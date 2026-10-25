const crypto = require("crypto");

const { env } = require("../config/env");
const { execute, one, transaction } = require("../db/pool");
const { HttpError } = require("../utils/httpError");
const { logger } = require("../utils/logger");
const { signSessionAccessToken } = require("../utils/auth");
const { loadAccessContext } = require("./scopeService");

/*
 * Sessoes moveis revogaveis (CP4, ADR-009).
 *
 * - access token JWT curto com `sid` = familia da sessao, conferido no servidor a cada
 *   requisicao (logout/revogacao valem antes da expiracao do JWT);
 * - refresh token opaco (32 bytes aleatorios), armazenado SOMENTE como SHA-256;
 * - cada refresh rotaciona: a linha atual vira `rotated` e nasce outra na mesma familia;
 * - reutilizar um refresh ja rotacionado e tratado como roubo/replay: a familia inteira
 *   e revogada e o chamador recebe 401;
 * - o login web legado (JWT de 7 dias) permanece inalterado.
 */

const REFRESH_TOKEN_BYTES = 32;
const DEVICE_LABEL_MAX = 80;

function hashRefreshToken(token) {
  return crypto.createHash("sha256").update(String(token), "utf8").digest("hex");
}

function newRefreshToken() {
  return crypto.randomBytes(REFRESH_TOKEN_BYTES).toString("base64url");
}

function normalizeDeviceLabel(value) {
  if (typeof value !== "string") {
    return null;
  }
  const trimmed = value.trim().replace(/[\u0000-\u001f]/g, "");
  return trimmed ? trimmed.slice(0, DEVICE_LABEL_MAX) : null;
}

function invalidSession(message = "Sessão inválida ou expirada.") {
  return new HttpError(401, message, { code: "SESSION_INVALID" });
}

function isRefreshTokenShape(value) {
  return typeof value === "string" && value.length >= 20 && value.length <= 200;
}

function buildTokenResponse({ user, familyUuid, refreshToken, refreshExpiresAt, now }) {
  const ttlSeconds = env.mobileAuth.accessTokenTtlSeconds;
  return {
    tokenType: "Bearer",
    accessToken: signSessionAccessToken(user, familyUuid, ttlSeconds),
    accessTokenExpiresAt: new Date(now.getTime() + ttlSeconds * 1000).toISOString(),
    refreshToken,
    refreshTokenExpiresAt: refreshExpiresAt.toISOString(),
    sessionId: familyUuid,
  };
}

async function insertSessionRow(executor, { familyUuid, userId, deviceLabel, refreshToken, now }) {
  const refreshExpiresAt = new Date(now.getTime() + env.mobileAuth.refreshTokenTtlDays * 86400 * 1000);
  await execute(
    executor,
    `
      INSERT INTO auth_sessions (
        session_uuid, family_uuid, user_id, client_type, device_label,
        refresh_token_hash, status, created_at, expires_at
      )
      VALUES (?, ?, ?, 'mobile', ?, ?, 'active', ?, ?)
    `,
    [crypto.randomUUID(), familyUuid, userId, deviceLabel, hashRefreshToken(refreshToken), now, refreshExpiresAt],
  );
  return refreshExpiresAt;
}

/** Cria uma familia de sessao para um usuario ja autenticado por senha. */
async function createMobileSession({ user, deviceLabel = null, now = new Date() }) {
  const familyUuid = crypto.randomUUID();
  const refreshToken = newRefreshToken();
  const refreshExpiresAt = await insertSessionRow(null, {
    familyUuid,
    userId: user.id,
    deviceLabel: normalizeDeviceLabel(deviceLabel),
    refreshToken,
    now,
  });
  logger.info("Sessao movel criada.", { userId: user.id, sessionId: familyUuid });
  return buildTokenResponse({ user, familyUuid, refreshToken, refreshExpiresAt, now });
}

async function revokeFamily(executor, familyUuid, reason) {
  const result = await execute(
    executor,
    `
      UPDATE auth_sessions
      SET status = 'revoked', revoked_reason = ?, revoked_at = UTC_TIMESTAMP()
      WHERE family_uuid = ? AND status <> 'revoked'
    `,
    [reason, familyUuid],
  );
  return result.affectedRows;
}

/**
 * Rotaciona o refresh token. Atomico: a linha e travada (FOR UPDATE), entao dois
 * refresh concorrentes com o mesmo token resultam em exatamente um sucesso; o segundo
 * enxerga a linha `rotated` e revoga a familia (reuso).
 */
async function refreshMobileSession({ refreshToken, organizationId = null, now = new Date() }) {
  if (!isRefreshTokenShape(refreshToken)) {
    throw invalidSession();
  }
  const tokenHash = hashRefreshToken(refreshToken);

  const outcome = await transaction(async (connection) => {
    const row = await one(
      connection,
      `
        SELECT id, family_uuid, user_id, device_label, status, expires_at
        FROM auth_sessions
        WHERE refresh_token_hash = ?
        FOR UPDATE
      `,
      [tokenHash],
    );
    if (!row) {
      return { error: "unknown" };
    }
    if (row.status === "rotated") {
      const revoked = await revokeFamily(connection, row.family_uuid, "refresh_reuse");
      return { error: "reuse", familyUuid: row.family_uuid, revoked };
    }
    if (row.status === "revoked") {
      return { error: "revoked", familyUuid: row.family_uuid };
    }
    if (new Date(row.expires_at).getTime() <= now.getTime()) {
      await revokeFamily(connection, row.family_uuid, "expired");
      return { error: "expired", familyUuid: row.family_uuid };
    }

    // Revalida usuario e organizacao no servidor (conta desativada/sem vinculo => erro).
    const accessContext = await loadAccessContext(
      { userId: Number(row.user_id), requestedOrganizationId: organizationId },
      connection,
    );

    await execute(
      connection,
      `UPDATE auth_sessions SET status = 'rotated', rotated_at = ?, last_used_at = ? WHERE id = ?`,
      [now, now, row.id],
    );
    const nextRefresh = newRefreshToken();
    const refreshExpiresAt = await insertSessionRow(connection, {
      familyUuid: row.family_uuid,
      userId: Number(row.user_id),
      deviceLabel: row.device_label,
      refreshToken: nextRefresh,
      now,
    });
    return { accessContext, familyUuid: row.family_uuid, nextRefresh, refreshExpiresAt };
  });

  if (outcome.error) {
    // Sem detalhes ao cliente: qualquer falha de refresh e o mesmo 401.
    logger.warn("Refresh de sessao movel recusado.", {
      reason: outcome.error,
      sessionId: outcome.familyUuid || null,
      revokedRows: outcome.revoked ?? null,
    });
    throw invalidSession();
  }

  return {
    user: outcome.accessContext.user,
    ...buildTokenResponse({
      user: outcome.accessContext.user,
      familyUuid: outcome.familyUuid,
      refreshToken: outcome.nextRefresh,
      refreshExpiresAt: outcome.refreshExpiresAt,
      now,
    }),
  };
}

/** Logout pelo refresh token (funciona mesmo com o access token ja expirado). Idempotente. */
async function logoutByRefreshToken(refreshToken) {
  if (!isRefreshTokenShape(refreshToken)) {
    return { revoked: 0 };
  }
  const row = await one(
    null,
    `SELECT family_uuid FROM auth_sessions WHERE refresh_token_hash = ?`,
    [hashRefreshToken(refreshToken)],
  );
  if (!row) {
    return { revoked: 0 };
  }
  const revoked = await revokeFamily(null, row.family_uuid, "logout");
  logger.info("Sessao movel encerrada.", { sessionId: row.family_uuid, revokedRows: revoked });
  return { revoked };
}

async function logoutBySessionId(familyUuid) {
  const revoked = await revokeFamily(null, familyUuid, "logout");
  logger.info("Sessao movel encerrada.", { sessionId: familyUuid, revokedRows: revoked });
  return { revoked };
}

/** Familia ativa e nao expirada? Usado pelo middleware para todo access token com `sid`. */
async function isSessionActive(familyUuid, now = new Date()) {
  if (typeof familyUuid !== "string" || familyUuid.length !== 36) {
    return false;
  }
  const row = await one(
    null,
    `
      SELECT 1 AS active
      FROM auth_sessions
      WHERE family_uuid = ? AND status = 'active' AND expires_at > ?
      LIMIT 1
    `,
    [familyUuid, now],
  );
  return Boolean(row?.active);
}

module.exports = {
  createMobileSession,
  hashRefreshToken,
  isSessionActive,
  logoutByRefreshToken,
  logoutBySessionId,
  normalizeDeviceLabel,
  refreshMobileSession,
};
