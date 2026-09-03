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

- `tcc-baseline-v0.9.0`: baseline importada/validada;
- `v0.9.1` em `dfffbac28f3f9465a1b7c2cd511b185bf2db64f1`: correções P1 do detector;
- `v0.9.2` em `35905f3aaf9fd2c6a6c259017eaba9cd482c3ed0`: replay sintético e fechamento da Etapa 3;
- `v0.9.3` em `ec42eafbbae7bd65bbb361cff2a585b91579b105`: contratos HTTP/MQTT e fechamento da Etapa 4.

As três tags SemVer são anotadas e não alteram commits ou documentação histórica.
