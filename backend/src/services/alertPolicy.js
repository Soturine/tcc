/*
 * Politica de alerta por tipo de evento e evidencia (F-31, ADR-029).
 *
 * Prioridade NAO e diagnostico medico; descreve a acao esperada do cuidador:
 * - urgent:    pedido de ajuda manual ou queda com evidencia (local e/ou telemetria);
 * - attention: informacao que exige verificacao, sem urgencia presumida:
 *              queda suspeita, queda declarada sem evidencia suficiente e falha de sensor.
 * Eventos de teste e informativos nunca geram alerta.
 *
 * Mudanca deliberada (2026-10-25): `fall_detected` sem evidencia ("none") deixava de
 * gerar alerta; agora gera alerta de atencao para nao silenciar um possivel falso
 * negativo. Nenhum limiar do detector foi alterado.
 */
const ALERT_PRIORITIES = Object.freeze({
  URGENT: "urgent",
  ATTENTION: "attention",
});

const ALERT_EVENT_TYPES = new Set([
  "fall_detected",
  "fall_suspected",
  "sos_pressed",
  "manual_sos",
  "sensor_fault",
]);
const TEST_EVENT_TYPES = new Set(["alert_test"]);
const EVIDENCE_SOURCES = new Set(["device", "server_telemetry", "both", "none"]);

function hasTelemetryEvidence(event) {
  return ["linked", "partial"].includes(event?.evidenceStatus);
}

// Registros antigos sem evidence_source mantem a regra baseada so na telemetria.
function fallHasEvidence(event) {
  if (EVIDENCE_SOURCES.has(event.evidenceSource)) {
    return event.evidenceSource !== "none";
  }
  return hasTelemetryEvidence(event);
}

function resolveAlertPolicy(event) {
  const eventType = event?.eventType;
  if (TEST_EVENT_TYPES.has(eventType)) {
    return { createAlert: false, priority: null, reason: "test_event" };
  }
  if (!ALERT_EVENT_TYPES.has(eventType)) {
    return { createAlert: false, priority: null, reason: "informational_event" };
  }

  switch (eventType) {
    case "sos_pressed":
    case "manual_sos":
      return { createAlert: true, priority: ALERT_PRIORITIES.URGENT, reason: "manual_help_request" };
    case "fall_detected":
      return fallHasEvidence(event)
        ? { createAlert: true, priority: ALERT_PRIORITIES.URGENT, reason: "fall_with_evidence" }
        : { createAlert: true, priority: ALERT_PRIORITIES.ATTENTION, reason: "fall_evidence_insufficient" };
    case "fall_suspected":
      return { createAlert: true, priority: ALERT_PRIORITIES.ATTENTION, reason: "fall_unconfirmed" };
    default:
      // sensor_fault: a protecao esta degradada; requer verificacao do dispositivo.
      return { createAlert: true, priority: ALERT_PRIORITIES.ATTENTION, reason: "protection_degraded" };
  }
}

module.exports = {
  ALERT_EVENT_TYPES,
  ALERT_PRIORITIES,
  EVIDENCE_SOURCES,
  TEST_EVENT_TYPES,
  hasTelemetryEvidence,
  resolveAlertPolicy,
};
