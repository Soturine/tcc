# JSON Schemas MQTT

| Schema | Estado | Finalidade |
|---|---|---|
| `status.schema.json` | current | payload produzido pelo firmware em `/status` |
| `telemetry.schema.json` | current | amostra válida produzida em `/telemetry` |
| `event.schema.json` | current | eventos produzidos em `/events` |
| `critical-event-v1.schema.json` | planned | envelope confiável alvo da próxima evolução |
| `critical-event-ack-v1.schema.json` | implemented | ACK pós-commit correlacionado por UUID (devices sem `ingest_result_version`) |
| `critical-event-result-v1.schema.json` | implemented | resultado de ingestão `committed/duplicate/retryable/rejected_permanent` com `device_id` e `payload_sha256`; negativos nunca confirmam persistência (ADR-026) |

Schemas `current` descrevem o publisher ESP32 atual. O backend aceita um subconjunto legado mais permissivo; essa compatibilidade não torna campos omitidos recomendados. Ausência de `schema_version` representa o perfil implícito `0`. Os schemas v1 planejados exigem `schema_version: 1`.

Exemplos em `examples/` são executáveis pelos contract tests. `additionalProperties: true` é deliberado para permitir adições compatíveis; campos obrigatórios, tipos, unidades e versões continuam validados.
