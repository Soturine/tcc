# Estratégia de QA, Verificação e Validação

## Objetivo

Provar comportamento crítico do sistema com testes proporcionais ao risco, sem depender exclusivamente de mocks ou validação manual de última hora.

A prioridade de QA é demonstrar **propriedades**, não coverage cosmético:

- um evento crítico não some;
- retry não duplica efeito lógico;
- identidade do device não pode ser forjada pelo payload;
- offline/reboot não reescreve a história do evento;
- push e ação mobile funcionam nos estados reais do Android;
- falha vira estado observável, não sucesso fictício.

## Gate de baseline antes do porte

Reexecutar no código importado, sem assumir os resultados históricos:

- backend check/test;
- backend integration/MQTT suites existentes;
- stress dry existente;
- frontend lint/build;
- PlatformIO build;
- smoke com MySQL/Mosquitto reais;
- validações físicas que forem reproduzíveis com segurança.

A documentação v0.9.0 registra resultados verdes em junho de 2026, mas isso é evidência histórica, não substituto do novo run no SHA do TCC.

## Firmware

Extrair gradualmente componentes puros:

- `FallDetector`;
- `FeatureExtractor`;
- `CriticalEventOutbox`;
- `EventIdentity`;
- `EventPayloadCodec`;
- `DeviceConfig`;
- command/config state machine.

### Testes host/native

Cobrir:

- FSM e thresholds;
- feature extraction;
- UUID/boot ID uniqueness strategy;
- queue/outbox FIFO e overflow;
- persist/restore após reboot simulado;
- retry/backoff;
- ACK correto, ACK duplicado, ACK desconhecido;
- payload/schema serialization;
- config validation/rollback;
- duplicate/replayed command.

### HIL / ESP32 real

Cobrir:

- boot sem NTP;
- Wi‑Fi connect/reconnect;
- MQTT/TLS certificate validation;
- QoS 1 real no cliente escolhido;
- reboot com critical event pendente;
- NVS restore;
- MPU6050/I2C recovery;
- watchdog/loop responsiveness;
- buzzer/SOS;
- provisioning;
- comportamento com broker/backend indisponíveis.

## Backend

Cobrir:

- unitários de regras puras;
- integração com MySQL real;
- integração MQTT real com Mosquitto;
- migrations;
- contratos HTTP/MQTT;
- autorização e isolamento multi-tenant;
- idempotência/deduplicação;
- state machine de alertas;
- transactional notification outbox;
- application ACK;
- mobile sessions/refresh/revocation;
- rate limiting;
- stress/recovery.

### Casos adversariais essenciais

```text
same event_uuid twice
same event_uuid after backend restart
same event_uuid with conflicting payload
topic device != payload device
unauthorized topic/device
unknown schema_version
stale/replayed command
invalid JSON
oversized payload
DB transient failure
broker disconnect
worker restart
notification provider transient failure
cross-tenant object ID
```

`event_uuid` repetido com payload materialmente conflitante deve ser diagnosticado; idempotência não pode mascarar corrupção/ataque.

## Offline fall test

Teste obrigatório específico:

```text
1. ESP32 online e saudável
2. cortar Internet/broker
3. gerar evento controlado seguro
4. confirmar evento pendente na outbox local
5. manter telemetria server-side ausente para aquela janela
6. restaurar rede
7. mesmo event_uuid é reenviado
8. backend persiste exatamente um evento lógico
9. alerta é criado pela evidência edge válida
10. backend emite application ACK
11. outbox local remove somente após ACK
12. Android recebe push
```

Esse teste prova o CUJ mais importante da arquitetura edge-first.

## Android

Cobrir:

- ViewModels/use cases/repositories;
- Compose UI;
- navegação/deep links;
- autenticação/refresh/revogação;
- cache/offline/staleness;
- FCM;
- notification actions;
- Protection Health;
- provisioning;
- permissões;
- hardware físico quando necessário.

### Matriz mínima em aparelho real

```text
foreground
background
processo encerrado
Doze/economia de bateria
sem Internet
Internet retorna
reboot do celular
notification permission denied/re-enabled
lock screen preview
font scaling
TalkBack
Bluetooth desligado/negado (quando aplicável)
Wi-Fi troca de rede durante provisioning
FCM token rotation
sessão revogada enquanto app está aberto
```

### Protection Health tests

Induzir falhas uma a uma e verificar que UI muda corretamente:

- device offline;
- sensor invalid;
- outbox crítica acumulando;
- config drift;
- notification permission off;
- FCM registration ausente;
- backend indisponível;
- dado stale.

Também testar recuperação para evitar indicador preso em estado degradado.

## Web

A baseline histórica possui lint/build, mas não suíte de testes frontend. Ao portar/evoluir:

- Vitest + React Testing Library ou equivalente;
- testes de auth/tenant/alert actions mais importantes;
- poucos E2E de administração/pesquisa;
- acessibilidade automática + revisão manual;
- não duplicar E2E mobile sem necessidade.

## Contracts

Criar testes automatizados para:

- exemplos OpenAPI;
- schemas MQTT válidos/invalids;
- compatibilidade de versão;
- required fields de critical event;
- event ACK;
- config command/ACK;
- topic/payload mismatch;
- payload size budget quando definido.

## Migrations e banco

Para cada migration relevante:

1. aplicar em banco vazio quando parte do bootstrap;
2. aplicar sobre snapshot/schema da baseline suportada;
3. executar testes/invariantes;
4. verificar índices/constraints;
5. validar backup/restore para mudanças de risco;
6. não usar `schema.sql` destrutivo como mecanismo de upgrade.

## Virtual device

Criar `tools/virtual-device/` para simular:

- online/offline;
- telemetria;
- queda/SOS;
- sensor fault;
- pending/retry/duplicate;
- application ACK perdido;
- backend restart;
- payload inválido;
- topic/payload mismatch;
- clock skew/offline replay;
- bateria reportada/estimada;
- firmware/schema versions diferentes;
- reconnect storm controlado.

O simulador não substitui HIL; serve para carga, contrato e fault injection reprodutível.

## Golden E2E

A jornada final que o TCC deve provar em hardware real:

```text
ESP32 físico
→ ensaio seguro de evento
→ event_uuid + evidência edge
→ critical-event outbox
→ MQTT/TLS QoS 1
→ broker
→ backend autentica/valida
→ transaction MySQL
→ event/alert + notification outbox
→ COMMIT
→ application ACK → ESP32
→ FCM
→ Android físico
→ usuário abre/acknowledges
→ alert_action + audit
```

Registrar SHAs, configurações e timestamps t0..t5.

## CUJs de regressão

- **CUJ-01:** fall → commit/ACK → push → view → acknowledge.
- **CUJ-02:** network loss → persistent outbox → reconnect → one logical alert.
- **CUJ-03:** tenant isolation.
- **CUJ-04:** app background/killed → push.
- **CUJ-05:** secure provisioning → pairing → device online.
- **CUJ-06:** backend restart after commit → notification resumes.
- **CUJ-07:** Protection Health detects/reports degradation.
- **CUJ-08:** test alert verifies push path without fall.

## Fault matrix

Antes da release final, executar pelo menos cenários controlados de:

| Falha | Comportamento esperado |
|---|---|
| Wi‑Fi device cai | event permanece local; health degrada |
| broker cai | reconnect/backoff; event permanece pendente |
| backend cai | broker/session/outbox preservam caminho conforme contrato; sem ACK falso |
| MySQL cai | backend não ACKa evento não commitado |
| notification worker cai | notification outbox retoma depois |
| FCM falha temporariamente | retry observável/limitado |
| Android sem permissão | Protection Health alerta; backend continua registrando evento |
| device reboot pendente | outbox restaura e reenvia mesmo UUID |
| duplicate delivery | um evento/alerta lógico |
| topic spoof/mismatch | mensagem rejeitada/auditada |

## Performance e resource budgets

Medir antes de definir meta. Coletar quando aplicável:

- device→backend receive;
- backend transaction;
- commit→push submit;
- app observed;
- human acknowledge;
- MQTT reconnect/recovery;
- memória/heap do ESP32;
- tamanho de payload crítico;
- tamanho/idade de outbox;
- DB growth de telemetria;
- CPU/memória da VM.

Analisar p50/p95/p99 quando houver amostra suficiente. Não transformar poucas execuções em SLA.

## Evidência de TCC

Guardar resultados reproduzíveis de:

- versões/SHAs;
- hardware e montagem realmente usados;
- config/schema/firmware versions;
- condições do ensaio;
- logs relevantes;
- dados brutos;
- scripts de análise;
- métricas calculadas;
- datasets/licenças;
- limitações/falhas/anomalias.

Não registrar números/percentuais antes da medição real.
