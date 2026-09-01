# AGENTS.md

## Purpose

This repository is the official TCC evolution of `Soturine/iot-fall-monitor`. Agents must preserve engineering rigor and project traceability.

## Before changing code

1. Read `README.md`.
2. Read `ENGINEERING_CONSTITUTION.md`.
3. Read the architecture/research document relevant to the task.
4. Read applicable ADRs under `docs/adr/`.
5. Inspect existing code/tests before proposing a rewrite.
6. Never invent hardware capabilities, experimental results, measurements, references or implemented features.

## Repository intent

Target layout:

```text
apps/
  android/        # Kotlin + Jetpack Compose
  web/            # React/Vite, secondary console
backend/          # Node.js/Express modular monolith
firmware/
  esp32/          # PlatformIO/C++
contracts/
  openapi/
  mqtt/
database/
  migrations/
infra/
  compose/
  mqtt/
  cloud/
docs/
scripts/
tools/
  virtual-device/
```

Do not mass-move the inherited code just to match this tree. Migrate incrementally while preserving a green baseline.

## Core architecture invariants

- ESP32/wearable edge logic must not depend on the mobile app being open.
- Backend is the domain/persistence authority.
- Mobile/Web never connect directly to MySQL.
- MQTT is device/server transport, not the general mobile business API.
- HTTP/REST is the normal command/query API for app/web.
- Socket.IO is foreground realtime only.
- FCM is the Android background notification path.
- Wearable transport is behind an abstraction; do not assume BLE until hardware is selected.
- `event_uuid` or equivalent unique event identity must make critical-event processing idempotent.
- Tenant isolation and object-level authorization are mandatory.

## Engineering rules

- Prefer modular monolith; no microservices/Kafka/Kubernetes unless a measured need emerges.
- Prefer existing Node/Express/MySQL code over gratuitous rewrites.
- Prefer Kotlin/Jetpack Compose for the Android app.
- Extract pure logic from framework/hardware code when it improves testability.
- New API behavior requires contract + tests.
- New DB state requires a versioned migration.
- New external dependency requires a reason and security/maintenance evaluation.
- Secrets never enter Git.
- No fabricated SLA/SLO/performance numbers; measure first.

## Workflow

Preferred unit of work:

```text
inspect → implement small coherent change → focused tests → commit → push → remote validation
```

At milestones, run the full relevant suite. For releases, verify the exact SHA is green before tagging.

## Definition of Done

A change is Done only when applicable items are satisfied:

- acceptance criteria met;
- tests added/updated and passing;
- lint/typecheck/build passing;
- contracts/migrations/docs updated;
- no secrets introduced;
- security/tenant implications reviewed;
- logs/errors are actionable;
- accessibility checked for UI changes;
- status reported truthfully as implemented/validated/partial/experimental/deferred.

## Safety/research

This is an experimental academic prototype. Do not introduce claims that it diagnoses, prevents or medically validates falls. Do not design test procedures that ask a person to intentionally fall. Use safe fixtures, objects/mannequins or ethically approved/supervised protocols.
