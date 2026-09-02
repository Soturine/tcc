const assert = require("node:assert/strict");
const test = require("node:test");

const { loadWithMocks } = require("../helpers/moduleSandbox");

function buildDevice(overrides = {}) {
  return {
    id: 5,
    deviceUid: "legacy:esp32_01",
    deviceIdentifier: "esp32_01",
    organization: { id: 1, name: "Familia Demo" },
    currentPatient: { id: 2, fullName: "Paciente Demo" },
    currentAssignmentHistoryId: 3,
    ...overrides,
  };
}

function buildHarness(telemetryRows = [], options = {}) {
  const calls = {
    duplicateQueries: [],
    eventInserts: [],
    evidenceInserts: [],
    telemetryQueries: [],
    logs: [],
  };
  let insertedEvent = null;
  let duplicateQueryIndex = 0;

  const fakePool = {
    execute: async (_executor, sql, params) => {
      if (/FROM telemetry_logs/.test(sql)) {
        calls.telemetryQueries.push({ sql, params });
        const [deviceId, organizationId, patientId, assignmentId, windowStart, windowEnd] = params;

        return telemetryRows.filter((row) => {
          const createdAt = new Date(row.created_at).getTime();
          return row.device_id === deviceId
            && (row.organization_id ?? null) === organizationId
            && (row.patient_id ?? null) === patientId
            && (row.device_assignment_history_id ?? null) === assignmentId
            && createdAt >= new Date(windowStart).getTime()
            && createdAt <= new Date(windowEnd).getTime();
        });
      }

      if (/INSERT INTO events/.test(sql)) {
        calls.eventInserts.push({ sql, params });
        if (options.insertError) {
          throw options.insertError;
        }
        const columnBlock = sql.match(/INSERT INTO events\s*\(([^)]+)\)/i)?.[1] || "";
        const columns = columnBlock.split(",").map((column) => column.trim());
        insertedEvent = Object.fromEntries(columns.map((column, index) => [column, params[index]]));
        return { insertId: 40, affectedRows: 1 };
      }

      if (/INSERT IGNORE INTO event_telemetry_evidence/.test(sql)) {
        calls.evidenceInserts.push({ sql, params });
        return { insertId: 1, affectedRows: 1 };
      }

      return [];
    },
    one: async (_executor, sql, params) => {
      if (/WHERE e\.event_uuid = \?/.test(sql)) {
        calls.duplicateQueries.push({ sql, params });
        if (options.duplicateRows) {
          return options.duplicateRows[duplicateQueryIndex++] || null;
        }
        return options.duplicateRow || null;
      }

      return {
        id: 40,
        ...insertedEvent,
        persisted_at: new Date("2026-05-13T14:38:15.000Z"),
        created_at: new Date("2026-05-13T14:38:15.000Z"),
        deviceId: 5,
        deviceUid: "legacy:esp32_01",
        deviceIdentifier: "esp32_01",
        deviceName: "Pulseira ESP32",
        patientName: "Paciente Demo",
      };
    },
  };

  const { module, restore } = loadWithMocks("src/services/eventService.js", {
    "src/db/pool.js": fakePool,
    "src/utils/logger.js": {
      logger: {
        debug(message, metadata) {
          calls.logs.push({ level: "debug", message, metadata });
        },
        error(message, metadata) {
          calls.logs.push({ level: "error", message, metadata });
        },
        info(message, metadata) {
          calls.logs.push({ level: "info", message, metadata });
        },
        warn(message, metadata) {
          calls.logs.push({ level: "warn", message, metadata });
        },
      },
    },
  });

  return {
    calls,
    eventService: module,
    restore,
  };
}

function eventInsertValue(call, columnName) {
  const columnBlock = call.sql.match(/INSERT INTO events\s*\(([^)]+)\)/i)?.[1] || "";
  const columns = columnBlock.split(",").map((column) => column.trim());
  return call.params[columns.indexOf(columnName)];
}

test("fall_detected com telemetria recente vincula evidencia", async () => {
  const eventTime = new Date("2026-05-13T14:38:10.000Z");
  const harness = buildHarness([
    {
      id: 21,
      device_id: 5,
      organization_id: 1,
      patient_id: 2,
      device_assignment_history_id: 3,
      accel_magnitude: 1.2,
      gyro_magnitude: 18,
      created_at: new Date("2026-05-13T14:38:03.000Z"),
    },
    {
      id: 22,
      device_id: 5,
      organization_id: 1,
      patient_id: 2,
      device_assignment_history_id: 3,
      accel_magnitude: 3.9,
      gyro_magnitude: 181,
      created_at: new Date("2026-05-13T14:38:09.900Z"),
    },
  ]);

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "fall_detected",
        event_uuid: "evt-fall-001",
        event_sequence: 7,
        sample_seq: 123,
        timestamp: Math.floor(eventTime.getTime() / 1000),
        accel_magnitude: 3.9,
        immobility_confirmed: true,
        decision_source: "firmware",
        algorithm_version: "threshold_fsm_v2_time_features_v1",
        confidence: null,
        confidence_status: "not_available",
        features_time_domain: {
          available: true,
          sample_count: 64,
          peak_jerk: 8.4,
        },
      },
      correlationId: "trace_evidence",
    });

    assert.equal(event.evidenceStatus, "linked");
    assert.equal(event.evidenceTelemetryId, 22);
    assert.equal(event.evidenceSampleCount, 2);
    assert.equal(event.severity, "critical");
    assert.equal(event.evidenceSummary.firmwareDecision.decisionSource, "firmware");
    assert.equal(event.eventUuid, "evt-fall-001");
    assert.equal(event.sampleSeq, 123);
    assert.equal(event.evidenceSummary.firmwareDecision.eventUuid, "evt-fall-001");
    assert.equal(event.evidenceSummary.firmwareDecision.sampleSeq, 123);
    assert.equal(
      event.evidenceSummary.firmwareDecision.algorithmVersion,
      "threshold_fsm_v2_time_features_v1",
    );
    assert.equal(event.evidenceSummary.firmwareDecision.confidence, null);
    assert.equal(
      event.evidenceSummary.firmwareDecision.confidenceStatus,
      "not_available",
    );
    assert.equal(event.evidenceSummary.firmwareDecision.featuresTimeDomain.sample_count, 64);
    assert.equal(harness.calls.evidenceInserts.length, 2);
  } finally {
    harness.restore();
  }
});

test("fall_detected sem telemetria recente vira evento tecnico sem evidencia", async () => {
  const harness = buildHarness([]);

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "fall_detected",
        timestamp: Math.floor(new Date("2026-05-13T14:38:10.000Z").getTime() / 1000),
        immobility_confirmed: true,
      },
    });

    assert.equal(event.evidenceStatus, "none");
    assert.equal(event.evidenceTelemetryId, null);
    assert.equal(event.evidenceSampleCount, 0);
    assert.equal(event.severity, "medium");
    assert.equal(harness.calls.evidenceInserts.length, 0);
    assert.ok(harness.calls.logs.some((entry) => entry.level === "warn"));
  } finally {
    harness.restore();
  }
});

test("fall_detected reenviado com mesmo event_uuid reaproveita evento existente", async () => {
  const duplicateRow = {
    id: 44,
    organization_id: 1,
    patient_id: 2,
    device_id: 5,
    device_assignment_history_id: 3,
    event_uuid: "evt-repeat-001",
    event_type: "fall_detected",
    severity: "critical",
    intensity: 3.7,
    immobility: 1,
    message: "Queda com imobilidade confirmada.",
    evidence_status: "linked",
    evidence_telemetry_id: 22,
    evidence_sample_count: 2,
    evidence_window_seconds: 4,
    evidence_summary_json: JSON.stringify({ firmwareDecision: { eventUuid: "evt-repeat-001" } }),
    event_time: new Date("2026-05-13T14:38:10.000Z"),
    raw_payload_json: JSON.stringify({
      event_type: "fall_detected",
      event_uuid: "evt-repeat-001",
      sample_seq: 99,
      timestamp: Math.floor(new Date("2026-05-13T14:38:10.000Z").getTime() / 1000),
      immobility_confirmed: true,
      accel_magnitude: 3.7,
    }),
    created_at: new Date("2026-05-13T14:38:15.000Z"),
    deviceId: 5,
    deviceUid: "legacy:esp32_01",
    deviceIdentifier: "esp32_01",
    deviceName: "Pulseira ESP32",
    patientName: "Paciente Demo",
    alertId: 90,
    alertStatus: "open",
  };
  const harness = buildHarness([], { duplicateRow });

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "fall_detected",
        event_uuid: "evt-repeat-001",
        timestamp: Math.floor(new Date("2026-05-13T14:38:10.000Z").getTime() / 1000),
        immobility_confirmed: true,
        accel_magnitude: 3.7,
        sample_seq: 99,
      },
      correlationId: "trace_duplicate",
    });

    assert.equal(event.id, 44);
    assert.equal(event.eventUuid, "evt-repeat-001");
    assert.equal(event.deduplicated, true);
    assert.equal(event.duplicateReason, "event_uuid");
    assert.equal(harness.calls.duplicateQueries.length, 1);
    assert.deepEqual(harness.calls.duplicateQueries[0].params, ["evt-repeat-001"]);
    assert.doesNotMatch(harness.calls.duplicateQueries[0].sql, /JSON_EXTRACT/);
    assert.equal(harness.calls.eventInserts.length, 0);
    assert.equal(harness.calls.evidenceInserts.length, 0);
    assert.equal(harness.calls.telemetryQueries.length, 0);
    assert.ok(
      harness.calls.logs.some(
        (entry) => entry.message === "Evento MQTT duplicado ignorado por event_uuid estruturado.",
      ),
    );
  } finally {
    harness.restore();
  }
});

test("evento legado sem UUID permanece aceito com identidade estruturada nula", async () => {
  const harness = buildHarness([]);

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "sos_pressed",
        timestamp: Math.floor(new Date("2026-05-13T14:38:10.000Z").getTime() / 1000),
      },
      receivedAt: new Date("2026-05-13T14:38:12.123Z"),
    });

    assert.equal(event.eventUuid, null);
    assert.equal(event.clockQuality, "unknown");
    assert.equal(harness.calls.eventInserts.length, 1);
    assert.equal(eventInsertValue(harness.calls.eventInserts[0], "event_uuid"), null);
  } finally {
    harness.restore();
  }
});

test("event_uuid presente mas invalido e rejeitado em vez de virar legado", async () => {
  const harness = buildHarness([]);

  try {
    await assert.rejects(
      harness.eventService.recordEventFromMqtt({
        device: buildDevice(),
        payload: { event_type: "sos_pressed", event_uuid: "   " },
      }),
      (error) => error.code === "INVALID_EVENT_UUID" && error.details.reason === "empty",
    );
    assert.equal(harness.calls.eventInserts.length, 0);
    assert.ok(harness.calls.logs.some((entry) =>
      entry.message === "Evento MQTT rejeitado por event_uuid invalido."));
  } finally {
    harness.restore();
  }
});

test("mesmo UUID com dados criticos divergentes gera conflito auditavel", async () => {
  const duplicateRow = {
    id: 50,
    organization_id: 1,
    patient_id: 2,
    device_id: 5,
    event_uuid: "evt-conflict-001",
    event_type: "sos_pressed",
    severity: "high",
    intensity: null,
    immobility: 0,
    evidence_status: "none",
    evidence_sample_count: 0,
    raw_payload_json: JSON.stringify({
      event_uuid: "evt-conflict-001",
      event_type: "sos_pressed",
      event_sequence: 8,
    }),
    created_at: new Date("2026-05-13T14:38:15.000Z"),
    deviceId: 5,
    deviceUid: "legacy:esp32_01",
    deviceIdentifier: "esp32_01",
  };
  const harness = buildHarness([], { duplicateRow });

  try {
    await assert.rejects(
      harness.eventService.recordEventFromMqtt({
        device: buildDevice(),
        payload: {
          event_uuid: "evt-conflict-001",
          event_type: "fall_detected",
          event_sequence: 8,
        },
      }),
      (error) => error.code === "EVENT_UUID_CONFLICT"
        && error.details.conflictingFields.includes("eventType"),
    );
    assert.equal(harness.calls.eventInserts.length, 0);
    assert.ok(harness.calls.logs.some((entry) =>
      entry.metadata?.reason === "event_uuid_conflict"));
  } finally {
    harness.restore();
  }
});

test("duplicate-key concorrente consulta constraint e retorna retry idempotente", async () => {
  const payload = {
    event_uuid: "evt-race-001",
    event_type: "sos_pressed",
    event_sequence: 9,
    timestamp: 1_777_000_000,
  };
  const duplicateRow = {
    id: 51,
    organization_id: 1,
    patient_id: 2,
    device_id: 5,
    event_uuid: payload.event_uuid,
    event_type: payload.event_type,
    severity: "high",
    intensity: null,
    immobility: 0,
    evidence_status: "none",
    evidence_sample_count: 0,
    raw_payload_json: JSON.stringify(payload),
    created_at: new Date("2026-05-13T14:38:15.000Z"),
    deviceId: 5,
    deviceUid: "legacy:esp32_01",
    deviceIdentifier: "esp32_01",
  };
  const insertError = Object.assign(new Error("Duplicate entry"), { code: "ER_DUP_ENTRY" });
  const harness = buildHarness([], {
    duplicateRows: [null, duplicateRow],
    insertError,
  });

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload,
    });

    assert.equal(event.id, 51);
    assert.equal(event.deduplicated, true);
    assert.equal(harness.calls.eventInserts.length, 1);
    assert.equal(harness.calls.duplicateQueries.length, 2);
    assert.match(harness.calls.duplicateQueries[1].sql, /FOR UPDATE/);
    assert.ok(harness.calls.logs.some((entry) =>
      entry.metadata?.duplicateSource === "unique_constraint"));
  } finally {
    harness.restore();
  }
});

test("tempos de ocorrencia, recebimento e persistencia permanecem distintos", async () => {
  const harness = buildHarness([]);
  const occurredAt = "2026-05-13T14:38:10.000Z";
  const receivedAt = new Date("2026-05-13T14:40:00.123Z");

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "sos_pressed",
        event_uuid: "evt-time-001",
        occurred_at_device: occurredAt,
        boot_id: "boot-001",
        device_uptime_ms: 4567,
        clock_quality: "synced",
      },
      receivedAt,
    });
    const insert = harness.calls.eventInserts[0];

    assert.equal(event.occurredAtDevice, occurredAt);
    assert.equal(event.receivedAt, receivedAt.toISOString());
    assert.equal(event.persistedAt, "2026-05-13T14:38:15.000Z");
    assert.equal(event.bootId, "boot-001");
    assert.equal(event.deviceUptimeMs, 4567);
    assert.equal(event.clockQuality, "synced");
    assert.equal(eventInsertValue(insert, "received_at"), receivedAt);
    assert.equal(eventInsertValue(insert, "occurred_at_device").toISOString(), occurredAt);
  } finally {
    harness.restore();
  }
});

test("fall_detected nao usa telemetria stale ou de outro device", async () => {
  const eventTime = new Date("2026-05-13T14:38:10.000Z");
  const harness = buildHarness([
    {
      id: 31,
      device_id: 5,
      organization_id: 1,
      patient_id: 2,
      device_assignment_history_id: 3,
      accel_magnitude: 4.1,
      gyro_magnitude: 190,
      created_at: new Date("2026-05-13T14:37:30.000Z"),
    },
    {
      id: 32,
      device_id: 9,
      organization_id: 1,
      patient_id: 2,
      device_assignment_history_id: 3,
      accel_magnitude: 4.1,
      gyro_magnitude: 190,
      created_at: new Date("2026-05-13T14:38:09.000Z"),
    },
  ]);

  try {
    const event = await harness.eventService.recordEventFromMqtt({
      device: buildDevice(),
      payload: {
        event_type: "fall_detected",
        timestamp: Math.floor(eventTime.getTime() / 1000),
      },
    });

    assert.equal(event.evidenceStatus, "none");
    assert.equal(harness.calls.telemetryQueries[0].params[0], 5);
    assert.equal(harness.calls.telemetryQueries[0].params[1], 1);
    assert.equal(harness.calls.telemetryQueries[0].params[2], 2);
    assert.equal(harness.calls.telemetryQueries[0].params[3], 3);
  } finally {
    harness.restore();
  }
});
