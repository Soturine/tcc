# Índice da Documentação

Este índice define quais documentos devem ser lidos como canônicos durante a implementação. Documentos futuros em `docs/legacy/` serão evidência histórica e não substituem decisões atuais.

## Extensão acadêmica TG1 — pulso e laboratório (proposta 2026-10-21)

- [Seleção do wearable, versões, compra brasileira e qualificação](hardware/wearable-selection-2026-10-21.md)
- [Conectividade independente do celular, Android, gateway opcional](architecture/wearable-mobile-and-resilience.md)
- [Registro de datasets de pulso, elegibilidade/licenças e cross-dataset](research/wrist-dataset-registry.md)
- [MotionLab Live: seis eixos, HAR sentar/levantar, ground truth, streaming e relatórios](research/motionlab-live.md)
- [Calibração opcional, personalização e feedback em modo shadow](research/personalization-feedback.md)
- [Plano de QA/Gates do pulso e ML](quality/wrist-motionlab-verification.md)
- [Estratégia acadêmica, artigo, TG1/TG2 e escopo](research/publication-and-scope.md)

**Todos planejados;** nenhum wearable, Android, novo dataset real ou MotionLab foi qualificado/implementado por estes documentos. *Atualização 2026-10-25:* ver [evidência P2.1](quality/p2-1-implementation-evidence-2026-10-25.md), [resultado WEDA-FALL](research/wrist-baseline-weda-fall-2026-10-25.md) (offline, sem HIL) e [MotionLab offline](../tools/motionlab/README.md); wearable continua ausente; o app Android A1/A2 entrou em 0.10.0-dev.0 (ver abaixo).

## Plano de execução e remediação — 2026-10-22

A complementação pós-PR #8/#9 **não implementa** os itens restantes: mantém `main` como fonte do código efetivamente testado e especifica as próximas entregas com bloqueios e critérios.

- [P2.1: rejeições/overflow de eventos, ACK, ACL/TLS, flash, clocks e cenários negativos](quality/critical-reliability-remediation-2026-10-22.md)
- [Qualificação prática do wearable, HAL, consumo, OTA/partições e HIL](hardware/wearable-qualification-protocol-2026-10-22.md)
- [Protocolo Nyquist e amostragem IMU: antialiasing, splits e medição](research/wrist-sampling-nyquist-protocol.md)
- [Plano executivo integrado TG1→TG2, dependências, entregas e gates](roadmap/tg1-execution-gates-2026-10-22.md)
- [Prompt de implementação de remediações para Claude](../prompts/claude-p2-1-remediation.md)

- [Versões, componentes, marcos e gates de release](roadmap/version-milestones.md)

## Comece aqui

1. [`../README.md`](../README.md) — visão geral e status.
2. [`../ENGINEERING_CONSTITUTION.md`](../ENGINEERING_CONSTITUTION.md) — padrão de engenharia.
3. [`../AGENTS.md`](../AGENTS.md) — regras para Codex/agentes.
4. [`audit/iot-fall-monitor-port-audit-2026-09-01.md`](audit/iot-fall-monitor-port-audit-2026-09-01.md) — auditoria da baseline histórica.
5. [`audit/iot-fall-monitor-port-audit-addendum-2026-09-01.md`](audit/iot-fall-monitor-port-audit-addendum-2026-09-01.md) — adendo com achados de detector, runtime, portal, privacidade e semântica de entrega.
6. [`migration/port-plan.md`](migration/port-plan.md) — sequência concreta do porte.
7. [`../BACKLOG.md`](../BACKLOG.md) — ordem/gates de implementação.

## Arquitetura canônica

- [`architecture/overview.md`](architecture/overview.md) — visão lógica e invariantes.
- [`architecture/device-connectivity.md`](architecture/device-connectivity.md) — device, critical event reliability, provisioning, wearable.
- [`architecture/contracts.md`](architecture/contracts.md) — HTTP/MQTT, ACKs, schemas, identidade.
- [`architecture/data-model.md`](architecture/data-model.md) — MySQL/migrations/event evidence/mobile sessions.
- [`architecture/mobile-android.md`](architecture/mobile-android.md) — Kotlin/Compose, FCM, Protection Health, provisioning; seção "Estado implementado" separa o que existe em 0.10.0-dev.0 do que é proposta.
- [`../apps/android/README.md`](../apps/android/README.md) — rodar e testar o app (emulador, aparelho via `adb reverse`, backend local).
- [`audit/android-a1-a2-audit-2026-10-25.md`](audit/android-a1-a2-audit-2026-10-25.md) — auditoria ponta a ponta A1/A2 (FIXED/PARTIAL/DEFERRED/BLOCKED) e Assertiva da suíte Android.
- [`architecture/mobile-technology-evaluation.md`](architecture/mobile-technology-evaluation.md) — racional Kotlin × Flutter/RN/KMP.
- [`architecture/cloud-deployment.md`](architecture/cloud-deployment.md) — staging provider-agnostic.
- [`architecture/cloud-options-evaluation.md`](architecture/cloud-options-evaluation.md) — opções de fornecedores avaliadas.
- [`data/data-classification.md`](data/data-classification.md) — classes, PII, frequência, crescimento medido e impacto da exclusão.
- [`data/retention-policy.md`](data/retention-policy.md) — retenção manual de telemetria, proteção de evidência e decisões pendentes.

## Produto e requisitos

- [`product/scope-and-features.md`](product/scope-and-features.md) — core/importante/stretch e CUJs.
- [`requirements/requirements.md`](requirements/requirements.md) — RF/RNF e critérios de aceitação.

## Segurança, QA e operação

- [`security/threat-model.md`](security/threat-model.md) — trust boundaries e mitigação.
- [`quality/qa-strategy.md`](quality/qa-strategy.md) — testes, fault matrix e Golden E2E.
- [`quality/fall-detector-replay.md`](quality/fall-detector-replay.md) — harness host/native e caracterização sintética da FSM de queda.
- [`devops/sdlc-and-ci-cd.md`](devops/sdlc-and-ci-cd.md) — Git/CI/release.
- [`roadmap/roadmap.md`](roadmap/roadmap.md) — fases do TCC.
- [`adr/README.md`](adr/README.md) — decisões arquiteturais aceitas/superseded.

## Contratos executáveis

- [`contracts/README.md`](contracts/README.md) — mapa do fluxo e estados current/planned.
- [`contracts/http-api.md`](contracts/http-api.md) — inventário das 35 operações HTTP reais.
- [`contracts/openapi.yaml`](contracts/openapi.yaml) — OpenAPI validado.
- [`contracts/mqtt-topics.md`](contracts/mqtt-topics.md) — tópicos, QoS, payloads e budgets medidos.
- [`contracts/mqtt/README.md`](contracts/mqtt/README.md) — JSON Schemas e exemplos MQTT.
- [`contracts/data-authority.md`](contracts/data-authority.md), [`contracts/event-identity.md`](contracts/event-identity.md) e [`contracts/time-semantics.md`](contracts/time-semantics.md) — autoridade, identidades, ACK futuro e tempo.
- [`../database/migrations/README.md`](../database/migrations/README.md) — auditoria, upgrade versionado, histórico/checksum e rollback controlado do MySQL.

## Pesquisa/TCC

- [`research/tcc-plan.md`](research/tcc-plan.md) — pergunta, objetivos e método inicial.
- [`research/sources-and-evidence.md`](research/sources-and-evidence.md) — regras de fontes/evidência.
- [`research/comparable-systems-and-patterns.md`](research/comparable-systems-and-patterns.md) — Apple/Pixel/ThingsBoard/Espressif/Home Assistant/SmartFall/datasets e padrões aproveitáveis.

## Autoridade em caso de conflito

Quando dois documentos parecerem divergir:

1. ADR mais recente que explicitamente supersede decisão anterior;
2. arquitetura/requisitos canônicos atuais;
3. auditoria + adendos + port plan;
4. README/backlog/roadmap;
5. documentação legada/histórica.

Se o conflito permanecer, não escolher silenciosamente: registrar/atualizar ADR/documento canônico na mesma mudança.

## Estado versus evidência

Use estes termos de forma consistente:

- **planned** — documentado, não implementado;
- **implemented** — código existe;
- **validated** — comportamento foi testado com evidência apropriada;
- **experimental** — existe, mas ainda sob avaliação;
- **partial** — parte do comportamento/ambiente está pronta;
- **deferred** — conscientemente adiado;
- **legacy** — histórico, não autoridade atual.

## Legado importado

A documentação de Projetos II integrada com o código da v0.9.0 está classificada no [índice legado](legacy/project-ii/README.md). Os caminhos históricos foram preservados no primeiro merge; seu conteúdo não substitui este índice nem os documentos canônicos atuais.
