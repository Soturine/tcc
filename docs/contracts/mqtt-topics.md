# Inventário MQTT real

Inventário derivado de `include/app_config.h`, `src/device_config.cpp`, `src/main.cpp`, `src/mqtt_client.cpp`, `backend/src/mqtt/`, `backend/src/services/mqttIngestionService.js`, scripts, mocks e testes.

## Fluxo implementado

```text
ESP32 -- publish QoS 0, retain=false --> broker
                                          |
                                          +-- backend subscribe QoS 1
                                                |
                                                +-- status/telemetry/event persistence
                                                +-- Socket.IO scoped events
```

O base default é `queda/devices`. O firmware monta o tópico com o `device_id` configurado. O backend aceita outro base via `MQTT_TOPIC_BASE` e assina os três wildcards abaixo.

## Matriz canônica

| Topic/padrão | Publisher real | Subscriber real | QoS real | Retained | Payload | Criticidade | Offline/duplicata/erro | Estado |
|---|---|---|---:|---|---|---|---|---|
| `queda/devices/{device_id}/status` | ESP32; mocks/scripts | backend; `mqttWatch` diagnóstico | ESP32 0; mock/test 0; subscription backend 1 | false no ESP32/scripts | `status.schema.json` | status/diagnostic | firmware não bufferiza; perda eventual; backend atualiza `device_status`; JSON/canal inválido é descartado | active |
| `queda/devices/{device_id}/telemetry` | ESP32; mocks/scripts/stress | backend; `mqttWatch` | 0 em todos os publishers encontrados; subscription backend 1 | false | `telemetry.schema.json` | telemetry | firmware descarta se offline/falha; backend exige seis eixos finitos e `sensor_valid != false`; inválida atualiza health, mas não grava `telemetry_logs` | active |
| `queda/devices/{device_id}/events` | ESP32; mocks/scripts/stress | backend; `mqttWatch` | **ESP32 0**; `mockPublisher` e `mqttPublishTest` usam 1 para evento; stress real usa 0; subscription backend 1 | false | `event.schema.json` | critical-event ou informational | firmware usa fila RAM 10 e snapshot NVS de 4, mas remove após `publish()` local; backend materializa `event_uuid` com `UNIQUE`, deduplica retry idêntico e rejeita conflito; payload legado sem UUID é aceito sem essa garantia | active |
| `queda/devices/{device_id}/critical-event-acks` | backend | ESP32 | ainda não definido na implementação; alvo QoS 1 | alvo false | `critical-event-ack-v1.schema.json` | critical-event ACK | publicar somente depois de commit; ACK perdido causa retry do mesmo UUID; duplicata deve repetir ACK do mesmo evento lógico | planned |
| `queda/devices/{device_id}/commands` | backend | ESP32 | inexistente | inexistente | nenhum schema current | command/configuration | citado apenas como evolução de buzzer/configuração | planned, nome já citado em `docs/alerting-architecture.md` |

QoS solicitado pelo subscriber não eleva o QoS com que a mensagem foi publicada. Portanto, a subscription QoS 1 do backend não torna o envio atual do ESP32 QoS 1.

## Identidade e isolamento atuais

- O segmento `{device_id}` do tópico é a identidade de roteamento.
- O JSON contém `device_id` redundante e, normalmente, `device_uid` técnico derivado do eFuse MAC.
- Divergência entre tópico e `payload.device_id` agora é rejeitada antes de criar device, persistir ou emitir realtime.
- `device_uid` ainda não é provado por principal MQTT. Backend e firmware podem usar credenciais globais; não há credencial/ACL individual implementada.
- Organização e paciente não vêm do tópico nem são aceitos do payload como autoridade: o backend resolve ownership/assignment no banco.
- Device desconhecido pode ser criado como `unclaimed`. Isso é descoberta técnica, não ownership.
- Em ambiente externo, a direção aprovada permanece principal autenticado + ACL por tópico como identidade autoritativa. Isso está `planned`.

## Payloads atuais

### Campos comuns emitidos pelo ESP32

| Campo | Tipo | Obrigatório no publisher atual | Origem/autoridade | Unidade/nullable | Consumidores/semântica |
|---|---|---:|---|---|---|
| `device_uid` | string | sim | ESP32, identidade técnica declarada | não nulo | reconciliação de cadastro; não é autenticação |
| `device_id` | string | sim | configuração NVS do ESP32; tópico deve coincidir | não nulo | roteamento e lookup legado |
| `timestamp` | integer | sim | ESP32 | Unix s quando NTP plausível; senão uptime s | backend normaliza; não prova hora de ocorrência |
| `battery_percent_source` | enum string | sim | configuração/sensor do ESP32 | `not_configured`, `manual` atuais | backend battery/status; Web |
| `battery_level`, `battery_percent`, `battery_manual_percent` | number | condicional | valor manual no ESP32 | %, nullable/omitido | backend; não é leitura automática |
| `battery_manual_updated_at` | integer | condicional | portal/NVS | Unix s, pode ser fallback imperfeito | estimação backend |
| `battery_calibration_sequence` | integer | condicional | ESP32 | contador local | dedupe de calibração |
| `battery_estimated_minutes_per_percent` | number | condicional | constante firmware | min/% | semente experimental de estimação |
| `battery_estimate_mode` | string | condicional | firmware | `time_decay` | diagnóstico |
| `detector_mode` | string | sim | configuração NVS | `normal` ou `demo` | identificação explícita de Demo |
| `threshold_profile` | string | sim | configuração NVS | `low`,`normal`,`high`,`demo` | auditoria da decisão |
| `sample_interval_ms` | integer | sim | configuração efetiva firmware | ms | diagnóstico |
| `telemetry_interval_ms` | integer | sim | configuração efetiva firmware | ms | diagnóstico |
| `fft_enabled` | boolean | sim | build/config firmware | sem unidade | indica feature experimental |

O backend tolera vários campos ausentes para compatibilidade. “Obrigatório no publisher atual” descreve o construtor de payload do firmware, não uma validação estrita já aplicada pelo ingestor.

### Status

Além dos campos comuns:

| Campo | Tipo | Obrigatório/condição | Origem/autoridade | Unidade/nullable | Semântica/consumidores |
|---|---|---|---|---|---|
| `event_type` | string | sim | firmware | `device_status` | diagnóstico; backend roteia pelo tópico |
| `accel_magnitude`, `gyro_magnitude` | number | se amostra fresh | sensor/firmware | g; deg/s | observabilidade, não persistidos como telemetry |
| `immobility_confirmed` | boolean | sim | firmware | false no status | compatibilidade |
| `wifi_rssi`, `rssi` | integer | sim | Wi-Fi stack | dBm | backend usa `wifi_rssi` |
| `buffered_events` | integer | sim | fila RAM | contagem | health; não distingue snapshot NVS |
| `sample_seq` | integer | sim | firmware | contador desde boot | correlação diagnóstica |
| `sensor_ready`, `sensor_valid`, `sensor_read_ok` | boolean | sim | firmware | — | health do sensor |
| `sensor_sample_age_ms` | integer | sim | firmware | ms | freshness da amostra |
| `sensor_failures`, `i2c_error_count`, `i2c_recovery_count` | integer | sim | driver firmware | contadores desde boot | diagnóstico |
| `i2c_last_error` | string | sim | driver firmware | string, pode ser vazia | diagnóstico |
| `firmware_version` | string | somente mocks/scripts | tooling | nullable | backend aceita/persiste; firmware real atual não emite |

### Telemetry

Além dos campos comuns e diagnósticos de sensor/rede:

| Campo | Tipo | Obrigatório no envio real | Origem/autoridade | Unidade | Consumidores/semântica |
|---|---|---:|---|---|---|
| `ax`,`ay`,`az` | number finito | sim | IMU + escala/offset firmware | g | backend exige e persiste; Web |
| `gx`,`gy`,`gz` | number finito | sim | IMU + escala/offset firmware | deg/s | backend exige e persiste; Web |
| `accel_magnitude` | number | sim | firmware | g | persistência/gráfico |
| `gyro_magnitude` | number | sim | firmware | deg/s | persistência/gráfico |
| `pitch_deg`,`roll_deg` | number | sim | firmware | graus | persistência/evidência |
| `sample_seq` | integer | sim | firmware | contador desde boot | correlação |
| `temperature` | number | somente mock | mock | não há unidade formalizada no runtime | backend ignora |

### Events

Campos base emitidos para `fall_detected`, `fall_suspected`, `movement_detected` e `sos_pressed`:

| Campo | Tipo | Obrigatório/condição | Origem/autoridade | Unidade/nullable | Semântica/consumidores |
|---|---|---|---|---|---|
| `event_type` | string | sim | firmware | tipos acima | backend decide regra/alerta |
| `event_uuid` | string | sim no firmware atual | firmware | máx. aceito 160 chars | identidade de retry/dedup |
| `event_sequence` | integer | sim | firmware | contador desde boot | diagnóstico; não persistido em coluna própria |
| `sample_seq` | integer | sim | firmware | contador desde boot | ligação à amostra |
| `event_uptime_ms` | integer | sim | firmware | ms desde boot | tempo monotônico local |
| `accel_magnitude`,`gyro_magnitude` | number | sim | observação firmware | g; deg/s | intensidade/evidência |
| `immobility_confirmed` | boolean | sim | decisão firmware | — | evidência, não severidade autoritativa |
| `algorithm` | string | sim | firmware | versão/nome | auditoria |
| `alert_settings` | object | sim | config firmware | thresholds/unidades próprias | auditoria do contexto |
| diagnósticos do sensor | vários | sim | firmware | como status | auditoria |
| `severity` | string | não emitido pelo firmware real; scripts podem enviar | **backend é autoridade** | payload é ignorado para classificação | raw payload preserva tentativa; `deriveSeverity` decide |
| `message` | string | opcional/scripts | device/tooling | nullable | backend aceita texto; default deriva do tipo |

Campos adicionais quando existe `FallAlert` (`fall_detected`, suspeita/movimento experimental):

| Campo/grupo | Tipo | Origem | Unidade/nullable | Semântica |
|---|---|---|---|---|
| `decision_source` | string | firmware | não nulo | hoje `firmware` |
| `algorithm_version` | string | firmware | não nulo | versão da heurística |
| `detected`,`candidate` | boolean | firmware | — | estado da decisão |
| `reason`,`fall_reason`,`fall_decision_reason` | string | firmware | — | motivo redundante de compatibilidade |
| `activity_state_estimate` | string | firmware | — | estimativa experimental |
| `confidence` | null | firmware | sempre null | probabilidade calibrada indisponível |
| `confidence_status` | string | firmware | `not_available` | semântica explícita |
| `window_started_at_ms`,`window_ended_at_ms`,`analysis_window_ms` | integer | firmware | ms desde boot/duração | janela local |
| `sample_count`,`samples_considered` | integer | firmware | amostras | redundância de compatibilidade |
| `peak_accel_g`,`accel_magnitude_g` | number | firmware | g | features |
| `peak_gyro_dps`,`gyro_magnitude_dps` | number | firmware | deg/s | features |
| `pitch_deg`,`roll_deg`,`orientation_delta_deg` | number | firmware | graus | orientação normalizada |
| `immobility_duration_ms`,`immobility_accumulated_ms` | integer | firmware | ms observados | não inclui gaps não observados |
| `impact_detected`,`orientation_change_detected`,`immobility_detected` | boolean | firmware | — | etapas da FSM |
| `features` | object | firmware | campos resumidos | duplicação compatível do resumo |
| `features_time_domain` | object | firmware | amostras, duração, desvio/picos/jerk | implementado |
| `features_frequency_domain` | object | firmware | `available=false`, `experimental=true` hoje | experimental, não validado |
| `linked_telemetry_window` | object | firmware | marca intenção; backend cria ligação real própria | não prova vínculo SQL |
| `thresholds` | object | firmware | g, deg/s, graus, ms | configuração efetiva |

O backend preserva o payload inteiro em `raw_payload_json`, cria seu resumo e resolve organização/paciente/assignment pelo banco. `fall_detected` ainda é bloqueado como alerta automático quando a evidência SQL está ausente; isso conflita com replay offline e permanece aberto para a etapa de confiabilidade.

O ingestor backend é mais permissivo que o publisher current: aceita qualquer `event_type` no canal e usa `device_event` quando o campo falta. Além dos tipos emitidos pelo ESP32, `manual_sos` e `sensor_fault` são candidatos a alerta, enquanto `heartbeat`/desconhecidos são apenas persistidos. Não foi encontrado publisher real de firmware para esses três nomes; são compatibilidade/capacidade do backend, não eventos produzidos pelo device atual.

## Tooling e divergências objetivas

- `mockPublisher.js` usa `device_uid=legacy:{device_id}`, inclui `temperature`, `firmware_version`, omite parte dos campos operacionais atuais e publica eventos com QoS 1.
- `mqttPublishTest.js` também publica eventos QoS 1 e pode enviar `severity`; o backend agora ignora esse campo como política.
- `runRealStressSuite.js` publica tudo, inclusive quedas, em QoS 0.
- Schemas desta etapa validam exemplos representativos do firmware atual. O ingestor permanece permissivo para payloads legados, explicitamente documentados como perfil implícito 0.

## Payload budget medido

O cliente ESP32 configura `PubSubClient` com buffer de **4096 bytes**. No código da versão instalada, o publish falha se `5 + 2 + bytes(topic) + bytes(payload) > 4096`. Para o tópico default de evento, o teto técnico depende do tamanho real de `{device_id}`; ele não é um budget recomendado.

Os contract tests medem os exemplos versionados e verificam:

- status e telemetry representativos: no máximo 1024 bytes cada;
- evento crítico representativo: no máximo 3072 bytes;
- pacote completo de cada exemplo abaixo de 4096 bytes.

Esses budgets são provisórios e conservadores, baseados nos exemplos atuais, nas capacidades `StaticJsonDocument<1024>`, `<1152>` e `<3584>` e no buffer MQTT real. Eles não são limite do broker. O `device_id` ainda não tem comprimento máximo imposto; até isso ser corrigido, o único teto técnico exato é a fórmula acima.

Configuração/comandos/ACKs não possuem payload real para medir. Seus limites serão fechados com a implementação correspondente, sem reutilizar números inventados.
