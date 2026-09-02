# Arquitetura Geral

## 1. Objetivo

Evoluir o sistema existente para uma plataforma IoT mobile-first para monitoramento experimental de quedas e imobilidade, preservando autonomia no edge, confiabilidade ponta a ponta, backend como autoridade e aplicativo Android como principal interface operacional.

A arquitetura abaixo incorpora a auditoria da baseline de 2026-09-01. Ver [`../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../audit/iot-fall-monitor-port-audit-2026-09-01.md).

## 2. Arquitetura lógica alvo

```text
┌──────────────────────────────────────┐
│ Device / Edge                        │
│ ESP32 + IMU agora                    │
│ wearable futuro depois               │
│                                      │
│ detector + local evidence            │
│ persistent critical-event outbox     │
└─────────────────┬────────────────────┘
                  │ MQTT/TLS QoS 1 críticos
                  │ QoS 0 telemetria quando aceitável
                  ▼
┌──────────────────────────────────────┐
│ Broker MQTT                          │
│ Mosquitto self-hosted inicialmente   │
│ HiveMQ/managed broker como opção     │
│ TLS + per-device identity + ACL      │
└─────────────────┬────────────────────┘
                  ▼
┌──────────────────────────────────────┐
│ Backend modular monolith             │
│ Node.js 24 LTS + Express             │
│                                      │
│ auth/sessions/tenants/patients       │
│ device identity/config/telemetry     │
│ critical events/alerts               │
│ MQTT/application ACK                 │
│ notification outbox/realtime/audit   │
└───────┬─────────────┬────────────────┘
        │             │
        │ SQL         │ FCM / Socket.IO
        ▼             ▼
┌──────────────┐   ┌────────────────────────┐
│ MySQL        │   │ Clientes               │
│ source of    │   │ Android principal      │
│ truth        │   │ React admin/research   │
└──────────────┘   └────────────────────────┘
        ▲
        │ application ACK after commit
        └────────────────────────────── device
```

## 3. Invariantes de arquitetura

1. Device detecta sem depender de app/site/cloud online.
2. Evento crítico permanece pendente no device até confirmação da aplicação/backend.
3. MQTT QoS/PUBACK não equivale a commit do banco.
4. `event_uuid` torna retry idempotente e é robusto a reboot.
5. Identidade do device em MQTT vem do principal/ACL/tópico autenticado.
6. Backend é autoridade de domínio/persistência; clientes não acessam MySQL diretamente.
7. Evidência local de queda sobrevive a período offline; telemetria do servidor enriquece, não apaga a decisão edge.
8. `occurred_at_device` e `received_at` são distintos.
9. Push e realtime são canais de apresentação; o evento já existe no backend antes deles.
10. Falha deve produzir estado observável/degradado, não sucesso fictício.
11. Site não é caminho crítico.
12. Wearable e ML não são dependências do core.

## 4. Responsabilidades

### Edge/device

- amostrar sensores;
- executar detector/FSM;
- produzir evidência local da decisão;
- gerar `event_uuid` robusto;
- persistir critical-event outbox;
- publicar/retry sem bloquear aquisição;
- remover evento somente após application ACK válido;
- sinalização/SOS local;
- persistir configuração validada;
- reportar sensor/connectivity/outbox/firmware/config health;
- provisioning seguro.

### Broker MQTT

- transporte assíncrono device↔backend;
- QoS/sessão/LWT semanticamente definidos;
- TLS;
- autenticação por device;
- ACL mínima;
- rejeição de publicação/subscription fora da identidade autorizada.

Broker não decide tenant, alerta ou regra de negócio.

### Backend

- autenticação/sessões/autorização;
- multi-tenancy/object authorization;
- validar identidade e schemas MQTT;
- idempotência;
- migrations/integridade;
- persistir eventos/evidências;
- criar alertas por regra explícita;
- application ACK após commit;
- notification outbox;
- REST;
- Socket.IO foreground;
- FCM;
- auditoria;
- desired/reported config;
- liveness/readiness/observabilidade.

### Android

- interface principal do cuidador/familiar;
- sessão segura/revogável;
- alertas/histórico/pacientes/devices;
- provisioning/pairing;
- deep links e ações idempotentes;
- FCM;
- cache de último estado com staleness;
- Protection Health;
- Testar alerta;
- acessibilidade.

### Web

- console secundário de administração/pesquisa/diagnóstico;
- telemetria/evidência ampla;
- exportação;
- calibração/análise;
- auditoria/suporte;
- não necessário para receber/responder queda.

## 5. Protocolos por finalidade

| Necessidade | Tecnologia/direção |
|---|---|
| telemetria device → backend | MQTT/TLS; QoS 0 quando perda ocasional é aceitável |
| evento crítico device → backend | MQTT/TLS QoS 1 + `event_uuid` + persistent outbox |
| confirmação de persistência backend → device | MQTT application ACK depois do commit |
| configuração/comando backend → device | MQTT/TLS + command ID/version + ACK |
| app/web → backend | HTTPS REST |
| cliente ativo | Socket.IO |
| Android background/killed | FCM |
| provisioning ESP32 | ESP-IDF Unified Provisioning, BLE/SoftAP conforme spike |
| recovery local | portal do ESP32 com autoridade limitada |
| wearable futuro | transporte abstrato; BLE/Wi‑Fi/vendor SDK conforme hardware |

## 6. Dois outboxes, dois problemas

### Device critical-event outbox

Protege **device → backend**.

```text
queued
→ publish/retry
→ backend commit
→ application ACK
→ confirmed/remove
```

### Backend notification outbox

Protege **backend commit → push/realtime**.

```text
transaction event/alert/notification_intent
→ COMMIT
→ worker
→ FCM/realtime
→ attempt/result
```

Não confundir os dois nem introduzir Kafka/RabbitMQ apenas para implementar esse padrão na escala atual.

## 7. Queda offline

Cenário obrigatório:

```text
Internet cai
→ detector confirma queda
→ evidence + event_uuid ficam persistentes no ESP32
→ Internet volta
→ evento reenvia
→ backend reconhece mesma identidade
→ evento/alerta são persistidos uma vez
→ ACK retorna
→ push chega ao Android
```

Telemetria periódica ausente no banco durante a queda não pode transformar automaticamente o alerta em inexistente.

## 8. Device shadow simplificado

Separar:

- `desired_config`;
- `reported_config`;
- `command_id`/version.

```text
app → REST → desired N
backend → MQTT command
ESP32 valida/persiste/aplica
ESP32 → ACK/reported N
app → synchronized / pending / drift / error
```

Sucesso de publicação não significa sucesso de aplicação.

## 9. Protection Health

Estado composto, não uma flag arbitrária:

```text
Device health
+ sensor
+ connectivity/application ACK
+ pending outbox
+ config sync
+ battery source/state
+ backend health
+ Android notification permission/FCM
= protection status apresentado ao usuário
```

O indicador é operacional/experimental, não promessa médica.

## 10. Observabilidade e latência

Registrar estágios semanticamente distintos:

- `t0`: detector confirma/gera evento;
- `t1`: backend recebe;
- `t2`: commit;
- `t2a`: device recebe application ACK quando observável;
- `t3`: push submetido ao provider;
- `t4`: app recebe/abre quando observável;
- `t5`: ação humana.

Também registrar `received_at` e `occurred_at_device` separadamente.

Medir primeiro p50/p95/p99; depois definir metas.

## 11. Infraestrutura

A topologia deve caber inicialmente em uma VM Linux + FCM, mas ser provider-agnostic. Oracle é candidato, não dependência. Ver [`cloud-deployment.md`](cloud-deployment.md).

## 12. Evolução sem reescrita

A baseline deve ser migrada/refatorada em pequenas etapas com characterization tests. Arquivos grandes são dívida de modularidade, não justificativa para big-bang rewrite.

Ordem de alto nível:

```text
lineage/baseline/CI
→ contracts/migrations
→ critical delivery + device trust
→ backend hardening
→ Android
→ push
→ provisioning
→ cloud/failure testing
→ wearable/ML
```
