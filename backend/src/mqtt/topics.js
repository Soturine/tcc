const { env } = require("../config/env");

function normalizeBase(base) {
  return String(base || env.mqtt.topicBase).replace(/\/+$/, "");
}

function getSubscriptionTopics(base = env.mqtt.topicBase) {
  const topicBase = normalizeBase(base);
  return [
    `${topicBase}/+/events`,
    `${topicBase}/+/status`,
    `${topicBase}/+/telemetry`,
  ];
}

// Topico do ACK de aplicacao (critical-event-ack-v1). Usa o identificador do
// topico autenticado de origem, nunca um identificador escolhido pelo payload.
function buildCriticalEventAckTopic(deviceIdentifier, base = env.mqtt.topicBase) {
  return `${normalizeBase(base)}/${deviceIdentifier}/critical-event-acks`;
}

// Topico do resultado de ingestao (critical-event-result-v1), mesma regra de identidade.
function buildCriticalEventResultTopic(deviceIdentifier, base = env.mqtt.topicBase) {
  return `${normalizeBase(base)}/${deviceIdentifier}/critical-event-results`;
}

function parseDeviceTopic(topic, base = env.mqtt.topicBase) {
  const normalizedBase = normalizeBase(base);
  const baseParts = normalizedBase.split("/");
  const topicParts = String(topic || "").split("/");

  if (topicParts.length !== baseParts.length + 2) {
    return null;
  }

  for (let index = 0; index < baseParts.length; index += 1) {
    if (topicParts[index] !== baseParts[index]) {
      return null;
    }
  }

  return {
    topic,
    deviceIdentifier: topicParts[baseParts.length],
    channel: topicParts[baseParts.length + 1],
  };
}

module.exports = {
  buildCriticalEventAckTopic,
  buildCriticalEventResultTopic,
  getSubscriptionTopics,
  parseDeviceTopic,
};
