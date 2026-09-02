# JSON Schemas MQTT

| Schema | Estado | Finalidade |
|---|---|---|
| `status.schema.json` | current | payload produzido pelo firmware em `/status` |
| `telemetry.schema.json` | current | amostra válida produzida em `/telemetry` |
| `event.schema.json` | current | eventos produzidos em `/events` |
| `critical-event-v1.schema.json` | planned | envelope confiável alvo da próxima evolução |
| `critical-event-ack-v1.schema.json` | planned | ACK pós-commit correlacionado por UUID |

Schemas `current` descrevem o publisher ESP32 atual. O backend aceita um subconjunto legado mais permissivo; essa compatibilidade não torna campos omitidos recomendados. Ausência de `schema_version` representa o perfil implícito `0`. Os schemas v1 planejados exigem `schema_version: 1`.

Exemplos em `examples/` são executáveis pelos contract tests. `additionalProperties: true` é deliberado para permitir adições compatíveis; campos obrigatórios, tipos, unidades e versões continuam validados.
