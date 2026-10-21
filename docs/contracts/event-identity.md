# Identidade do evento crítico e ACK de aplicação

## Estado atual (2026-10-21)

### Firmware

`event_uuid` é gerado por `EventIdentity::Generator` (`src/event_identity.cpp`) **antes** do payload ser montado e gravado na outbox:

```text
event_uuid = {device_uid}:{boot_id}:{sequência hex8}
boot_id    = b{contador de boot persistido hex8}-{nonce de hardware hex8}
```

- o contador de boot é incrementado e gravado em NVS no início de cada boot; falha de gravação é sinalizada em `boot_id_counter_persisted=false`;
- o nonce vem de `esp_random()` e cobre boots com o mesmo contador (persistência falhou ou NVS apagada);
- a sequência é por boot e nunca reutiliza 0;
- não depende de NTP nem de `millis()` (ADR-017). Testes adversariais: `test/test_critical_delivery` (50 boots sem relógio, falha do contador, limite de 160 caracteres).

`CriticalEventOutbox` (`src/critical_outbox.cpp`) persiste cada evento em LittleFS antes da primeira tentativa e o mantém até o ACK de aplicação:

- capacidade 16 itens, payload ≤ 3.800 bytes; outbox cheia recusa o **novo** evento e incrementa `critical_outbox.overflow_count` (nada já aceito é descartado);
- retry com backoff exponencial de 2 s a 60 s + jitter, por item (um item em backoff não bloqueia os demais);
- reboot restaura os itens na ordem de criação e reenvia imediatamente com o mesmo UUID;
- falha de gravação mantém o item em RAM (`not_persisted`) e é re-tentada; arquivo corrompido é detectado por CRC32 e quarentenado como `.bad`;
- eventos gravados pelo buffer legado em NVS são migrados com o UUID original; as chaves antigas só são apagadas se todos forem gravados.

Limitações: transporte device→broker ainda QoS 0 (ADR-020); sem validação em ESP32 físico (HIL pendente); capacidade física finita.

### Backend

`event_uuid` é materializado em `events.event_uuid` com unicidade global. A constraint `UNIQUE` é a autoridade inclusive sob concorrência. Retry com o mesmo UUID e a mesma identidade crítica retorna o evento lógico existente sem repetir alerta; reutilização com dados críticos divergentes resulta em `EVENT_UUID_CONFLICT` e o evento anterior não é sobrescrito.

Eventos legados sem UUID continuam aceitos com `event_uuid = NULL`, sem garantia de deduplicação e sem ACK.

## ACK de aplicação (implementado)

Tópico: `queda/devices/{device_id}/critical-event-acks`. Payload: [critical-event-ack-v1.schema.json](mqtt/critical-event-ack-v1.schema.json).

```text
device persiste event(event_uuid) na outbox
  -> publish
  -> backend valida tópico/identidade/UUID
  -> transação: evento + alerta (idempotentes)
  -> COMMIT MySQL
  -> backend publica ACK {event_uuid, event_id, status, committed_at} (QoS 1, retain=false)
  -> device valida tópico/schema e remove o item da outbox
```

| Situação | Comportamento verificado |
|---|---|
| primeira persistência | ACK `committed` |
| retry / MQTT duplicado | nenhum novo evento ou alerta; ACK `duplicate` com o mesmo `event_id` |
| ACK perdido | device retransmite o mesmo UUID e recebe `duplicate` |
| ACK duplicado / UUID desconhecido | device ignora de forma idempotente e conta em diagnóstico |
| rollback, banco indisponível, falha de commit | nenhum ACK; device retransmite |
| payload conflitante, UUID inválido, `device_id` divergente do tópico | nenhum ACK; nada é sobrescrito |
| backend fora do ar ou reiniciado | mensagem pode se perder (sessão limpa); retry da outbox entrega após o retorno |
| ACK de outro device/tópico | firmware rejeita (`wrong_topic`) |

O tópico do ACK é derivado do `device_id` do **tópico** de origem, nunca do payload (ADR-015). Evidência: `backend/tests/mqtt/mqttIngestionService.test.js`, `backend/tests/contracts/contracts.test.js`, `test/test_critical_ack`, `test/test_critical_delivery` e o ensaio `npm run test:e2e:critical` ([registro](../quality/critical-delivery-validation.md)).

## Contrato v1 planejado

O schema [critical-event-v1.schema.json](mqtt/critical-event-v1.schema.json) continua sendo a direção para um envelope estrito e compacto. O firmware atual envia o payload current (perfil implícito 0) acrescido de `boot_id`, `device_uptime_ms`, `clock_quality`, `occurred_at_device` e `local_evidence`, que o backend já interpreta.

## Compatibilidade

- ausência de `schema_version` continua significando payload current/legacy implícito 0 durante a migração;
- eventos antigos sem `event_uuid` continuam ingestíveis, mas não recebem a garantia v1 nem ACK;
- campos novos opcionais podem ser adicionados a v1 sem mudar significado;
- remover/renomear campo obrigatório ou mudar unidade/semântica exige nova versão;
- versão desconhecida deve ser rejeitada/quarentenada de forma observável.
