#!/usr/bin/env node
/*
 * Ensaios de falha do fluxo critico device -> MQTT -> backend -> MySQL -> ACK.
 *
 * O "virtual device" emula a semantica da CriticalEventOutbox do firmware:
 * publica, espera o ACK de aplicacao e retransmite o MESMO event_uuid ate recebe-lo.
 * O backend real e iniciado/parado por este script; MySQL e Mosquitto devem ser
 * descartaveis. Resultados sao de EMULACAO e nao substituem ensaio com ESP32 (HIL).
 *
 * Uso: npm run test:e2e:critical
 * Variaveis: MYSQL_*, MQTT_BROKER_URL, E2E_BACKEND_PORT (4100),
 *            E2E_DB_CONTAINER (opcional: habilita o cenario de banco indisponivel via docker pause).
 */
const { spawn, execFileSync } = require("child_process");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const mqtt = require("mqtt");
const mysql = require("mysql2/promise");

const BACKEND_ROOT = path.resolve(__dirname, "../..");
const BROKER_URL = process.env.MQTT_BROKER_URL || "mqtt://127.0.0.1:1883";
const TOPIC_BASE = process.env.MQTT_TOPIC_BASE || "queda/devices";
const PORT = Number(process.env.E2E_BACKEND_PORT || 4100);
const ACK_TIMEOUT_MS = Number(process.env.E2E_ACK_TIMEOUT_MS || 4000);
const RUN_ID = crypto.randomBytes(3).toString("hex");

const dbConfig = {
  host: process.env.MYSQL_HOST || "127.0.0.1",
  port: Number(process.env.MYSQL_PORT || 3306),
  user: process.env.MYSQL_USER || "root",
  password: process.env.MYSQL_PASSWORD || "",
  database: process.env.MYSQL_DATABASE || "queda_monitor",
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// ---------------------------------------------------------------- backend

let backend = null;

async function startBackend() {
  const logPath = path.join(BACKEND_ROOT, "logs", "e2e", `backend-${RUN_ID}.log`);
  fs.mkdirSync(path.dirname(logPath), { recursive: true });
  const log = fs.openSync(logPath, "a");
  backend = spawn(process.execPath, ["src/server.js"], {
    cwd: BACKEND_ROOT,
    env: {
      ...process.env,
      NODE_ENV: "test",
      PORT: String(PORT),
      JWT_SECRET: process.env.JWT_SECRET || "e2e-local-only-not-for-staging",
      MQTT_BROKER_URL: BROKER_URL,
      MQTT_CLIENT_ID: `queda-backend-e2e-${RUN_ID}`,
      MQTT_TOPIC_BASE: TOPIC_BASE,
      ALERT_DELIVERY_DRY_RUN: "true",
    },
    stdio: ["ignore", log, log],
  });

  for (let attempt = 0; attempt < 60; attempt += 1) {
    try {
      const response = await fetch(`http://127.0.0.1:${PORT}/health`);
      if (response.ok) {
        // Aguarda a assinatura MQTT do backend antes de publicar.
        await sleep(800);
        return;
      }
    } catch {
      // ainda subindo
    }
    await sleep(500);
  }
  throw new Error(`backend nao ficou pronto; ver ${logPath}`);
}

async function stopBackend() {
  if (!backend) {
    return;
  }
  const child = backend;
  backend = null;
  await new Promise((resolve) => {
    child.once("exit", resolve);
    child.kill("SIGTERM");
    setTimeout(() => {
      child.kill("SIGKILL");
      resolve();
    }, 5000);
  });
}

// ---------------------------------------------------------------- virtual device

class VirtualDevice {
  constructor(deviceId) {
    this.deviceId = deviceId;
    this.deviceUid = `e2e-${RUN_ID}-${deviceId}`;
    this.eventTopic = `${TOPIC_BASE}/${deviceId}/events`;
    this.ackTopic = `${TOPIC_BASE}/${deviceId}/critical-event-acks`;
    this.acks = [];
    this.sequence = 0;
    this.bootId = `b${crypto.randomBytes(4).toString("hex")}-${crypto.randomBytes(4).toString("hex")}`;
  }

  async connect() {
    this.client = mqtt.connect(BROKER_URL, { clientId: `${this.deviceUid}-${Date.now()}`, clean: true });
    await new Promise((resolve, reject) => {
      this.client.once("connect", resolve);
      this.client.once("error", reject);
    });
    await new Promise((resolve, reject) => {
      this.client.subscribe(this.ackTopic, { qos: 1 }, (error) => (error ? reject(error) : resolve()));
    });
    this.client.on("message", (topic, buffer) => {
      if (topic === this.ackTopic) {
        this.acks.push({ receivedAt: Date.now(), ack: JSON.parse(buffer.toString()) });
      }
    });
  }

  close() {
    return new Promise((resolve) => this.client.end(false, {}, resolve));
  }

  nextUuid() {
    this.sequence += 1;
    return `${this.deviceUid}:${this.bootId}:${String(this.sequence).padStart(8, "0")}`;
  }

  buildFall({ occurredAt = new Date(), localEvidence = true, clockQuality = "synced", eventType = "fall_detected" } = {}) {
    const payload = {
      device_id: this.deviceId,
      device_uid: this.deviceUid,
      event_type: eventType,
      event_uuid: this.nextUuid(),
      event_sequence: this.sequence,
      boot_id: this.bootId,
      device_uptime_ms: 120000 + this.sequence,
      clock_quality: clockQuality,
      occurred_at_device: clockQuality === "synced" ? occurredAt.toISOString() : null,
      timestamp: Math.floor(occurredAt.getTime() / 1000),
      immobility_confirmed: eventType === "fall_detected",
      accel_magnitude: 3.2,
      gyro_magnitude: 210,
    };
    if (localEvidence) {
      payload.local_evidence = {
        version: 1,
        decision_source: "edge",
        algorithm_version: "threshold_fsm_v2_time_features_v1",
        detector_mode: "normal",
        impact_detected: true,
        orientation_change_detected: true,
        immobility_detected: true,
        peak_accel_g: 3.2,
        peak_gyro_dps: 210,
        orientation_delta_deg: 82,
        immobility_duration_ms: 2000,
      };
    }
    return payload;
  }

  publishRaw(payload) {
    return new Promise((resolve, reject) => {
      this.client.publish(this.eventTopic, JSON.stringify(payload), { qos: 1 }, (error) =>
        error ? reject(error) : resolve());
    });
  }

  async waitAck(eventUuid, afterIndex = 0, timeoutMs = ACK_TIMEOUT_MS) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const found = this.acks.slice(afterIndex).find((entry) => entry.ack.event_uuid === eventUuid);
      if (found) {
        return found;
      }
      await sleep(50);
    }
    return null;
  }

  // Semantica da outbox: retransmite o mesmo UUID ate o ACK (ou esgotar tentativas).
  async deliver(payload, { attempts = 6, ignoreFirstAcks = 0, beforeRetry = null } = {}) {
    const startedAt = Date.now();
    let ignored = 0;
    for (let attempt = 1; attempt <= attempts; attempt += 1) {
      const before = this.acks.length;
      await this.publishRaw(payload);
      const ack = await this.waitAck(payload.event_uuid, before);
      if (ack && ignored < ignoreFirstAcks) {
        ignored += 1; // ACK "perdido": o device nao o processa e retransmite.
      } else if (ack) {
        return { ack: ack.ack, attempts: attempt, elapsedMs: ack.receivedAt - startedAt };
      }
      if (beforeRetry) {
        await beforeRetry(attempt);
      }
    }
    return { ack: null, attempts, elapsedMs: Date.now() - startedAt };
  }
}

// ---------------------------------------------------------------- verificacao

let db = null;

async function eventRows(eventUuid) {
  const [rows] = await db.query(
    `SELECT e.id, e.event_type, e.event_time, e.occurred_at_device, e.received_at, e.persisted_at,
            e.evidence_status, e.evidence_source, e.severity, e.clock_quality,
            (SELECT COUNT(*) FROM alerts a WHERE a.event_id = e.id) AS alert_count
     FROM events e WHERE e.event_uuid = ?`,
    [eventUuid],
  );
  return rows;
}

async function alertCountForDevice(deviceId) {
  const [[row]] = await db.query(
    `SELECT COUNT(*) AS total FROM alerts a INNER JOIN devices d ON d.id = a.device_id
     WHERE d.device_identifier = ?`,
    [deviceId],
  );
  return Number(row.total);
}

function check(results, condition, message) {
  results.push({ ok: Boolean(condition), message });
}

// ---------------------------------------------------------------- cenarios

const scenarios = [];
function scenario(name, run, options = {}) {
  scenarios.push({ name, run, ...options });
}

scenario("1. queda com rede ativa: um evento, um alerta, ACK committed", async (results) => {
  const device = new VirtualDevice(`e2e-online-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall();
  const delivery = await device.deliver(payload);
  const rows = await eventRows(payload.event_uuid);
  check(results, delivery.ack?.status === "committed", `ACK committed (tentativas=${delivery.attempts})`);
  check(results, rows.length === 1 && Number(rows[0].alert_count) === 1, "1 evento e 1 alerta");
  check(results, rows[0]?.evidence_source === "device", `evidence_source=${rows[0]?.evidence_source}`);
  check(results, delivery.ack?.event_id === Number(rows[0]?.id), "event_id do ACK corresponde ao registro");
  await device.close();
  return { ackLatencyMs: delivery.elapsedMs };
});

scenario("2. queda confirmada offline entregue 40 min depois sem telemetria SQL", async (results) => {
  const device = new VirtualDevice(`e2e-offline-${RUN_ID}`);
  await device.connect();
  const occurredAt = new Date(Date.now() - 40 * 60 * 1000);
  const payload = device.buildFall({ occurredAt });
  const delivery = await device.deliver(payload);
  const rows = await eventRows(payload.event_uuid);
  const row = rows[0] || {};
  check(results, delivery.ack?.status === "committed", "ACK committed");
  check(results, Number(row.alert_count) === 1, `um alerta logico (alert_count=${row.alert_count})`);
  check(results, row.evidence_status === "none" && row.evidence_source === "device",
    `sem telemetria SQL, evidencia local (status=${row.evidence_status}, source=${row.evidence_source})`);
  check(results, Math.abs(new Date(row.event_time).getTime() - occurredAt.getTime()) < 1500,
    "event_time = instante de ocorrencia, nao de reconexao");
  check(results, new Date(row.received_at).getTime() - occurredAt.getTime() > 39 * 60 * 1000,
    "received_at preserva o atraso real");
  await device.close();
});

scenario("3. MQTT duplicado (3 publicacoes simultaneas do mesmo UUID)", async (results) => {
  const device = new VirtualDevice(`e2e-dup-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall();
  await Promise.all([device.publishRaw(payload), device.publishRaw(payload), device.publishRaw(payload)]);
  await sleep(ACK_TIMEOUT_MS);
  const acks = device.acks.filter((entry) => entry.ack.event_uuid === payload.event_uuid);
  const rows = await eventRows(payload.event_uuid);
  check(results, rows.length === 1, `1 evento logico (linhas=${rows.length})`);
  check(results, Number(rows[0]?.alert_count) === 1, "1 alerta logico");
  check(results, await alertCountForDevice(device.deviceId) === 1, "nenhum alerta extra no device");
  check(results, acks.length === 3 && acks.filter((entry) => entry.ack.status === "committed").length === 1,
    `3 ACKs, exatamente 1 committed (${acks.map((entry) => entry.ack.status).join(",")})`);
  check(results, new Set(acks.map((entry) => entry.ack.event_id)).size === 1, "todos os ACKs com o mesmo event_id");
  await device.close();
});

scenario("4. ACK perdido: device retransmite e recebe duplicate", async (results) => {
  const device = new VirtualDevice(`e2e-acklost-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall();
  const delivery = await device.deliver(payload, { ignoreFirstAcks: 1 });
  const rows = await eventRows(payload.event_uuid);
  check(results, delivery.attempts === 2 && delivery.ack?.status === "duplicate", `segunda tentativa recebe duplicate (${delivery.ack?.status})`);
  check(results, rows.length === 1 && Number(rows[0].alert_count) === 1, "1 evento e 1 alerta");
  await device.close();
});

scenario("5. payload conflitante com o mesmo UUID nao e confirmado", async (results) => {
  const device = new VirtualDevice(`e2e-conflict-${RUN_ID}`);
  await device.connect();
  const original = device.buildFall();
  await device.deliver(original);
  const conflicting = { ...original, event_type: "sos_pressed" };
  const delivery = await device.deliver(conflicting, { attempts: 1 });
  const rows = await eventRows(original.event_uuid);
  check(results, delivery.ack === null, "sem ACK para o payload conflitante");
  check(results, rows.length === 1 && rows[0].event_type === "fall_detected", "evento original preservado");
  await device.close();
});

scenario("6. backend indisponivel e reiniciado: evento pendente entregue apos retorno", async (results) => {
  const device = new VirtualDevice(`e2e-restart-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall();
  await stopBackend();
  const delivery = await device.deliver(payload, {
    attempts: 3,
    beforeRetry: async (attempt) => {
      if (attempt === 1) {
        await startBackend();
      }
    },
  });
  const rows = await eventRows(payload.event_uuid);
  check(results, delivery.attempts >= 2 && delivery.ack?.status === "committed",
    `primeira tentativa sem backend, ACK apos retorno (tentativas=${delivery.attempts})`);
  check(results, rows.length === 1 && Number(rows[0].alert_count) === 1, "1 evento e 1 alerta");

  // Commit ja ocorreu; backend reinicia antes de o device processar o ACK.
  await stopBackend();
  await startBackend();
  const replay = await device.deliver(payload, { attempts: 2 });
  check(results, replay.ack?.status === "duplicate", "apos restart, retransmissao recebe duplicate");
  check(results, (await eventRows(payload.event_uuid)).length === 1, "restart nao duplica evento");
  await device.close();
});

scenario("7. banco indisponivel: sem ACK ate o banco voltar", async (results) => {
  const container = process.env.E2E_DB_CONTAINER;
  const device = new VirtualDevice(`e2e-dbdown-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall();
  // docker pause congela o MySQL: a transacao fica bloqueada (nao recusada).
  execFileSync("docker", ["pause", container]);
  let unpausedAt = null;
  let delivery;
  try {
    delivery = await device.deliver(payload, {
      attempts: 4,
      beforeRetry: async (attempt) => {
        if (attempt === 1) {
          unpausedAt = Date.now();
          execFileSync("docker", ["unpause", container]);
          await sleep(1000);
        }
      },
    });
  } finally {
    try {
      execFileSync("docker", ["unpause", container], { stdio: "ignore" });
    } catch {
      // ja despausado
    }
  }
  const acks = device.acks.filter((entry) => entry.ack.event_uuid === payload.event_uuid);
  const rows = await eventRows(payload.event_uuid);
  check(results, unpausedAt !== null && acks.every((entry) => entry.receivedAt >= unpausedAt),
    "nenhum ACK enquanto o banco estava indisponivel");
  check(results, delivery.attempts >= 2 && Boolean(delivery.ack),
    `ACK apos o banco voltar (tentativas=${delivery.attempts}, status=${delivery.ack?.status})`);
  check(results, acks.filter((entry) => entry.ack.status === "committed").length === 1,
    "exatamente um ACK committed; os demais duplicate");
  check(results, rows.length === 1 && Number(rows[0].alert_count) === 1, "1 evento e 1 alerta");
}, { requiresEnv: "E2E_DB_CONTAINER" });

scenario("8. entregas fora de ordem preservam instantes e geram alertas distintos", async (results) => {
  const device = new VirtualDevice(`e2e-order-${RUN_ID}`);
  await device.connect();
  const later = device.buildFall({ occurredAt: new Date(Date.now() - 10 * 60 * 1000) });
  const earlier = device.buildFall({ occurredAt: new Date(Date.now() - 30 * 60 * 1000) });
  const first = await device.deliver(later);
  const second = await device.deliver(earlier);
  const [laterRow] = await eventRows(later.event_uuid);
  const [earlierRow] = await eventRows(earlier.event_uuid);
  check(results, first.ack && second.ack, "ambos confirmados");
  check(results, new Date(earlierRow.event_time) < new Date(laterRow.event_time), "ordem de ocorrencia preservada");
  check(results, new Date(earlierRow.received_at) > new Date(laterRow.received_at), "ordem de recebimento invertida");
  check(results, Number(laterRow.alert_count) === 1 && Number(earlierRow.alert_count) === 1,
    "quedas separadas por 20 min nao sao fundidas");
  await device.close();
});

scenario("9. queda sem evidencia local nem telemetria: persistida e confirmada, sem alerta", async (results) => {
  const device = new VirtualDevice(`e2e-noevidence-${RUN_ID}`);
  await device.connect();
  const payload = device.buildFall({ localEvidence: false, clockQuality: "unsynced" });
  const delivery = await device.deliver(payload);
  const [row] = await eventRows(payload.event_uuid);
  check(results, delivery.ack?.status === "committed", "ACK committed (persistencia nao depende de alerta)");
  check(results, row?.evidence_source === "none" && Number(row.alert_count) === 0, "evidencia insuficiente: sem alerta");
  check(results, row?.clock_quality === "unsynced", "clock_quality unsynced preservado");
  await device.close();
});

scenario("10. SOS gera alerta; alert_test nao gera alerta real", async (results) => {
  const device = new VirtualDevice(`e2e-types-${RUN_ID}`);
  await device.connect();
  const sos = { ...device.buildFall({ localEvidence: false }), event_type: "sos_pressed", immobility_confirmed: false };
  const testEvent = { ...device.buildFall({ localEvidence: false }), event_type: "alert_test", immobility_confirmed: false };
  const sosDelivery = await device.deliver(sos);
  const testDelivery = await device.deliver(testEvent);
  const [sosRow] = await eventRows(sos.event_uuid);
  const [testRow] = await eventRows(testEvent.event_uuid);
  check(results, sosDelivery.ack && Number(sosRow.alert_count) === 1, "SOS: ACK e alerta");
  check(results, testDelivery.ack && Number(testRow.alert_count) === 0 && testRow.severity === "low",
    "alert_test: ACK, sem alerta, severidade low");
  await device.close();
});

scenario("11. device_id do payload divergente do topico: rejeitado sem ACK", async (results) => {
  const device = new VirtualDevice(`e2e-mismatch-${RUN_ID}`);
  await device.connect();
  const payload = { ...device.buildFall(), device_id: "outro-device" };
  const delivery = await device.deliver(payload, { attempts: 1 });
  check(results, delivery.ack === null, "sem ACK");
  check(results, (await eventRows(payload.event_uuid)).length === 0, "nada persistido");
  await device.close();
});

scenario("12. device_uid de device pareado usado em outro topico: rejeitado sem remapear tenant", async (results) => {
  const victim = new VirtualDevice(`e2e-victim-${RUN_ID}`);
  await victim.connect();
  const first = await victim.deliver(victim.buildFall());
  check(results, Boolean(first.ack), "device vitima registrado");

  const [[organization]] = await db.query("SELECT id FROM organizations ORDER BY id LIMIT 1");
  await db.query(
    "UPDATE devices SET claim_status = 'claimed', organization_id = ? WHERE device_uid = ?",
    [organization.id, victim.deviceUid],
  );

  const attacker = new VirtualDevice(`e2e-attacker-${RUN_ID}`);
  await attacker.connect();
  const forged = { ...attacker.buildFall(), device_uid: victim.deviceUid };
  const delivery = await attacker.deliver(forged, { attempts: 1 });
  const [[victimRow]] = await db.query(
    "SELECT device_identifier, organization_id FROM devices WHERE device_uid = ?",
    [victim.deviceUid],
  );
  check(results, delivery.ack === null, "sem ACK para a mensagem forjada");
  check(results, (await eventRows(forged.event_uuid)).length === 0, "nenhum evento atribuido a vitima");
  check(results, victimRow.device_identifier === victim.deviceId, "identificador da vitima preservado");
  check(results, Number(victimRow.organization_id) === Number(organization.id), "tenant da vitima preservado");
  await victim.close();
  await attacker.close();
});

// ---------------------------------------------------------------- execucao

async function main() {
  db = await mysql.createConnection({ ...dbConfig, timezone: "Z" });
  const report = {
    kind: "emulation",
    disclaimer: "Virtual device sobre backend, MySQL e Mosquitto reais e descartaveis; nao e ensaio com ESP32.",
    runId: RUN_ID,
    startedAt: new Date().toISOString(),
    brokerUrl: BROKER_URL,
    scenarios: [],
  };
  let failed = 0;

  try {
    // Pre-condicao: nenhum outro backend pode estar assinando o mesmo broker,
    // senao ACKs "estrangeiros" invalidam os cenarios de indisponibilidade.
    const probe = new VirtualDevice(`e2e-probe-${RUN_ID}`);
    await probe.connect();
    const probePayload = probe.buildFall();
    await probe.publishRaw(probePayload);
    const foreignAck = await probe.waitAck(probePayload.event_uuid, 0, 2500);
    await probe.close();
    if (foreignAck) {
      throw new Error("outro backend respondeu ao probe antes do inicio; encerre-o antes do ensaio");
    }

    await startBackend();
    for (const item of scenarios) {
      if (item.requiresEnv && !process.env[item.requiresEnv]) {
        report.scenarios.push({ name: item.name, status: "skipped", reason: `${item.requiresEnv} ausente` });
        console.log(`SKIP ${item.name} (${item.requiresEnv} ausente)`);
        continue;
      }
      if (!backend) {
        await startBackend();
      }
      const results = [];
      let metrics = null;
      let error = null;
      try {
        metrics = await item.run(results);
      } catch (caught) {
        error = caught.message;
      }
      const ok = !error && results.length > 0 && results.every((result) => result.ok);
      failed += ok ? 0 : 1;
      report.scenarios.push({ name: item.name, status: ok ? "passed" : "failed", checks: results, error, metrics });
      console.log(`${ok ? "PASS" : "FAIL"} ${item.name}`);
      for (const result of results) {
        console.log(`   ${result.ok ? "ok " : "NOK"} ${result.message}`);
      }
      if (error) {
        console.log(`   erro: ${error}`);
      }
    }
  } finally {
    await stopBackend();
    await db.end();
  }

  report.finishedAt = new Date().toISOString();
  const outPath = path.join(BACKEND_ROOT, "logs", "e2e", `critical-delivery-${RUN_ID}.json`);
  fs.writeFileSync(outPath, JSON.stringify(report, null, 2));
  console.log(`relatorio: ${outPath}`);
  process.exitCode = failed > 0 ? 1 : 0;
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
