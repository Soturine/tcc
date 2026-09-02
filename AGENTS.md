# AGENTS.md

## Purpose

This repository is the official TCC evolution of `Soturine/iot-fall-monitor`. Agents must preserve engineering rigor, project traceability and the reliability guarantees documented after the 2026-09-01 baseline audit.

## Before changing code

1. Read `README.md`.
2. Read `ENGINEERING_CONSTITUTION.md`.
3. Read `docs/audit/iot-fall-monitor-port-audit-2026-09-01.md` for inherited code work.
4. Read the architecture/research document relevant to the task.
5. Read applicable ADRs under `docs/adr/`.
6. Inspect existing code/tests before proposing a rewrite.
7. Check `BACKLOG.md` and do not skip a prerequisite gate silently.
8. Never invent hardware capabilities, experimental results, measurements, references or implemented features.

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
  audit/
  legacy/
scripts/
tools/
  virtual-device/
```

Do not mass-move inherited code merely to match this tree. Migrate incrementally while preserving a green/reproducible baseline.

## Core architecture invariants

- ESP32/wearable edge logic must not depend on the mobile app being open.
- Backend is domain/persistence authority.
- Mobile/Web never connect directly to MySQL.
- MQTT is device/server transport, not the general mobile business API.
- HTTP/REST is the normal command/query API for app/web.
- Socket.IO is foreground realtime only.
- FCM is the Android background notification path.
- Wearable transport is behind an abstraction; do not assume BLE until hardware is selected.
- Tenant isolation and object-level authorization are mandatory.

### Critical event invariants

- A critical event gets one stable `event_uuid` before first transmission.
- Event identity must be robust to reboot and not rely only on wall clock/NTP.
- Retries preserve the same UUID.
- MQTT QoS/PUBACK is not server persistence confirmation.
- Device must not remove a pending critical event solely because a local publish call succeeded.
- Target guarantee is device persistent outbox → QoS 1 → backend commit → application ACK → remove from device outbox.
- Backend must enforce uniqueness/idempotence for `event_uuid`.
- Duplicate transport delivery must not create duplicate logical event/alert/notification.
- An offline `fall_detected` confirmed by edge logic must not lose alert semantics merely because periodic telemetry was absent from the server during the outage.
- `occurred_at_device` and `received_at` are distinct concepts.

### Device trust invariants

- Authenticated MQTT principal/ACL/topic is authoritative identity in external environments.
- Payload device ID is redundancy; topic/payload identity mismatch must be rejected/quarantined, not used to remap identity.
- External MQTT uses TLS verification and per-device least-privilege credentials/ACL.
- Do not silently fall back from TLS/authenticated transport to plaintext in staging.

### Provisioning invariants

- Do not make a custom plaintext HTTP API over an open SoftAP the normal path for Wi‑Fi/MQTT secrets.
- Prefer Espressif Unified Provisioning and current supported security mechanisms unless a spike proves a better option.
- Portal HTML may remain recovery/diagnostic with appropriately limited authority.
- Pairing/claim and Wi‑Fi provisioning are distinct trust operations even if the app orchestrates both.

### Mobile safety invariants

- Push payload is minimal; sensitive details are fetched after auth/authorization.
- Notification actions are idempotent.
- UI must distinguish stale/unknown/degraded state from healthy state.
- Protection Health must never claim medical-grade availability.
- A “Testar alerta” path must be distinguishable from real fall data.

## Engineering rules

- Prefer modular monolith; no microservices/Kafka/Kubernetes unless a measured need emerges.
- Prefer existing Node/Express/MySQL code over gratuitous rewrites.
- Prefer Kotlin/Jetpack Compose for Android.
- Extract pure logic from framework/hardware code when it improves testability.
- New API behavior requires contract + tests.
- New MQTT behavior requires schema/contract + compatibility consideration + tests.
- New DB state requires a versioned migration.
- New external dependency requires a reason, maintenance/security/license evaluation and preferably an exit path when critical.
- Secrets never enter Git.
- No fabricated SLA/SLO/performance numbers; measure first.
- Do not label provider acceptance as delivery-to-human without evidence.
- Do not label planning/code-written as validated.

## CI gate

The historical repo did not have GitHub Actions workflows. In this TCC repo, remote checks are foundational infrastructure.

Before major refactors:

- baseline build/tests must be reproduced;
- minimal CI must exist;
- the exact remote SHA must be checked.

Do not defer all CI to a late “QA phase”.

## Workflow

Preferred unit of work:

```text
inspect
→ identify invariant/acceptance criteria
→ implement small coherent change
→ focused tests
→ commit
→ push
→ remote validation
→ continue while CI runs when safe
```

At milestones, run the full relevant suite. For releases, verify the exact SHA is green before tagging.

For tasks developed on a branch/pull request, when the PR targets `main`, is mergeable and conflict-free, and all required checks are green, the agent must complete the task with a **merge commit** into `main` unless the task explicitly requests human review before merge. Do not use squash or rebase merge unless the task explicitly requires a different merge strategy.

## Porting inherited code

Classify every inherited component as one of:

```text
PRESERVE
PRESERVE + REFACTOR
MIGRATE WITH CONTRACT CHANGE
LEGACY EVIDENCE
DEPRECATE
DEFER
```

Do not copy historical docs into canonical locations without deciding their status. Preserve source SHA/provenance.

For behavior changes, create characterization tests before extraction when practical.

## Definition of Done

A change is Done only when applicable items are satisfied:

- acceptance criteria met;
- invariants preserved;
- tests added/updated and passing;
- lint/typecheck/build passing;
- contracts/migrations/docs updated;
- no secrets introduced;
- security/tenant/device-identity implications reviewed;
- failure/retry behavior considered;
- logs/errors are actionable and do not leak secrets;
- accessibility checked for UI changes;
- remote SHA validated when tooling exists;
- status reported truthfully as implemented/validated/partial/experimental/deferred.

## Safety/research

This is an experimental academic prototype. Do not introduce claims that it diagnoses, prevents or medically validates falls. Do not design test procedures that ask a person to intentionally fall. Use safe fixtures, objects/mannequins, public datasets or ethically approved/supervised protocols.

Do not invent references, DOI, authors, results, numerical performance, thresholds, sample sizes or conclusions.
