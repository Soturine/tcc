# Arquitetura Geral

## 1. Objetivo

Evoluir o sistema existente para uma plataforma IoT mobile-first para monitoramento de quedas e imobilidade, preservando autonomia no edge, confiabilidade de eventos críticos, backend como autoridade e aplicativo Android como principal interface operacional.

## 2. Arquitetura lógica alvo

```text
┌──────────────────────────────┐
│ Dispositivo / Edge           │
│ ESP32 + IMU hoje             │
│ wearable futuro depois       │
└─────────────┬────────────────┘
              │ MQTT/TLS
              ▼
┌──────────────────────────────┐
│ Broker MQTT                  │
│ Mosquitto self-hosted        │
│ HiveMQ como alternativa      │
└─────────────┬────────────────┘
              ▼
┌──────────────────────────────┐
│ Backend modular monolith     │
│ Node.js + Express            │
│                              │
│ auth / tenants / patients    │
│ devices / config / telemetry │
│ events / alerts / notify     │
│ mqtt / realtime / audit      │
└───────┬─────────┬────────────┘
        │         │
        │ SQL     │ FCM / Socket.IO
        ▼         ▼
┌────────────┐   ┌──────────────────┐
│ MySQL      │   │ Clientes         │
│ autoridade │   │ Android + Web    │
└────────────┘   └──────────────────┘
```

## 3. Responsabilidades

### Edge/device

- amostrar sensores;
- executar processamento local e FSM/algoritmo de detecção;
- gerar identidade única para evento crítico;
- manter buffer/retry quando a rede falhar;
- sinalização local quando aplicável;
- persistir configuração local validada;
- reportar estado/firmware/sensores;
- nunca depender do app aberto para detectar queda.

### Broker MQTT

- transporte assíncrono device↔backend;
- tópicos segregados por dispositivo/tenant conforme política;
- QoS e retained/LWT escolhidos semanticamente;
- TLS e ACL em ambiente externo.

### Backend

- autenticação/autorização;
- multi-tenancy;
- ingestão MQTT;
- idempotência/deduplicação;
- persistência;
- estado de alertas;
- emissão realtime;
- fila/outbox de notificações;
- API REST para app/web;
- auditoria;
- comando/configuração remota.

### Android

- interface principal do cuidador/familiar;
- alertas, histórico, pacientes e dispositivos;
- provisioning/pairing;
- diagnóstico local;
- push FCM;
- cache de último estado útil;
- deep links e ações sobre alertas.

### Web

- console secundário;
- administração;
- visualização ampla de telemetria/evidências;
- pesquisa e análise;
- diagnóstico e suporte;
- não deve ser requisito para receber/responder um alerta.

## 4. Protocolos por finalidade

| Necessidade | Tecnologia |
|---|---|
| dispositivo → backend, eventos/telemetria | MQTT/TLS |
| backend → dispositivo, comandos/config | MQTT/TLS |
| app/web → backend, CRUD/comandos/histórico | HTTPS REST |
| atualização com cliente ativo | Socket.IO |
| Android em background/fechado | FCM |
| provisioning local inicial | SoftAP + API local versionada |
| wearable futuro | transporte abstrato; BLE se o hardware exigir |

## 5. Confiabilidade do evento crítico

Fluxo esperado:

```text
detecção local
→ event_uuid/event_sequence
→ publish MQTT QoS apropriado
→ backend valida + serializa por dispositivo quando necessário
→ transaction: event + alert + outbox
→ commit
→ worker entrega FCM / realtime
→ ação do usuário
→ alert_action + audit
```

A entrega de rede pode ser "at least once"; o resultado lógico deve ser idempotente.

## 6. Transactional Outbox

Para evitar o erro clássico "gravou no banco mas caiu antes de notificar":

1. evento, alerta e item da outbox são gravados na mesma transação;
2. worker busca itens pendentes;
3. entrega para FCM/realtime;
4. registra tentativa/resultado;
5. retries possuem limites/backoff e idempotência.

Não há justificativa atual para Kafka/RabbitMQ/Redis apenas para este mecanismo.

## 7. Device shadow simplificado

Separar:

- `desired_config`: configuração desejada pelo backend/app;
- `reported_config`: configuração confirmada pelo dispositivo.

Fluxo:

```text
app → REST → desired version N
backend → MQTT command
ESP32 valida/persiste/aplica
ESP32 → ACK + reported version N
app mostra synchronized / pending / drift
```

## 8. Estado e latência observável

O pipeline deverá registrar timestamps equivalentes a:

- `t0`: detecção no dispositivo;
- `t1`: recebimento pelo backend;
- `t2`: commit no banco;
- `t3`: submissão ao provedor push;
- `t4`: recepção/abertura no app quando observável;
- `t5`: acknowledgment humano.

Primeiro medir p50/p95/p99; só depois formalizar metas numéricas.

## 9. Evolução sem reescrita

A baseline já existente deve ser migrada/refatorada em pequenas etapas. Áreas grandes do firmware/backend/web devem ser modularizadas com testes de caracterização, não substituídas em um big bang.
