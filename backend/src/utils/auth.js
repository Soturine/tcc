const jwt = require("jsonwebtoken");

const { env } = require("../config/env");

function signToken(user) {
  return jwt.sign(
    {
      sub: user.id,
      id: user.id,
      name: user.name,
      email: user.email,
      globalRole: user.globalRole,
    },
    env.jwtSecret,
    { expiresIn: "7d" },
  );
}

// Access token de sessao movel: curto e vinculado a uma familia de sessao (`sid`)
// verificada no servidor a cada requisicao, para que logout/revogacao valham na hora.
function signSessionAccessToken(user, sessionFamilyUuid, ttlSeconds) {
  return jwt.sign(
    {
      sub: user.id,
      id: user.id,
      name: user.name,
      email: user.email,
      globalRole: user.globalRole,
      sid: sessionFamilyUuid,
      typ: "access",
    },
    env.jwtSecret,
    { expiresIn: ttlSeconds },
  );
}

function verifyToken(token) {
  return jwt.verify(token, env.jwtSecret);
}

module.exports = {
  signSessionAccessToken,
  signToken,
  verifyToken,
};
