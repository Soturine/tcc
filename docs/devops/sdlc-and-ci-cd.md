# SDLC, Git, CI/CD e Release

## Fluxo de trabalho

Kanban leve:

```text
Backlog → Ready → In Progress → Remote Validation → Done
```

WIP baixo. Cada item possui critérios de aceitação e evidência proporcional ao risco.

## Regra introduzida pela auditoria

A baseline histórica `iot-fall-monitor` possui bons comandos/testes locais, mas não possui workflows GitHub Actions. O TCC **não herda essa lacuna**.

CI é infraestrutura da fase de fundação:

```text
import lineage
→ reproduce baseline locally
→ minimal remote CI
→ only then major refactors
```

A suíte cresce depois, mas não fica toda para uma fase tardia de QA.

## Commits e push

Preferir commits lógicos, pequenos e reversíveis.

Fluxo:

```text
focused check
→ commit
→ push real
→ remote CI checkpoint
→ próxima tarefa enquanto CI roda quando seguro
```

Se uma CI anterior ficar vermelha, corrigir cedo. Uma CI verde antiga nunca valida um SHA novo.

## CI incremental

### Stage 0 — logo após o porte

Backend:

- install determinístico;
- suíte existente;
- integration/MQTT existente quando ambiente estiver preparado.

Web:

- install;
- lint;
- build.

Firmware:

- PlatformIO build.

Security:

- secret scanning/dependency checks viáveis.

### Stage 1 — contratos e migrations

Adicionar:

- MySQL real em CI;
- Mosquitto real;
- migration tests;
- OpenAPI/schema validation;
- topic/payload identity tests;
- event ACK/duplicate/offline contracts.

### Stage 2 — Android

- Gradle wrapper validation;
- formatting/lint;
- JVM unit tests;
- Compose tests adequados;
- debug build;
- release candidate/signing checks apenas quando necessário, sem secrets em PRs não confiáveis.

### Stage 3 — maturidade

- web component tests;
- firmware native tests;
- CodeQL;
- SCA/dependency review;
- container scan;
- E2E seletivo;
- artifacts/SBOM/provenance quando proporcional à release.

## CI alvo por subsistema

### Backend

- install determinístico;
- check/lint quando houver;
- unit tests;
- integration tests com MySQL;
- MQTT integration com Mosquitto;
- migrations;
- contract/security tests;
- stress seletivo em milestone, não em todo commit se caro.

### Android

- format/lint;
- unit tests;
- Compose/UI tests adequados;
- build;
- instrumented tests em pipeline/device farm somente se benefício justificar custo/complexidade.

Testes de BLE/Doze/FCM continuam exigindo device físico/controlado em parte do ciclo.

### Web

- lint;
- typecheck;
- unit/component tests;
- build;
- poucos E2E críticos em milestone.

### Firmware

- native tests para lógica pura;
- compile para ESP32 alvo;
- static analysis quando configurada;
- HIL fora do runner genérico, com procedimento registrável.

### Contracts

- OpenAPI lint/examples;
- JSON Schema MQTT;
- compatibility/breaking checks quando contratos estabilizarem;
- payload size budget quando medido;
- generated-client check somente se geração for adotada.

### Security

- CodeQL onde suportado;
- SCA/dependency review;
- secret scanning;
- container scan;
- Dependabot sem auto-merge cego;
- High/Critical tratados cedo, com false positive/risco documentado quando aplicável.

## Concorrência e custo

- cancelar runs superseded quando seguro;
- evitar matriz redundante;
- cache com chaves corretas;
- não ocultar falha por `continue-on-error` em gate crítico;
- separar checks rápidos de suites de milestone quando necessário.

## Ambientes

```text
local
ci
staging
production (futuro, se existir)
```

### Local

Feedback rápido e HIL manual.

### CI

Ambiente efêmero reproduzível; MySQL/Mosquitto services quando necessário.

### Staging

Cloud real + ESP32/Android físicos + FCM + TLS.

### Production

Não existe por nomenclatura apenas; somente se o projeto sair do escopo acadêmico.

## Deployment de staging

Quando chegar a hora:

```text
green SHA
→ artifact/image identificado
→ backup/precondition quando migration exigir
→ deploy
→ migrations controladas
→ /live + /ready
→ post-deploy smoke
→ registrar SHA implantado
```

Não automatizar deploy antes de existir rollback/recovery básico.

## Database delivery

Migration é parte do artifact/release logicamente.

Antes de aplicar migration arriscada:

- compatibilidade do app/backend;
- backup quando necessário;
- teste de upgrade da baseline;
- plano de rollback/forward-fix;
- nenhum `schema.sql` destrutivo em staging como “migration”.

## Release

Antes de tag/release:

1. `HEAD == origin/main`;
2. CI verde no SHA exato;
3. migrations testadas;
4. artifacts gerados do SHA;
5. golden/smoke E2E relevante;
6. changelog/docs/status atualizados;
7. High/Critical avaliadas;
8. backup/restore e rollback conhecidos quando aplicáveis;
9. deployment config/secrets não estão no Git;
10. tag somente após comprovação.

Tags patch podem marcar marcos técnicos integrados quando a release for explicitamente aprovada e o SHA de `main` satisfizer os gates acima. Merge verde não cria autorização automática para uma nova tag; versão, mensagem e momento precisam estar registrados na tarefa/release. Não reescrever nem reposicionar tags publicadas.

Versão acadêmica final pode ser `v1.0.0-tcc` ou SemVer equivalente definida perto da entrega.

## Post-deploy verification

Verificar:

- `/live` e `/ready`;
- MySQL/schema;
- broker auth/TLS;
- backend MQTT subscription/session;
- critical application ACK path;
- notification outbox depth/age;
- FCM test path;
- devices offline;
- disk/CPU/memory;
- login/API;
- React console;
- Protection Health não mostra falso saudável.

## IaC

Uma única VM não exige Terraform/Kubernetes para ser profissional.

Começar com:

- Docker Compose/config declarativa;
- scripts idempotentes;
- backup/runbook;
- env/secrets documentados.

Avaliar Ansible/Terraform apenas se realmente reduzirem toil/reconstrução. Não transformar o TCC em exercício de cloud tooling.

## Supply chain

Na maturidade de release:

- lockfiles;
- actions pinning deliberado;
- dependências revisadas;
- secret scanning;
- artifact checksums;
- SBOM/provenance se proporcional;
- release notes referenciam SHA/tag real.
