const assert = require("node:assert/strict");
const test = require("node:test");

const { isLocalNodeEnv, validateRuntimeConfig } = require("../../src/config/env");

const STRONG_SECRET = "a".repeat(16) + "B".repeat(16) + "0123";

function externalConfig(overrides = {}) {
  return {
    nodeEnv: "production",
    jwtSecret: STRONG_SECRET,
    corsAllowedOrigins: ["https://console.example.test"],
    ...overrides,
    mqtt: {
      brokerUrl: "mqtts://broker.example.test:8883",
      username: "backend",
      password: "broker-password",
      tlsRejectUnauthorized: true,
      tlsCaFile: "",
      tlsCaReadable: null,
      ...overrides.mqtt,
    },
  };
}

test("somente development e test sao perfis locais", () => {
  assert.equal(isLocalNodeEnv("development"), true);
  assert.equal(isLocalNodeEnv("test"), true);
  assert.equal(isLocalNodeEnv("production"), false);
  assert.equal(isLocalNodeEnv("staging"), false);
  assert.equal(isLocalNodeEnv(""), false);
});

test("ambiente externo seguro inicia sem erros", () => {
  const result = validateRuntimeConfig(externalConfig());
  assert.deepEqual(result.errors, []);
  assert.equal(result.ok, true);
});

test("ambiente externo recusa JWT_SECRET ausente, default ou curto", () => {
  for (const jwtSecret of ["", "change-me", "CHANGE-ME", "ci-local-only-not-for-staging", "short-secret"]) {
    const result = validateRuntimeConfig(externalConfig({ jwtSecret }));
    assert.equal(result.ok, false, `deveria recusar segredo de ${jwtSecret.length} caracteres`);
    assert.ok(result.errors.some((message) => message.startsWith("JWT_SECRET")));
  }
});

test("staging e tratado como externo", () => {
  const result = validateRuntimeConfig(externalConfig({ nodeEnv: "staging", jwtSecret: "change-me" }));
  assert.equal(result.local, false);
  assert.equal(result.ok, false);
});

test("ambiente externo recusa MQTT sem TLS ou com verificacao desabilitada", () => {
  const plaintext = validateRuntimeConfig(externalConfig({ mqtt: { brokerUrl: "mqtt://broker:1883" } }));
  assert.equal(plaintext.ok, false);
  assert.ok(plaintext.errors.some((message) => message.includes("mqtts://")));

  const insecure = validateRuntimeConfig(externalConfig({ mqtt: { tlsRejectUnauthorized: false } }));
  assert.equal(insecure.ok, false);

  const unreadableCa = validateRuntimeConfig(
    externalConfig({ mqtt: { tlsCaFile: "missing-ca.pem", tlsCaReadable: false } }),
  );
  assert.equal(unreadableCa.ok, false);
});

test("ambiente externo recusa credenciais MQTT ausentes e CORS permissivo", () => {
  assert.equal(validateRuntimeConfig(externalConfig({ mqtt: { username: "", password: "" } })).ok, false);
  assert.equal(validateRuntimeConfig(externalConfig({ corsAllowedOrigins: ["*"] })).ok, false);
  assert.equal(
    validateRuntimeConfig(externalConfig({ corsAllowedOrigins: ["http://console.example.test"] })).ok,
    false,
  );
});

test("ambiente local aceita defaults de conveniencia apenas como warnings", () => {
  const result = validateRuntimeConfig({
    nodeEnv: "development",
    jwtSecret: "change-me",
    corsAllowedOrigins: ["http://localhost:5173"],
    mqtt: { brokerUrl: "mqtt://127.0.0.1:1883", username: "", password: "" },
  });

  assert.equal(result.ok, true);
  assert.deepEqual(result.errors, []);
  assert.ok(result.warnings.length >= 3);
});

test("mensagens de validacao nao vazam o valor do segredo", () => {
  const leakedSecret = "leak-me-please";
  const result = validateRuntimeConfig(externalConfig({ jwtSecret: leakedSecret }));
  const allMessages = [...result.errors, ...result.warnings].join(" ");
  assert.equal(allMessages.includes(leakedSecret), false);
});

test("CORS '*' e recusado mesmo localmente", () => {
  const result = validateRuntimeConfig({
    nodeEnv: "development",
    jwtSecret: "change-me",
    corsAllowedOrigins: ["*"],
    mqtt: { brokerUrl: "mqtt://127.0.0.1:1883" },
  });
  assert.equal(result.ok, false);
});
