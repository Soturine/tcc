# Contratos HTTP e MQTT

## Objetivo

Evitar que firmware, backend, Android e Web evoluam por acordos implícitos.

## HTTP

OpenAPI será a fonte canônica do contrato HTTP quando formalizado:

```text
contracts/openapi/openapi.yaml
```

Princípios:

- versionamento explícito quando necessário;
- exemplos válidos;
- erros estruturados;
- autorização documentada;
- paginação/filtros consistentes;
- breaking changes detectados em CI após estabilização.

O app Android e o frontend React são clientes pares da mesma API.

## MQTT

Schemas alvo:

```text
contracts/mqtt/
├── event.schema.json
├── telemetry.schema.json
├── status.schema.json
├── config-command.schema.json
└── config-ack.schema.json
```

## Tópicos

Os nomes finais devem partir dos tópicos já existentes e ser auditados antes de alteração. A semântica deve distinguir pelo menos:

- eventos críticos;
- telemetria;
- status/LWT;
- comandos/configuração;
- acknowledgments.

Evitar colocar autorização apenas no payload; ACL do broker deve restringir tópicos por credencial/dispositivo.

## Envelope de evento crítico

Conceitualmente deve conter identidade e rastreabilidade suficientes:

```json
{
  "schema_version": 1,
  "event_uuid": "...",
  "event_sequence": 123,
  "device_id": "...",
  "event_type": "fall",
  "device_timestamp": "...",
  "algorithm_version": "...",
  "data": {}
}
```

O exemplo é estrutural; campos/tipos finais devem refletir a baseline real antes de virar schema oficial.

## Idempotência

- retransmissão preserva `event_uuid`;
- backend valida uniqueness;
- processamento repetido retorna resultado lógico consistente;
- ações HTTP críticas devem usar mecanismos de idempotência quando houver risco real de retry duplicado.

## Configuração remota

Comando deve carregar:

- versão desejada;
- campos alterados/configuração;
- correlation/command id;
- timestamp/expiração quando aplicável.

ACK deve carregar:

- command/correlation id;
- versão reportada;
- aplicado/rejeitado;
- motivo estruturado quando rejeitado.

## Compatibilidade

Mudança em contrato deve responder:

1. firmware antigo continuará funcionando?
2. app antigo continuará funcionando?
3. backend aceita versões anteriores?
4. migration/deprecation é necessária?

Não aumentar `schema_version` por toda mudança interna; somente quando o contrato externo exigir.
