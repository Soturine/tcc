# MotionLab Live — especificação implementável

**Status: design, não implementado.** Estudo piloto: `sitting`, `sit_to_stand`, `standing`, `stand_to_sit`, `other_movement`, `unknown`. No pulso, postura global do corpo não é diretamente observável; classes sentado/em pé devem ser inferidas com incerteza. Hipótese prioritária = **transições**. O detector crítico de queda e SOS não depende do resultado HAR.

## Arquitetura
```text
IMU física (ax/ay/az gx/gy/gz) + id, range, clock monotônico
 → Sensor HAL/Signal (uma aquisição) → ring buffer + quality counters
 ├─ FallDetector (prioridade crítica, inalterado)
 ├─ MotionLabRecorder → chunks locais com integridade / manifest
 ├─ ActivityEngine (baseline simples / RF / TinyML em shadow)
 └─ LabStream (bounded queue; Wi-Fi TLS / BLE sessão presencial)
       → backend lab ingest / authenticated scoped Socket.IO
       → Android Compose MotionLab + Web Research Console
```
Nunca bloquear `FallDetector`, SOS, outbox, ACK ou watchdog por stream/SD/UI. Backpressure: descartar frames **live** com contador e preservação local conforme capacidade, nunca descartá-los silenciosamente. MQTT operacional 2 s **não** é fonte suficiente para classificar transições. Streaming lab deve ser canal versionado e limitado, com ACL; não reutilizar `telemetry_logs` SQL de produção para 50Hz. Preferir lotes binários (12 B por amostra em 6×int16) com cabeçalho, CRC/hash/chunk seq, e o arquivo raw permanece fonte de verdade. Transferência após sessão com checksum/resume, sem assumir 100% de entrega via Wi-Fi/BLE.

## Aquisição e contrato proposto
Perfil de experimento candidato: **50 Hz nominal**, verificar ODR real/jitter/gaps/IRQ/FIFO no hardware; baseline atual é **20 Hz nominal** (`SENSOR_NORMAL_SAMPLE_INTERVAL_MS=50`), telemetria `2000 ms`, demo 40Hz (`25 ms`). Perfis por placa devem conter IMU, revisão, faixa, filtro e capacidade; não mudar frequência da FSM sem benchmark A/B. Timestamp monotônico 64bit ou mecanismo com wrap testado; `device_time_us`, `boot_id`, `sample_seq`, `session_id`, seis raw e valores normalizados, `valid`, `axis_map`, `odr`, `sensor_model`, `firmware_sha`. Clock servidor/Android medido por troca de timestamps, guardar offset/incerteza em vez de falsa precisão.

Exemplo **ilustrativo** (não payload atual):
```json
{"schema_version":1,"session_id":"lab-001","boot_id":"boot-xyz","sample_seq":153,"device_time_us":3060000,"ax_g":0.1,"ay_g":0.2,"az_g":0.97,"gx_dps":2.1,"gy_dps":0.8,"gz_dps":-1.0,"valid":true}
```

## Máquina de estados e ground truth
`created → preparing → recording → paused → finalizing → finalized | interrupted | rejected`. Comandos `start/pause/mark/stop/export` são autenticados, possuem `command_id`, idempotência, deadline e ACK do dispositivo; UI não afirma "gravando" antes de receber confirmação. Durante `recording`, qualquer emergência tem prioridade.

Sessão supervisionada opcional: autenticar/pseudonimizar participante → consentimento/protocolo institucional → validar pulseira, bateria, sensor/clock/espaço → rótulo de atividade **pretendida** → sentar/levantar em cadeira estável, sem movimentos forçados → observador marca `sit_to_stand` e `stand_to_sit` → revisar limites em replay → exportar e qualificar. Nunca pedir queda humana nem exigir mobilidade que o usuário não possua.

**Três níveis de verdade**:
1. `intended_activity`: intenção selecionada (NÃO ground truth dos intervalos).
2. `observer_annotation`: transições delimitadas por humano com revisão e incerteza; marcação no Android sofre delay.
3. `independent_instrument` opcional: sensor de contato/pressão de cadeira ou vídeo consentido, com sincronismo e limitações; contato na cadeira não identifica automaticamente início biomecânico.

Anotação: `label`, `source`, `confidence/quality`, `time_start/end` + base temporal + incerteza, `reviewer` pseudonimizado, `review_status`, `annotation_version`; suporte `unknown`, `ambiguous`. Predição não vira rótulo automaticamente.

## Classificação e visualização
Janelas de 2 s com stride 250 ms como **configuração inicial de estudo**, ajustável e avaliada; média/std/RMS/pico/jerk/magnitude/gyro e orientação relativa, conforme viável. Condições: regra simples, RF offline, outros modelos se comprovado ganho; modelo `unknown` se dados insuficientes; raw score não é probabilidade calibrada. Reconstruir eventos com pós-processamento, histerese e matching temporal 1:1, não contar cada janela sobreposta como evento independente. UI Android: gráficos downsampled de aceleração/giro, classe prevista e GT **lado a lado**, estado FSM, bateria, taxa efetiva, lag, qualidade, record/mark/pause/review/export. Wearable exibe apenas estado curto/vibração; interface não exige tela sempre ligada.

## Contratos/endpoints propostos (AINDA NÃO EXISTEM)
`POST /api/lab/sessions`, `GET /api/lab/sessions/{id}`, `POST /api/lab/sessions/{id}/annotations`, `POST /api/lab/sessions/{id}/stop`, `GET /api/lab/sessions/{id}/recordings`, `GET /api/lab/sessions/{id}/report`; autorização **por paciente/tenant/finalidade**, não dar acesso ao corpus a todo cuidador. Socket.IO sala por sessão autorizada. MQTT hipotético `queda/devices/{id}/lab-frames`: nova ACL, parser, schema e limitação; não implantar até aprovação do contrato.

## Persistência e reprodutibilidade
Metadados MySQL (`lab_sessions`, `lab_recordings`, `lab_annotations`, `lab_predictions`, `lab_experiments`, `lab_reports`) somente após migration; raw em arquivos/objeto segregados, não um registro SQL por amostra. `manifest.json`, `imu.raw`, `imu.normalized.csv`, `annotations.json`, `predictions.jsonl`, `quality.json`, `report.json`; SHA256 de arquivos; ID do participante pseudonimizado, versão do protocolo/algoritmo, fonte/licença, eixo, unidades, hardware, firmware/modelo/config SHA, gaps, relógio e taxa real. Dados sensíveis fora do Git, minimização/retention/erasure e revisão ética antes de voluntários.

## Relatórios e métricas
Métrica de transição **por evento** (TP/FP/FN, precisão, recall, F1) com matching 1:1 e tolerância definida antes da avaliação, além de matriz de confusão por classe, macro-F1, `unknown` rate, onset/offset erro, falsos alarmes/h, latência de stream/inferência, packet loss/jitter, continuidade gravada, heap/CPU/PSRAM/energia, por usuário/sessão/dispositivo, intervalo de confiança e casos excluídos. Dataset de teste deve ser independente da calibração.

## Gates e testes
ML-0 contratos e fixtures sintéticas (não resultados científicos); ML-1 recorder start/stop/crash/export e corrupção; ML-2 stream dedupe/reorder/buffer overflow/resume; ML-3 GT atrasado e incerto, clock sync; ML-4 classificadores com split por sujeito/sessão; ML-5 Android acessibilidade + process death; ML-6 relatório determinístico; ML-7 HIL com perda de Wi-Fi/SD, reboot, SOS no meio, I²C inválido, bateria baixa, perfil errado/tenant isolation. Falha do laboratório deve **falhar de forma observável sem desligar a proteção**.
