# Replay e caracterização host/native do FallDetector

## Status e objetivo

O replay está **implemented** e foi **validated** com testes PlatformIO native usando sinais sintéticos. Ele torna a FSM experimental reproduzível sem ESP32, MPU6050, Wi-Fi, MQTT, NVS, `sleep` ou relógio real.

Esta validação comprova somente o comportamento do código para as sequências definidas. Ela não mede acurácia, sensibilidade, especificidade, precisão, F1 ou taxa real de falsos positivos e não substitui HIL ou ensaio físico seguro.

## Arquitetura

```text
gerador sintético atual          parser separado futuro
          │                       CSV/JSON capturado
          └──────────┬───────────────────┘
                     ▼
          std::vector<SensorReading>
                     │
                     ▼
          FallDetectorReplay::run()
                     │
                     ▼
              FallDetector real
                     │
                     ▼
      alertas + índices + candidato pendente
```

O harness cria um detector novo por execução, percorre as leituras na ordem recebida e chama `FallDetector::update()` uma vez por amostra. O único tempo usado pela FSM é `SensorReading.timestampMs`.

Componentes:

- `test/support/fall_detector_replay.h`: runner e resumo do replay;
- `test/support/synthetic_sensor_readings.h`: builders nomeados para sinais sintéticos;
- `test/test_fall_detector_replay/test_main.cpp`: cenários de caracterização;
- `test/native_stubs/Arduino.h`: superfície mínima de teste para compilar o detector no host;
- `platformio.ini`: inclui `fall_detector.cpp` no ambiente native.

## Cenários sintéticos caracterizados

| Cenário | Resultado esperado e observado após a correção |
|---|---|
| repouso | nenhuma queda |
| impacto isolado | nenhuma queda; candidato expira sem orientação |
| impacto + orientação sem imobilidade suficiente | nenhuma queda; candidato expira |
| impacto + orientação + imobilidade suficiente | exatamente uma queda |
| sequência equivalente com timestamps não uniformes | exatamente uma queda |
| movimento/orientação sem impacto | nenhuma queda |
| cruzamento `+179°/-179°` após impacto | nenhuma falsa mudança angular e nenhuma queda |
| leitura inválida durante candidato | nenhum falso evento por intervalo sem observação |
| duas quedas válidas separadas | exatamente duas quedas, em ordem |

Os valores usam os thresholds normais existentes e margens nomeadas nos builders. Nenhum threshold foi alterado para produzir os resultados.

## Bug objetivo encontrado

O primeiro replay do cenário com leitura inválida produziu uma queda falsa. O detector mantinha o timestamp da última leitura válida e, na amostra válida seguinte, somava todo o intervalo desconhecido como imobilidade.

Classificação: **bug objetivo de contabilidade temporal**. Uma lacuna sem leitura válida não comprova imobilidade.

Correção: uma leitura inválida zera a duração de imobilidade observada e impede que a próxima amostra válida inclua o intervalo desconhecido. A FSM, as janelas e os thresholds permaneceram inalterados.

## Execução

```text
platformio test -e native
```

O ambiente native executa as regressões P1 anteriores e os cenários de replay. O build embarcado continua sendo verificado separadamente com:

```text
platformio run -e esp32dev
```

## Limitações e evolução futura

- sinais atuais são sintéticos e não representam movimentos humanos validados;
- não há parser CSV/JSON nesta etapa;
- não há golden files ou datasets grandes;
- não há HIL, MPU6050 físico ou validação de orientação dinâmica;
- o harness caracteriza a FSM, mas não calibra thresholds;
- sensor fusion, Madgwick/Mahony, DMP, ML e TinyML permanecem **deferred** até existir evidência compatível.

Quando leituras reais forem capturadas com protocolo seguro, um parser separado poderá convertê-las em `SensorReading[]` e reutilizar o mesmo runner sem acoplar formatos de arquivo ao detector.
