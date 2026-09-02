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

test("schemas planned v1 exigem versao e correlacao do ACK", () => {
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
    ["events", "examples/event.current.json", 3072],
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
