# Baseline congelada do detector (condição A)

## Identidade

| Campo | Valor |
|---|---|
| `algorithm_version` | `threshold_fsm_v2_time_features_v1` (`include/app_config.h`, `FALL_DECISION_ENGINE_VERSION`) |
| Perfil | `normal` (default operacional); `demo` só por seleção explícita e nunca usado para métricas |
| Identidade semântica | `detector_parameters_sha256 = f1c21aec8826da17e425e43f44d1da1109495feb6e77be9f746433c8eccd6f8f` (perfil normal) |
| Como recalcular | `python -m fallbench evaluate ...` grava o hash em `results.json → protocol.detector_parameters_sha256`; o hash cobre exatamente a linha `config` emitida pelo binário `fall_replay` |
| Hashes de arquivo | `results.json → provenance.detector.files` (fim de linha normalizado para LF) |

O hash semântico muda somente se um parâmetro efetivo do detector mudar. Qualquer mudança desse tipo cria uma nova condição (B ou posterior) e **não** pode ser apresentada como a baseline A.

## Cadeia de processamento por amostra

```text
MPU (±8 g, ±500 °/s solicitados)                  dataset normalizado (g, °/s)
        │ leitura a cada 50 ms (normal)                     │ mapeamento de eixos explícito
        ▼                                                   │ reamostragem zero-order hold a 50 ms
  offsets de calibração do acelerômetro                     │ saturação em ±8 g / ±500 °/s
        │                                                   │ lacuna > max_gap_ms → valid=0
        └──────────────────────┬────────────────────────────┘
                               ▼
     SensorSignal::applyLowPass  (IIR por amostra, α = 0,75 acc e giro)
                               ▼
     SensorSignal::computeDerivedValues
       |a|, |ω|, pitch = atan2(−ax, √(ay²+az²)), roll = atan2(ay, az)
                               ▼
                    FallDetector::update (FSM)
```

`src/sensor_signal.cpp` foi extraído de `src/sensor_mpu6050.cpp` nesta rodada para que firmware e replay usem a mesma função; o comportamento foi preservado e caracterizado em `test/test_sensor_signal`.

## FSM

| Estado | Entrada | Saída |
|---|---|---|
| `Monitoring` | — | impacto: `|a| ≥ 2,2 g` **e** `|ω| ≥ 120 °/s` na mesma amostra → congela pitch/roll de referência |
| `WaitingForOrientationChange` | impacto | `max(Δpitch, Δroll) ≥ 45°` (menor diferença angular, com wrap ±180°) → próximo estado; senão rejeita após 1.500 ms |
| `WaitingForImmobility` | orientação | acumula tempo entre amostras com `||a| − 1| ≤ 0,15 g` e `|ω| ≤ 15 °/s`; qualquer amostra móvel zera o acumulado; confirma ao atingir 2.000 ms; rejeita após 4.000 ms desde a entrada no estado |

Regras adicionais preservadas:

- referência de postura atualizada somente em amostras estáveis (`|ω| < 25 °/s` e `||a| − 1| < 0,20 g`), por média circular com peso 0,15;
- amostra inválida zera o acumulado de imobilidade e a contagem temporal recomeça na próxima válida (lacuna não prova imobilidade);
- timestamp regressivo produz `Δt = 0`;
- `confidence = null`, `confidence_status = "not_available"`: nenhuma probabilidade é fabricada.

## Comportamentos caracterizados (sintéticos)

Os testes `test/test_fall_detector_replay` e `tools/research/tests/test_fallbench.py` registram, sobre sinais **sintéticos**:

- repouso, impacto isolado, movimento sem impacto e wrap angular não geram detecção;
- impacto + orientação + imobilidade gera exatamente uma detecção;
- impacto + orientação seguidos de levantar e ficar imóvel **em pé** dentro da janela de 4 s ainda confirmam queda: a FSM não verifica a postura final. É uma propriedade da baseline, registrada como hipótese a investigar com dados reais, não um defeito corrigido nesta rodada;
- por causa do IIR por amostra a 20 Hz, um pico de impacto precisa persistir por mais de uma leitura para cruzar 2,2 g após o filtro (ex.: degrau de 4 g leva duas leituras). O efeito sobre impactos reais curtos é uma hipótese experimental (H1b no [protocolo](experimental-protocol.md)).

Essas observações descrevem o código; não são taxas de acerto.

## O que não está congelado aqui

- calibração automática de offsets do acelerômetro (dependente de hardware; datasets já entregam g);
- filtro digital interno (DLPF) do MPU, cuja configuração não é reproduzida no replay — ameaça à validade registrada no protocolo;
- motor experimental `precalibration_threshold_v1` (`ALERT_*`), que não participa da confirmação de queda.
