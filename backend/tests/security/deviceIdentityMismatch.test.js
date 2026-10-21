const assert = require("node:assert/strict");
const test = require("node:test");

const { loadWithMocks } = require("../helpers/moduleSandbox");

// Auditoria F-11: payload MQTT com device_uid de um device pareado de outra
// organizacao nao pode renomear esse device nem herdar seu tenant.
function buildHarness(devicesByUid) {
  const writes = [];
  const fakePool = {
    one: async (_executor, sql, params) => {
      if (/WHERE device_uid = \?/.test(sql)) {
        return devicesByUid[params[0]] || null;
      }
      return null;
    },
    execute: async (_executor, sql, params) => {
      if (/^\s*(INSERT|UPDATE)/i.test(sql)) {
        writes.push({ sql: sql.trim().split("\n")[0], params });
      }
      return [];
    },
    transaction: async (work) => work({ connection: true }),
  };
  const logs = [];
  const { module, restore } = loadWithMocks("src/services/deviceService.js", {
    "src/db/pool.js": fakePool,
    "src/utils/logger.js": {
      logger: {
        debug() {},
        error() {},
        info() {},
        warn(message, metadata) {
          logs.push({ message, metadata });
        },
      },
    },
  });
  return { module, restore, writes, logs };
}

const victim = {
  id: 77,
  device_uid: "esp32-VICTIM00000001",
  device_identifier: "esp32_vitima",
  claim_status: "claimed",
  organization_id: 9,
};

test("MQTT com device_uid de device pareado e topico diferente e rejeitado sem escrita", async () => {
  const harness = buildHarness({ [victim.device_uid]: victim });
  try {
    await assert.rejects(
      harness.module.getOrCreateDeviceByIdentity(
        { deviceUid: victim.device_uid, deviceIdentifier: "esp32_atacante", name: "x" },
        { connection: true },
        { rejectClaimedIdentifierChange: true },
      ),
      (error) => error.code === "DEVICE_IDENTITY_MISMATCH",
    );
    assert.deepEqual(harness.writes, []);
    assert.equal(harness.logs[0]?.metadata?.reason, "device_uid_identifier_mismatch");
    assert.equal(JSON.stringify(harness.logs).includes(victim.device_uid), false);
  } finally {
    harness.restore();
  }
});

test("fluxos HTTP autenticados mantem a renomeacao permitida (sem a opcao)", async () => {
  const harness = buildHarness({ [victim.device_uid]: victim });
  try {
    await harness.module
      .getOrCreateDeviceByIdentity(
        { deviceUid: victim.device_uid, deviceIdentifier: "esp32_renomeado", name: "x" },
        { connection: true },
      )
      .catch(() => {
        // o snapshot posterior depende de joins nao simulados aqui; importa a escrita
      });
    assert.ok(harness.writes.some((write) => /INSERT INTO devices/.test(write.sql)));
  } finally {
    harness.restore();
  }
});

test("device nao pareado ou com identificador igual segue o caminho normal", async () => {
  const unclaimed = { ...victim, claim_status: "unclaimed", organization_id: null };
  for (const [devices, identifier] of [
    [{ [victim.device_uid]: unclaimed }, "esp32_bancada"],
    [{ [victim.device_uid]: victim }, victim.device_identifier],
  ]) {
    const harness = buildHarness(devices);
    try {
      await harness.module
        .getOrCreateDeviceByIdentity(
          { deviceUid: victim.device_uid, deviceIdentifier: identifier, name: "x" },
          { connection: true },
          { rejectClaimedIdentifierChange: true },
        )
        .catch((error) => {
          assert.notEqual(error.code, "DEVICE_IDENTITY_MISMATCH");
        });
      assert.ok(harness.writes.some((write) => /INSERT INTO devices/.test(write.sql)));
    } finally {
      harness.restore();
    }
  }
});
