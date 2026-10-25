const path = require("path");

require("dotenv").config({ path: path.resolve(process.cwd(), ".env"), quiet: true });

function toNumber(value, fallback) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function toTrimmed(value, fallback = "") {
  if (typeof value !== "string") {
    return fallback;
  }

  const trimmed = value.trim();
  return trimmed.length > 0 ? trimmed : fallback;
}

function toBoolean(value, fallback = false) {
  if (typeof value !== "string") {
    return fallback;
  }

  const normalized = value.trim().toLowerCase();
  if (["1", "true", "yes", "on"].includes(normalized)) {
    return true;
  }

  if (["0", "false", "no", "off"].includes(normalized)) {
    return false;
  }

  return fallback;
}

function toLogLevel(value, fallback = "info") {
  const normalized = toTrimmed(value, fallback).toLowerCase();
  return ["error", "warn", "info", "debug"].includes(normalized)
    ? normalized
    : fallback;
}

function toCsv(value, fallback = []) {
  if (typeof value !== "string") {
    return fallback;
  }

  const entries = value
    .split(",")
    .map((entry) => entry.trim())
    .filter(Boolean);

  return entries.length > 0 ? entries : fallback;
}

// Somente estes perfis aceitam defaults de conveniencia. Qualquer outro valor
// de NODE_ENV (production, staging, ...) e tratado como ambiente externo.
const LOCAL_NODE_ENVS = new Set(["development", "test"]);
const LOCAL_DEV_JWT_SECRET = "change-me";
const KNOWN_WEAK_JWT_SECRETS = new Set([
  LOCAL_DEV_JWT_SECRET,
  "ci-local-only-not-for-staging",
  "secret",
  "changeme",
  "jwt-secret",
]);
const MIN_EXTERNAL_JWT_SECRET_LENGTH = 32;

function isLocalNodeEnv(nodeEnv) {
  return LOCAL_NODE_ENVS.has(nodeEnv);
}

const nodeEnv = toTrimmed(process.env.NODE_ENV, "development");

const env = {
  nodeEnv,
  isProduction: nodeEnv === "production",
  isLocal: isLocalNodeEnv(nodeEnv),
  port: toNumber(process.env.PORT, 4000),
  // Em ambiente externo nao existe fallback: a validacao de startup recusa iniciar.
  jwtSecret: toTrimmed(
    process.env.JWT_SECRET,
    isLocalNodeEnv(nodeEnv) ? LOCAL_DEV_JWT_SECRET : "",
  ),
  logLevel: toLogLevel(process.env.LOG_LEVEL, "info"),
  corsAllowedOrigins: toCsv(process.env.CORS_ALLOWED_ORIGINS, [
    "http://localhost:5173",
    "http://127.0.0.1:5173",
  ]),
  mysql: {
    host: toTrimmed(process.env.MYSQL_HOST, "localhost"),
    port: toNumber(process.env.MYSQL_PORT, 3306),
    user: toTrimmed(process.env.MYSQL_USER, "root"),
    password: process.env.MYSQL_PASSWORD || "",
    database: toTrimmed(process.env.MYSQL_DATABASE, "queda_monitor"),
  },
  mqtt: {
    brokerUrl: toTrimmed(process.env.MQTT_BROKER_URL, "mqtt://localhost:1883"),
    username: process.env.MQTT_USERNAME || "",
    password: process.env.MQTT_PASSWORD || "",
    clientId: toTrimmed(process.env.MQTT_CLIENT_ID, "queda-backend"),
    topicBase: toTrimmed(process.env.MQTT_TOPIC_BASE, "queda/devices"),
    reconnectPeriodMs: toNumber(process.env.MQTT_RECONNECT_PERIOD_MS, 4000),
    connectTimeoutMs: toNumber(process.env.MQTT_CONNECT_TIMEOUT_MS, 30000),
    keepaliveSeconds: toNumber(process.env.MQTT_KEEPALIVE_SECONDS, 60),
    tlsRejectUnauthorized: toBoolean(
      process.env.MQTT_TLS_REJECT_UNAUTHORIZED,
      true,
    ),
    tlsCaFile: toTrimmed(process.env.MQTT_TLS_CA_FILE, ""),
    // Migracao (ADR-026): device com critical-event-result-v1 tambem recebe o ACK v1
    // para eventos persistidos. Evita retry infinito se a ACL ainda nao libera a
    // leitura de critical-event-results. Desligar somente apos a ACL nova estar ativa.
    legacyAckForResultDevices: toBoolean(
      process.env.MQTT_LEGACY_ACK_FOR_RESULT_DEVICES,
      true,
    ),
  },
  // Sessao movel (CP4): access token curto + refresh rotativo revogavel.
  mobileAuth: {
    accessTokenTtlSeconds: toNumber(process.env.MOBILE_ACCESS_TOKEN_TTL_SECONDS, 900),
    refreshTokenTtlDays: toNumber(process.env.MOBILE_REFRESH_TOKEN_TTL_DAYS, 30),
  },
  deviceOfflineThresholdSeconds: toNumber(
    process.env.DEVICE_OFFLINE_THRESHOLD_SECONDS,
    120,
  ),
};

/**
 * Valida a configuracao efetiva sem efeitos colaterais.
 * `errors` impedem o startup; `warnings` sao aceitaveis apenas localmente.
 * Mensagens nunca incluem o valor de segredos.
 */
function validateRuntimeConfig(config) {
  const errors = [];
  const warnings = [];
  const local = isLocalNodeEnv(config.nodeEnv);
  const report = (message) => (local ? warnings : errors).push(message);
  const secret = config.jwtSecret || "";

  if (!secret) {
    errors.push("JWT_SECRET ausente.");
  } else if (KNOWN_WEAK_JWT_SECRETS.has(secret.toLowerCase())) {
    report("JWT_SECRET usa um valor default/conhecido.");
  } else if (secret.length < MIN_EXTERNAL_JWT_SECRET_LENGTH) {
    report(`JWT_SECRET tem menos de ${MIN_EXTERNAL_JWT_SECRET_LENGTH} caracteres.`);
  }

  const brokerUrl = config.mqtt?.brokerUrl || "";
  if (!brokerUrl.startsWith("mqtts://")) {
    report("MQTT_BROKER_URL nao usa mqtts:// (TLS).");
  } else {
    if (config.mqtt.tlsRejectUnauthorized === false) {
      report("MQTT_TLS_REJECT_UNAUTHORIZED=false desabilita a verificacao do certificado do broker.");
    }
    if (config.mqtt.tlsCaFile && config.mqtt.tlsCaReadable === false) {
      errors.push("MQTT_TLS_CA_FILE configurado mas ilegivel; recusando seguir sem a CA declarada.");
    }
  }

  if (!config.mqtt?.username || !config.mqtt?.password) {
    report("Credenciais MQTT do backend ausentes.");
  }

  const origins = config.corsAllowedOrigins || [];
  if (origins.includes("*")) {
    errors.push("CORS_ALLOWED_ORIGINS nao pode conter '*'.");
  }
  if (!local && origins.some((origin) => /^http:\/\//i.test(origin))) {
    errors.push("CORS_ALLOWED_ORIGINS contem origem http:// em ambiente externo.");
  }

  return { ok: errors.length === 0, local, errors, warnings };
}

module.exports = {
  env,
  isLocalNodeEnv,
  validateRuntimeConfig,
};
