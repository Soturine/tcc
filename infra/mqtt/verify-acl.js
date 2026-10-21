#!/usr/bin/env node
/*
 * Verifica a ACL de referencia contra um Mosquitto real e descartavel.
 *
 * 1. Gere tres senhas aleatorias (PB, PA, PBB) apenas para o teste.
 * 2. Suba o broker gerando config/senhas DENTRO do container (bind mount do
 *    Windows nao preserva o dono exigido pelo mosquitto):
 *      docker run -d --rm --name tcc-acl-mosquitto -p 127.0.0.1:18831:1883 \
 *        -v "$PWD/infra/mqtt:/src:ro" -e PB -e PA -e PBB eclipse-mosquitto:2 sh -c '
 *        cp /src/mosquitto-auth.example.conf /mosquitto/config/mosquitto.conf &&
 *        cp /src/mosquitto-acl.example /mosquitto/config/acl &&
 *        mosquitto_passwd -c -b /mosquitto/config/passwd queda-backend "$PB" &&
 *        mosquitto_passwd -b /mosquitto/config/passwd esp32_a "$PA" &&
 *        mosquitto_passwd -b /mosquitto/config/passwd esp32_b "$PBB" &&
 *        chown mosquitto:mosquitto /mosquitto/config/passwd /mosquitto/config/acl &&
 *        chmod 0700 /mosquitto/config/passwd /mosquitto/config/acl &&
 *        exec mosquitto -c /mosquitto/config/mosquitto.conf'
 * 3. A partir de backend/ (que ja possui a dependencia mqtt):
 *      node ../infra/mqtt/verify-acl.js mqtt://127.0.0.1:18831 "$PB" "$PA" "$PBB"
 * As senhas nunca sao versionadas.
 */
const path = require("path");
const mqtt = require(require.resolve("mqtt", { paths: [path.resolve(process.cwd())] }));

const [brokerUrl, backendPassword, devicePasswordA, devicePasswordB] = process.argv.slice(2);
const base = "queda/devices";
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function connect(username, password) {
  return new Promise((resolve, reject) => {
    const client = mqtt.connect(brokerUrl, { username, password, clientId: `${username}-${Date.now()}`, reconnectPeriod: 0 });
    client.once("connect", () => resolve(client));
    client.once("error", reject);
  });
}

function collect(client, topic) {
  const received = [];
  client.on("message", (incoming, payload) => {
    if (incoming === topic) {
      received.push(payload.toString());
    }
  });
  return new Promise((resolve) => client.subscribe(topic, { qos: 1 }, () => resolve(received)));
}

async function main() {
  const results = [];
  const check = (ok, message) => results.push({ ok, message });

  try {
    await connect("intruso", "senha-invalida");
    check(false, "credencial invalida deveria ser recusada");
  } catch {
    check(true, "credencial invalida recusada");
  }

  const backend = await connect("queda-backend", backendPassword);
  const deviceA = await connect("esp32_a", devicePasswordA);
  const deviceB = await connect("esp32_b", devicePasswordB);

  const ackTopicA = `${base}/esp32_a/critical-event-acks`;
  const ackSeenByA = await collect(deviceA, ackTopicA);
  const ackSeenByB = await collect(deviceB, ackTopicA);
  const eventsSeenByBackend = await collect(backend, `${base}/esp32_a/events`);

  deviceB.publish(ackTopicA, JSON.stringify({ forged: true }), { qos: 1 });
  deviceB.publish(`${base}/esp32_a/events`, JSON.stringify({ spoofed: true }), { qos: 1 });
  deviceA.publish(`${base}/esp32_a/events`, JSON.stringify({ legit: true }), { qos: 1 });
  backend.publish(ackTopicA, JSON.stringify({ legit_ack: true }), { qos: 1 });
  await sleep(1500);

  check(ackSeenByA.length === 1 && ackSeenByA[0].includes("legit_ack"), "device A recebe somente o ACK publicado pelo backend");
  check(!ackSeenByA.some((value) => value.includes("forged")), "ACK forjado por outro device nao chega");
  check(ackSeenByB.length === 0, "device B nao le o ACK do device A");
  check(eventsSeenByBackend.length === 1 && eventsSeenByBackend[0].includes("legit"), "backend recebe apenas o evento do proprio device A");

  for (const client of [backend, deviceA, deviceB]) {
    client.end(true);
  }
  for (const result of results) {
    console.log(`${result.ok ? "ok " : "NOK"} ${result.message}`);
  }
  process.exitCode = results.every((result) => result.ok) ? 0 : 1;
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
