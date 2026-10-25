"""Adaptador WEDA-FALL (Marques e Moreno, Sensors 23(3):1146, 2023; github.com/joaojtmarques/WEDA-FALL).

Fatos verificados no README e nos arquivos do repositorio (2026-10-25):
- Fitbit Sense no pulso; diretorios ``dataset/<taxa>/<codigo>/U<id>_R<trial>_<sensor>.csv``
  com taxas 50Hz, 40Hz, 25Hz, 10Hz e 5Hz; as 4 menores foram DERIVADAS da de 50 Hz
  (metodo de reamostragem/antialias nao documentado: nao usar como condicao Nyquist).
- CSV ``accel_time_list,accel_x_list,...`` e ``gyro_time_list,...``; tempo em segundos.
- Codigos F01..F08 sao quedas (jovens, em colchao); D01..D11 sao ADLs. Participantes
  U01..U14 jovens; U21..U31 idosos, que NAO realizaram quedas.
- ``dataset/fall_timestamps.csv``: ``filename,start_time,end_time`` (s), anotacao manual
  do inicio/fim da queda, que o proprio README diz poder conter erros.

Inferido dos dados (nao declarado no README) e por isso explicito aqui:
- aceleracao em m/s^2 (|a| em repouso ~ 9,8) e giroscopio em rad/s;
- os timestamps de "50Hz" chegam em rajadas (~4 amostras em poucos ms, depois ~120 ms),
  com taxa media efetiva perto de 40 Hz: parecem instante de ENTREGA do lote, nao de
  conversao. Por isso a base de tempo e uma opcao explicita:
    * ``recorded`` (padrao): usa os timestamps como estao;
    * ``uniform_index``: suposicao declarada de amostragem uniforme dentro do trial.

Licenca: o repositorio nao declara licenca. Uso autorizado pelo autor por e-mail, conforme
relatado pelo responsavel do projeto (2026-10-25): ver e analisar, SEM copiar/redistribuir.
Os dados nunca sao versionados neste repositorio.
"""

from __future__ import annotations

import csv
import math
import re
from pathlib import Path
from typing import Dict, Iterator, List, Optional, Sequence, Tuple

from ..model import DatasetFormatError, Sample, Trial
from . import sha256_file

DATASET = "weda_fall"
SENSOR_POSITION = "wrist"
STANDARD_GRAVITY = 9.80665
RAD_TO_DEG = 180.0 / math.pi
TIMEBASES = ("recorded", "uniform_index")
NOMINAL_RATES = {"50Hz": 50.0, "40Hz": 40.0, "25Hz": 25.0, "10Hz": 10.0, "5Hz": 5.0}
FILE_PATTERN = re.compile(r"^U(?P<user>\d{2})_R(?P<trial>\d{2})_(?P<sensor>accel|gyro)\.csv$")
CODE_PATTERN = re.compile(r"^(?P<kind>[FD])(?P<number>\d{2})$")
ELDER_MIN_USER = 21

Series = List[Tuple[float, float, float, float]]


def population_for(user_id: int) -> str:
    return "elder" if user_id >= ELDER_MIN_USER else "young"


def read_series(path: Path, prefix: str) -> Series:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, None)
        expected = [f"{prefix}_time_list", f"{prefix}_x_list", f"{prefix}_y_list", f"{prefix}_z_list"]
        if header != expected:
            raise DatasetFormatError(f"{path.name}: cabecalho esperado {','.join(expected)}")
        series: Series = []
        for row in reader:
            if not row:
                continue
            try:
                values = tuple(float(value) for value in row[:4])
            except ValueError as error:
                raise DatasetFormatError(f"{path.name}: valor nao numerico") from error
            if len(values) != 4 or not all(math.isfinite(value) for value in values):
                raise DatasetFormatError(f"{path.name}: linha incompleta ou nao finita")
            series.append(values)  # type: ignore[arg-type]
    if len(series) < 2:
        raise DatasetFormatError(f"{path.name}: menos de 2 amostras")
    return series


def retime_uniform(series: Series) -> Series:
    """Suposicao declarada: amostras uniformes entre o primeiro e o ultimo timestamp."""
    start, end = series[0][0], series[-1][0]
    step = (end - start) / (len(series) - 1)
    return [(start + index * step, x, y, z) for index, (_t, x, y, z) in enumerate(series)]


def timing_quality(series: Series) -> Dict[str, float]:
    times = [row[0] for row in series]
    deltas = sorted(b - a for a, b in zip(times, times[1:]))
    span = times[-1] - times[0]
    return {
        "samples": len(series),
        "span_s": span,
        "effective_rate_hz": (len(series) - 1) / span if span > 0 else float("nan"),
        "dt_median_ms": deltas[len(deltas) // 2] * 1000.0,
        "dt_max_ms": deltas[-1] * 1000.0,
        "non_increasing_steps": sum(1 for delta in deltas if delta <= 0),
    }


def merge_zero_order_hold(accel: Series, gyro: Series) -> List[Sample]:
    """Une os dois fluxos em t crescente, segurando o ultimo valor de cada sensor.

    Nao interpola nem extrapola: comeca quando ambos ja tem amostra e colapsa
    timestamps repetidos (fica o ultimo valor daquele instante).
    """
    events = sorted([(row[0], 0, row[1:]) for row in accel] + [(row[0], 1, row[1:]) for row in gyro],
                    key=lambda item: (item[0], item[1]))
    last: List[Optional[Sequence[float]]] = [None, None]
    merged: List[Sample] = []
    for t_s, kind, values in events:
        last[kind] = values
        if last[0] is None or last[1] is None:
            continue
        ax, ay, az = (value / STANDARD_GRAVITY for value in last[0])
        gx, gy, gz = (value * RAD_TO_DEG for value in last[1])
        sample = (t_s * 1000.0, ax, ay, az, gx, gy, gz)
        if merged and merged[-1][0] == sample[0]:
            merged[-1] = sample
        else:
            merged.append(sample)
    return merged


def load_fall_windows(path: Path) -> Dict[str, Tuple[float, float]]:
    windows: Dict[str, Tuple[float, float]] = {}
    if not path.exists():
        return windows
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if set(reader.fieldnames or []) < {"filename", "start_time", "end_time"}:
            raise DatasetFormatError("fall_timestamps.csv sem colunas filename,start_time,end_time")
        for row in reader:
            key = row["filename"].strip()
            try:
                start, end = float(row["start_time"]), float(row["end_time"])
            except ValueError as error:
                raise DatasetFormatError(f"fall_timestamps.csv: janela invalida para {key}") from error
            if end < start:
                raise DatasetFormatError(f"fall_timestamps.csv: fim antes do inicio para {key}")
            windows[key] = (start, end)
    return windows


def iter_trials(root: Path,
                subjects: Optional[set] = None,
                rate_dir: str = "50Hz",
                timebase: str = "recorded",
                excluded: Optional[List[Dict]] = None,
                **_unused) -> Iterator[Trial]:
    """``root`` e o diretorio ``dataset`` do WEDA-FALL."""
    if timebase not in TIMEBASES:
        raise ValueError(f"timebase deve ser um de {TIMEBASES}")
    if rate_dir not in NOMINAL_RATES:
        raise ValueError(f"rate_dir deve ser um de {sorted(NOMINAL_RATES)}")
    excluded = excluded if excluded is not None else []
    rate_root = root / rate_dir
    if not rate_root.is_dir():
        raise DatasetFormatError(f"diretorio {rate_root} ausente")
    windows = load_fall_windows(root / "fall_timestamps.csv")

    for code_dir in sorted(path for path in rate_root.iterdir() if path.is_dir()):
        code = CODE_PATTERN.match(code_dir.name)
        if not code:
            continue
        label = "fall" if code["kind"] == "F" else "adl"
        pairs: Dict[Tuple[int, int], Dict[str, Path]] = {}
        for path in sorted(code_dir.glob("*.csv")):
            match = FILE_PATTERN.match(path.name)
            if match:
                key = (int(match["user"]), int(match["trial"]))
                pairs.setdefault(key, {})[match["sensor"]] = path

        for (user, trial_number), files in sorted(pairs.items()):
            subject_id = f"U{user:02d}"
            if subjects and subject_id not in subjects:
                continue
            trial_id = f"{code_dir.name}/{subject_id}_R{trial_number:02d}"
            if set(files) != {"accel", "gyro"}:
                excluded.append({"trial_id": trial_id, "reason": f"sensores incompletos: {sorted(files)}"})
                continue
            try:
                accel = read_series(files["accel"], "accel")
                gyro = read_series(files["gyro"], "gyro")
            except DatasetFormatError as error:
                excluded.append({"trial_id": trial_id, "reason": str(error)})
                continue

            quality = {"accel": timing_quality(accel), "gyro": timing_quality(gyro)}
            if timebase == "uniform_index":
                accel, gyro = retime_uniform(accel), retime_uniform(gyro)
            samples = merge_zero_order_hold(accel, gyro)
            if len(samples) < 2:
                excluded.append({"trial_id": trial_id, "reason": "sem sobreposicao temporal accel/gyro"})
                continue

            onset_ms = impact_ms = None
            notes = [
                f"population={population_for(user)}",
                f"timebase={timebase}",
                f"accel_effective_rate_hz={quality['accel']['effective_rate_hz']:.2f}",
                f"accel_dt_max_ms={quality['accel']['dt_max_ms']:.1f}",
            ]
            if label == "fall":
                window = windows.get(f"{code_dir.name}/{subject_id}_R{trial_number:02d}")
                if window is None:
                    notes.append("fall_window=missing")
                else:
                    # O inicio anotado e a referencia; "impacto" nao e anotado separadamente.
                    onset_ms = window[0] * 1000.0
                    notes.append(f"fall_window_s={window[0]}-{window[1]}")

            yield Trial(
                trial_id=trial_id,
                dataset=DATASET,
                subject_id=subject_id,
                activity_code=code_dir.name,
                label=label,
                samples=samples,
                source_rate_hz=NOMINAL_RATES[rate_dir],
                sensor_position=SENSOR_POSITION,
                data_kind="real",
                source_file=f"{rate_dir}/{trial_id}",
                source_sha256=sha256_file(files["accel"]) + ":" + sha256_file(files["gyro"]),
                fall_onset_ms=onset_ms,
                fall_impact_ms=impact_ms,
                notes=notes,
            )
