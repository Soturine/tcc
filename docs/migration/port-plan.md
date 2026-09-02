# Plano de Porte — `iot-fall-monitor` → `tcc`

## Objetivo

Transformar a auditoria em uma sequência executável sem big-bang rewrite.

**Origem auditada:** `Soturine/iot-fall-monitor` @ `09ad767b5e1615331d0da5c25fa469423759dc39`  
**Auditoria:** [`../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../audit/iot-fall-monitor-port-audit-2026-09-01.md)

O SHA acima é o observado na auditoria; no momento do porte deve ser confirmado novamente e registrado como origem efetiva.

## Registro da execução de 2026-09-01

- **Origem efetivamente importada:** `Soturine/iot-fall-monitor` @ `09ad767b5e1615331d0da5c25fa469423759dc39`;
- **Base do destino:** `Soturine/tcc` @ `3057b78d263133f1335b9f2aaf0b0158e6143b09`;
- **Merge de histórias não relacionadas:** `9daa8ec`;
- **Commits posteriores ao SHA auditado:** nenhum;
- **Estratégia:** merge com histórias não relacionadas, `README.md` e `AGENTS.md` resolvidos em favor das versões canônicas do TCC;
- **Evidência de reprodução:** [`baseline-validation-2026-09-01.md`](baseline-validation-2026-09-01.md).

A CI mínima foi implementada em workflows independentes. O checkpoint `aed3a266e384da7874a1c76376a1bfb25340976e` passou nos workflows de push e pull request para backend, web, firmware e segurança. O primeiro `dependency-review` identificou corretamente que o Dependency Graph estava desabilitado; o recurso foi habilitado e somente o job falho foi reexecutado com sucesso.

## Princípio

Não fazer:

```text
baixar ZIP
→ copiar tudo
→ reorganizar 100 arquivos
→ corrigir depois
```

Fazer:

```text
preservar lineage
→ baseline no novo repo
→ reproduzir checks
→ CI
→ mudanças por propriedade/contrato
→ refactors incrementais
```

## Estratégia Git

O porte deve preservar histórico do projeto anterior. A operação Git exata será escolhida no ambiente local/Codex com acesso a ambos os remotes, mas deve resultar em:

- histórico anterior alcançável/rastreável;
- SHA de origem documentado;
- nenhum force-push destrutivo;
- checkpoint/tag da baseline somente depois de validação;
- `main` remoto como fonte de verdade.

Se for necessário merge de histórias inicialmente não relacionadas por causa da criação antecipada do repo `tcc`, fazê-lo deliberadamente e documentar o merge, em vez de apagar a documentação já criada ou copiar arquivos sem história.

## Classificação de subsistemas

| Origem | Classificação | Destino/direção |
|---|---|---|
| `src/` + `include/` + `platformio.ini` | PRESERVE + REFACTOR | inicialmente preservar caminhos; migrar depois para `firmware/esp32/` apenas em mudança coerente |
| `backend/` | PRESERVE + REFACTOR | manter Node/Express/MySQL; modularizar por extração |
| `frontend/` | PRESERVE + REFACTOR | futuramente `apps/web/`; não mass-move na baseline |
| `database/schema.sql` | PRESERVE AS DEV BOOTSTRAP | não usar como upgrade; criar migrations |
| `database/seed.sql` | PRESERVE/REVIEW | dados demo somente; garantir ausência de dados pessoais reais |
| backend migration scripts | MIGRATE | converter gradualmente para runner/versioned migrations |
| PowerShell root scripts | PRESERVE AS WINDOWS WRAPPER | adicionar caminhos cross-platform/CI |
| `docs/` canônicos antigos | REVIEW/ABSORB | consolidar conteúdo válido nos docs do TCC |
| demos/reports/screenshots/INIC | LEGACY EVIDENCE | `docs/legacy/project-ii/` ou referência histórica |
| `.github/pull_request_template.md` | PRESERVE/EVOLVE | adicionar workflows e políticas do TCC |
| backend tests | PRESERVE + EXTEND | characterization asset |
| frontend no-tests | PRESERVE + ADD TESTS | Vitest/RTL + poucos E2E |
| firmware test placeholder | REPLACE/EXTEND | host/native tests + HIL |

## Fase M0 — Reconhecimento

Antes de qualquer merge:

1. confirmar HEAD de `iot-fall-monitor/main`;
2. confirmar HEAD de `tcc/main`;
3. registrar ambos no log da tarefa;
4. comparar mudanças ocorridas depois da auditoria;
5. confirmar que não há secrets/dados sensíveis a migrar;
6. verificar arquivos binários grandes e sua real necessidade no histórico do TCC.

## Fase M1 — Integrar lineage

Resultado esperado:

```text
tcc main
├── história/documentação TCC já criada
└── história do Projeto II preservada/alcançável
```

Não reorganizar código nesta etapa.

Commit deve tratar apenas de lineage/merge quando possível.

## Fase M2 — Reproduzir a baseline

Sem alterar lógica:

### Backend

- instalar dependências;
- rodar suíte completa existente;
- integration tests;
- MQTT tests;
- stress dry;
- registrar resultados.

### Web

- lint;
- build;
- smoke conforme possível.

### Firmware

- PlatformIO compile;
- flash/HIL somente quando necessário.

### Integração

- MySQL real;
- Mosquitto real;
- smoke API/auth/tenant/MQTT.

Se algum check histórico falhar, corrigir/reconciliar antes de chamar a baseline de verde.

## Fase M3 — CI mínima

Criar workflows pequenos, não um mega workflow:

```text
backend.yml
web.yml
firmware.yml
security.yml          # pode crescer depois
```

Contratos entram quando existirem.

Requisitos:

- caching apropriado sem cachear secrets;
- cancelamento de runs superseded quando útil;
- versões de runtime explícitas;
- artifacts só quando agregarem valor;
- checks remotos no SHA.

## Fase M4 — Scripts cross-platform

Manter PowerShell, mas criar uma experiência canônica que também funcione em Linux/CI.

Possível direção:

```text
npm scripts por workspace/subproject
Docker Compose para MySQL/Mosquitto
scripts Node/Python pequenos quando necessário
```

Não adicionar monorepo orchestrator pesado só para resolver comandos.

## Fase M5 — Contratos da baseline

Antes de mudar payload:

1. capturar exemplos reais válidos de status/telemetry/event;
2. criar JSON Schemas de caracterização;
3. inventariar tópicos;
4. gerar OpenAPI inicial a partir das rotas reais;
5. criar contract tests;
6. só depois introduzir versões/ACKs novos.

## Fase M6 — Banco/migrations

Primeiro estabilizar mecanismo:

```text
database/migrations/
runner/history
```

Depois migrations funcionais:

1. colunas/índice `event_uuid` com backfill seguro;
2. campos temporais/evidence metadata necessários;
3. mobile sessions/installations;
4. notification outbox;
5. device/config/credentials conforme feature real.

Não criar dez tabelas futuras de uma vez.

## Fase M7 — Critical Event Reliability

Esta é a primeira grande mudança comportamental.

Separar em commits/spikes:

1. characterization tests da fila atual;
2. `EventIdentity` robusta;
3. spike ESP-MQTT;
4. persistent outbox state machine;
5. event ACK schema/topic;
6. backend commit→ACK;
7. offline evidence;
8. failure tests;
9. remover comportamento antigo só depois da equivalência/migração.

### Compatibilidade transitória

Durante migração, backend pode precisar aceitar evento schema legado e novo. Essa janela deve ser explícita e removida quando todo firmware suportado migrar.

## Fase M8 — Device Trust

- Mosquitto auth/ACL local de integração;
- identity mapping;
- reject topic/payload mismatch;
- credential lifecycle;
- TLS staging;
- tests negativos.

Não abrir broker na Internet antes deste gate.

## Fase M9 — Backend extraction

Agora os grandes services podem ser extraídos ao redor de contratos estáveis.

Prioridade:

```text
mqttIngestionService
→ critical-event handling/device identity

eventService
→ evidence/dedupe/query modules

deviceService
→ config/status/assignment/command responsibilities
```

Cada extração:

- characterization tests antes;
- mesmo comportamento observável;
- commit pequeno;
- remote validation.

## Fase M10 — Web migration

Não mover e refatorar simultaneamente.

1. adicionar testes básicos onde haverá mudança;
2. estabilizar API client/contract usage;
3. quebrar páginas grandes;
4. somente depois mover para `apps/web/` se ainda fizer sentido.

Web passa explicitamente a Admin/Research Console.

## Fase M11 — Android

Criar `apps/android/` depois de API/event reliability estáveis.

Primeiro REST read/actions; depois FCM; depois Protection Health; depois provisioning. Isso permite desenvolver mobile sem bloquear no hardware.

## Fase M12 — Provisioning

Executar como migração controlada:

```text
portal legado/recovery continua
+ Unified Provisioning novo
→ Android onboarding usa novo
→ validar recovery
→ remover/limitar inputs sensíveis do portal se apropriado
```

## Docs legados

Criar índice com metadados:

```text
docs/legacy/project-ii/README.md
```

Cada documento histórico deve indicar:

- origem/SHA;
- período;
- finalidade;
- `historical`, `evidence`, `superseded` ou `still-relevant`;
- documento canônico novo quando houver.

Isso evita uma IA/agente ler `quickstart-windows.md` ou arquitetura antiga e tratá-la como decisão atual.

## Arquivos grandes/binaries

O artigo INIC e screenshots podem ter valor histórico, mas binários grandes não devem virar padrão para novos artefatos do repo. Para novos datasets/releases grandes, usar artifact/release/storage apropriado e manter metadados/checksum no Git quando necessário.

Não reescrever histórico apenas para remover um arquivo grande sem necessidade crítica.

## Definition of Done do porte

O porte estrutural inicial termina quando:

- lineage preservada;
- docs TCC continuam canônicos;
- baseline histórica compila/testa no novo repo;
- CI mínima está verde;
- nenhum secret foi introduzido;
- comandos funcionam em CI/Linux e continuam utilizáveis no Windows;
- código ainda se comporta como baseline antes das mudanças P1/P2;
- status do `README` reflete a realidade.

Depois disso começa a evolução funcional, não mais “o porte”.
