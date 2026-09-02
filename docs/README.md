# Índice da Documentação

Este índice define quais documentos devem ser lidos como canônicos durante a implementação. Documentos futuros em `docs/legacy/` serão evidência histórica e não substituem decisões atuais.

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
- [`architecture/mobile-android.md`](architecture/mobile-android.md) — Kotlin/Compose, FCM, Protection Health, provisioning.
- [`architecture/mobile-technology-evaluation.md`](architecture/mobile-technology-evaluation.md) — racional Kotlin × Flutter/RN/KMP.
- [`architecture/cloud-deployment.md`](architecture/cloud-deployment.md) — staging provider-agnostic.
- [`architecture/cloud-options-evaluation.md`](architecture/cloud-options-evaluation.md) — opções de fornecedores avaliadas.

## Produto e requisitos

- [`product/scope-and-features.md`](product/scope-and-features.md) — core/importante/stretch e CUJs.
- [`requirements/requirements.md`](requirements/requirements.md) — RF/RNF e critérios de aceitação.

## Segurança, QA e operação

- [`security/threat-model.md`](security/threat-model.md) — trust boundaries e mitigação.
- [`quality/qa-strategy.md`](quality/qa-strategy.md) — testes, fault matrix e Golden E2E.
- [`devops/sdlc-and-ci-cd.md`](devops/sdlc-and-ci-cd.md) — Git/CI/release.
- [`roadmap/roadmap.md`](roadmap/roadmap.md) — fases do TCC.
- [`adr/README.md`](adr/README.md) — decisões arquiteturais aceitas/superseded.

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
