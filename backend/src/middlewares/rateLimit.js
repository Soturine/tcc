const { rateLimit } = require("express-rate-limit");

function buildRateLimit({ windowMs, limit }) {
  return rateLimit({
    windowMs,
    limit,
    standardHeaders: "draft-8",
    legacyHeaders: false,
    handler(_req, res) {
      res.status(429).json({
        message: "Muitas tentativas. Aguarde antes de tentar novamente.",
      });
    },
  });
}

const apiRateLimit = buildRateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 300,
});

const authRateLimit = buildRateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 10,
});

const pairingRateLimit = buildRateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 20,
});

module.exports = {
  apiRateLimit,
  authRateLimit,
  buildRateLimit,
  pairingRateLimit,
};
