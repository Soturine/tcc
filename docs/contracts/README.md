# Contratos canônicos

Esta área registra os contratos observados no código do TCC em 2026-09-02. O estado `current` descreve comportamento implementado; `planned` descreve somente a direção aprovada e não deve ser interpretado como disponível.

```text
ESP32 -- HTTP pairing/profile --> Backend -- SQL --> MySQL
  |
  +-- MQTT status/telemetry/events --> Broker --> Backend
                                                |
                                                +-- HTTP/Socket.IO --> Web
                                                +-- HTTP/FCM ------> Android (planned)
```

Fontes canônicas desta etapa:

- [API HTTP](http-api.md) e [OpenAPI](openapi.yaml);
- [tópicos e payloads MQTT](mqtt-topics.md) e [JSON Schemas](mqtt/README.md);
- [autoridade e identidade do device](data-authority.md);
- [identidade de evento e ACK futuro](event-identity.md);
- [semântica temporal](time-semantics.md).

## Estados e compatibilidade

- `active`: existe no código e está disponível no runtime atual;
- `legacy`: ainda aceito para compatibilidade, sem ser o formato preferido;
- `internal`: operacional, sem consumidor de produto;
- `test-only`: somente tooling/teste;
- `candidate-for-removal`: existe, mas requer decisão antes de remover;
- `planned`: contrato de direção futura, ainda não implementado.

Os payloads MQTT atuais não carregam `schema_version`. Eles são tratados como perfil legado implícito `0`. Durante a migração, o backend continuará aceitando ausência do campo; envelopes novos usarão o inteiro `1`. Um emissor não deve anunciar `schema_version: 1` até cumprir o schema v1 correspondente. Os validadores rejeitam versões desconhecidas; enforcement no ingestor runtime ainda é `planned` e deve rejeitar ou quarentenar com motivo observável, nunca reinterpretar silenciosamente.

O OpenAPI e os schemas são validados por `npm run test:contracts --prefix backend`.

As dependências de desenvolvimento usadas para validação são `@apidevtools/swagger-parser`, `ajv` e `ajv-formats`, fixadas no lockfile e licenciadas sob MIT. Elas não entram no runtime de produção.

## Versionamento do repositório

- tag existente `tcc-baseline-v0.9.0`: baseline importada/validada;
- o fechamento correto de `v0.9.1` seria `dfffbac28f3f9465a1b7c2cd511b185bf2db64f1` (P1 já integrado e CHANGELOG detalhado, antes do replay);
- o fechamento correto de `v0.9.2` seria `35905f3aaf9fd2c6a6c259017eaba9cd482c3ed0` (replay integrado e documentação/SDLC fechados);
- após esta etapa, `v0.9.3` é a próxima versão candidata coerente, mas só deve ser criada por decisão explícita depois do merge e CI verde.

Nenhuma tag retroativa ou nova foi criada nesta etapa.
