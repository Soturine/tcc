# Protocolo de engenharia de hardware e portabilidade — wearable de pulso

> **Estado: proposta técnica.** Nenhuma compra, port nem validação física da shortlist ocorreu até a revisão de 2026-10-22. Complementa a [matriz de candidatos](wearable-selection-2026-10-21.md); não a substitui. Critério eliminatório: firmware próprio, Wi-Fi sem telefone do cuidador como gateway, dados brutos dos seis eixos e solução de fixação ao pulso.

## 1. Seleção sem hype: candidatos e alternativas

| Plataforma | IMU | Aplicação principal | Principal risco / decisão |
|---|---|---|---|
| Waveshare ESP32-S3 Touch AMOLED 2.06, 32MB/8MB | QMI8658 accel+gyro | **Primeiro bring-up sugerido**, pelo preço indicativo e referências de driver | kit `-EN` sem bateria, autonomia/ergonomia, presença física dos acessórios |
| Waveshare ESP32-S3 Touch AMOLED 1.8 **V2**, 16MB/8MB | QMI8658 | opção compacta | confirmar PCB V2: display CO5300/touch CST820 vs V1 SH8601/FT3168, GPIO/driver |
| LILYGO T-Watch Ultra, 16MB/8MB | Bosch BHI260AP | pesquisa avançada com sensor inteligente, rádio e maior bateria nominal | custo/estoque, porte, integração firmware Bosch/SDK, consumo e disponibilidade |
| M5Stack StopWatch ESP32-S3 | BMI270 | banco para IMU, interface/háptica circular | não é automaticamente relógio com pulseira; montagem e peso |
| M5Stack StickS3 | BMI270 | alternativa de bancada com perfil compacto | bateria menor e forma não-wrist, não aprovação final |
| Waveshare ESP32-C6 Touch AMOLED 2.06/1.8 | QMI8658 | hipótese de Thread/Zigbee via gateway | orçamento de memória, framework e necessidade real do 802.15.4 |
| Waveshare S3 AMOLED 1.75/1.75B/1.75G/1.75C/2.16 | QMI8658 conforme SKU | referências de hardware e projeto de caixa custom | display, bateria, mecânica e variantes não equivalentes |
| Hacktor-Watch 2.0 e designs abertos | LSM6DSLTR no projeto referenciado | benchmark de construção própria/serviço | disponibilidade de unidade pronta não confirmada |
| LILYGO T-Watch S3/S3 Plus; Watchy/PineTime/Bangle.js 2 | somente acelerômetro no SKU considerado | referências de firmware, energia, BLE/LoRa | **não elegíveis como IMU 6 eixos do flagship**, salvo revisão comprovada |

**Não inventar ranking numérico:** para compra aprovar gates eliminatórios e registrar notas ponderadas com razões, incerteza, pesos congelados e preço final real. Melhor desenvolvimento inicial ≠ melhor produto para monitorar idosos. A lista não prova certificação, autonomia nem capacidade clínica.

## 2. Dossiê de compra/recebimento (HW-00/HW-08)

Registrar em `docs/hardware/evidence/<session>/` apenas metadados publicáveis; documentos privados e dados de pessoas ficam fora do Git. Evidência mínima: `vendor_url`, data da oferta, título exato, SKU, PCB/lot/revision, tensão/bateria/acessórios, código de anúncio, total BRL (produto+impostos+frete), prazo, política de devolução, fotos de frente/verso que não revelem dados sensíveis. Preços anteriores dos prints (Waveshare 2.06 sem bateria R$277,13; LILYGO S3 R$418,27; S3 Plus R$641,81) são **registros históricos**, não checkout atual.

Analisar peso e dimensões reais com pulseira, fixação, localização de antena, encaixe de USB, possibilidade de acionamento SOS com baixa destreza, recarga por cuidador, temperatura sob carga, risco de água/suor e fragilidade de placa de desenvolvimento. Nenhuma placa é considerada impermeável sem comprovação de fabricante/teste apropriado; não realizar ensaio destrutivo em voluntários.

## 3. Plano de port sem regressão da baseline

O `platformio.ini` da bancada fixa `esp32dev`, Arduino e `espressif32@7.1.3`; `SensorMPU6050` e `SensorSignal` são portáveis **apenas em parte**. Portar por adaptadores mínimos, não por reescrita do repositório.

Interface alvo `MotionSensor` deve retornar `SensorReading` com `ax/ay/az (g)`, `gx/gy/gz (°/s)`, `monotonic_us`, `sequence`, `validity`, `sensor_id`, `board_id`, `mount_transform`, `accel_range`, `gyro_range`, `accel_odr_hz`, `gyro_odr_hz`, `filter_profile`, `calibration_revision` e flags para saturação/drop. Separar status da leitura do conteúdo dos seis valores; `NaN`/falha I²C não equivalem a repouso. Não simular giro em sensor accel-only e não reutilizar eixo do MPU sem mapeamento da PCB/mão.

Implementação incremental:
1. Criar contrato, teste host fake e regressão de `FallDetector`, preservando hash e comportamento semântico da baseline A.
2. Introduzir driver `QMI8658` sob flag/build profile específico, primeiro ID/bus e conversão unidades; documentar taxa efetiva, FIFO, IRQ, saturação e orientação. Não supor que todos os sensores trabalham exatamente a 50 Hz; validar ODR nativo e remapeamento.
3. Qualificar plataforma S3: display/touch/PMU/SD/buttons **depois** do sensor e outbox; nunca permitir UI impedir SOS, watchdog ou leitura.
4. Somente após primeiro port, spikes isolados de `BMI270`/`BHI260AP` para concorrentes. No Bosch, separar dados crus de sensores virtuais/fusão e documentar firmware do coprocessador.
5. Não habilitar OTA, perfis aprendidos, BLE gateway ou DL antes de memória e falhas básicas medidas. Manter plataforma antiga buildável e teste nativo verde.

## 4. Memória, flash e OTA (F-28)

O build anterior **1.193.313 B / 1.310.720 B ≈ 91%** da antiga partição de aplicativo não implica que um chip de 16/32 MB comporte OTA automaticamente. Arquitetar dois layouts explicitamente:
- `bench-legacy`: particionamento atual para regressão/recovery.
- `wearable-s3`: partição de app com margem, NVS protegida, filesystem de outbox separado do storage científico, tabela de partição versionada e duas imagens OTA apenas se houver espaço demonstrado.
- `wearable-c6`: escolha independente com orçamento próprio e sem presumir PSRAM S3.

Produzir `partition.csv`, tamanhos verificáveis, `firmware.bin`, `map`, uso máximo de heap/PSRAM/stack, tamanho do arquivo crítico mínimo e pior caso de 16 eventos. Regressões obrigatórias: upgrade conservando eventos pendentes, power-cut em migração, rollback de binário e schema, particionamento incompatível detectado e recuperação de fábrica sem liberar segredos. Never erase without backup plan: a flash layout change may format critical data; codify migration/rollback or block the release.

## 5. Energia: medir sem confundir bateria nominal com autonomia

Medir corrente, picos e carga consumida com instrumento apropriado (por exemplo monitor USB/bateria compatível; garantir que a medição inclua consumo real da PMU). Separar: `IMU 20 Hz display off`, `IMU 50/62.5/100/125Hz`, `Wi-Fi conectado idle`, `MQTT/TLS reconnect`, `SOS+ACK`, `screen on`, `SD recorder`, `bad RSSI`, `offline retry`, `IMU interrupt wake`, `low battery`. Duração medida com firmware e bateria reais, capacidade confirmada e temperatura ambiente registrados. Calcular intervalos de confiança ao repetir, não extrapolar o consumo de deep sleep para detector ativo.

Prioridade de design: IMU/decisão local sempre disponíveis dentro das restrições de energia; display e pesquisa podem degradar primeiro. Modos de suspensão que perdem amostras críticas são **experimentos**, não otimizações aprovadas.

## 6. HIL — aceitação física e gate operacional

| Gate | Teste físico mínimo | Evidência | Estado |
|---|---|---|---|
| HW-01 | 6 canais com gesto estático/dinâmico conhecido, taxa efetiva, perda/jitter, conversão, IRQ e eixo | CSV raw + manifest + oscilogramas/plots + logs | **pendente** |
| HW-02 | nosso firmware faz boot e build no SKU/revisão; recovery stock documentado | SHA, PlatformIO env, USB log, foto PCB | **pendente** |
| HW-03 | Wi-Fi próprio, TLS/ACL, rede cai/volta, backend commit+ACK | trace de eventos/outbox/DB, nenhum Android gateway | **pendente** |
| HW-04 | autonomia, descarga, reconexão e temperatura por perfil | traço de consumo, bateria especificada, medidas repetidas | **pendente** |
| HW-05 | SOS físico, leitura, vibração/áudio, conforto de pulseira | checklist funcional, sem participantes vulneráveis | **pendente** |
| HW-06 | flash/layout/OTA candidato sem apagar fila; stress heap | bin/map/partition e regressões de reset | **pendente** |
| HW-07 | gravação raw e export íntegra com metadados e sequência | hash do raw, contadores de gap, reimport | **pendente** |
| HW-09 | reboot com evento pendente, flash cheio, broker down, ACK falso e watchdog | registros de fault injection, evidências | **pendente** |

Os experimentos de queda física iniciais usam objeto/manequim e condições seguras; não inferir eficácia em idosos por esse ensaio. Se a placa não foi adquirida, registrar `blocked: hardware not available`, não `passed`.

## 7. Fontes e decisões
- [Waveshare S3 1.8 V1/V2](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8): controladores diferentes, revisar pinagem por PCB.
- [Waveshare S3 2.06 exemplos](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06).
- [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch).
- [Bosch BHI260AP](https://www.bosch-sensortec.com/products/smart-sensor-systems/bhi260ap/).
- [ADR-021](../adr/README.md#adr-021--pulso--imu-completa-como-alvo-de-qualificação), [Nyquist](../research/wrist-sampling-nyquist-protocol.md), [gates QA](../quality/wrist-motionlab-verification.md).

**Decisão sugerida:** comprar inicialmente **um** Waveshare S3 2.06 com bateria e montagem confirmadas; candidato provisório, não homologado. Reavaliar segundo hardware após dados físicos.
