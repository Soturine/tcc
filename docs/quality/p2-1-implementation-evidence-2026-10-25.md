# Evidência de implementação P2.1 — confiabilidade crítica e segurança MQTT (2026-10-25)

> **Tipo:** registro de implementação e execução desta rodada. Plano de origem: [remediação P2.1](critical-reliability-remediation-2026-10-22.md). Branch `fix/p2-1-critical-assurance-wrist-foundation` a partir de `main@73d3b4c` (PRs #8, #9 e #10 integrados; Backend/Firmware/Web/Security verdes nesse SHA). **Nada aqui é HIL**: não houve ESP32 físico nesta rodada. Níveis de evidência: `host-tested` (testes nativos/unitários), `emulated` (virtual device + backend/MySQL/Mosquitto reais e descartáveis), `reference-broker` (Mosquitto real com CA/senhas efêmeras, não staging implantado), `build-measured` (saída de `pio run`).

## 1. Ambiente

| Item | Valor |
|---|---|
| Host | Windows 10 Home 10.0.19045 + Docker Desktop 28.5.1 |
| Node / npm | 24.11.1 / 11.6.2 |
| PlatformIO | 6.1.19 em `python:3.12` (container; o host não tem gcc), `espressif32@7.1.3` |
| MySQL | `mysql:8.4` descartável |
| Mosquitto | 2.1.2 — `eclipse-mosquitto:2@sha256:38c0da4f2ef84284d47b3b3eeea1cb3bdeabe81ee10caf0cd5c5ff61ee3ea408` (a tag `2.1.2` não existe no registro; fixado por digest) |
| OpenSSL (geração efêmera de CA) | 3.2.2 (Git for Windows); `openssl` do runner Ubuntu no CI |

## 2. CP0 — baseline reproduzida antes de qualquer mudança (`main@73d3b4c`)

| Verificação | Resultado |
|---|---|
| Backend `npm test` | 129 testes: 124 pass, 0 fail, **5 skipped** (exigem MySQL) |
| MySQL descartável: migrations / retenção | 4/4 e 1/1 |
| Firmware `pio test -e native` | 42/42 |
| Build `esp32dev` | SUCCESS; Flash **1.193.313 / 1.310.720 B (91,0%)**; RAM estática 55.360 B |
| CI no SHA | Backend, Firmware, Web e Security `success` |

Fragilidades encontradas no CP0 (não documentadas antes):
- O cenário E2E 7 (MySQL congelado) era **skipped no CI** porque `E2E_DB_CONTAINER` não era definido.
- Rejeições de ingestão (`EVENT_UUID_CONFLICT`, UUID inválido, identidade divergente) só geravam log: o device retentava a cada 60 s indefinidamente, ocupando 1 dos 16 slots (F-29 confirmado no código, não só no documento).
- `transaction()` mascarava o erro original quando a conexão MySQL morria (o `rollback()` lançava) e devolvia a conexão quebrada ao pool.
- `serviceCriticalOutbox`/roteamento de ACK em `main.cpp` não tinham teste algum.

## 3. Matriz requisito/risco → código → teste → evidência → status

| Finding / risco | Código | Teste | Evidência | Status |
|---|---|---|---|---|
| **F-29** rejeição permanente sem resposta, retry infinito | `backend/src/mqtt/criticalEventResult.js`, `mqttIngestionService.js`; `src/critical_result.cpp`, `src/critical_outbox.cpp` | 13 testes de ingestão (9 falham no serviço anterior), contrato `critical-event-result-v1`, 16 nativos, E2E 13/14/15 | host + emulated | **FIXED (host/emulação)**; HIL `BLOCKED` |
| Mesmo UUID com payload divergente | `resolveDuplicateEvent` → `rejected_permanent/event_uuid_conflict`; device só aplica com hash igual | ingestão, nativo `HashMismatch`, E2E 14 e 20 | host + emulated | FIXED |
| Overflow / SOS com fila cheia | reservas `CRITICAL_OUTBOX_SOS_RESERVE=4`, `FALL_RESERVE=4`; sem despejo | nativo 16/17/100 eventos + 2 reboots; contadores `sos_overflow_count` | host | **PARTIAL**: reserva garante 4 SOS mesmo com 12 quedas pendentes; 5º SOS simultâneo é recusado (limite físico declarado, alarme local continua) |
| Quarentena persistente limitada | `EntryState::Quarantined`, codec `CEO2`, `quarantineCapacity=8`, `clearQuarantined` | nativos (reboot, cheia, falha de escrita, confirmação tardia) | host | FIXED; **expiração automática não implementada** (seria perda deliberada; requer decisão humana) |
| Janela de perda ao regravar `.evt` (introduzida pela quarentena) | `OutboxCodec::decideTmpRecovery`, `CriticalOutboxFsStore::loadAll` | nativo `testTmpRecoveryNeverLosesRewrittenEvent` | host | FIXED na lógica; LittleFS real `BLOCKED` (HIL) |
| Migração de arquivos da outbox | `decode` lê `CEO1` e `CEO2` | nativo `testLegacyCeo1FilesAreRestoredAfterUpgrade` | host | FIXED (sem formatar) |
| ACK/resultado autenticado e correlacionado | ACL backend-only + `device_id` + `payload_sha256` + tópico próprio | parser nativo (cross-device, hash, campos incoerentes), broker 19/19 | host + reference-broker | FIXED na referência; staging real `BLOCKED` |
| Replay de resultado / reboot | `recentlyAcked`, hash por conteúdo | nativos `DuplicateResult`/`HashMismatch` | host | FIXED |
| `transaction()` mascarando erro | `backend/src/db/pool.js` | 2 testes novos (falham antes) | host | FIXED |
| **F-17** broker público de fábrica | `DEFAULT_MQTT_HOST = ""` | nativo de defaults | host + build | FIXED (firmware); credencial por device no ESP `DEFERRED` (provisionamento P6) |
| **F-27** ACL efetiva | `infra/mqtt/mosquitto-staging.conf`, `mosquitto-acl.example` | `infra/mqtt/run-security-verification.sh` (19 casos) no CI | reference-broker | FIXED na referência; staging implantado **não testado** |
| **F-33** broker parado, MySQL recusado, TCP reset, MySQL congelado | proxy `TcpFaultProxy`, `E2E_BROKER_CONTAINER`, `E2E_DB_CONTAINER` | E2E 7, 16, 17, 18 | emulated | FIXED (emulação); HIL `BLOCKED` |
| **F-31** `fall_suspected` urgente; queda sem evidência silenciosa | `backend/src/services/alertPolicy.js` (ADR-029) | matriz + E2E 9 | host + emulated | FIXED — **decisão de produto para revisão** |
| **F-28** flash 91% | sem mudança de partição | builds comparativos (seção 5) | build-measured | **DEFERRED** com dados; troca de layout exige HIL e migração |
| **F-30** UTC offline | semântica preservada; estimativa por âncora não implementada | E2E 20 | emulated | **PARTIAL** |
| **F-32** payload crítico grande | sem `critical-event-v2` | — | — | **DEFERRED** (seção 6) |
| **F-13** sessão MQTT limpa | inalterado | — | — | DEFERRED (ADR-020: correção não depende dela) |
| **F-14** JWT 7 dias em `localStorage` | inalterado | — | — | DEFERRED (P3/Android) |
| **F-15** portal sempre ativo | inalterado de propósito | — | análise abaixo | **DEFERRED com novo risco identificado** |
| **F-16** PII do paciente no ESP | inalterado | — | — | DEFERRED |
| QoS 1 / ESP-MQTT | interface `CriticalTransport` extraída; cliente inalterado | 5 nativos do laço | host | **DEFERRED** (ADR-020; troca só com HIL) |

**F-15 — por que não foi desligado:** com `SETUP_PORTAL_ALWAYS_ON=false`, `ConnectivityManager` chama `enterSetupMode` após ~30 s ou 6 falhas de MQTT. Uma simples queda do broker tiraria o device do modo de operação e bloquearia a entrega da outbox até reconfiguração. Desligar o portal sem HIL e sem separar "recuperação" de "falha transitória do broker" arriscaria o caminho do SOS.

## 4. Fail-open × fail-closed por componente (estado após a rodada)

| Componente | Situação de falha | Comportamento |
|---|---|---|
| Outbox do device | resultado desconhecido / sem resposta | **fail-safe**: mantém e retenta (backoff até 60 s) |
| Outbox do device | `rejected_permanent` com hash correto | quarentena persistente; não conta como entregue; sinaliza `degraded` |
| Outbox do device | quarentena cheia ou falha de escrita | mantém ativo com retry lento; não apaga |
| Outbox do device | fila cheia | recusa o NOVO evento (contado por prioridade); nada aceito é despejado |
| Device | ACK legado `critical-event-ack-v1` | aceito por compatibilidade (`CRITICAL_ACCEPT_LEGACY_ACK=true`) — **fail-open consciente** para downgrade; desligar após backend novo + ACL efetiva |
| Backend | erro desconhecido na ingestão | `retryable` (nunca `committed`) |
| Backend | COMMIT incerto (conexão perdida) | `retryable`; a retransmissão vira `duplicate` se o COMMIT ocorreu |
| Backend | CA TLS ilegível em ambiente externo | recusa iniciar (validação existente em `env.js`) |
| Broker (perfil staging) | plaintext, anônimo, CA/hostname errados, credencial revogada | recusado (verificado) |
| Firmware TLS sem CA e sem `tlsInsecure` | comportamento depende do `WiFiClientSecure` do core | **não verificado** sem hardware |

## 5. F-28 — orçamento de flash medido (mesmo código, build real, sem HIL)

| Layout | App máx. (B) | Uso (B) | Uso | OTA dupla | Dados (LittleFS) |
|---|---|---|---|---|---|
| `esp32dev` `default.csv` (**atual**) | 1.310.720 | 1.204.785 | **91,9%** | sim | 0x160000 = 1.408 KiB |
| `esp32dev` `min_spiffs.csv` | 1.966.080 | 1.204.785 | 61,3% | sim | 0x20000 = 128 KiB |
| `esp32dev` `huge_app.csv` | 3.145.728 | 1.204.785 | 38,3% | **não** | 0xE0000 = 896 KiB |
| ESP32-S3 16 MB `default_16MB.csv` (`esp32-s3-devkitc-1`) | 6.553.600 | 1.157.661 | 17,7% | sim | 0x360000 = 3.456 KiB |

RAM estática: 55.504 B (esp32dev) e 54.032 B (S3). Heap mínimo, stack e PSRAM em execução: **`BLOCKED` (sem HIL)**.

Crescimento nesta rodada: +11.472 B de flash (91,0% → 91,9%), +144 B de RAM estática. Pior caso da outbox + quarentena: (16 + 8) × (3.800 + ~512) B ≈ 101 KiB.

**Decisão: não trocar o layout nesta rodada.** Motivos: (1) `min_spiffs` reduz os dados para 128 KiB, apertado para o pior caso de ~101 KiB mais o overhead do LittleFS; (2) `huge_app` perde a OTA dupla; (3) qualquer troca move o offset da partição de dados, e o LittleFS existente (outbox com eventos pendentes) **seria perdido** sem uma migração explícita, que exige HIL; (4) o alvo wearable provável é S3 com 16 MB, onde a margem é ampla. Pré-condição para ESP-MQTT/TLS adicional/LVGL no `esp32dev`: definir a margem-alvo antes do spike e testar a migração (exportar a outbox → regravar a tabela → restaurar) em bancada.

## 6. F-30 e F-32 — decisões

- **F-30 (PARTIAL):** a semântica é preservada e agora testada E2E (cenário 20). Sem NTP: `occurred_at_device=NULL`, `clock_quality=unsynced`, `boot_id` e `device_uptime_ms` gravados, `event_time=received_at` como fallback declarado, e o mesmo UUID com timestamp diferente vira conflito. **Não implementado:** a estimativa de ocorrência por âncora (status do mesmo `boot_id` com `received_at` + uptime) com incerteza explícita. Ela exige novas colunas (migração) e uma definição de limite de atraso de transporte; fica como próximo passo.
- **F-32 (DEFERRED):** o exemplo `event.current.json` cabe no orçamento de contrato (≤ 3.584 B) e no buffer MQTT de 4.096 B; o pior caso de flash da outbox é ~101 KiB de 1.408 KiB. Não há benchmark que justifique `critical-event-v2` agora; criar outra versão de contrato aumentaria a superfície de compatibilidade sem ganho medido.

## 7. Execuções desta rodada

| Suíte | Comando | Resultado |
|---|---|---|
| Backend | `npm --prefix backend test` | 148 testes: 143 pass, 0 fail, 5 skipped (MySQL) |
| MySQL descartável | `MYSQL_INTEGRATION=1 npm run test:mysql:migrations` / `test:mysql:retention` | 4/4 e 1/1 |
| Contratos | `npm --prefix backend run test:contracts` | 7/7 (inclui `critical-event-result-v1`) |
| Firmware nativo | `pio test -e native` (container `python:3.12`) | 65/65 (42 anteriores + 23 novos) |
| Build | `pio run -e esp32dev` | SUCCESS, 1.204.785 B (91,9%) |
| E2E (emulação) | `npm run test:e2e:critical` com `E2E_DB_CONTAINER` e `E2E_BROKER_CONTAINER` | 20/20 em 49 s, saída 0 (relatórios `critical-delivery-5ac774.json`, `-ba1a22.json`; o anterior `-90b2a0` tinha 19 cenários) |
| Broker (referência) | `bash infra/mqtt/run-security-verification.sh` | 19/19 |
| Web | `npm --prefix frontend run lint` / `build` | sem erros / build OK |

Métricas de emulação, **não de desempenho de produto**: recuperação após reinício do broker de 12,3 s (3 tentativas; timeout de ACK do virtual device de 4 s); MySQL recusado e TCP reset produziram 2 resultados `retryable` antes do commit.

Achados durante a execução e corrigidos: o processo E2E não terminava depois do cenário de broker (travaria o job de CI); o verificador de broker travava em publicações negadas sem PUBACK; o advisory GHSA-27r3-jxmh-p828 (`body-parser` 2.3.0) começou a falhar o Security por motivo externo. Pendência operacional: o `mosquitto_passwd` 2.1.2 avisa que versões futuras exigirão `passwd` do root, mas `root:root 0600` impediu o perfil de subir; decidir no host de staging real.

## 8. Limites e o que esta rodada não prova

- Nenhum ESP32 físico: LittleFS real, power-cut real, heap/stack, TLS do `WiFiClientSecure`, reconexão Wi-Fi e a migração `CEO1→CEO2` em flash real estão `BLOCKED`.
- O virtual device **emula** a regra da outbox; ele não executa o C++ do firmware (essa lógica é coberta pelos testes nativos).
- O broker verificado é uma referência descartável; staging implantado, rotação de credenciais e provisionamento de credencial por device no ESP **não existem ainda**.
- Não há FCM/Android: "prioridade" de alerta afeta apenas a listagem e o realtime; não houve entrega a humano.
