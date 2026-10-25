# Matriz de rastreabilidade

Snapshot de 2026-10-21 (branch `tcc/audit-tg1-p2`). Um requisito só aparece como coberto quando há teste ou ensaio que verifica o comportamento prometido; nome parecido de teste não conta.

**Estados:** `implemented` (código existe) · `tested` (teste automatizado verifica) · `emulated` (ensaio ponta a ponta com virtual device e serviços reais) · `hw-validated` (ESP32 físico) · `exp-validated` (resultado experimental) · `partial` · `planned` · `blocked` · `deferred` · `not reassessed` (não reavaliado nesta rodada; nenhuma afirmação feita).

Nenhum item está `hw-validated` ou `exp-validated` nesta data.

## 1. Requisitos → implementação → evidência

| Requisito | Componente | Implementação | Teste / evidência | Estado |
|---|---|---|---|---|
| RF-021 persistir evento crítico até confirmação de commit | firmware | `src/critical_outbox.cpp`, `src/critical_outbox_fs_store.cpp` | `test/test_critical_delivery` (publish não remove; reboot restaura; overflow); E2E 1–7 | implemented, tested, emulated; HIL pendente |
| RF-022 reenviar preservando `event_uuid` após reconexão/reboot | firmware | `src/event_identity.cpp`, outbox | `testRebootRestoresPendingInOrderAndAllowsImmediateRetry`, `testRebootWithoutClockDoesNotCollide`; E2E 4 e 6 | implemented, tested, emulated; HIL pendente |
| RF-023 ACK de aplicação só após persistência transacional | backend | `mqttIngestionService.publishCriticalEventAck` | `mqttIngestionService.test.js` (ACK após commit; sem ACK em falha de commit); contrato do ACK gerado; E2E 6 e 7 | implemented, tested, emulated |
| RF-024 alerta para queda confirmada no edge sem telemetria da janela | backend + firmware | `local_evidence` v1; `resolveLocalEvidence`, `shouldCreateAlertForEvent` | `eventEvidence.test.js` (offline com evidência local; evidência incompleta não confirma); E2E 2 e 9 | implemented, tested, emulated |
| RF-025 rejeitar identidade do payload que contradiz o tópico | backend | `device_id` vs tópico; `rejectClaimedIdentifierChange` | `mqttIngestionService.test.js`, `deviceIdentityMismatch.test.js`; E2E 11 e 12 | implemented, tested, emulated |
| RF-027 teste de alerta sem queda física | backend | `alert_test` persistido, confirmado, sem alerta | `eventEvidence.test.js`; E2E 10 | partial: backend apenas; app/push (P5) planned |
| RF-001 a RF-020, RF-026, RF-028 a RF-030 | web/backend/Android | — | — | not reassessed / planned (Android, FCM, provisioning) |
| RNF-SEC-003 secrets fora do Git | repo/CI | workflow `secret-patterns`; senhas de teste geradas em runtime | CI Security | implemented, tested (CI) |
| RNF-SEC-004 credencial por device e ACL mínima | broker | `infra/mqtt/mosquitto-acl.example`, `infra/mqtt/mosquitto-staging.conf` | `infra/mqtt/run-security-verification.sh` (19 casos: TLS/CA/hostname, anônimo, revogação, forja cruzada, auto-ACK, curinga, retained) no CI; histórico `verify-acl.js` (5/5) | partial: referência verificada; staging implantado e credencial por device no ESP pendentes |
| RNF-SEC-006 segredo JWT default/fraco não inicia ambiente externo | backend | `validateRuntimeConfig` | `tests/security/runtimeConfig.test.js` | implemented, tested |
| RNF-SEC-001 TLS externo | backend | validação de `mqtts://` e `rejectUnauthorized` | `runtimeConfig.test.js` | partial: backend recusa; firmware/staging pendentes |
| RNF-SEC-007 identidade derivada do tópico | backend | ver RF-025; tópico do ACK derivado do tópico de origem | idem RF-025 | implemented, tested, emulated |
| RNF-REL-001 reenvio não cria múltiplos eventos lógicos | backend | `UNIQUE(event_uuid)` + deduplicação | `eventEvidence.test.js`, `eventIdentityMysql.test.js` (concorrência); E2E 3 | implemented, tested, emulated |
| RNF-REL-002 outbox persistente no device | firmware | LittleFS, CRC32, escrita atômica | `test_critical_delivery` (codec, corrupção, falha de gravação) | implemented, tested; HIL pendente |
| RNF-REL-003 transactional outbox de notificação | backend | — | — | planned (P3/P5) |
| RNF-REL-005 publish local ≠ persistência | firmware + backend | ACK pós-commit | `testPublishDoesNotRemoveOnlyAckDoes`; E2E | implemented, tested, emulated |
| RNF-REL-006 pendente sobrevive a reboot | firmware | restauração da outbox | `testRebootRestores…`, `testSameOrderAcrossBootsNeverOverwritesPendingEvent` | implemented, tested; HIL pendente |
| RNF-REL-007 `event_uuid` independente de NTP | firmware | contador de boot + nonce + sequência | `test_critical_delivery` | implemented, tested |
| RNF-REL-008 alerta após período offline | backend | ver RF-024 | E2E 2 | implemented, tested, emulated |
| RNF-REL-009 restart/duplicata/reorder não alteram resultado lógico | backend | idempotência + ACK | E2E 3, 6, 8, 16–18 (MySQL recusado, TCP reset, broker parado) | emulated; HIL pendente |
| RNF-REL-010 overflow detectável | firmware | `overflow_count`, `sos_overflow_count`, `degraded`, reservas SOS/queda | `testOverflowRejectsNewEventWithoutDroppingAccepted`, `testCapacity16_17_100WithTwoRebootsNeverDropsAccepted`, `testReservesKeepSlotsForSosAndFall` | implemented, tested; HIL pendente |
| RNF-DATA-001 ocorrência ≠ recebimento | backend | `occurred_at_device`, `received_at`, `event_time` por ocorrência | `criticalEventTime.test.js`; E2E 2 e 8 | implemented, tested, emulated |
| RNF-DATA-002 clock quality preservada | firmware + backend | `clock_quality` no payload e no banco | E2E 9 | implemented, emulated |
| RNF-DATA-003 dado do servidor enriquece, não apaga decisão edge | backend | `evidence_source` | `eventEvidence.test.js` (`both`, `device`) | implemented, tested |
| RNF-DATA-004 campos de integridade normalizados | banco | `events.evidence_source` (migration 003) | `eventIdentityMysql.test.js` (upgrade/rollback) | implemented, tested |
| RNF-PRI-004 teste distinguível de real | backend | `alert_test`, severidade `low`, sem alerta | E2E 10 | partial |
| RNF-MAN-002 contratos versionados | contratos | schemas MQTT/OpenAPI | `tests/contracts/contracts.test.js` | implemented, tested |
| RNF-MAN-003 migrations versionadas | banco | `003_event_evidence_source` | MySQL descartável | implemented, tested |
| RNF-MAN-004 regras críticas testáveis sem hardware | firmware | `event_identity`, `critical_outbox`, `outbox_codec`, `critical_ack`, `critical_result`, `critical_delivery`, `sha256`, `sensor_signal` puros | 65 testes nativos | implemented, tested |
| RNF-MAN-005 scripts em CI/Linux | repo | `scripts/pio-docker.sh`, workflows | CI | implemented |
| RNF-OBS-001 identificador ponta a ponta | todos | `event_uuid` em logs, banco e ACK | E2E (ACK correlacionado ao registro) | implemented, emulated |
| RNF-OBS-005 health cobre outbox | firmware | `status.critical_outbox` | schema/exemplo de status | partial: exibição no app (P5) planned |

## 2. Objetivos acadêmicos → experimento → métrica → resultado

Detalhes no [protocolo experimental](../research/experimental-protocol.md).

| Objetivo | Pergunta / hipótese | Experimento | Métrica | Resultado |
|---|---|---|---|---|
| O1 qualidade da baseline A | H1a, H1b, H1c | replay SisFall (cintura) e KFall (lombar) com `fallbench` | sensibilidade, especificidade, precisão, F1 por trial (IC de Wilson); falsos alarmes/h | **não executado**: aguarda dados e confirmação de conversão do SisFall |
| O2 latência de confirmação | — | KFall (onset/impacto anotados) | mediana/p95 de `detecção − impacto` | **não executado** |
| O3 entrega crítica sob falhas | H2 | cenários 1–12 em emulação; HIL pendente | eventos/alertas lógicos por UUID, duplicatas, perdas | emulação: 12/12 cenários com 1 evento e no máximo 1 alerta lógico por UUID; **não é resultado experimental de hardware** |
| O4 recuperação | — | HIL com Wi‑Fi derrubado e reboot | tempo até ACK; idade máxima na outbox | **não executado** (só uma observação isolada de 55 ms em emulação, sem valor estatístico) |
| O5 calibração B sem vazamento | — | split por participante + comparação A × B | diferença de métricas no conjunto teste | **planned** |

## Candidatos wearable/MotionLab (2026-10-21; **não implementados**)
| ID | Especificação | Evidência futura | Estado |
|---|---|---|---|
| RF-W01/W02 | [HW selection](../hardware/wearable-selection-2026-10-21.md), [comunicação](../architecture/wearable-mobile-and-resilience.md) | HW/HIL + Android | proposed |
| RF-W03/W04/W05 | [MotionLab](../research/motionlab-live.md) | ML-0..ML-7, resultados físicos | proposed |
| RF-W06/W07 | [Personalização](../research/personalization-feedback.md) | CAL/shadow/rollback | proposed |
| RF-W08 | [Datasets](../research/wrist-dataset-registry.md) | acesso/licenças/replays reais | proposed |
| RNF-W01..03 | [Gates QA](../quality/wrist-motionlab-verification.md) | segurança/privacy/latência | proposed |

A trilha é de planejamento; não acrescenta cobertura testada nem métricas validadas.


## Complemento de rastreabilidade — 2026-10-22, **planned**

| Grupo | Requisito / risco | Código/evidência atual | Evidência requerida para aprovação | Status |
|---|---|---|---|---|
| P2.1 | F-29 rejeição permanente, SOS e outbox cheia | `critical-event-result-v1`, quarentena, reservas | 13 testes de ingestão, 23 nativos, E2E 13–15 ([evidência](docs/quality/p2-1-implementation-evidence-2026-10-25.md)) | implemented, tested, emulated; HIL pendente |
| P2.1 | F-31 suspeita × confirmada × SOS | `alertPolicy.js` (ADR-029) | matriz de prioridade, E2E 9 | implemented, tested; revisão de produto pendente |
| Segurança | F-17/27 MQTT TLS/ACL por device | `infra/mqtt/*example*` | broker staging real e testes cruzados de identidade | planned |
| Firmware | F-28 flash/partições/OTA | `platformio.ini`, build 91% na partição antiga | build/memória/migração/recovery e HIL do modelo escolhido | planned |
| Tempo | F-30 clock sem sincronização | eventos com clock_quality | replay offline e âncoras temporais com incerteza | planned |
| Alertas | F-31 `fall_suspected` / evidência insuficiente | `alertService.js` | matriz severidade e testes FNs | planned |
| Payload | F-32 envelope MQTT redundante | `event.current.json` | schema versionado e testes compatibilidade v1/v2 | planned |
| Falhas | F-33 broker down/DB refused | E2E com DB pause | nova matriz E2E + HIL | planned |
| Pesquisa | pulso, Nyquist, ML offline | `tools/research` synthetic | licenças/hash real, splits e medições SIG00–06 | planned |
| Produto | Android push/Protection Health | docs de arquitetura | apps Android, FCM real, RBAC e notificação testada | planned |

[Plano detalhado](../roadmap/tg1-execution-gates-2026-10-22.md) · [P2.1](../quality/critical-reliability-remediation-2026-10-22.md).

