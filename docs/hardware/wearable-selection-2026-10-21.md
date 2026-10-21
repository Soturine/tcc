# Seleção de wearable de pulso — matriz técnica e aquisição (2026-10-21)

**Estado: proposta de qualificação; nenhum modelo comprado, portado ou validado fisicamente.** A localização principal da IMU no produto é **pulso**. Cintura/lombar continuam como evidência histórica, nunca equivalentes metodologicamente. Candidatos só passam no filtro obrigatório com **acelerômetro triaxial + giroscópio triaxial acessíveis**, Wi-Fi autônomo, firmware próprio, SDK/driver, bateria identificável e possibilidade de uso no pulso. Excluem-se Watchy, PineTime, Bangle.js 2, LILYGO S3 e S3 Plus e variantes apenas com acelerômetro, ainda que tenham LoRa/GNSS. Não inferir giroscópio pelo nome "watch".

## Matriz de candidaturas (especificação documental, não ensaio)
| Modelo / revisão | ESP/IMU | Memória | Aquisição, firmware e telemetria | Energia, construção e maior incerteza | Decisão |
|---|---|---|---|---|---|
| Waveshare ESP32-S3 Touch AMOLED 2.06 | ESP32-S3, QMI8658 6 eixos | 32 MB flash / 8 MB PSRAM | Exemplos Arduino/ESP-IDF/PlatformIO de terceiros, tela LVGL, IMU, PMU AXP2101, microSD, USB; driver novo obrigatório | Referência 400 mAh, caixa/pulseira conforme kit, autonomia do nosso firmware **não medida**, variante -EN sem bateria | **Primeiro candidato para bring-up**, condicionado ao gate |
| Waveshare ESP32-S3 Touch AMOLED 1.8 **V2** | ESP32-S3, QMI8658 | 16 MB / 8 MB | Drivers e documentação; conferir pinos, display e touch V1 vs V2 | Bateria/pulseira/case devem ser confirmados por SKU; ergonomia a medir | Reserva compacta |
| LILYGO **T-Watch Ultra** | ESP32-S3, Bosch BHI260AP 6 eixos | 16 MB / 8 MB | LilyGoLib e firmware Bosch no sensor; USB, Wi-Fi/BLE; GNSS/LoRa/NFC conforme configuração; recursos inteligentes exigem spike | Bateria nominal 1100 mAh, construção grande, autonomia não medida, estoque/preço brasileiro não comprovados | Finalista avançado condicionado |
| M5Stack **StopWatch** revisão confirmada | ESP32-S3, BMI270 6 eixos | Conferir SKU; documentação de desenvolvimento | APIs M5Unified, acesso accel/gyro e interface circular | Necessita comprovação de caixa e pulseira utilizáveis; não pressupor wearable pronto | Plataforma de laboratório/design, condicionada |
| Waveshare ESP32-C6 Touch AMOLED 2.06 | ESP32-C6, QMI8658 | Capacidade/revisão conferir; menos memória operacional que S3 com PSRAM | Wi-Fi 6, BLE, 802.15.4/Thread/Zigbee, IMU acessível; port C6 separado | Thread/Zigbee exigem gateway; budget UI/ML/flash a medir | Spike alternativo |
| Waveshare ESP32-S3 AMOLED 1.75/1.75C, 2.16; M5Stack StickS3 | ESP32-S3, QMI8658/BMI270 conforme SKU | Consultar revisão específica | Boas plataformas de experimentação, não pressupor kit de relógio | Exigem avaliação de pulseira/caixa e energia | Reserva mecânica/bench |

**Não confundir** capacidade nominal de bateria com autonomia; espessura, resistência à água, conforto, carga, temperatura, certificações e disponibilidade devem ser verificados e documentados, sem declarar adequação clínica.

## Compra no Brasil
Priorizar AliExpress Brasil, Mercado Livre, Shopee e revendedores nacionais; documentos oficiais apenas para especificações. Guardar URL de anúncio exato, SKU/variante, capturas com data, vendedor, reputação, valor em BRL, tributos, frete, devolução, estoque, prazo e evidência da pulseira e bateria. Ofertas **históricas/ilustrativas**, NÃO cotação vigente: captura do usuário para Waveshare 2.06 sem bateria ~R$277,13 somados preço/impostos/frete; S3 e S3 Plus descartados apesar de anúncios ~R$418,27/~R$641,81; estimativa histórica T-Watch Ultra ~R$876 **antes** de tributos sem checkout atual confirmado. Não inventar preço para outros finalistas. Antes da compra, reconfirmar checkout real.

## Qualificação objetiva e evidências
| Gate | Prova necessária | Situação |
|---|---|---|
| HW-00 identidade | SKU, PCB/revisão, foto, pinout, display/touch, firmware stock e estratégia de recovery | pending |
| HW-01 IMU | WHO_AM_I/ID, seis canais **reais**, faixas, unidades, precisão, eixos, montagem, Hz, FIFO/IRQ, drift e gaps | pending |
| HW-02 port | Build/upload/boot nosso, HAL e drivers, compatibilidade PlatformIO, Arduino/ESP-IDF, regressão do detector | pending |
| HW-03 rede | Wi-Fi sem Android, MQTT/TLS, ACL device, quedas de link e outbox ACK pós-commit | pending |
| HW-04 energia | PMU, bateria real, corrente por modo, execução 20/50Hz, Wi-Fi, tela, gravação, reconexão, calor, carregamento | pending |
| HW-05 ergonomia | Massa/dimensões no braço, cinta, conforto, SOS, interface legível, segurança elétrica, resistência declarada vs observada | pending |
| HW-06 memória | flash/PSRAM, partições de app dupla/OTA, LittleFS, buffers, stack/heap, watchdog, tabela de partições e recuperação | pending |
| HW-07 coleta | 6 eixos sem sinais sintetizados, clock monotônico, sequência, microSD/local, hash, export/reimport, captura sem perda crítica | pending |
| HW-08 compra | final BRL, impostos, garantia, disponibilidade, política de devolução, kit, caixa/pulseira/bateria | pending |
| HW-09 HIL | testes com dispositivo e Android físicos, bateria/energia/reboot/falha e evidência reproduzível | pending |

**Corte:** se faltar IMU completa ou firmware próprio, eliminar; se bateria ou ergonomia falhar, não aprovar para monitoração prolongada. Decidir primeiro protótipo após HW-00/08; decisão definitiva apenas HW-09.

## Integração no repo
Hoje `platformio.ini` fixa `esp32dev` e Arduino `espressif32@7.1.3`; driver `SensorMPU6050` possui compensações e assume configurações do MPU. Criar interface `MotionSensor` + `BoardProfile`; drivers QMI8658, BHI260AP, BMI270 por perfil e build flags separadas, preservando a FSM e `SensorSignal`. Nunca fingir que passou o port pela compilação de demos do fabricante. `FallDetector` atual parte de impacto→orientação→imobilidade e pode não generalizar ao pulso.

## Fontes técnicas para verificação
- Waveshare 2.06: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06 ; exemplos: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06
- Waveshare 1.8: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8
- LILYGO Ultra: https://wiki.lilygo.cc/products/t-watch-series/t-watch-ultra/
- Bosch BHI260AP: https://www.bosch-sensortec.com/products/smart-sensor-systems/bhi260ap/
- M5Stack StopWatch: https://docs.m5stack.com/en/core/StopWatch
- Ofertas são voláteis; jamais usar URL de manual como confirmação de preço/estoque.
