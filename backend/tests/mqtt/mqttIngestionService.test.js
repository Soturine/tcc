const assert = require("node:assert/strict");
const test = require("node:test");

const { loadWithMocks } = require("../helpers/moduleSandbox");

function topicInfo(channel = "telemetry", deviceIdentifier = "esp32_01") {
  return {
    topic: `queda/devices/${deviceIdentifier}/${channel}`,
    deviceIdentifier,
    channel,
  };
}

function buildHarness(options = {}) {
  const calls = {
    acks: [],
    alerts: [],
    events: [],
    emits: [],
    identities: [],
    logs: [],
    status: [],
    telemetry: [],
    transactions: 0,
  };
  let activeIdentityCalls = 0;
  let maxActiveIdentityCalls = 0;
  let eventId = 100;
  let telemetryId = 200;
  const organization = options.organization === null
    ? null
    : { id: options.organizationId || 1, name: "Familia Demo" };
  const currentPatient = options.patient === null
    ? null
    : { id: options.patientId || 2, fullName: "Paciente Demo" };
  const device = {
    id: options.deviceId || 5,
    deviceUid: options.deviceUid || "legacy:esp32_01",
    deviceIdentifier: options.deviceIdentifier || "esp32_01",
    name: "Pulseira ESP32",
    organization,
    currentPatient,
    currentAssignmentHistoryId: currentPatient ? 3 : null,
  };
  const logger = {
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
  };
  const fakePool = {
    transaction: async (work) => {
      calls.transactions += 1;
      const result = await work({ connection: true });
      if (options.failCommit) {
        // Simula falha no COMMIT: o trabalho executou, mas nada foi confirmado.
        throw Object.assign(new Error("commit failed"), { code: "PROTOCOL_CONNECTION_LOST" });
      }
      return result;
    },
  };
  const fakeDeviceService = {
    getDeviceBehaviorSnapshot: async () => ({
      state: "pre_calibracao",
      confidence: "baixo",
      reason: "Teste",
      experimental: true,
      version: "test",
      source: "test",
      updatedAt: new Date().toISOString(),
      telemetrySampleCount: 1,
      telemetryWindowSeconds: 0,
      plannedFutureStates: [],
    }),
    getOrCreateDeviceByIdentity: async (identity) => {
      calls.identities.push(identity);
      activeIdentityCalls += 1;
      maxActiveIdentityCalls = Math.max(maxActiveIdentityCalls, activeIdentityCalls);
      if (options.identityDelayMs) {
        await new Promise((resolve) => setTimeout(resolve, options.identityDelayMs));
      }
      activeIdentityCalls -= 1;
      return device;
    },
    upsertDeviceStatus: async (deviceId, fields, scope, _executor, updateOptions = {}) => {
      calls.status.push({ deviceId, fields, scope, options: updateOptions });
      const statusPatch = {
        online: fields.online ?? true,
        wifiRssi: fields.wifiRssi ?? null,
        batteryPercent: fields.batteryPercent ?? null,
        batteryPercentSource: fields.batteryPercentSource ?? null,
        batteryCalibrationCount: fields.batteryCalibrationCount ?? 0,
        firmwareVersion: fields.firmwareVersion ?? null,
        lastSeenAt: fields.lastSeenAt instanceof Date
          ? fields.lastSeenAt.toISOString()
          : fields.lastSeenAt || null,
        updatedAt: new Date().toISOString(),
      };

      if (updateOptions.returnSnapshot === false) {
        return {
          status: statusPatch,
          scope,
        };
      }

      return {
        ...device,
        status: statusPatch,
      };
    },
  };
  const fakeEventService = {
    validateTelemetryPayload: (payload) => {
      const requiredFields = ["ax", "ay", "az", "gx", "gy", "gz"];
      const missingFields = requiredFields.filter((field) => {
        const parsed = Number(payload[field]);
        return !Number.isFinite(parsed);
      });
      const sensorValid = payload.sensor_valid === undefined
        ? true
        : payload.sensor_valid === true || payload.sensor_valid === 1 || payload.sensor_valid === "true";

      return {
        valid: sensorValid && missingFields.length === 0,
        missingFields,
        sensorValid,
        reason: !sensorValid
          ? "sensor_invalid"
          : missingFields.length
            ? "missing_sensor_axes"
            : null,
      };
    },
    recordEventFromMqtt: async ({ payload, eventTime, receivedAt }) => {
      calls.events.push({ payload, eventTime, receivedAt });
      if (options.recordEventError) {
        throw options.recordEventError;
      }
      const eventType = payload.event_type || "device_event";
      if (options.deduplicateEventUuid && payload.event_uuid) {
        return {
          id: 707,
          organizationId: organization?.id || null,
          patientId: currentPatient?.id || null,
          eventType,
          eventUuid: payload.event_uuid,
          sampleSeq: payload.sample_seq ?? null,
          severity: payload.immobility_confirmed ? "critical" : "high",
          evidenceStatus: eventType === "fall_detected" ? "linked" : "none",
          evidenceTelemetryId: eventType === "fall_detected" ? 200 : null,
          evidenceSampleCount: eventType === "fall_detected" ? 2 : 0,
          evidenceWindowSeconds: eventType === "fall_detected" ? 3 : 0,
          evidenceSummary: null,
          deduplicated: true,
          duplicateReason: "event_uuid",
          persistedAt: "2026-10-21T00:00:00.000Z",
          device: {
            id: device.id,
            deviceUid: device.deviceUid,
            deviceIdentifier: device.deviceIdentifier,
            name: device.name,
          },
        };
      }
      const evidenceStatus = eventType === "fall_detected"
        ? options.fallEvidenceStatus || "linked"
        : "none";
      return {
        id: eventId += 1,
        organizationId: organization?.id || null,
        patientId: currentPatient?.id || null,
        eventType,
        eventUuid: payload.event_uuid || null,
        persistedAt: "2026-10-21T00:00:01.000Z",
        severity: payload.immobility_confirmed ? "critical" : "high",
        evidenceStatus,
        evidenceTelemetryId: evidenceStatus === "none" ? null : 200,
        evidenceSampleCount: evidenceStatus === "linked" ? 2 : evidenceStatus === "partial" ? 1 : 0,
        evidenceWindowSeconds: evidenceStatus === "none" ? 0 : 3,
        evidenceSummary: null,
        device: {
          id: device.id,
          deviceUid: device.deviceUid,
          deviceIdentifier: device.deviceIdentifier,
          name: device.name,
        },
      };
    },
    recordTelemetryFromMqtt: async ({ payload, createdAt, receivedAt }) => {
      calls.telemetry.push({ payload, createdAt, receivedAt });
      return {
        id: telemetryId += 1,
        deviceId: device.id,
        organizationId: organization?.id || null,
        patientId: currentPatient?.id || null,
        ax: Number(payload.ax || 0),
        ay: Number(payload.ay || 0),
        az: Number(payload.az || 1),
        gx: Number(payload.gx || 0),
        gy: Number(payload.gy || 0),
        gz: Number(payload.gz || 0),
        accelMagnitude: Number(payload.accel_magnitude || 1),
        gyroMagnitude: Number(payload.gyro_magnitude || 0),
        pitchDeg: Number(payload.pitch_deg || 0),
        rollDeg: Number(payload.roll_deg || 0),
        createdAt: new Date().toISOString(),
      };
    },
    shouldCreateAlert: (eventType) =>
      [
        "fall_detected",
        "fall_suspected",
        "sos_pressed",
        "manual_sos",
        "sensor_fault",
      ].includes(eventType),
    shouldCreateAlertForEvent: (event) =>
      [
        "fall_suspected",
        "sos_pressed",
        "manual_sos",
        "sensor_fault",
      ].includes(event.eventType) ||
      (event.eventType === "fall_detected" && ["linked", "partial"].includes(event.evidenceStatus)),
  };
  const fakeAlertService = {
    createAlertForEvent: async (event) => {
      calls.alerts.push(event);
      return {
        id: 900 + calls.alerts.length,
        organizationId: event.organizationId,
        patientId: event.patientId,
        status: "open",
        device: event.device,
        event,
      };
    },
  };
  const fakeEmitter = {
    emitScopedEvent: (_io, eventName, payload, scope, diagnostics) => {
      calls.emits.push({ eventName, payload, scope, diagnostics });
    },
  };
  const { module, restore } = loadWithMocks("src/services/mqttIngestionService.js", {
    "src/db/pool.js": fakePool,
    "src/services/deviceService.js": fakeDeviceService,
    "src/services/eventService.js": fakeEventService,
    "src/services/alertService.js": fakeAlertService,
    "src/socket/scopedEmitter.js": fakeEmitter,
    "src/utils/logger.js": { logger },
  });

  return {
    calls,
    handleMqttMessage: (args) => module.handleMqttMessage({
      publishAck: options.noAckPublisher
        ? null
        : (topic, ack, callback) => {
          calls.acks.push({ topic, ack, transactionsAtPublish: calls.transactions });
          if (callback) {
            callback(options.ackPublishError ? new Error("broker offline") : undefined);
          }
        },
      ...args,
    }),
    maxActiveIdentityCalls: () => maxActiveIdentityCalls,
    restore,
  };
}

async function withHarness(options, work) {
  const harness = buildHarness(options);
  try {
    await work(harness);
  } finally {
    harness.restore();
  }
}

test("JSON invalido nao derruba o processo e gera descarte claro", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: "{",
      io: {},
    });

    assert.equal(calls.transactions, 0);
    assert.equal(calls.logs[0].metadata.reason, "invalid_json");
  });
});

test("payload sem device_id/topico sem device e canal invalido sao rejeitados", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("status", ""),
      payloadText: JSON.stringify({ online: true }),
      io: {},
    });
    await handleMqttMessage({
      topicInfo: topicInfo("bad"),
      payloadText: JSON.stringify({ device_id: "esp32_01" }),
      io: {},
    });

    assert.equal(calls.transactions, 0);
    assert.deepEqual(
      calls.logs.filter((entry) => entry.level === "warn").map((entry) => entry.metadata.reason),
      ["missing_device_id", "unsupported_channel"],
    );
  });
});

test("device_id divergente entre topico e payload e rejeitado antes da persistencia", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events", "esp32_authorized"),
      payloadText: JSON.stringify({
        device_id: "esp32_spoofed",
        event_type: "sos_pressed",
        timestamp: Math.floor(Date.now() / 1000),
      }),
      io: {},
    });

    assert.equal(calls.transactions, 0);
    assert.equal(calls.events.length, 0);
    assert.equal(calls.alerts.length, 0);
    assert.equal(calls.emits.length, 0);
    assert.ok(
      calls.logs.some(
        (entry) => entry.level === "warn" &&
          entry.metadata?.reason === "topic_payload_device_mismatch",
      ),
    );
  });
});

test("status atualiza device_status e emite device:status no escopo correto", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("status"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        device_uid: "legacy:esp32_01",
        timestamp: Math.floor(Date.now() / 1000),
        wifi_rssi: -58,
        battery_percent: 78,
        battery_percent_source: "manual",
      }),
      io: {},
    });

    assert.equal(calls.status.length, 1);
    assert.equal(calls.status[0].fields.batteryPercent, 78);
    assert.equal(calls.status[0].fields.batteryPercentSource, "manual");
    assert.equal(calls.emits.length, 1);
    assert.equal(calls.emits[0].eventName, "device:status");
    assert.equal(calls.emits[0].payload.status.batteryPercent, 78);
    assert.equal(calls.emits[0].payload.status.batteryPercentSource, "manual");
    assert.deepEqual(calls.emits[0].scope, { organizationId: 1, patientId: 2 });
    assert.ok(calls.emits[0].diagnostics.correlationId);
  });
});

test("status sem bateria configurada limpa placeholder antigo", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("status"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        timestamp: Math.floor(Date.now() / 1000),
        battery_percent_source: "not_configured",
      }),
      io: {},
    });

    assert.equal(calls.status.length, 1);
    assert.equal(calls.status[0].fields.batteryPercent, null);
    assert.equal(calls.status[0].fields.batteryPercentSource, "not_configured");
    assert.equal(calls.emits[0].payload.status.batteryPercent, null);
    assert.equal(calls.emits[0].payload.status.batteryPercentSource, "not_configured");
  });
});

test("telemetry grava amostra, atualiza status e emite telemetry:new", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        timestamp: Math.floor(Date.now() / 1000),
        ax: 0.04,
        ay: -0.02,
        az: 0.98,
        gx: 5.2,
        gy: -1.1,
        gz: 3.6,
        accel_magnitude: 0.98,
        gyro_magnitude: 6.4,
      }),
      io: {},
    });

    assert.equal(calls.telemetry.length, 1);
    assert.equal(calls.status.length, 1);
    assert.equal(calls.status[0].fields.batteryCalibrationCount, 0);
    assert.equal(calls.emits[0].eventName, "telemetry:new");
    assert.equal(calls.emits[0].payload.deviceId, 5);
    assert.equal(calls.emits[0].payload.organizationId, 1);
    assert.equal(calls.emits[0].payload.deviceStatusPatch.batteryCalibrationCount, 0);
  });
});

test("quedas e SOS geram alerta; movement_detected permanece evento informativo", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    for (const eventType of [
      "fall_detected",
      "fall_suspected",
      "movement_detected",
      "sos_pressed",
    ]) {
      await handleMqttMessage({
        topicInfo: topicInfo("events"),
        payloadText: JSON.stringify({
          device_id: "esp32_01",
          event_type: eventType,
          immobility_confirmed: eventType === "fall_detected",
          timestamp: Math.floor(Date.now() / 1000),
        }),
        io: {},
      });
    }

    assert.equal(calls.events.length, 4);
    assert.equal(calls.alerts.length, 3);
    assert.deepEqual(
      calls.emits.map((entry) => entry.eventName),
      ["alert:new", "alert:new", "alert:new"],
    );
  });
});

test("evento critico duplicado por event_uuid nao cria alerta nem realtime duplicado", async () => {
  await withHarness({ deduplicateEventUuid: true }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        event_type: "fall_detected",
        event_uuid: "evt-repeat-001",
        sample_seq: 77,
        immobility_confirmed: true,
        timestamp: Math.floor(Date.now() / 1000),
      }),
      io: {},
    });

    assert.equal(calls.events.length, 1);
    assert.equal(calls.alerts.length, 0);
    assert.equal(calls.emits.length, 0);
    assert.ok(
      calls.logs.some(
        (entry) => entry.message === "MQTT event duplicado ignorado sem criar alerta." &&
          entry.metadata?.eventUuid === "evt-repeat-001",
      ),
    );
  });
});

test("fall_detected sem evidencia suficiente nao cria alerta automatico", async () => {
  await withHarness({ fallEvidenceStatus: "none" }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        event_type: "fall_detected",
        immobility_confirmed: true,
        timestamp: Math.floor(Date.now() / 1000),
      }),
      io: {},
    });

    assert.equal(calls.events.length, 1);
    assert.equal(calls.alerts.length, 0);
    assert.equal(calls.emits.length, 0);
    assert.ok(
      calls.logs.some(
        (entry) => entry.metadata?.eventType === "fall_detected" &&
          entry.metadata?.evidenceStatus === "none",
      ),
    );
  });
});

test("timestamp absurdo usa fallback do backend e loga diagnostico", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        timestamp: 1,
        ax: 0,
        ay: 0,
        az: 1,
        gx: 0,
        gy: 0,
        gz: 0,
      }),
      io: {},
    });

    const lastSeenAt = calls.status[0].fields.lastSeenAt;
    assert.ok(lastSeenAt instanceof Date);
    assert.ok(Date.now() - lastSeenAt.getTime() < 10_000);
    assert.ok(
      calls.logs.some(
        (entry) => entry.metadata?.reason === "implausible_device_timestamp",
      ),
    );
  });
});

test("timestamp plausivel mas stale nao derruba status realtime para offline", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        timestamp: Math.floor(new Date("2026-01-01T00:00:00.000Z").getTime() / 1000),
        ax: 0,
        ay: 0,
        az: 1,
        gx: 0,
        gy: 0,
        gz: 0,
      }),
      io: {},
    });

    const lastSeenAt = calls.status[0].fields.lastSeenAt;
    const createdAt = calls.telemetry[0].createdAt;

    assert.ok(lastSeenAt instanceof Date);
    assert.ok(Date.now() - lastSeenAt.getTime() < 10_000);
    assert.ok(createdAt instanceof Date);
    assert.ok(Date.now() - createdAt.getTime() < 10_000);
    assert.ok(
      calls.logs.some(
        (entry) => entry.metadata?.reason === "device_clock_skew_exceeded",
      ),
    );
  });
});

test("device sem organizacao nao vaza realtime tenant indevido", async () => {
  await withHarness({ organization: null, patient: null }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry", "stress_orphan"),
      payloadText: JSON.stringify({
        device_id: "stress_orphan",
        timestamp: Math.floor(Date.now() / 1000),
        ax: 0,
        ay: 0,
        az: 1,
        gx: 0,
        gy: 0,
        gz: 0,
      }),
      io: {},
    });

    assert.deepEqual(calls.emits[0].scope, { organizationId: null, patientId: null });
    assert.ok(
      calls.logs.some(
        (entry) => entry.metadata?.reason === "device_without_organization_scope",
      ),
    );
  });
});

test("telemetry sem amostra valida atualiza status mas nao grava telemetry_logs", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: JSON.stringify({
        device_id: "esp32_01",
        device_uid: "legacy:esp32_01",
        timestamp: Math.floor(Date.now() / 1000),
        sensor_ready: true,
        sensor_valid: false,
        sensor_read_ok: false,
        sensor_sample_age_ms: 0,
        i2c_last_error: "raw_read_failed",
      }),
      io: {},
    });

    assert.equal(calls.telemetry.length, 0);
    assert.equal(calls.status.length, 1);
    assert.equal(calls.emits.length, 1);
    assert.equal(calls.emits[0].eventName, "device:status");
    assert.ok(
      calls.logs.some(
        (entry) => entry.metadata?.validation?.reason === "sensor_invalid",
      ),
    );
  });
});

test("mensagens concorrentes do mesmo device passam pelo lock por chave", async () => {
  await withHarness({ identityDelayMs: 20 }, async (harness) => {
    await Promise.all(
      Array.from({ length: 5 }, () =>
        harness.handleMqttMessage({
          topicInfo: topicInfo("status"),
          payloadText: JSON.stringify({
            device_id: "esp32_01",
            timestamp: Math.floor(Date.now() / 1000),
          }),
          io: {},
        }),
      ),
    );

    assert.equal(harness.calls.transactions, 5);
    assert.equal(harness.maxActiveIdentityCalls(), 1);
  });
});

function criticalEventPayload(overrides = {}) {
  return JSON.stringify({
    device_id: "esp32_01",
    event_type: "fall_detected",
    event_uuid: "esp32-aabb:b00000002-1234abcd:00000001",
    immobility_confirmed: true,
    timestamp: Math.floor(Date.now() / 1000),
    ...overrides,
  });
}

test("ACK de aplicacao e publicado somente apos commit, no topico do device autenticado", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} });

    assert.equal(calls.acks.length, 1);
    assert.equal(calls.acks[0].topic, "queda/devices/esp32_01/critical-event-acks");
    assert.equal(calls.acks[0].transactionsAtPublish, 1);
    assert.deepEqual(calls.acks[0].ack, {
      schema_version: 1,
      event_uuid: "esp32-aabb:b00000002-1234abcd:00000001",
      event_id: 101,
      status: "committed",
      committed_at: "2026-10-21T00:00:01.000Z",
    });
  });
});

test("retry com o mesmo UUID recebe ACK duplicate sem novo alerta", async () => {
  await withHarness({ deduplicateEventUuid: true }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} });
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} });

    assert.equal(calls.alerts.length, 0);
    assert.equal(calls.acks.length, 2);
    assert.ok(calls.acks.every((entry) => entry.ack.status === "duplicate" && entry.ack.event_id === 707));
  });
});

test("falha no commit nao publica ACK e propaga erro para o device retransmitir", async () => {
  await withHarness({ failCommit: true }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(
      handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} }),
      /commit failed/,
    );
    assert.equal(calls.acks.length, 0);
    assert.equal(calls.emits.length, 0);
  });
});

test("evento rejeitado por identidade divergente nao recebe ACK", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events", "esp32_01"),
      payloadText: criticalEventPayload({ device_id: "esp32_99" }),
      io: {},
    });
    assert.equal(calls.events.length, 0);
    assert.equal(calls.acks.length, 0);
  });
});

test("evento legado sem event_uuid e persistido sem ACK de aplicacao", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: criticalEventPayload({ event_uuid: undefined }),
      io: {},
    });
    assert.equal(calls.events.length, 1);
    assert.equal(calls.acks.length, 0);
  });
});

test("falha ao publicar ACK e registrada sem desfazer o evento committed", async () => {
  await withHarness({ ackPublishError: true }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} });
    assert.equal(calls.events.length, 1);
    assert.equal(calls.alerts.length, 1);
    assert.ok(calls.logs.some((entry) => entry.metadata?.reason === "ack_publish_failed"));
  });
});

test("evento entregue apos periodo offline usa occurred_at_device sincronizado como instante", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    const occurred = new Date(Date.now() - 45 * 60 * 1000);
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: criticalEventPayload({
        clock_quality: "synced",
        occurred_at_device: occurred.toISOString(),
        timestamp: Math.floor(occurred.getTime() / 1000),
      }),
      io: {},
    });

    assert.equal(calls.events[0].eventTime.getTime(), occurred.getTime());
    assert.ok(calls.events[0].receivedAt.getTime() > occurred.getTime());
  });
});

test("sem publisher de ACK o evento e persistido e a ausencia e registrada", async () => {
  await withHarness({ noAckPublisher: true }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} });
    assert.equal(calls.events.length, 1);
    assert.ok(calls.logs.some((entry) => entry.metadata?.reason === "ack_publisher_missing"));
  });
});

// ---------------------------------------------------------------- critical-event-result-v1 (F-29)

const crypto = require("node:crypto");

function sha256(text) {
  return crypto.createHash("sha256").update(Buffer.from(text, "utf8")).digest("hex");
}

function resultCapablePayload(overrides = {}) {
  return criticalEventPayload({ ingest_result_version: 1, ...overrides });
}

function codedError(code, message = code) {
  return Object.assign(new Error(message), { code });
}

test("F-29 reproducao: sem ingest_result_version uma rejeicao permanente continua sem resposta (legado)", async () => {
  await withHarness({ recordEventError: codedError("EVENT_UUID_CONFLICT") }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(
      handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: criticalEventPayload(), io: {} }),
      /EVENT_UUID_CONFLICT/,
    );
    // Contrato v1 preservado: dispositivos antigos nao recebem nada novo.
    assert.equal(calls.acks.length, 0);
  });
});

test("device com resultado v1 recebe committed pos-commit com hash dos bytes recebidos e sem ACK legado", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    const payloadText = resultCapablePayload();
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText, io: {} });

    assert.equal(calls.acks.length, 1);
    const [{ topic, ack, transactionsAtPublish }] = calls.acks;
    assert.equal(topic, "queda/devices/esp32_01/critical-event-results");
    assert.equal(transactionsAtPublish, 1);
    assert.equal(ack.schema_version, 1);
    assert.equal(ack.status, "committed");
    assert.equal(ack.device_id, "esp32_01");
    assert.equal(ack.event_uuid, "esp32-aabb:b00000002-1234abcd:00000001");
    assert.equal(ack.payload_sha256, sha256(payloadText));
    assert.equal(ack.event_id, 101);
    assert.equal(ack.reason_code, null);
    assert.equal(ack.retry_after_ms, null);
    assert.equal(ack.committed_at, "2026-10-21T00:00:01.000Z");
  });
});

test("hash informado pelo bridge (bytes brutos) prevalece sobre o recalculado", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: resultCapablePayload(),
      payloadSha256: "a".repeat(64),
      io: {},
    });
    assert.equal(calls.acks[0].ack.payload_sha256, "a".repeat(64));
  });
});

test("retransmissao de evento ja persistido recebe duplicate com o mesmo event_id", async () => {
  await withHarness({ deduplicateEventUuid: true }, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: resultCapablePayload(), io: {} });
    assert.equal(calls.acks[0].ack.status, "duplicate");
    assert.equal(calls.acks[0].ack.event_id, 707);
    assert.equal(calls.alerts.length, 0);
  });
});

for (const [code, reason] of [
  ["EVENT_UUID_CONFLICT", "event_uuid_conflict"],
  ["INVALID_EVENT_UUID", "invalid_event_uuid"],
  ["DEVICE_IDENTITY_MISMATCH", "identity_rejected"],
]) {
  test(`${code} gera rejected_permanent sem event_id e sem confirmacao de persistencia`, async () => {
    await withHarness({ recordEventError: codedError(code) }, async ({ calls, handleMqttMessage }) => {
      const payloadText = resultCapablePayload();
      await assert.rejects(handleMqttMessage({ topicInfo: topicInfo("events"), payloadText, io: {} }));
      assert.equal(calls.acks.length, 1);
      const { ack } = calls.acks[0];
      assert.equal(ack.status, "rejected_permanent");
      assert.equal(ack.reason_code, reason);
      assert.equal(ack.event_id, null);
      assert.equal(ack.committed_at, null);
      assert.equal(ack.payload_sha256, sha256(payloadText));
      assert.equal(calls.alerts.length, 0);
      assert.equal(calls.emits.length, 0);
    });
  });
}

test("MySQL recusando conexao gera retryable com retry_after_ms; nunca committed", async () => {
  await withHarness({ recordEventError: codedError("ECONNREFUSED") }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: resultCapablePayload(), io: {} }));
    assert.equal(calls.acks[0].ack.status, "retryable");
    assert.equal(calls.acks[0].ack.reason_code, "storage_unavailable");
    assert.equal(calls.acks[0].ack.retry_after_ms, 5000);
    assert.equal(calls.acks[0].ack.event_id, null);
  });
});

test("falha no COMMIT (incerta) gera retryable, nunca committed", async () => {
  await withHarness({ failCommit: true }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: resultCapablePayload(), io: {} }));
    assert.equal(calls.acks.length, 1);
    assert.equal(calls.acks[0].ack.status, "retryable");
  });
});

test("erro desconhecido e retryable (fail-safe: o device mantem o evento)", async () => {
  await withHarness({ recordEventError: new Error("boom") }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(handleMqttMessage({ topicInfo: topicInfo("events"), payloadText: resultCapablePayload(), io: {} }));
    assert.equal(calls.acks[0].ack.status, "retryable");
    assert.equal(calls.acks[0].ack.reason_code, "internal_error");
  });
});

test("identidade divergente topico/payload: rejected_permanent no topico autenticado, sem vazar o outro id", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events", "esp32_01"),
      payloadText: resultCapablePayload({ device_id: "esp32_99" }),
      io: {},
    });
    assert.equal(calls.events.length, 0);
    assert.equal(calls.acks.length, 1);
    assert.equal(calls.acks[0].topic, "queda/devices/esp32_01/critical-event-results");
    assert.equal(calls.acks[0].ack.status, "rejected_permanent");
    assert.equal(calls.acks[0].ack.reason_code, "identity_rejected");
    assert.equal(calls.acks[0].ack.device_id, "esp32_01");
    assert.ok(!JSON.stringify(calls.acks[0].ack).includes("esp32_99"));
  });
});

test("UUID nao correlacionavel (longo demais) nao recebe resultado", async () => {
  await withHarness({ recordEventError: codedError("INVALID_EVENT_UUID") }, async ({ calls, handleMqttMessage }) => {
    await assert.rejects(handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: resultCapablePayload({ event_uuid: "x".repeat(161) }),
      io: {},
    }));
    assert.equal(calls.acks.length, 0);
  });
});

test("versao de resultado desconhecida cai no ACK legado", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("events"),
      payloadText: criticalEventPayload({ ingest_result_version: 9 }),
      io: {},
    });
    assert.equal(calls.acks[0].topic, "queda/devices/esp32_01/critical-event-acks");
    assert.equal(calls.acks[0].ack.status, "committed");
  });
});

test("telemetria com ingest_result_version nao gera resultado critico", async () => {
  await withHarness({}, async ({ calls, handleMqttMessage }) => {
    await handleMqttMessage({
      topicInfo: topicInfo("telemetry"),
      payloadText: JSON.stringify({ device_id: "esp32_01", ingest_result_version: 1, ax: 0, ay: 0, az: 1, gx: 0, gy: 0, gz: 0 }),
      io: {},
    });
    assert.equal(calls.acks.length, 0);
  });
});
