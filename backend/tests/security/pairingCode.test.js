const assert = require("node:assert/strict");
const test = require("node:test");

const { generatePairingCode } = require("../../src/services/pairingService");

test("generatePairingCode usa seis caracteres não ambíguos", () => {
  for (let index = 0; index < 200; index += 1) {
    assert.match(generatePairingCode(), /^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{6}$/);
  }
});
