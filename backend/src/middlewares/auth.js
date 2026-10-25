const { HttpError } = require("../utils/httpError");
const { verifyToken } = require("../utils/auth");
const { loadAccessContext } = require("../services/scopeService");
const { isSessionActive } = require("../services/sessionService");

const REVOKED = "Sessão revogada ou expirada.";

// Tokens de sessao movel carregam `sid`; sao aceitos somente com a familia ativa no
// servidor. Tokens legados (web, sem `sid`) seguem a regra anterior.
async function assertSessionActive(payload) {
  if (payload && payload.sid !== undefined) {
    if (!(await isSessionActive(payload.sid))) {
      throw new HttpError(401, REVOKED, { code: "SESSION_REVOKED" });
    }
  }
}

async function requireAuth(req, res, next) {
  const authorization = req.headers.authorization || "";

  if (!authorization.startsWith("Bearer ")) {
    return next(new HttpError(401, "Token de acesso ausente."));
  }

  const token = authorization.replace("Bearer ", "").trim();

  try {
    const payload = verifyToken(token);
    await assertSessionActive(payload);
    req.sessionId = payload.sid || null;
    req.user = {
      id: Number(payload.id || payload.sub),
      name: payload.name,
      email: payload.email,
      role: payload.role,
    };

    return next();
  } catch (error) {
    if (error instanceof HttpError) {
      return next(error);
    }
    return next(new HttpError(401, "Token de acesso inválido."));
  }
}

async function requireAccessContext(req, res, next) {
  const authorization = req.headers.authorization || "";

  if (!authorization.startsWith("Bearer ")) {
    return next(new HttpError(401, "Token de acesso ausente."));
  }

  const token = authorization.replace("Bearer ", "").trim();

  try {
    const payload = verifyToken(token);
    await assertSessionActive(payload);
    req.sessionId = payload.sid || null;
    const accessContext = await loadAccessContext({
      userId: Number(payload.id || payload.sub),
      requestedOrganizationId: req.headers["x-organization-id"],
    });

    req.user = accessContext.user;
    req.access = accessContext;
    return next();
  } catch (error) {
    if (error instanceof HttpError) {
      return next(error);
    }

    return next(new HttpError(401, "Token de acesso inválido."));
  }
}

// Logout: aceita Bearer opcional (sem exigir token valido) para identificar a sessao.
function optionalSession(req, _res, next) {
  const authorization = req.headers.authorization || "";
  if (authorization.startsWith("Bearer ")) {
    try {
      const payload = verifyToken(authorization.replace("Bearer ", "").trim());
      req.sessionId = payload.sid || null;
    } catch {
      req.sessionId = null;
    }
  }
  next();
}

module.exports = {
  assertSessionActive,
  optionalSession,
  requireAuth,
  requireAccessContext,
};
