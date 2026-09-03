# Identidade do evento crítico e ACK de aplicação

## Estado atual

O firmware cria `event_uuid` antes da primeira tentativa de publish em `buildCriticalEventUuid()`:

```text
{device_uid}-{event_type}-{timestamp_seconds}-{millis}-{event_sequence}
```

O mesmo payload, inclusive UUID, é preservado no buffer e nos retries. `event_sequence` é um contador RAM reiniciado no boot; `sample_seq` também é local ao boot. O UUID inclui eFuse UID, tipo, wall clock/fallback, uptime e sequência, mas **não possui `boot_id` persistente ou aleatório**. Portanto, a robustez contra colisão após reboot precoce/sem NTP não está demonstrada.

O buffer atual:

- fila circular RAM com capacidade 10, política `drop_oldest`;
- snapshot NVS de no máximo 4 eventos, com escrita adiada;
- restauração do snapshot no boot;
- remoção assim que `PubSubClient.publish()` retorna sucesso local.

Isso é uma fila persistida parcial, não a outbox confiável alvo. `publish()` não prova PUBACK nem commit no MySQL.

No backend, `event_uuid` também é materializado em `events.event_uuid`, com unicidade global no banco. A ingestão valida o UUID recebido, persiste a identidade estruturada e trata a restrição `UNIQUE` como autoridade inclusive sob concorrência entre transações ou instâncias.

Um retry com o mesmo UUID e a mesma identidade crítica retorna o evento lógico existente e não repete o alerta. Reutilizar o UUID com device, tipo ou evidência crítica divergente resulta em `EVENT_UUID_CONFLICT`; o evento anterior não é sobrescrito e o payload conflitante não é aceito como sucesso. Logs registram somente identificadores e nomes dos campos divergentes, sem despejar o payload completo.

Eventos legados sem UUID continuam aceitos com `event_uuid = NULL` e não recebem garantia de deduplicação. A migration `001_event_identity` recupera somente UUIDs legados válidos e únicos; ausentes ou inválidos permanecem nulos, sem identidade fabricada. O procedimento operacional está em [`database/migrations/README.md`](../../database/migrations/README.md).

## Contrato v1 planejado

O schema [critical-event-v1.schema.json](mqtt/critical-event-v1.schema.json) é uma direção imediatamente necessária, não um payload já emitido. Ele exige:

- `schema_version: 1`;
- `event_uuid` opaco e estável;
- `device_id` redundante ao tópico;
- `boot_id` para distinguir boots;
- `device_uptime_ms` monotônico;
- `event_type` crítico;
- `occurred_at_device` nullable;
- `clock_quality` explícito;
- `detector_version`, `mode` e evidência compacta;
- `confidence_status`, sem probabilidade fabricada.

`event_uuid` deve nascer uma vez, ser persistido antes da primeira transmissão e nunca mudar em retry, reconnect ou reboot. A materialização e unicidade no backend estão **implemented** e cobertas por testes; a geração robusta a reboot e a outbox persistente do device continuam **planned**.

## ACK de aplicação planejado

Topic proposto e reservado nesta formalização:

```text
queda/devices/{device_id}/critical-event-acks
```

Fluxo obrigatório:

```text
device event(event_uuid)
  -> broker
  -> backend valida identidade/schema
  -> transação persiste evento e efeitos idempotentes
  -> COMMIT MySQL
  -> backend publica ACK(event_uuid)
  -> device valida tópico/UUID e remove da outbox
```

O payload está em [critical-event-ack-v1.schema.json](mqtt/critical-event-ack-v1.schema.json). Semântica:

- publisher: backend;
- subscriber: somente o device autorizado;
- condição: ACK apenas depois do commit bem-sucedido;
- correlação: `event_uuid` obrigatório; `event_id` identifica o registro server-side;
- status: `committed` para primeira persistência ou `duplicate` quando o mesmo UUID já estava committed;
- duplicata de evento: backend não repete efeitos, mas republica ACK com o mesmo `event_id` lógico;
- ACK perdido: device mantém evento e retransmite o mesmo UUID; não cria UUID novo;
- ACK duplicado: device trata idempotentemente;
- ACK desconhecido ou de outro tópico/device: device ignora e registra diagnóstico;
- retain: false; QoS alvo 1, a validar com o transporte futuro.

Este contrato não está implementado nesta etapa. Não existe subscriber de ACK no firmware nem publisher correspondente no backend.

## Compatibilidade

- ausência de `schema_version` continua significando payload current/legacy implícito 0 durante a migração;
- eventos antigos sem `event_uuid` continuam ingestíveis, mas não recebem a garantia v1;
- campos novos opcionais podem ser adicionados a v1 sem mudar significado;
- remover/renomear campo obrigatório ou mudar unidade/semântica exige nova versão;
- versão desconhecida deve ser rejeitada/quarentenada de forma observável.
