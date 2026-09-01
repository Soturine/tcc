# Estratégia de QA, Verificação e Validação

## Objetivo

Provar comportamento crítico do sistema com uma pirâmide de testes proporcional ao risco, sem depender exclusivamente de mocks ou testes manuais de última hora.

## Firmware

Extrair gradualmente componentes puros como:

- `FallDetector`;
- `FeatureExtractor`;
- `EventBuffer`;
- `DeviceConfig`.

Testar em host/native quando possível e complementar com HIL no ESP32 físico.

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
- transactional outbox;
- stress/recovery.

## Android

Cobrir:

- ViewModels/use cases/repositories;
- Compose UI;
- navegação/deep links;
- autenticação;
- cache/offline;
- FCM;
- provisioning;
- permissões;
- hardware físico quando necessário.

Matriz mínima em aparelho real:

```text
foreground
background
processo encerrado
Doze/economia de bateria
sem Internet
reconexão
permissão negada
Bluetooth desligado (quando aplicável)
font scaling
```

## Web

Adicionar cobertura que a baseline ainda não possuía:

- unit/component tests com Vitest/RTL ou equivalente;
- poucos E2E de fluxos administrativos/pesquisa críticos;
- acessibilidade automática + revisão manual.

## Virtual device

Criar `tools/virtual-device/` para simular:

- online/offline;
- telemetria;
- queda;
- SOS;
- falha de sensor;
- buffer/retry;
- duplicata de `event_uuid`;
- payload inválido;
- bateria reportada/estimada;
- reconnect storm controlado.

## Golden E2E

A jornada final que o TCC deve provar em hardware real:

```text
ESP32 físico
→ ensaio seguro de evento
→ firmware detecta
→ MQTT/TLS
→ broker
→ backend
→ transaction MySQL
→ alert + outbox
→ FCM
→ celular Android físico
→ usuário abre/acknowledges
→ alert_action + audit
```

## CUJs de regressão

- CUJ-01: fall → push → view → acknowledge.
- CUJ-02: network loss → buffer → reconnect → one logical alert.
- CUJ-03: tenant isolation.
- CUJ-04: app background/killed → push.
- CUJ-05: provisioning → pairing → device online.
- CUJ-06: backend restart after outbox commit → delivery resumes.

## Evidência de TCC

Guardar resultados reproduzíveis de:

- versões/SHAs usados;
- configuração do hardware;
- condições do ensaio;
- logs relevantes;
- métricas calculadas;
- datasets e scripts de análise;
- limitações e falhas observadas.

Não registrar números/percentuais antes da medição real.
