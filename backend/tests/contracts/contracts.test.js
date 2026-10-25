const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const SwaggerParser = require("@apidevtools/swagger-parser");
const Ajv2020 = require("ajv/dist/2020");
const addFormats = require("ajv-formats");

const { getSubscriptionTopics } = require("../../src/mqtt/topics");

const repositoryRoot = path.resolve(__dirname, "../../..");
const contractsRoot = path.join(repositoryRoot, "docs", "contracts");
const mqttRoot = path.join(contractsRoot, "mqtt");
const openApiPath = path.join(contractsRoot, "openapi.yaml");

const routeModules = [
  ["/api/auth", "../../src/routes/authRoutes"],
  ["/api/pairing", "../../src/routes/pairingRoutes"],
  ["/api/devices", "../../src/routes/deviceRoutes"],
  ["/api/events", "../../src/routes/eventRoutes"],
  ["/api/alerts", "../../src/routes/alertRoutes"],
  ["/api/dashboard", "../../src/routes/dashboardRoutes"],
  ["/api/organization", "../../src/routes/organizationRoutes"],
  ["/api/patients", "../../src/routes/patientRoutes"],
  ["/api/system", "../../src/routes/systemRoutes"],
];

function normalizePath(value) {
  return value.replace(/:([A-Za-z0-9_]+)/g, "{$1}").replace(/\/$/, "") || "/";
}

function implementedOperations() {
  const operations = new Set(["GET /health", "GET /api/me"]);

  for (const [mount, modulePath] of routeModules) {
    const router = require(modulePath);
    for (const layer of router.stack) {
      if (!layer.route) {
        continue;
      }

      const fullPath = normalizePath(`${mount}${layer.route.path === "/" ? "" : layer.route.path}`);
      for (const method of Object.keys(layer.route.methods)) {
        operations.add(`${method.toUpperCase()} ${fullPath}`);
      }
    }
  }

  return [...operations].sort();
}

function documentedOperations(api) {
  const operations = [];
  const httpMethods = new Set(["get", "post", "put", "patch", "delete", "head", "options"]);

  for (const [routePath, pathItem] of Object.entries(api.paths)) {
    for (const method of Object.keys(pathItem)) {
      if (httpMethods.has(method)) {
        operations.push(`${method.toUpperCase()} ${routePath}`);
      }
    }
  }

  return operations.sort();
}

function readJson(relativePath) {
  return JSON.parse(fs.readFileSync(path.join(mqttRoot, relativePath), "utf8"));
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function buildAjv() {
  const ajv = new Ajv2020({
    allErrors: true,
    strict: true,
    strictRequired: false,
    allowUnionTypes: true,
  });
  addFormats(ajv);
  return ajv;
}

test("OpenAPI e valido e cobre exatamente as 35 operacoes registradas", async () => {
  const api = await SwaggerParser.validate(openApiPath);
  const implemented = implementedOperations();
  const documented = documentedOperations(api);

  assert.equal(implemented.length, 35);
  assert.deepEqual(documented, implemented);
  assert.equal(new Set(documented).size, documented.length);
});

test("schemas MQTT current validam exemplos reais e rejeitam campos obrigatorios invalidos", () => {
  const ajv = buildAjv();
  const cases = [
    ["status.schema.json", "examples/status.current.json", "device_id"],
    ["telemetry.schema.json", "examples/telemetry.current.json", "ax"],
    ["event.schema.json", "examples/event.current.json", "event_uuid"],
  ];

  for (const [schemaFile, exampleFile, requiredField] of cases) {
    const validate = ajv.compile(readJson(schemaFile));
    const example = readJson(exampleFile);
    assert.equal(validate(example), true, JSON.stringify(validate.errors));

    const missing = clone(example);
    delete missing[requiredField];
    assert.equal(validate(missing), false, `${schemaFile} aceitou ausencia de ${requiredField}`);

    const wrongType = { ...example, [requiredField]: { invalid: true } };
    assert.equal(validate(wrongType), false, `${schemaFile} aceitou tipo incorreto em ${requiredField}`);

    const wrongVersion = { ...example, schema_version: 2 };
    assert.equal(validate(wrongVersion), false, `${schemaFile} aceitou schema_version desconhecida`);

    if (schemaFile === "event.schema.json") {
      assert.equal(
        validate(readJson("examples/event-sos.current.json")),
        true,
        JSON.stringify(validate.errors),
      );
    }
  }
});

test("envelope planned v1 e ACK implementado exigem versao e correlacao", () => {
  const ajv = buildAjv();
  const eventValidator = ajv.compile(readJson("critical-event-v1.schema.json"));
  const ackValidator = ajv.compile(readJson("critical-event-ack-v1.schema.json"));
  const event = readJson("examples/critical-event-v1.planned.json");
  const ack = readJson("examples/critical-event-ack-v1.planned.json");

  assert.equal(eventValidator(event), true, JSON.stringify(eventValidator.errors));
  assert.equal(ackValidator(ack), true, JSON.stringify(ackValidator.errors));

  assert.equal(eventValidator({ ...event, schema_version: 2 }), false);
  assert.equal(ackValidator({ ...ack, event_uuid: "" }), false);

  const ackWithoutEventUuid = clone(ack);
  delete ackWithoutEventUuid.event_uuid;
  assert.equal(ackValidator(ackWithoutEventUuid), false);
  assert.equal(ackValidator({ ...ack, status: "duplicate" }), true);
  assert.equal(ackValidator({ ...ack, status: "rejected" }), false);
  assert.equal(ackValidator({ ...ack, retain: true }), false);

});

test("topicos e payloads representativos respeitam o buffer MQTT atual", () => {
  assert.deepEqual(getSubscriptionTopics("queda/devices"), [
    "queda/devices/+/events",
    "queda/devices/+/status",
    "queda/devices/+/telemetry",
  ]);

  const packetBufferBytes = 4096;
  const mqttMaxHeaderBytes = 5;
  const topicLengthPrefixBytes = 2;
  const cases = [
    ["status", "examples/status.current.json", 1024],
    ["telemetry", "examples/telemetry.current.json", 1024],
    // 3584: budget revisado em 2026-10-21 apos boot_id/clock/local_evidence; o firmware
    // recusa payload critico > 3800 bytes e o pacote MQTT precisa caber em 4096 bytes.
    ["events", "examples/event.current.json", 3584],
  ];
  const measured = {};

  for (const [channel, exampleFile, payloadBudgetBytes] of cases) {
    const payload = JSON.stringify(readJson(exampleFile));
    const topic = `queda/devices/esp32_01/${channel}`;
    const payloadBytes = Buffer.byteLength(payload, "utf8");
    const packetBytes = mqttMaxHeaderBytes + topicLengthPrefixBytes +
      Buffer.byteLength(topic, "utf8") + payloadBytes;

    measured[channel] = { payloadBytes, packetBytes, payloadBudgetBytes };
    assert.ok(payloadBytes <= payloadBudgetBytes, `${channel} excede budget: ${payloadBytes}`);
    assert.ok(packetBytes <= packetBufferBytes, `${channel} excede buffer: ${packetBytes}`);
  }

  console.log(`contract payload sizes: ${JSON.stringify(measured)}`);
});

test("ACK gerado pelo backend valida contra critical-event-ack-v1", () => {
  const { buildCriticalEventAck } = require("../../src/services/mqttIngestionService");
  const validate = buildAjv().compile(readJson("critical-event-ack-v1.schema.json"));

  const committed = buildCriticalEventAck({
    id: 42,
    eventUuid: "esp32-aabb:b00000002-1234abcd:00000001",
    persistedAt: "2026-10-21T10:00:00.123Z",
  });
  const duplicate = buildCriticalEventAck({
    id: "42",
    eventUuid: "esp32-aabb:b00000002-1234abcd:00000001",
    deduplicated: true,
    persistedAt: "2026-10-21T10:00:00.123Z",
  });

  assert.equal(validate(committed), true, JSON.stringify(validate.errors));
  assert.equal(validate(duplicate), true, JSON.stringify(validate.errors));
  assert.equal(committed.status, "committed");
  assert.equal(duplicate.status, "duplicate");
  assert.equal(duplicate.event_id, 42);
});

test("critical-event-result-v1: exemplos validam e combinacoes inseguras sao recusadas", () => {
  const validate = buildAjv().compile(readJson("critical-event-result-v1.schema.json"));
  const committed = readJson("examples/critical-event-result-v1.committed.json");
  const rejected = readJson("examples/critical-event-result-v1.rejected.json");

  assert.equal(validate(committed), true, JSON.stringify(validate.errors));
  assert.equal(validate(rejected), true, JSON.stringify(validate.errors));
  assert.equal(validate({ ...committed, status: "duplicate" }), true);

  // Rejeicao nunca pode carregar identidade de persistencia.
  assert.equal(validate({ ...rejected, event_id: 5 }), false);
  assert.equal(validate({ ...rejected, committed_at: committed.committed_at }), false);
  assert.equal(validate({ ...rejected, reason_code: "storage_unavailable" }), false);
  assert.equal(validate({ ...rejected, reason_code: null }), false);
  // Persistido exige event_id e nao aceita motivo.
  assert.equal(validate({ ...committed, event_id: null }), false);
  assert.equal(validate({ ...committed, reason_code: "internal_error" }), false);
  // retryable nao confirma persistencia.
  const retryable = { ...rejected, status: "retryable", reason_code: "storage_unavailable", retry_after_ms: 5000 };
  assert.equal(validate(retryable), true, JSON.stringify(validate.errors));
  assert.equal(validate({ ...retryable, event_id: 1 }), false);
  assert.equal(validate({ ...retryable, reason_code: "event_uuid_conflict" }), false);
  // Correlacao obrigatoria.
  assert.equal(validate({ ...committed, payload_sha256: "ABC" }), false);
  assert.equal(validate({ ...committed, payload_sha256: "A".repeat(64) }), false);
  const withoutDevice = clone(committed);
  delete withoutDevice.device_id;
  assert.equal(validate(withoutDevice), false);
  assert.equal(validate({ ...committed, status: "accepted" }), false);
  assert.equal(validate({ ...committed, schema_version: 2 }), false);
});

test("resultados gerados pelo backend validam contra critical-event-result-v1", () => {
  const {
    buildCriticalEventResult,
    buildPersistedResult,
    classifyIngestError,
  } = require("../../src/mqtt/criticalEventResult");
  const validate = buildAjv().compile(readJson("critical-event-result-v1.schema.json"));
  const base = {
    deviceIdentifier: "esp32_01",
    eventUuid: "esp32-aabb:b00000002-1234abcd:00000001",
    payloadSha256: "0".repeat(64),
  };
  const persisted = buildPersistedResult({
    deviceIdentifier: base.deviceIdentifier,
    payloadSha256: base.payloadSha256,
    event: { id: "9", eventUuid: base.eventUuid, persistedAt: "2026-10-25T00:00:00.000Z" },
  });
  assert.equal(validate(persisted), true, JSON.stringify(validate.errors));
  assert.equal(persisted.event_id, 9);

  for (const code of ["EVENT_UUID_CONFLICT", "INVALID_EVENT_UUID", "DEVICE_IDENTITY_MISMATCH", "ECONNREFUSED", "ECONNRESET", undefined]) {
    const classification = classifyIngestError(code ? Object.assign(new Error(code), { code }) : new Error("x"));
    const result = buildCriticalEventResult({
      ...base,
      status: classification.status,
      reasonCode: classification.reasonCode,
      retryAfterMs: classification.retryAfterMs ?? null,
    });
    assert.equal(validate(result), true, `${code}: ${JSON.stringify(validate.errors)}`);
  }
});

test("MotionLab: frame e anotacao validam; predicao de modelo nao e ground truth", () => {
  const motionlabRoot = path.join(contractsRoot, "motionlab");
  const read = (name) => JSON.parse(fs.readFileSync(path.join(motionlabRoot, name), "utf8"));
  const ajv = buildAjv();
  const frame = ajv.compile(read("lab-frame-v1.schema.json"));
  const annotation = ajv.compile(read("annotation-v1.schema.json"));
  const example = read("examples/lab-frame-v1.synthetic.json");

  assert.equal(frame(example), true, JSON.stringify(frame.errors));
  assert.equal(frame({ ...example, ax_g: "1" }), false);
  assert.equal(frame({ ...example, sample_seq: -1 }), false);
  assert.equal(frame({ ...example, event_uuid: "x" }), false);

  const label = {
    schema_version: 1, label: "unknown", source: "observer", time_start_us: 0,
    time_end_us: 10, uncertainty_us: 5, review_status: "draft",
  };
  assert.equal(annotation(label), true, JSON.stringify(annotation.errors));
  assert.equal(annotation({ ...label, source: "model" }), false);
  assert.equal(annotation({ ...label, label: "fall" }), false);
  const withoutUncertainty = clone(label);
  delete withoutUncertainty.uncertainty_us;
  assert.equal(annotation(withoutUncertainty), false);
});
