# Evolução do Modelo de Dados

Este documento descreve a evolução arquitetural do banco. A baseline MySQL é preservada e evoluída pelo runner versionado; partes posteriores continuam planejadas.

Auditoria relacionada: [`../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../audit/iot-fall-monitor-port-audit-2026-09-01.md).

## Entidades existentes a preservar/evoluir

- users;
- organizations;
- organization memberships/roles;
- patients;
- devices;
- patient-device assignment history;
- pairing sessions;
- device status;
- telemetry;
- events;
- event evidence;
- alerts;
- alert actions;
- audit logs;
- battery calibration/estimation quando mantida.

## Evento crítico

`event_uuid` está materializado em campo nullable com índice global `UNIQUE` pela migration `001_event_identity`. O JSON bruto permanece evidência, não autoridade de deduplicação.

Modelo implementado nesta etapa e extensões planejadas:

```text
events
- id
- event_uuid UNIQUE
- organization_id
- device_id
- patient_id
- assignment_history_id
- event_type
- severity
- occurred_at_device NULL
- received_at NULL para compatibilidade histórica; preenchido em novas ingestões
- persisted_at NOT NULL
- clock_quality
- boot_id NULL
- device_uptime_ms NULL
- event_sequence NULL
- schema_version
- algorithm_version NULL
- config_version NULL
- evidence_source
- evidence_status
- raw_payload_json
- evidence_summary_json
```

`event_sequence`, `schema_version`, `algorithm_version`, `config_version`, `evidence_source` e `evidence_status` permanecem extensões planejadas quando não já representadas nos JSONs existentes.

### Regra temporal

Não substituir `occurred_at_device` por `received_at` quando o relógio do device for incerto. Guardar ambos e registrar qualidade/origem do tempo. Isso é essencial para replay após longos períodos offline.

### Regra de identidade

O `event_uuid` deve ser robusto a reboot e não depender somente do wall clock.

### Conflito de duplicata

Se o mesmo `event_uuid` chegar novamente, o comportamento implementado é:

- mesmo conteúdo lógico → retorno idempotente do evento existente;
- conteúdo materialmente incompatível → registrar conflito de integridade/security, não sobrescrever silenciosamente.

## Evidência de evento

Distinguir origem:

```text
evidence_source = device | server_telemetry | both | none
```

A evidência do device precisa sobreviver a período offline. A telemetria SQL pode enriquecer o evento, mas não pode ser pré-condição exclusiva para alertar uma queda confirmada no edge.

Dependendo do tamanho real, a evidência local pode ficar:

- estruturada em colunas/JSON validado no próprio evento;
- em tabela/bundle associado;
- com amostras compactas vinculadas ao evento.

A escolha deve considerar consultas reais, banda e volume; não criar uma tabela nova sem necessidade demonstrada.

## Notification Outbox

```text
notification_outbox
- id
- event_id/alert_id
- channel
- installation/destination reference
- dedupe_key
- state
- attempts
- available_at
- created_at
- processed_at
- last_error_code
- last_error_at
```

O payload deve ser mínimo; preferir referência ao recurso em vez de copiar dados sensíveis.

Estados finais dependem do provider, mas distinguir pelo menos intenção persistida de envio e tentativas. Não chamar provider acceptance de entrega ao humano.

## Notification Deliveries

```text
notification_deliveries
- id
- outbox_id
- installation_id
- provider_message_id NULL
- submitted_at NULL
- app_observed_at NULL      # somente se observável
- action_at NULL
- result/error metadata
```

Não inventar delivery receipt onde FCM/Android não fornecer evidência direta.

## Mobile installations e sessões

Direção:

```text
mobile_installations
- id
- user_id
- platform
- installation_id
- created_at
- last_seen_at
- revoked_at

push_tokens
- installation_id
- token_hash/reference or encrypted value as design requires
- issued_at
- refreshed_at
- revoked_at

refresh_sessions
- id
- user_id
- installation_id
- refresh_token_hash
- issued_at
- expires_at
- rotated_from_id NULL
- revoked_at
- last_used_at
```

Requisitos:

- token FCM pode mudar;
- um usuário pode possuir várias instalações;
- logout/revogação precisa ter efeito real;
- refresh token é rotativo e armazenado como hash;
- tokens obsoletos precisam de limpeza.

O schema final deve ser definido junto ao fluxo de autenticação, não antecipado sem necessidade.

## Device credentials / trust

O banco precisará representar lifecycle de credencial MQTT/device quando a autenticação por device for implementada. Pode ser em tabela própria ou metadados de provisionamento, mas deve permitir:

- emissão;
- fingerprint/key ID, nunca secret em texto claro no banco quando hash/referência for suficiente;
- status;
- rotação;
- revogação;
- timestamps/auditoria.

Não acoplar o domínio a um fornecedor específico de broker.

## Device sync/pairing token

A baseline já possui hash e `issued_at`. A evolução deve adicionar semântica real de:

- expiry;
- rotation;
- revocation;
- replacement/re-pairing.

Mensagem de erro e schema precisam refletir política verdadeira, não “expirado” sem expiração aplicada.

## Device shadow

```text
device_desired_config
- device_id
- version
- configuration
- command_id
- created_by
- created_at

device_reported_config
- device_id
- version
- configuration/status
- command_id NULL
- reported_at
```

Normalizar campos críticos usados para segurança/consulta; JSON validado pode manter extensões.

## Protection Health

Nem todo health precisa ser persistido como snapshot histórico. Reutilizar `device_status` e calcular estado agregado quando possível.

Campos úteis no device/status:

- last seen;
- sensor health;
- firmware/protocol version;
- boot ID/uptime;
- last application ACK;
- critical outbox depth/oldest age;
- desired/reported version;
- battery value/source.

No mobile/backend também entram notification permission/FCM health, que não pertencem ao `device_status` do ESP32.

## Firmware/protocol versions

```text
firmware_versions   # somente se catálogo realmente agregar valor
```

Independentemente de tabela própria, cada evento experimental deve ser rastreável à versão/algoritmo/protocolo realmente usado.

## Futuro condicionado ao wearable/pesquisa

Criar somente se o experimento exigir:

```text
wearable_profiles
calibration_sessions
datasets
experiments
experiment_runs
```

Evitar banco “preparado para tudo” antes dos casos de uso reais.

## Telemetria e lifecycle

Não reter amostragem de alta frequência indefinidamente. Separar:

1. telemetria operacional;
2. evidência associada a evento;
3. agregados;
4. dataset de pesquisa selecionado/protocolado.

Antes de staging contínuo, medir taxa de crescimento e definir política. Prazos só depois de necessidade/volume/LGPD serem avaliados.

## Migrations

Estrutura implementada:

```text
database/
├── migrations/
│   ├── README.md
│   └── 001_event_identity.js
├── schema.sql
└── seed.sql
```

O runner mantém `schema_migrations`, checksum por arquivo e advisory lock MySQL. O procedimento operacional está em [`database/migrations/README.md`](../../database/migrations/README.md).

Cada migration relevante deve ser validada contra:

- banco vazio quando aplicável;
- upgrade da baseline conhecida;
- dados de compatibilidade;
- constraints/índices;
- backup/restore em mudanças de maior risco.

`schema.sql` destrutivo pode continuar como bootstrap de dev/test se claramente identificado, mas não é mecanismo de upgrade.

## Integridade

Preferir constraints para invariantes reais:

- FKs quando coerentes com lifecycle;
- UNIQUE de identidade estável;
- transações para mudança crítica;
- timestamps/timezone consistentes;
- checks/enum only when evolution cost is understood;
- índices derivados de queries reais/query plans.

Não duplicar estado em várias tabelas sem definir qual é a autoridade.
