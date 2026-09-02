# Contratos HTTP e MQTT

## Objetivo

Evitar que firmware, backend, Android e Web evoluam por acordos implícitos. Contratos críticos devem expressar **identidade, versão, idempotência, tempo, evidência e confirmação**.

O comportamento `current` e os artefatos executáveis estão em [`docs/contracts`](../contracts/README.md). As seções de envelope v1, ACK, commands e configuração abaixo continuam direção `planned` até seus componentes existirem.

## HTTP

O inventário e o OpenAPI implementados são canônicos em:

```text
docs/contracts/http-api.md
docs/contracts/openapi.yaml
```

Princípios:

- versionamento explícito quando necessário;
- exemplos válidos;
- erros estruturados com códigos estáveis;
- autorização documentada;
- paginação/filtros consistentes;
- idempotency key quando uma ação crítica puder ser repetida por retry;
- breaking changes detectados em CI após estabilização;
- Android e React são clientes pares da mesma API.

## MQTT

Schemas current e planejados, com estado explícito:

```text
docs/contracts/mqtt/
├── event.schema.json
├── critical-event-v1.schema.json
├── critical-event-ack-v1.schema.json
├── telemetry.schema.json
└── status.schema.json
```

Config command/ACK permanecem planejados e ainda não possuem payload real suficiente para schema.

## Identidade MQTT

Em ambiente externo:

```text
MQTT authenticated principal
+ broker ACL
+ authorized topic
= authoritative device identity
```

O payload não escolhe outro device. `device_id`/`device_uid` dentro do JSON serve como redundância de verificação e telemetria de diagnóstico. Divergência deve gerar rejeição/quarentena e auditoria.

## Tópicos

Os nomes finais devem partir da baseline e ser migrados de forma compatível. A semântica deve distinguir pelo menos:

- eventos críticos;
- ACK de evento crítico;
- telemetria;
- status/LWT;
- comandos/configuração;
- ACK de comando.

ACL do broker restringe tópicos por credencial/dispositivo. Autorização nunca vive apenas no payload.

## Envelope de evento crítico

Direção conceitual; campos finais precisam ser confirmados contra a baseline antes de virar schema estável:

```json
{
  "schema_version": 1,
  "event_uuid": "...",
  "event_sequence": 123,
  "boot_id": "...",
  "device_uptime_ms": 123456,
  "device_id": "...",
  "event_type": "fall_detected",
  "occurred_at_device": "...",
  "clock_quality": "synced",
  "algorithm_version": "...",
  "config_version": 12,
  "evidence": {
    "decision_source": "edge",
    "features": {},
    "sample_bundle": null
  }
}
```

Regras:

- `event_uuid` nasce uma vez e sobrevive a retries/reboots enquanto pendente;
- identidade não depende somente de wall clock;
- `occurred_at_device` não é substituído por `received_at`;
- backend adiciona seu próprio `received_at` ao persistir;
- `clock_quality` explicita se tempo do device era confiável;
- evidência local suficiente deve permitir replay offline sem depender exclusivamente de telemetria SQL.

## ACK de evento crítico

O ACK de aplicação só pode ser emitido após o backend completar a persistência necessária.

Exemplo conceitual formalizado como contrato `planned`:

```json
{
  "schema_version": 1,
  "event_uuid": "...",
  "status": "committed",
  "event_id": 123,
  "committed_at": "..."
}
```

O device deve tolerar ACK repetido. O backend deve retornar a mesma identidade lógica em retry do mesmo `event_uuid`.

**Importante:** PUBACK MQTT QoS 1 confirma o broker; `event-ack` confirma a aplicação/backend.

## Idempotência

### Device event

- retransmissão preserva `event_uuid`;
- banco possui uniqueness explícita;
- processamento repetido resolve para o mesmo evento lógico;
- alert/notification creation não duplica.

### HTTP actions

Ações como acknowledge/cancel/resolve e ações vindas de notificação devem ser seguras sob double-tap/retry. Usar state transition invariants e, quando necessário, idempotency/action IDs.

### Commands

Comando deve carregar ID único e versão desejada. Device deve poder responder novamente a comando duplicado sem aplicar efeito cumulativo indevido.

## Evidência

Separar origem:

```text
device
server_telemetry
both
none
```

`server_telemetry` enriquece, mas não pode ser a única condição para aceitar um `fall_detected` localmente confirmado quando o evento ficou offline.

## Configuração remota

Comando:

- `schema_version`;
- `command_id`;
- desired config version;
- campos/configuração;
- timestamp/expiração quando aplicável.

ACK:

- `command_id`;
- versão reportada;
- aplicado/rejeitado/duplicate;
- motivo estruturado;
- firmware/protocol version quando útil.

## Status e health

Status deve permitir compor Protection Health sem afirmar garantia clínica. Campos candidatos:

- firmware/protocol version;
- sensor state;
- sample age;
- Wi-Fi/MQTT state;
- last backend application ACK;
- critical outbox depth/oldest age;
- desired/reported config versions;
- battery value + source;
- device uptime/boot ID.

## Compatibilidade

Toda mudança externa deve responder:

1. firmware antigo continua funcionando?
2. app antigo continua funcionando?
3. backend aceita versão anterior durante migração?
4. broker ACL precisa mudar?
5. migration/deprecation é necessária?
6. mudança altera significado de um evento já persistido?

Não aumentar `schema_version` por mudança interna sem impacto no contrato.

## Contract tests

A CI já valida OpenAPI, cobertura das 35 operações registradas, schemas/exemplos MQTT, campos obrigatórios, versões incompatíveis e budget representativo. A evolução de confiabilidade deverá acrescentar:

- mismatch topic/payload;
- duplicate event;
- ACK perdido/repetido;
- command duplicate/replay;
- versão desconhecida;
- campos obrigatórios de critical event.
