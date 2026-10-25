#!/usr/bin/env node
/*
 * Verificacao do perfil staging (mosquitto-staging.conf + mosquitto-acl.example) contra
 * um Mosquitto real. Executado por run-security-verification.sh, que gera CA, certificados
 * e senhas efemeros. Relatorio sanitizado: nunca grava senhas, chaves ou payloads.
 *
 * Cobre (F-17/F-27/F-33): sem listener plaintext; TLS com CA errada e hostname errado
 * recusados; credencial invalida, anonima e revogada recusadas; device forjando ACK ou
 * resultado para outro device; device publicando o PROPRIO ACK/resultado; device B
 * publicando eventos no topico de A; leitura alheia direta e por curinga; retained
 * recusado; backend como unico publisher de resultado.
 */
const { execFileSync } = require("child_process");
const fs = require("fs");
const net = require("net");
const path = require("path");
const mqtt = require(require.resolve("mqtt", { paths: [process.cwd()] }));

const env = process.env;
const BASE = "queda/devices";
const HOST = env.BROKER_HOST || "127.0.0.1";
const TLS_URL = `mqtts://${HOST}:${env.PORT_TLS}`;
const CA = fs.readFileSync(env.CA_FILE);
const ROGUE_CA = fs.readFileSync(env.ROGUE_CA_FILE);
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const results = [];
const check = (ok, message) => {
  results.push({ ok: Boolean(ok), message });
  console.log(`${ok ? "ok " : "NOK"} ${message}`);
};
// Toda espera tem teto: um passo pendurado vira falha explicita, nunca hang do CI.
const withTimeout = (promise, ms, label) =>
  Promise.race([promise, sleep(ms).then(() => { throw new Error(`timeout: ${label}`); })]);

function connect(username, password, { ca = CA, servername = "mqtt.test", url = TLS_URL } = {}) {
  return new Promise((resolve, reject) => {
    const client = mqtt.connect(url, {
      username,
      password,
      ca,
      servername,
      rejectUnauthorized: true,
      clientId: `${username || "anon"}-${process.pid}-${Math.random().toString(16).slice(2, 8)}`,
      reconnectPeriod: 0,
      connectTimeout: 4000,
    });
    client.once("connect", () => resolve(client));
    client.once("error", (error) => {
      client.end(true);
      reject(error);
    });
    client.once("close", () => reject(new Error("closed")));
  });
}

async function refused(promise) {
  try {
    const client = await promise;
    client.end(true);
    return { refused: false, reason: "connected" };
  } catch (error) {
    // Somente um codigo de erro de formato restrito: nunca a mensagem, que poderia
    // ecoar dados da tentativa (usuario/credencial) no log.
    const code = error && (error.code ?? error.reasonCode);
    const reason = /^[A-Z0-9_]{1,40}$/.test(String(code)) ? String(code)
      : Number.isInteger(code) ? `CONNACK_${code}` : "REFUSED";
    return { refused: true, reason };
  }
}

function collect(client, filter) {
  const received = [];
  client.on("message", (topic, payload) => received.push({ topic, body: payload.toString() }));
  return withTimeout(
    new Promise((resolve) => client.subscribe(filter, { qos: 1 }, () => resolve(received))),
    4000,
    `subscribe ${filter}`,
  );
}

function publish(client, topic, body, options = {}) {
  // PUBACK de publicacao negada pela ACL pode nao vir; o efeito e verificado no assinante.
  return Promise.race([
    new Promise((resolve) => {
      client.publish(topic, body, { qos: 1, ...options }, (error) => resolve(error || null));
    }),
    sleep(1500).then(() => "no_puback"),
  ]);
}

function plaintextPortOpen(port) {
  // Um listener MQTT plaintext responderia CONNACK; aqui basta provar que nada aceita MQTT.
  return new Promise((resolve) => {
    const client = mqtt.connect(`mqtt://${HOST}:${port}`, { reconnectPeriod: 0, connectTimeout: 2500 });
    client.once("connect", () => {
      client.end(true);
      resolve(true);
    });
    client.once("error", () => resolve(false));
    client.once("close", () => resolve(false));
    setTimeout(() => {
      client.end(true);
      resolve(false);
    }, 3000);
  });
}

async function main() {
  // ---------------- transporte
  check(!(await plaintextPortOpen(env.PORT_PLAIN)), "nenhum listener MQTT plaintext (1883) aceita conexao");
  check(net.isIP(HOST) !== 0, "teste via IP com SNI/hostname explicito");

  const wrongCa = await refused(connect("esp32_a", env.PA, { ca: ROGUE_CA }));
  check(wrongCa.refused, `CA errada recusada (${wrongCa.reason})`);
  const wrongHost = await refused(connect("esp32_a", env.PA, { servername: "evil.test" }));
  check(wrongHost.refused, `hostname errado recusado (${wrongHost.reason})`);

  // ---------------- autenticacao
  const anonymous = await refused(connect(undefined, undefined));
  check(anonymous.refused, `anonimo recusado (${anonymous.reason})`);
  const invalidCredentialAttempt = await refused(connect("esp32_a", "senha-invalida"));
  check(invalidCredentialAttempt.refused, `credencial invalida recusada (${invalidCredentialAttempt.reason})`);
  const unknownUser = await refused(connect("intruso", env.PA));
  check(unknownUser.refused, `usuario desconhecido recusado (${unknownUser.reason})`);

  const backend = await connect("queda-backend", env.PB);
  const deviceA = await connect("esp32_a", env.PA);
  const deviceB = await connect("esp32_b", env.PBB);
  check(true, "TLS valido + credenciais individuais conectam (backend, esp32_a, esp32_b)");

  // ---------------- ACL
  const ackA = `${BASE}/esp32_a/critical-event-acks`;
  const resultA = `${BASE}/esp32_a/critical-event-results`;
  const eventsA = `${BASE}/esp32_a/events`;
  const seenByA = await collect(deviceA, [ackA, resultA]);
  const seenByBDirect = await collect(deviceB, [ackA, resultA, eventsA]);
  const seenByBWildcard = await collect(await connect("esp32_b", env.PBB), `${BASE}/#`);
  const eventsSeenByBackend = await collect(backend, `${BASE}/+/events`);
  const ackSeenByBackend = await collect(backend, [ackA, resultA]);

  await publish(deviceB, ackA, "forged-ack-from-b");
  await publish(deviceB, resultA, "forged-result-from-b");
  await publish(deviceB, eventsA, "spoofed-event-from-b");
  await publish(deviceA, ackA, "self-ack-from-a");
  await publish(deviceA, resultA, "self-result-from-a");
  await publish(deviceA, eventsA, "legit-event-from-a");
  await publish(backend, resultA, "legit-result-from-backend");
  await publish(backend, ackA, "legit-ack-from-backend");
  await sleep(1500);

  const bodies = (list) => list.map((item) => item.body);
  check(bodies(seenByA).includes("legit-result-from-backend") && bodies(seenByA).includes("legit-ack-from-backend"),
    "device A recebe ACK e resultado publicados pelo backend");
  check(!bodies(seenByA).some((body) => body.startsWith("forged")), "ACK/resultado forjado por device B nao chega a A");
  check(!bodies(seenByA).some((body) => body.startsWith("self")), "device A nao consegue publicar o proprio ACK/resultado");
  check(!bodies(ackSeenByBackend).some((body) => body.startsWith("self") || body.startsWith("forged")),
    "nenhuma confirmacao de device chega ao topico de confirmacao");
  check(bodies(eventsSeenByBackend).includes("legit-event-from-a"), "backend recebe evento do proprio device A");
  check(!bodies(eventsSeenByBackend).includes("spoofed-event-from-b"), "evento de B no topico de A e descartado");
  check(seenByBDirect.length === 0, `B nao le ACK/resultado/eventos de A (${seenByBDirect.length})`);
  check(!seenByBWildcard.some((item) => item.topic.includes("/esp32_a/")), "curinga queda/devices/# nao vaza trafego de A para B");

  // ---------------- retained (retain_available false)
  const retainer = await connect("esp32_a", env.PA);
  const closed = new Promise((resolve) => retainer.once("close", () => resolve(true)));
  retainer.publish(eventsA, "retained-from-a", { qos: 1, retain: true });
  const disconnected = await Promise.race([closed, sleep(3000).then(() => false)]);
  check(disconnected, "publicacao retained desconecta o cliente (retain_available false)");
  retainer.end(true);
  const lateSubscriber = await connect("queda-backend", env.PB);
  const late = await collect(lateSubscriber, `${BASE}/+/events`);
  await sleep(800);
  check(!bodies(late).includes("retained-from-a"), "nenhuma mensagem retida fica no broker");

  // ---------------- revogacao
  const revokedBefore = await connect("esp32_revoked", env.PR);
  revokedBefore.end(true);
  execFileSync("docker", ["exec", env.MQTT_CONTAINER, "mosquitto_passwd", "-D", "/mosquitto/config/passwd", "esp32_revoked"]);
  execFileSync("docker", ["exec", env.MQTT_CONTAINER, "sh", "-c", "kill -HUP 1"]);
  await sleep(1000);
  const revokedAfter = await refused(connect("esp32_revoked", env.PR));
  check(revokedAfter.refused, `credencial revogada (removida + SIGHUP) recusada em nova conexao (${revokedAfter.reason})`);

  for (const client of [backend, deviceA, deviceB, lateSubscriber]) {
    client.end(true);
  }

  const report = {
    kind: "broker-security-verification",
    disclaimer: "Mosquitto real e descartavel com CA/senhas efemeras; nao e staging implantado.",
    finishedAt: new Date().toISOString(),
    profile: "infra/mqtt/mosquitto-staging.conf + infra/mqtt/mosquitto-acl.example",
    passed: results.filter((item) => item.ok).length,
    failed: results.filter((item) => !item.ok).length,
    results,
  };
  const outDir = path.join(process.cwd(), "logs", "security");
  fs.mkdirSync(outDir, { recursive: true });
  const outPath = path.join(outDir, `broker-security-${Date.now()}.json`);
  fs.writeFileSync(outPath, JSON.stringify(report, null, 2));
  console.log(`${report.passed}/${results.length} verificacoes; relatorio: ${outPath}`);
  // Clientes recusados/desconectados podem manter sockets; saida explicita evita hang no CI.
  process.exit(report.failed > 0 ? 1 : 0);
}

main().catch((error) => {
  console.error(`erro: ${error.message}`);
  process.exit(1);
});
