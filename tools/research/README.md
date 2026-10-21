# fallbench — avaliação offline reprodutível do detector

Somente biblioteca padrão do Python 3.12 e `g++` com C++17. O detector não é reimplementado: `fall_replay` é compilado de `src/fall_detector.cpp`, `src/sensor_signal.cpp` e `src/angle_math.cpp`.

Protocolo e limitações: [docs/research/experimental-protocol.md](../../docs/research/experimental-protocol.md). Datasets: [docs/research/datasets.md](../../docs/research/datasets.md).

## Compilar o replay

```bash
bash tools/research/build_replay.sh          # Linux/macOS/CI
bash scripts/pio-docker.sh research          # Windows sem g++: compila e testa em Docker
```

## Testes (fixtures sintéticas)

```bash
cd tools/research
python -m unittest discover -s tests -v      # FALLBENCH_REQUIRE_REPLAY=1 falha se o binário faltar
python fixtures/make_synthetic.py            # regenera as fixtures; o CI verifica que nada muda
```

## Avaliar um dataset

Dados reais ficam fora do Git (`tools/research/datasets/` e `tools/research/out/` são ignorados).

```bash
cd tools/research
# SisFall: confirme os fatores de conversão no Readme do dataset e passe-os explicitamente
python -m fallbench evaluate --adapter sisfall --root datasets/SisFall_dataset \
  --accel-g-per-count <confirmado> --gyro-dps-per-count <confirmado> \
  --axis-map "x=+x,y=+y,z=+z" --out out/sisfall-A

# KFall: exporte os rótulos .xlsx para CSV com subject_id,task_code,trial_id,fall_onset_frame,fall_impact_frame
python -m fallbench evaluate --adapter kfall --root datasets/KFall/sensor_data \
  --labels datasets/KFall/labels.csv --out out/kfall-A

# Split por participante para a condição B (registrar antes de calibrar)
python -m fallbench split --adapter sisfall --root datasets/SisFall_dataset --seed tg1-2026 --dev-fraction 0.5
```

Saídas: `results.json` (protocolo, proveniência, métricas, trials) e `per_trial.csv`. Um relatório com `data_kind = synthetic` nunca é resultado experimental.
