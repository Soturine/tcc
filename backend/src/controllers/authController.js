const { asyncHandler } = require("../utils/asyncHandler");
const { loginUser, registerUser } = require("../services/userService");
const {
  createMobileSession,
  logoutByRefreshToken,
  logoutBySessionId,
  refreshMobileSession,
} = require("../services/sessionService");

const register = asyncHandler(async (req, res) => {
  const result = await registerUser(req.body);
  res.status(201).json(result);
});

const login = asyncHandler(async (req, res) => {
  const result = await loginUser({
    ...req.body,
    organizationId: req.headers["x-organization-id"] || req.body.organizationId || null,
  });
  res.json(result);
});

// Sessao movel (CP4): mesmas regras de senha/organizacao do login legado, mas sem o
// JWT de 7 dias; devolve access token curto + refresh token rotativo.
const mobileLogin = asyncHandler(async (req, res) => {
  const { user } = await loginUser({
    ...req.body,
    organizationId: req.headers["x-organization-id"] || req.body?.organizationId || null,
  });
  const session = await createMobileSession({ user, deviceLabel: req.body?.deviceLabel });
  res.json({ user, ...session });
});

const mobileRefresh = asyncHandler(async (req, res) => {
  const result = await refreshMobileSession({
    refreshToken: req.body?.refreshToken,
    organizationId: req.headers["x-organization-id"] || null,
  });
  res.json(result);
});

// Idempotente: sempre 204, sem revelar se o token existia.
const mobileLogout = asyncHandler(async (req, res) => {
  if (req.body?.refreshToken) {
    await logoutByRefreshToken(req.body.refreshToken);
  } else if (req.sessionId) {
    await logoutBySessionId(req.sessionId);
  }
  res.status(204).end();
});

module.exports = {
  login,
  mobileLogin,
  mobileLogout,
  mobileRefresh,
  register,
};
