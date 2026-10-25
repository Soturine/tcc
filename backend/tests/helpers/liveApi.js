/*
 * Helpers para testes HTTP contra o app real + MySQL real (MYSQL_INTEGRATION=1).
 * Cada arquivo de teste roda em processo proprio, com limitadores de taxa proprios.
 */
const crypto = require("node:crypto");
const test = require("node:test");

const integrationEnabled = process.env.MYSQL_INTEGRATION === "1";
const integrationTest = integrationEnabled ? test : test.skip;

const SEED_EMAIL = "admin@queda.local";
const SEED_PASSWORD = "Admin@123";

const state = { server: null, baseUrl: null, pool: null, fixture: null };

async function api(method, path, { token, body, org } = {}) {
  const headers = { "content-type": "application/json" };
  if (token) headers.authorization = `Bearer ${token}`;
  if (org !== undefined) headers["x-organization-id"] = String(org);
  const response = await fetch(`${state.baseUrl}${path}`, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const text = await response.text();
  let json = null;
  try {
    json = text ? JSON.parse(text) : null;
  } catch {
    json = text;
  }
  return { status: response.status, json };
}

async function mobileLogin(extra = {}) {
  return api("POST", "/api/auth/mobile/login", {
    body: { email: SEED_EMAIL, password: SEED_PASSWORD, deviceLabel: "test-runner", ...extra },
  });
}

async function createForeignOrganization() {
  const bcrypt = require("bcrypt");
  const tag = crypto.randomBytes(4).toString("hex");
  const [org] = await state.pool.execute(
    "INSERT INTO organizations (name, type, status) VALUES (?, 'family', 'active')",
    [`Org B ${tag}`],
  );
  const [user] = await state.pool.execute(
    "INSERT INTO users (name, email, password_hash, global_role, status) VALUES (?, ?, ?, 'user', 'active')",
    [`User B ${tag}`, `b-${tag}@example.invalid`, await bcrypt.hash(`pw-${tag}`, 4)],
  );
  await state.pool.execute(
    "INSERT INTO organization_members (organization_id, user_id, role, status) VALUES (?, ?, 'organization_admin', 'active')",
    [org.insertId, user.insertId],
  );
  const [patient] = await state.pool.execute(
    "INSERT INTO patients (organization_id, full_name, status) VALUES (?, ?, 'active')",
    [org.insertId, `Pessoa B ${tag}`],
  );
  const [device] = await state.pool.execute(
    "INSERT INTO devices (organization_id, device_uid, device_identifier, name, claim_status) VALUES (?, ?, ?, ?, 'claimed')",
    [org.insertId, `uid-b-${tag}`, `dev-b-${tag}`, `Dispositivo B ${tag}`],
  );
  const [event] = await state.pool.execute(
    "INSERT INTO events (organization_id, patient_id, device_id, event_type, severity, message, event_time) VALUES (?, ?, ?, 'sos_pressed', 'high', 'teste', UTC_TIMESTAMP())",
    [org.insertId, patient.insertId, device.insertId],
  );
  const [alert] = await state.pool.execute(
    "INSERT INTO alerts (organization_id, patient_id, event_id, device_id, status) VALUES (?, ?, ?, ?, 'open')",
    [org.insertId, patient.insertId, event.insertId, device.insertId],
  );
  return {
    orgId: org.insertId,
    userId: user.insertId,
    patientId: patient.insertId,
    deviceId: device.insertId,
    eventId: event.insertId,
    alertId: alert.insertId,
  };
}

async function dropForeignOrganization(ids) {
  if (!ids) return;
  await state.pool.execute("DELETE FROM alerts WHERE id = ?", [ids.alertId]);
  await state.pool.execute("DELETE FROM events WHERE id = ?", [ids.eventId]);
  await state.pool.execute("DELETE FROM devices WHERE id = ?", [ids.deviceId]);
  await state.pool.execute("DELETE FROM patients WHERE id = ?", [ids.patientId]);
  await state.pool.execute("DELETE FROM organization_members WHERE user_id = ?", [ids.userId]);
  await state.pool.execute("DELETE FROM users WHERE id = ?", [ids.userId]);
  await state.pool.execute("DELETE FROM organizations WHERE id = ?", [ids.orgId]);
}

function useLiveApp() {
test.before(async () => {
  if (!integrationEnabled) return;
  process.env.MOBILE_ACCESS_TOKEN_TTL_SECONDS = process.env.MOBILE_ACCESS_TOKEN_TTL_SECONDS || "900";
  const { createApp } = require("../../src/app");
  state.pool = require("../../src/db/pool").pool;
  state.server = createApp().listen(0);
  await new Promise((resolve) => state.server.once("listening", resolve));
  state.baseUrl = `http://127.0.0.1:${state.server.address().port}`;
  state.fixture = await createForeignOrganization();
});

test.after(async () => {
  if (!integrationEnabled) return;
  await dropForeignOrganization(state.fixture);
  await new Promise((resolve) => state.server.close(resolve));
  await state.pool.end();
});
return state;
}


module.exports = { SEED_EMAIL, SEED_PASSWORD, api, crypto, integrationTest, mobileLogin, useLiveApp };
