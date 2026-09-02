const assert = require("node:assert/strict");
const test = require("node:test");

const express = require("express");

const { createApp } = require("../../src/app");
const { buildRateLimit } = require("../../src/middlewares/rateLimit");

async function withServer(app, work) {
  const server = app.listen(0);

  try {
    await new Promise((resolve) => server.once("listening", resolve));
    await work(server.address().port);
  } finally {
    await new Promise((resolve, reject) => {
      server.close((error) => (error ? reject(error) : resolve()));
    });
  }
}

test("CORS expõe somente origem local presente na allowlist default", async () => {
  await withServer(createApp(), async (port) => {
    const allowed = await fetch(`http://127.0.0.1:${port}/health`, {
      headers: { Origin: "http://localhost:5173" },
    });
    const denied = await fetch(`http://127.0.0.1:${port}/health`, {
      headers: { Origin: "https://unexpected.example" },
    });

    assert.equal(
      allowed.headers.get("access-control-allow-origin"),
      "http://localhost:5173",
    );
    assert.equal(denied.headers.get("access-control-allow-origin"), null);
  });
});

test("rate limiter retorna 429 depois do limite configurado", async () => {
  const app = express();
  app.use(buildRateLimit({ windowMs: 60_000, limit: 2 }));
  app.get("/", (_req, res) => res.json({ ok: true }));

  await withServer(app, async (port) => {
    const first = await fetch(`http://127.0.0.1:${port}/`);
    const second = await fetch(`http://127.0.0.1:${port}/`);
    const blocked = await fetch(`http://127.0.0.1:${port}/`);

    assert.equal(first.status, 200);
    assert.equal(second.status, 200);
    assert.equal(blocked.status, 429);
    assert.deepEqual(await blocked.json(), {
      message: "Muitas tentativas. Aguarde antes de tentar novamente.",
    });
  });
});
