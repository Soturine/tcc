/*
 * Sessao movel revogavel (CP4) contra MySQL REAL e o app HTTP real (MYSQL_INTEGRATION=1, banco com schema+seed+migrations).
 */
const assert = require("node:assert/strict");

const { SEED_EMAIL, SEED_PASSWORD, api, crypto, integrationTest, mobileLogin, useLiveApp } = require("../helpers/liveApi");

const state = useLiveApp();

integrationTest("login movel valido devolve tokens; senha invalida e corpo vazio sao recusados", async () => {
  const ok = await mobileLogin();
  assert.equal(ok.status, 200);
  assert.equal(ok.json.tokenType, "Bearer");
  assert.match(ok.json.sessionId, /^[0-9a-f-]{36}$/);
  assert.ok(ok.json.refreshToken.length >= 40);
  assert.ok(new Date(ok.json.accessTokenExpiresAt) > new Date());
  assert.equal(ok.json.token, undefined, "nao devolve o JWT legado de 7 dias");
  const [[row]] = await state.pool.query(
    "SELECT refresh_token_hash, device_label FROM auth_sessions WHERE family_uuid = ?",
    [ok.json.sessionId],
  );
  assert.notEqual(row.refresh_token_hash, ok.json.refreshToken, "somente o hash e armazenado");
  assert.equal(row.refresh_token_hash, crypto.createHash("sha256").update(ok.json.refreshToken).digest("hex"));
  assert.equal(row.device_label, "test-runner");

  assert.equal((await mobileLogin({ password: "errada" })).status, 401);
  assert.equal((await api("POST", "/api/auth/mobile/login", { body: {} })).status, 400);
});

integrationTest("access token da sessao acessa a API; refresh rotaciona e o token antigo vira reuso", async () => {
  const login = await mobileLogin();
  const me = await api("GET", "/api/me", { token: login.json.accessToken });
  assert.equal(me.status, 200);
  assert.equal(me.json.user.email, SEED_EMAIL);

  const rotated = await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: login.json.refreshToken } });
  assert.equal(rotated.status, 200);
  assert.notEqual(rotated.json.refreshToken, login.json.refreshToken);
  assert.equal(rotated.json.sessionId, login.json.sessionId);
  assert.equal((await api("GET", "/api/me", { token: rotated.json.accessToken })).status, 200);

  // Replay do refresh ja rotacionado: 401 e a familia inteira e revogada.
  const replay = await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: login.json.refreshToken } });
  assert.equal(replay.status, 401);
  assert.equal(replay.json.details?.code, "SESSION_INVALID");
  const afterReplay = await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: rotated.json.refreshToken } });
  assert.equal(afterReplay.status, 401, "o refresh legitimo mais novo tambem cai apos o reuso");
  const meAfter = await api("GET", "/api/me", { token: rotated.json.accessToken });
  assert.equal(meAfter.status, 401, "access token da familia revogada e recusado antes de expirar");
  assert.equal(meAfter.json.details?.code, "SESSION_REVOKED");
});

integrationTest("refresh concorrente com o mesmo token: exatamente um sucesso", async () => {
  const login = await mobileLogin();
  const results = await Promise.all([1, 2, 3].map(() =>
    api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: login.json.refreshToken } })));
  const statuses = results.map((result) => result.status).sort();
  assert.deepEqual(statuses, [200, 401, 401]);
  const [[{ active }]] = await state.pool.query(
    "SELECT COUNT(*) AS active FROM auth_sessions WHERE family_uuid = ? AND status = 'active'",
    [login.json.sessionId],
  );
  // O perdedor detectou reuso e revogou a familia: nenhuma sessao paralela sobrevive.
  assert.equal(Number(active), 0);
});

integrationTest("logout revoga na hora, e idempotente e nao revela se o token existia", async () => {
  const login = await mobileLogin();
  const out = await api("POST", "/api/auth/mobile/logout", { body: { refreshToken: login.json.refreshToken } });
  assert.equal(out.status, 204);
  assert.equal((await api("GET", "/api/me", { token: login.json.accessToken })).status, 401);
  assert.equal((await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: login.json.refreshToken } })).status, 401);
  assert.equal((await api("POST", "/api/auth/mobile/logout", { body: { refreshToken: login.json.refreshToken } })).status, 204);
  assert.equal((await api("POST", "/api/auth/mobile/logout", { body: { refreshToken: "x".repeat(40) } })).status, 204);

  // Logout so com o Bearer (sem corpo) tambem revoga.
  const second = await mobileLogin();
  assert.equal((await api("POST", "/api/auth/mobile/logout", { token: second.json.accessToken })).status, 204);
  assert.equal((await api("GET", "/api/me", { token: second.json.accessToken })).status, 401);
});

integrationTest("refresh expirado e recusado e a familia e revogada", async () => {
  const login = await mobileLogin();
  await state.pool.query(
    "UPDATE auth_sessions SET expires_at = UTC_TIMESTAMP() - INTERVAL 1 MINUTE WHERE family_uuid = ?",
    [login.json.sessionId],
  );
  assert.equal((await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken: login.json.refreshToken } })).status, 401);
  const [[row]] = await state.pool.query(
    "SELECT status, revoked_reason FROM auth_sessions WHERE family_uuid = ? ORDER BY id DESC LIMIT 1",
    [login.json.sessionId],
  );
  assert.equal(row.status, "revoked");
  assert.equal(row.revoked_reason, "expired");
  assert.equal((await api("GET", "/api/me", { token: login.json.accessToken })).status, 401);
});

integrationTest("refresh malformado e token adulterado sao recusados sem detalhes", async () => {
  for (const refreshToken of [undefined, "", "curto", 12345, "x".repeat(500)]) {
    const result = await api("POST", "/api/auth/mobile/refresh", { body: { refreshToken } });
    assert.equal(result.status, 401, String(refreshToken).slice(0, 20));
  }
  const login = await mobileLogin();
  const [header, payload, signature] = login.json.accessToken.split(".");
  const tampered = `${header}.${payload}.${signature.slice(0, -2)}xx`;
  assert.equal((await api("GET", "/api/me", { token: tampered })).status, 401);
});

