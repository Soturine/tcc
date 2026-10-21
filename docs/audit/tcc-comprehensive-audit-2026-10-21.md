# Auditoria abrangente do TCC — 2026-10-21

Documento canônico desta rodada de auditoria. Complementa, sem substituir, a [auditoria de porte de 2026-09-01](iot-fall-monitor-port-audit-2026-09-01.md) e seu [adendo](iot-fall-monitor-port-audit-addendum-2026-09-01.md).

## 1. Estado de partida verificado

| Item | Evidência |
|---|---|
| `origin/main` | `f4ea7ed418d3d4b03e1030a44d8c9ff872e12b61` (upload do artigo INIC revisado, 2026-10-01) |
| Último merge funcional | `2cb74f2` — PR #7 `data/retention-lifecycle` |
| Trabalho local não publicado | branch `data/growth-estimate` com estimativa de crescimento por device/dia; avaliada (aritmética conferida: 11.124.736 B ÷ 24.343 linhas = 456,999 B/linha; × 172.800 linhas/dia = 78.969.427,2 B), testes verdes, commit `b837148` |
| Branch desta auditoria | `tcc/audit-tg1-p2`, criada de `b837148` com merge de `origin/main` |
| Ferramentas locais | Node 24.11.1, npm 11.6.2, PlatformIO 6.1.19, Docker 28.5.1, GitHub CLI autenticado |
| Hardware ESP32 | não acessível nesta sessão — nenhuma validação física é declarada |
| Último Security remoto conhecido | run `36911144931` (2026-10-01) falhou no job de dependências |

O código é tratado como fonte de verdade; documentação foi usada como referência e confrontada com a implementação.

## 2. Escala e convenções

- **Severidade:** P0 compromete correção/segurança do fluxo crítico ou a baseline; P1 risco alto com mitigação parcial; P2 risco moderado; P3 consistência/manutenção.
- **Estado:** `CONFIRMED`, `PROBABLE`, `NEEDS_EVIDENCE`, `NOT_REPRODUCED`, `RESOLVED`, `DEFERRED`, `ACCEPTED_RISK`.
- Linhas citadas referem-se ao SHA de partida (`f4ea7ed` + `b837148`).
- A coluna "Correção" é preenchida conforme os checkpoints avançam; `RESOLVED` exige teste de regressão nomeado.

## 3. Síntese priorizada

| ID | Sev. | Categoria | Resumo | Estado inicial |
|---|---|---|---|---|
| F-01 | P0 | Segurança/supply chain | Advisories em dependências de produção do backend (`engine.io` high, `ip-address` moderate, `proxy-addr` **critical**) quebram o gate Security | CONFIRMED |
| F-02 | P2 | Segurança/supply chain | Advisories no frontend (vite, follow-redirects, cadeia tailwind 3/braces) | CONFIRMED |
| F-03 | P1 | Segurança/config | `JWT_SECRET` cai em `change-me` em qualquer ambiente; nenhum fail-fast | CONFIRMED |
| F-04 | P1 | Segurança/transporte | Backend aceita MQTT plaintext, `rejectUnauthorized=false` e CA ilegível em ambiente externo | CONFIRMED |
| F-05 | P0 | Confiabilidade/firmware | Evento crítico sai do buffer quando `PubSubClient.publish()` (QoS 0) retorna sucesso local | CONFIRMED |
| F-06 | P0 | Confiabilidade/firmware | Buffer `drop_oldest` silencioso; só 4 de 10 itens persistem em NVS | CONFIRMED |
| F-07 | P1 | Identidade de evento | `event_uuid` derivado de relógio/millis/contador RAM, sem `boot_id` | CONFIRMED |
| F-08 | P0 | Confiabilidade/backend | `fall_detected` só gera alerta com telemetria SQL na janela; replay offline perde o alerta | CONFIRMED |
| F-09 | P0 | Protocolo | ACK de aplicação pós-commit inexistente (contrato apenas `planned`) | CONFIRMED |
| F-10 | P1 | Confiabilidade/backend | Falha de processamento MQTT (ex.: banco indisponível) é só logada; PUBACK já foi enviado | CONFIRMED |
| F-11 | P1 | Identidade de device | `device_uid` do payload pode remapear o `device_identifier` de outro device e herdar seu tenant | CONFIRMED (leitura de código) |
| F-12 | P1 | Pesquisa | Não há infraestrutura de avaliação com dados reais nem módulo de métricas; pergunta de pesquisa não incorpora a avaliação do detector | CONFIRMED |
| F-13 | P2 | Confiabilidade/backend | Sessão MQTT do backend `clean: true` | CONFIRMED |
| F-14 | P2 | Segurança/sessão | JWT de 7 dias não revogável e guardado em `localStorage` | CONFIRMED |
| F-15 | P2 | Segurança/firmware | `SETUP_PORTAL_ALWAYS_ON = true` mantém portal mutável na LAN | CONFIRMED |
| F-16 | P2 | Privacidade | Nome, peso e altura do paciente sincronizados e guardados no ESP32 | CONFIRMED |
| F-17 | P2 | Segurança/firmware | Default de fábrica aponta para broker público `broker.hivemq.com:1883` sem TLS | CONFIRMED |
| F-18 | P2 | Alertas | Deduplicação de alerta recente usa `event_time`, que no replay offline vira `received_at` | PROBABLE |
| F-19 | P3 | Versionamento | `package.json` em 0.9.0, enquanto tags/CHANGELOG chegam a v0.9.4 | CONFIRMED |
| F-20 | P3 | Rastreabilidade | Issue #1 (porte da baseline) segue aberta apesar do PR #2 mergeado e do backlog marcado | CONFIRMED |
| F-21 | P3 | Documentação acadêmica | Duas revisões do artigo INIC versionadas (raiz e `docs/`) sem indicação de canônica | CONFIRMED |
| F-22 | P0 | Reprodutibilidade/firmware | `platform = espressif32` sem versão: a resolução atual (8.0.0) quebra o build que passou no CI com 7.1.3 | CONFIRMED (descoberto no CP1) |
| F-23 | P3 | Ambiente | Testes `native` exigem gcc/g++ no host; Windows do autor não tem toolchain | CONFIRMED |

## 4. Findings detalhados

### F-01 — Advisories em dependências de produção do backend
- **Evidência:** `npm audit --omit=dev` em `backend/`: `engine.io 6.6.0–6.6.9` (GHSA-2gc4-cqfq-p2gv, high), `ip-address <=10.7.0` (GHSA-j6r3-76f7-8jcv, GHSA-h3mg-xc3c-68pw, moderate), `proxy-addr 1.1.0–2.0.7` (GHSA-jqcg-44mw-7w3h, critical). Todas transitivas (Socket.IO, mysql2/socks, Express).
- **Esperado:** job `dependency-audit` verde com `--audit-level=high`.
- **Impacto:** gate Security vermelho; risco real no servidor HTTP/Socket.IO.
- **Correção:** `npm audit fix` sem `--force`, alterando somente `backend/package-lock.json` (engine.io 6.6.11, ip-address 10.7.4, proxy-addr 2.0.8, socks atualizado). Reauditoria: 0 vulnerabilidades (produção e total).
- **Regressão:** `npm run check`, `npm test` (backend) após a atualização.

### F-02 — Advisories no frontend
- **Evidência:** 11 advisories antes da correção; após `npm audit fix`, `npm audit --omit=dev` = 0. Restam 7 (5 high, 2 moderate) somente em dev/build: `tailwindcss@3.4.19 → chokidar/micromatch/fast-glob → braces@3.0.3` e `postcss-nested → postcss-selector-parser <7.1.6`.
- **Causa:** `braces` 3.0.3 é a última versão publicada da linha usada pelo Tailwind 3; a correção proposta pelo npm é migrar para Tailwind 4 (major).
- **Decisão:** `ACCEPTED_RISK` para o resíduo de build: os padrões de glob vêm do repositório, não de entrada externa, e nada disso é empacotado no bundle servido. Migração para Tailwind 4 registrada como `NEXT_PHASE` (exige revisão visual).

### F-03 — Fallback inseguro de `JWT_SECRET`
- **Evidência:** `backend/src/config/env.js:60` — `toTrimmed(process.env.JWT_SECRET, "change-me")`; `.env.example:2` distribui o mesmo valor.
- **Impacto:** em staging/produção sem variável, qualquer pessoa assina JWT válido → tomada de conta e de tenant.
- **Correção:** perfis locais explícitos (`development`, `test`); fora deles não há fallback e `validateRuntimeConfig()` recusa segredo ausente, conhecido/default ou com menos de 32 caracteres. `server.js` valida antes de abrir banco/MQTT. Mensagens não incluem o valor.
- **Regressão:** `backend/tests/security/runtimeConfig.test.js`; reprodução manual: `NODE_ENV=production JWT_SECRET=change-me node src/server.js` encerra com `UNSAFE_RUNTIME_CONFIG`.

### F-04 — Transporte MQTT do backend sem exigência de TLS em ambiente externo
- **Evidência:** `backend/src/mqtt/client.js:18-23` segue sem CA quando o arquivo não pode ser lido; nada impede `mqtt://` ou `MQTT_TLS_REJECT_UNAUTHORIZED=false` fora do ambiente local.
- **Correção:** a mesma validação recusa, fora do ambiente local, `mqtt://`, verificação de certificado desabilitada, CA declarada e ilegível, credenciais MQTT ausentes, CORS `*` ou origens `http://`.
- **Risco residual:** ACL por device no broker continua `planned` (P8); o backend não consegue provar a ACL do broker.

### F-05 — Remoção prematura do evento crítico no firmware
- **Evidência:** `src/main.cpp:680-691` (`flushBufferedEvents`) faz `eventBuffer.pop()` após `mqttClient.publish()` retornar `true`; `src/mqtt_client.cpp:468-474` usa `PubSubClient::publish`, que só publica QoS 0. O caminho imediato (`publishCriticalEvent`, `src/main.cpp:622-634`) nem chega a enfileirar quando o publish local tem sucesso.
- **Impacto:** perda silenciosa se o TCP cair após a escrita local, se o broker descartar ou se o backend falhar antes do commit.
- **Correção planejada:** Checkpoint 3 (outbox crítica com remoção somente após ACK de aplicação).

### F-06 — Overflow `drop_oldest` e persistência parcial
- **Evidência:** `src/event_buffer.cpp:4-9` descarta o mais antigo; `include/app_config.h:190-193` define 10 itens em RAM e somente 4 persistidos; `src/main.cpp:142-157` grava snapshot truncado.
- **Impacto:** com 5+ eventos pendentes, reboot perde eventos sem sinal observável no backend.
- **Correção planejada:** Checkpoint 3 (capacidade persistida igual à capacidade lógica, overflow rejeita o novo evento de forma observável em vez de apagar um crítico já aceito).

### F-07 — `event_uuid` sem identidade de boot
- **Evidência:** `src/main.cpp:63-67` usa `millis()/1000` sem NTP; `src/main.cpp:90-102` compõe `uid-tipo-segundos-millis-seq` com `criticalEventSeq` em RAM.
- **Cenário de colisão:** dois boots sem NTP com o mesmo tipo de evento no mesmo `millis()` e mesma sequência (ex.: `sensor_fault` determinístico no boot) geram UUID idêntico; o backend (corretamente) trataria o segundo como duplicata ou conflito → evento real descartado.
- **Correção planejada:** Checkpoint 3 (`boot_id` persistido por contador NVS + aleatoriedade de hardware, sequência por boot; UUID gerado e persistido antes do primeiro envio).

### F-08 — Alerta de queda dependente de telemetria SQL
- **Evidência:** `backend/src/services/eventService.js:211-221` (`shouldCreateAlertForEvent`) exige `evidenceStatus ∈ {linked, partial}`; `eventService.js:808-810` rebaixa severidade para `medium`; `backend/src/utils/time.js:62-69` troca o horário do device por `received_at` quando o desvio passa de 10 min. Para um evento confirmado offline e entregue depois, a janela de busca de telemetria aponta para o momento da reconexão e normalmente não encontra amostras.
- **Esperado (ADR-016):** queda confirmada no edge gera alerta lógico com evidência local; telemetria do servidor só enriquece.
- **Correção planejada:** Checkpoint 4 (política de evidência explícita e `evidence_source`).

### F-09 — ACK de aplicação inexistente
- **Evidência:** `docs/contracts/mqtt/critical-event-ack-v1.schema.json` com `$comment: PLANNED`; nenhum publisher no backend e nenhum subscriber no firmware.
- **Correção planejada:** Checkpoints 3–4.

### F-10 — Falha de processamento após PUBACK
- **Evidência:** `backend/src/mqtt/client.js:100-113` apenas registra erro de `handleMqttMessage`; o MQTT.js confirma QoS 1 ao broker ao receber a mensagem.
- **Impacto:** banco indisponível = evento perdido sem retransmissão.
- **Correção planejada:** com o ACK de aplicação, a ausência de commit implica ausência de ACK e o device retransmite o mesmo UUID. A correção não depende de sessão persistente (F-13).

### F-11 — Remapeamento de identidade por `device_uid` do payload
- **Evidência:** `backend/src/services/mqttIngestionService.js:111-114,189-196` passa `payload.device_uid` como identidade primária; `backend/src/services/deviceService.js:862-879` executa `INSERT ... ON DUPLICATE KEY UPDATE device_identifier = VALUES(device_identifier)`. O check de mismatch (`mqttIngestionService.js:141-157`) compara apenas `device_id`.
- **Cenário:** publicar em `queda/devices/esp32_atacante/events` com `device_id=esp32_atacante` e `device_uid=<uid de um device pareado em outro tenant>` renomeia o device da vítima e atribui o evento ao tenant dela.
- **Viola:** ADR-015 (payload não escolhe outro device).
- **Correção planejada:** Checkpoint 5 — rejeitar antes de qualquer escrita quando o `device_uid` pertence a um device pareado cujo `device_identifier` diverge do tópico; renomeação de device pareado passa a exigir fluxo autenticado.

### F-12 — Lacuna experimental do TG1
- **Evidência:** `test/support/synthetic_sensor_readings.h` e `docs/quality/fall-detector-replay.md` cobrem apenas séries sintéticas; não há adaptador de dataset, normalização de unidades, métricas ou ground truth. `docs/research/tcc-plan.md` define a pergunta apenas em termos de arquitetura/entrega.
- **Impacto:** o TG1 não teria como medir o detector com dados reais nem separar métricas de detecção das de entrega.
- **Correção planejada:** Checkpoint 2.

### F-13 — Sessão limpa no backend
- **Evidência:** `backend/src/mqtt/client.js:36` (`clean: true`).
- **Decisão:** `DEFERRED` como otimização de recovery. A correção ponta a ponta é garantida pela outbox + ACK; sessão persistente reduz latência de recuperação mas não substitui o ACK.

### F-14 a F-17 — Itens já previstos no backlog
- F-14: `backend/src/utils/auth.js:15` (`expiresIn: "7d"`), `frontend/src/lib/storage.ts:6-14`. ADR-009/P3. `DEFERRED`.
- F-15: `include/app_config.h:66`. P3/P6. `DEFERRED`.
- F-16: `include/device_config.h:29-33`. P1 aberto. `DEFERRED`.
- F-17: `include/app_config.h:33-39`. P8. `DEFERRED`; o validador do backend (F-04) impede que o lado servidor opere assim fora do ambiente local.

### F-18 — Deduplicação de alerta por `event_time`
- **Evidência:** `backend/src/services/alertService.js:43-82` compara `e.event_time` dentro de uma janela. Com F-08, um evento offline antigo recebe `event_time = received_at` e pode ser fundido com um alerta recente não relacionado.
- **Estado:** `PROBABLE`; reavaliado após o Checkpoint 4.

### F-19 — Versões divergentes
- **Evidência:** `backend/package.json` e `frontend/package.json` em `0.9.0`; tags `v0.9.1`…`v0.9.4` e seções correspondentes no CHANGELOG.
- **Política adotada:** os manifests passam a refletir a última versão publicada (`0.9.4`); mudanças posteriores ficam em `[Unreleased]` até uma release validada.

### F-20 — Issue #1 desatualizada
- **Evidência:** issue #1 "M0/M1 — Portar baseline" aberta; PR #2 mergeado em 2026-09-02; tag `tcc-baseline-v0.9.0`; itens P0 correspondentes marcados no BACKLOG.
- **Ação:** comentar evidência e fechar ao final desta rodada.

### F-21 — Revisões do artigo INIC
- **Evidência:** `Artigo_INIC_2026_Rafael_Ryan.docx` (raiz, `f4ea7ed`, SHA-256 `6e36a9fc…`, 2.430 palavras extraídas) e `docs/Artigo_INIC_2026_Rafael_Ryan.docx` (`09ad767`, SHA-256 `8fc7bcf0…`, 2.664 palavras). O texto difere em resumo, metodologia e tabela de parâmetros; a cópia da raiz é a revisão mais recente.
- **Decisão:** `DEFERRED` ao autor. Nenhum arquivo foi movido ou apagado; a escolha da versão canônica e o destino (`docs/academic/`) dependem de quem submeteu o artigo.

### F-22 — Plataforma ESP32 não fixada
- **Evidência:** `platformio.ini` com `platform = espressif32`; último Firmware verde (run 36911144666, 2026-10-01) registrou `espressif32@7.1.3`, ArduinoJson 6.21.6, PubSubClient 2.8.0. Em 2026-10-21 a resolução trouxe `espressif32@8.0.0` e o build falhou em `src/mqtt_client.cpp:156` (`'WiFi' was not declared`), pois o include de `WiFi.h` vinha transitivamente.
- **Impacto:** o próximo push em `main` ficaria vermelho sem mudança de código; builds não reprodutíveis.
- **Correção:** plataforma, `native` e bibliotecas fixadas nas versões validadas; `#include <WiFi.h>` explícito. Build ESP32 e 19 testes nativos verdes em Linux/Docker.
- **Risco residual:** atualização de plataforma vira mudança deliberada com build/testes.

### F-23 — Toolchain nativa ausente no Windows
- **Evidência:** `pio test -e native` falha com `'gcc' não é reconhecido`.
- **Correção:** `scripts/pio-docker.sh` reproduz firmware nativo, build ESP32 e testes de pesquisa no mesmo ambiente Linux do CI.

## 5. Coerência acadêmica

O histórico de orientação enfatiza a diferenciação entre quedas e atividades cotidianas; o plano atual enfatiza confiabilidade de alertas e arquitetura mobile-first. As duas linhas são compatíveis desde que medidas separadamente (o próprio `tcc-plan.md` já separa "desempenho do detector" e "desempenho do pipeline"). A proposta de pergunta principal e objetivos mensuráveis está em [`docs/research/tcc-plan.md`](../research/tcc-plan.md) e no [protocolo experimental](../research/experimental-protocol.md), sem declarar aprovação do orientador.

## 6. Plano reconciliado com o backlog

| Checkpoint | Escopo | Itens do backlog |
|---|---|---|
| CP1 | F-01, F-02, F-03, F-04, F-19 | P0 `.env.example`/fail-fast; P3 JWT fail-fast |
| CP2 | F-12 | P10 protocolo, scripts reproduzíveis; P11 split sem leakage (somente infraestrutura) |
| CP3 | F-05, F-06, F-07 | P2 firmware |
| CP4 | F-08, F-09, F-10, F-18 | P2 backend, offline fall evidence |
| CP5 | F-11 e regressões das mudanças | P3 identidade/autorização |
| CP6 | documentação, rastreabilidade, F-20, F-21 | P12 parcial |

## 7. Registro de execução

Atualizado a cada checkpoint (SHA, testes, estado). Ver seção 8.

## 8. Resultado por checkpoint

_Preenchido ao longo da execução._
