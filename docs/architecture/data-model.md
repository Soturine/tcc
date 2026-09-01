# Evolução do Modelo de Dados

Este documento descreve direção arquitetural, não uma migration pronta. A baseline MySQL existente deve ser auditada antes de alterar schema.

## Entidades existentes a preservar/evoluir

- organizations;
- organization memberships/roles;
- patients;
- devices;
- patient-device assignment history;
- events;
- alerts;
- alert actions;
- telemetry/status;
- audit/logs relacionados.

## Mudanças prioritárias

### Evento crítico

`event_uuid` deve ser promovido para campo explícito e indexado/UNIQUE quando a auditoria confirmar compatibilidade com os dados atuais.

Campos conceituais relevantes:

```text
event
- id
- event_uuid UNIQUE
- organization_id
- device_id
- patient_id (quando resolvido)
- type
- device_timestamp
- received_at
- persisted_at
- sequence
- algorithm_version
- payload/evidence metadata
```

### Outbox

```text
notification_outbox
- id
- event/alert reference
- channel
- destination key/reference
- payload version/reference
- state
- attempts
- available_at
- created_at
- processed_at
- last_error
```

Manter payload mínimo e evitar copiar dados sensíveis desnecessariamente.

### Instalações mobile e push

```text
mobile_installations
push_tokens
notification_deliveries
```

Requisitos:

- token FCM pode mudar;
- um usuário pode possuir vários dispositivos;
- logout/revogação deve invalidar associação apropriada;
- tokens inativos devem poder ser removidos.

### Device shadow

```text
device_desired_config
- device_id
- version
- configuration
- created_by
- created_at

device_reported_config
- device_id
- version
- configuration/status
- reported_at
```

O formato final pode normalizar campos críticos e usar JSON apenas para extensões devidamente validadas.

### Firmware

```text
firmware_versions
```

Útil para rastrear algoritmo/protocolo usados em um evento e facilitar reprodutibilidade experimental.

## Futuro condicionado ao wearable/pesquisa

Somente criar se necessário:

```text
wearable_profiles
calibration_sessions
datasets
experiments
experiment_runs
```

## Telemetria

Não reter amostragem de alta frequência indefinidamente. Separar:

1. telemetria operacional de curto/médio prazo;
2. janela de evidência associada ao evento;
3. agregados de longo prazo;
4. dataset de pesquisa explicitamente selecionado.

Os prazos serão definidos depois de medir volume e necessidade acadêmica.

## Migrations

Alvo:

```text
database/migrations/
  0001_...
  0002_...
```

Cada migration deve ser reproduzível, ordenada e testada contra banco vazio e, quando aplicável, upgrade da baseline conhecida.

## Integridade

Preferir constraints/índices para invariantes reais:

- FKs onde adequadas;
- UNIQUE para identidade estável de evento;
- índices compostos derivados das consultas reais;
- transações para mudança de estado crítica;
- timestamps consistentes e timezone explícito.

Não criar índices por suposição: usar query plans/telemetria quando o sistema crescer.
