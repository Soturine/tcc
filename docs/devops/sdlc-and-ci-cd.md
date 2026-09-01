# SDLC, Git, CI/CD e Release

## Fluxo de trabalho

Kanban leve:

```text
Backlog → Ready → In Progress → Remote Validation → Done
```

WIP baixo. Cada item deve possuir critérios de aceitação e evidência compatível com o risco.

## Commits

Preferir commits lógicos, pequenos e reversíveis. Após teste focado, fazer push real e usar a CI remota como checkpoint assíncrono.

## CI alvo

### Backend

- install determinístico;
- lint/check;
- unit tests;
- integration tests com MySQL;
- integration MQTT com Mosquitto;
- migrations up/down ou validação equivalente;
- contract tests.

### Android

- formatting/lint;
- `./gradlew test`;
- Compose/UI tests adequados;
- build Android debug/release candidate.

### Web

- lint;
- typecheck;
- unit/component tests;
- build;
- E2E crítico em milestone.

### Firmware

- PlatformIO native tests para lógica pura;
- compile para board ESP32 alvo;
- static analysis quando configurada.

### Contracts

- lint OpenAPI;
- validação de exemplos;
- breaking-change detection quando estável;
- JSON Schema para payloads MQTT.

### Security

- CodeQL;
- dependency/SCA review;
- secret scanning;
- container scan quando imagens forem introduzidas;
- Dependabot sem auto-merge cego.

## Concorrência de CI

Runs superseded podem ser cancelados quando seguro para reduzir desperdício. Uma CI verde antiga não valida um SHA novo.

## Ambientes

- local: desenvolvimento/testes rápidos;
- staging: cloud real e integração física;
- production: somente se houver necessidade posterior ao escopo acadêmico.

## Deployment de staging

Meta futura:

```text
merge/push em main
→ CI completa necessária
→ artifact/container identificado pelo SHA
→ deploy staging
→ migrations controladas
→ health/readiness
→ smoke test
→ marcar SHA conhecido como verde
```

Não automatizar deploy antes de haver rollback e validação básica.

## Release

Antes de release/tag:

1. `HEAD == origin/main`;
2. CI verde no SHA exato;
3. migrations testadas;
4. artefatos gerados do SHA;
5. smoke E2E relevante;
6. changelog/documentação atualizados;
7. vulnerabilidades High/Critical avaliadas;
8. rollback conhecido;
9. tag somente depois da comprovação.

Meta acadêmica eventual: `v1.0.0-tcc` ou versão SemVer equivalente definida perto da entrega.

## Observabilidade pós-deploy

Verificar:

- backend health/readiness;
- conexão com MySQL;
- conexão MQTT;
- tamanho/idade da outbox;
- erro de push;
- espaço em disco;
- CPU/memória;
- smoke de login/API.

## Infraestrutura como código

Introduzir IaC apenas quando a cloud estiver definida. Para uma única VM, scripts declarativos/Ansible/Terraform podem ser avaliados, mas não são obrigatórios se aumentarem mais complexidade que reprodutibilidade. Docker Compose + scripts idempotentes podem ser suficientes para o TCC.
