/*
 * Escopo de organizacao e acoes de alerta com sessao movel (CP4/CP5) contra MySQL REAL e o app HTTP real (MYSQL_INTEGRATION=1, banco com schema+seed+migrations).
 */
const assert = require("node:assert/strict");

const { SEED_EMAIL, SEED_PASSWORD, api, crypto, integrationTest, mobileLogin, useLiveApp } = require("../helpers/liveApi");

const state = useLiveApp();

integrationTest("organizacao nao autorizada: cabecalho alheio 403 e IDs de outra organizacao 404", async () => {
  const login = await mobileLogin();
  const token = login.json.accessToken;
  assert.equal((await api("GET", "/api/me", { token, org: state.fixture.orgId })).status, 403);
  assert.equal((await api("GET", "/api/patients", { token, org: state.fixture.orgId })).status, 403);
  assert.equal((await mobileLogin({ organizationId: state.fixture.orgId })).status, 403);

  assert.equal((await api("GET", `/api/patients/${state.fixture.patientId}`, { token })).status, 404);
  assert.equal((await api("GET", `/api/devices/${state.fixture.deviceId}`, { token })).status, 404);
  assert.equal((await api("GET", `/api/alerts/${state.fixture.alertId}`, { token })).status, 404);
  const action = await api("POST", `/api/alerts/${state.fixture.alertId}/acknowledge`, { token, body: {} });
  assert.equal(action.status, 404);
  const [[alert]] = await state.pool.query("SELECT status FROM alerts WHERE id = ?", [state.fixture.alertId]);
  assert.equal(alert.status, "open", "o alerta da outra organizacao nao foi alterado");
  const alerts = await api("GET", "/api/alerts?limit=200", { token });
  assert.ok(!alerts.json.items.some((item) => item.id === state.fixture.alertId));
});

integrationTest("acao repetida sobre alerta: a segunda vira 409 e o estado permanece coerente", async () => {
  const [event] = await state.pool.execute(
    "INSERT INTO events (organization_id, device_id, event_type, severity, message, event_time) SELECT organization_id, id, 'sos_pressed', 'high', 'teste repeticao', UTC_TIMESTAMP() FROM devices WHERE organization_id = 1 LIMIT 1",
  );
  const [[eventRow]] = await state.pool.query("SELECT device_id FROM events WHERE id = ?", [event.insertId]);
  const [alert] = await state.pool.execute(
    "INSERT INTO alerts (organization_id, event_id, device_id, status) VALUES (1, ?, ?, 'open')",
    [event.insertId, eventRow.device_id],
  );
  try {
    const login = await mobileLogin();
    const token = login.json.accessToken;
    const first = await api("POST", `/api/alerts/${alert.insertId}/acknowledge`, { token, body: {} });
    const second = await api("POST", `/api/alerts/${alert.insertId}/acknowledge`, { token, body: {} });
    assert.equal(first.status, 200);
    assert.equal(second.status, 409);
    const detail = await api("GET", `/api/alerts/${alert.insertId}`, { token });
    assert.equal(detail.json.alert.status, "acknowledged");
    assert.equal(detail.json.alert.actions.filter((a) => a.actionType === "acknowledge").length, 1);
    assert.equal((await api("POST", "/api/alerts/99999999/acknowledge", { token, body: {} })).status, 404);
  } finally {
    await state.pool.execute("DELETE FROM alerts WHERE id = ?", [alert.insertId]);
    await state.pool.execute("DELETE FROM events WHERE id = ?", [event.insertId]);
  }
});

integrationTest("papel viewer le o alerta mas o SERVIDOR recusa acoes (o app so esconde botoes)", async () => {
  const bcrypt = require("bcrypt");
  const tag = crypto.randomBytes(4).toString("hex");
  const email = `viewer-${tag}@example.invalid`;
  const password = `pw-${tag}`;
  const [user] = await state.pool.execute(
    "INSERT INTO users (name, email, password_hash, global_role, status) VALUES (?, ?, ?, 'user', 'active')",
    [`Viewer ${tag}`, email, await bcrypt.hash(password, 4)],
  );
  const [member] = await state.pool.execute(
    "INSERT INTO organization_members (organization_id, user_id, role, status) VALUES (1, ?, 'viewer', 'active')",
    [user.insertId],
  );
  const [event] = await state.pool.execute(
    "INSERT INTO events (organization_id, device_id, event_type, severity, message, event_time) SELECT organization_id, id, 'sos_pressed', 'high', 'teste viewer', UTC_TIMESTAMP() FROM devices WHERE organization_id = 1 LIMIT 1",
  );
  const [[eventRow]] = await state.pool.query("SELECT device_id FROM events WHERE id = ?", [event.insertId]);
  const [alert] = await state.pool.execute(
    "INSERT INTO alerts (organization_id, event_id, device_id, status) VALUES (1, ?, ?, 'open')",
    [event.insertId, eventRow.device_id],
  );
  try {
    const login = await mobileLogin({ email, password });
    assert.equal(login.status, 200);
    const token = login.json.accessToken;
    assert.equal((await api("GET", `/api/alerts/${alert.insertId}`, { token })).status, 200);
    for (const action of ["acknowledge", "cancel", "resolve"]) {
      const response = await api("POST", `/api/alerts/${alert.insertId}/${action}`, { token, body: {} });
      assert.equal(response.status, 403, `viewer nao pode ${action}`);
    }
    const [[row]] = await state.pool.query("SELECT status FROM alerts WHERE id = ?", [alert.insertId]);
    assert.equal(row.status, "open");
    const [actions] = await state.pool.query("SELECT id FROM alert_actions WHERE alert_id = ?", [alert.insertId]);
    assert.equal(actions.length, 0, "nenhuma acao registrada");
  } finally {
    await state.pool.execute("DELETE FROM alerts WHERE id = ?", [alert.insertId]);
    await state.pool.execute("DELETE FROM events WHERE id = ?", [event.insertId]);
    await state.pool.execute("DELETE FROM auth_sessions WHERE user_id = ?", [user.insertId]);
    await state.pool.execute("DELETE FROM organization_members WHERE id = ?", [member.insertId]);
    await state.pool.execute("DELETE FROM users WHERE id = ?", [user.insertId]);
  }
});

integrationTest("login web legado continua funcionando sem sid (frontend preservado)", async () => {
  const legacy = await api("POST", "/api/auth/login", { body: { email: SEED_EMAIL, password: SEED_PASSWORD } });
  assert.equal(legacy.status, 200);
  assert.ok(legacy.json.token);
  assert.equal((await api("GET", "/api/me", { token: legacy.json.token })).status, 200);
});
