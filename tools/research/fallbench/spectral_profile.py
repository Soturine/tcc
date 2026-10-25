"""Perfil espectral por trial do WEDA-FALL (protocolo Nyquist, sec. 7, passos 3-4).

Cada fluxo (acelerometro, giroscopio) e analisado SEPARADAMENTE na sua propria base
uniforme declarada (``uniform_index``), porque DFT exige amostragem uniforme e os
timestamps gravados chegam em rajadas. Os resultados sao limitados pela instrumentacao:
Nyquist da fonte (~25 Hz para 50 Hz) e filtragem interna do Fitbit nao documentada.
Uma banda observada ate 25 Hz NAO prova ausencia de conteudo acima.
"""

from __future__ import annotations

import math
import statistics
from pathlib import Path
from typing import Dict, List, Optional

from . import signal_lab
from .adapters import weda_fall

ENERGY_FRACTIONS = (0.90, 0.95, 0.99)
# Nyquist do laco do firmware (leitura a cada 50 ms).
FIRMWARE_NYQUIST_HZ = 10.0


def _magnitude(series, scale: float) -> List[float]:
    return [math.sqrt(x * x + y * y + z * z) * scale for _t, x, y, z in series]


def energy_above(signal: List[float], fs_hz: float, cutoff_hz: float) -> float:
    freqs, amps = signal_lab.magnitude_spectrum(signal, fs_hz)
    power = [a * a for a in amps]
    total = sum(power[1:])
    if total == 0:
        return 0.0
    return sum(p for f, p in zip(freqs[1:], power[1:]) if f > cutoff_hz) / total


def profile_stream(series, scale: float) -> Optional[Dict]:
    retimed = weda_fall.retime_uniform(series)
    span = retimed[-1][0] - retimed[0][0]
    if span <= 0 or len(retimed) < 64:
        return None
    fs = (len(retimed) - 1) / span
    signal = _magnitude(retimed, scale)
    return {
        "fs_hz": fs,
        "band_edge_hz": {str(f): signal_lab.band_edge_hz(signal, fs, f) for f in ENERGY_FRACTIONS},
        "energy_above_firmware_nyquist": energy_above(signal, fs, FIRMWARE_NYQUIST_HZ),
    }


def profile_dataset(root: Path, rate_dir: str = "50Hz") -> Dict:
    rows = []
    for code_dir in sorted(p for p in (root / rate_dir).iterdir() if p.is_dir()):
        code = weda_fall.CODE_PATTERN.match(code_dir.name)
        if not code:
            continue
        label = "fall" if code["kind"] == "F" else "adl"
        for accel_path in sorted(code_dir.glob("*_accel.csv")):
            match = weda_fall.FILE_PATTERN.match(accel_path.name)
            if not match:
                continue
            gyro_path = accel_path.with_name(accel_path.name.replace("_accel.csv", "_gyro.csv"))
            entry = {"label": label, "activity_code": code_dir.name,
                     "population": weda_fall.population_for(int(match["user"]))}
            try:
                entry["accel"] = profile_stream(weda_fall.read_series(accel_path, "accel"),
                                                1.0 / weda_fall.STANDARD_GRAVITY)
                entry["gyro"] = (profile_stream(weda_fall.read_series(gyro_path, "gyro"), weda_fall.RAD_TO_DEG)
                                 if gyro_path.exists() else None)
            except weda_fall.DatasetFormatError:
                continue
            rows.append(entry)
    return summarize(rows)


def _describe(values: List[float]) -> Optional[Dict]:
    if not values:
        return None
    ordered = sorted(values)
    return {
        "n": len(ordered),
        "median": round(statistics.median(ordered), 3),
        "p90": round(ordered[min(len(ordered) - 1, int(0.9 * len(ordered)))], 3),
        "max": round(ordered[-1], 3),
    }


def summarize(rows: List[Dict]) -> Dict:
    groups: Dict[str, List[Dict]] = {}
    for row in rows:
        groups.setdefault(f"{row['label']}/{row['population']}", []).append(row)
    summary = {}
    for group, items in sorted(groups.items()):
        summary[group] = {}
        for stream in ("accel", "gyro"):
            usable = [item[stream] for item in items if item.get(stream)]
            summary[group][stream] = {
                "trials": len(usable),
                "fs_hz": _describe([u["fs_hz"] for u in usable]),
                "energy_above_10hz": _describe([u["energy_above_firmware_nyquist"] for u in usable]),
                **{f"band_edge_{f}": _describe([u["band_edge_hz"][str(f)] for u in usable])
                   for f in ENERGY_FRACTIONS},
            }
    return {
        "data_kind": "real" if rows else "empty",
        "signal": "magnitude of each stream (accel in g, gyro in dps), mean removed, Hann window",
        "timebase": "uniform_index (declared assumption per stream)",
        "firmware_nyquist_hz": FIRMWARE_NYQUIST_HZ,
        "limits": [
            "Banda observavel limitada a ~fs/2 da fonte (~25 Hz) e a filtragem interna nao documentada do Fitbit.",
            "Magnitude remove direcao; analise por eixo fica para ensaio com a IMU alvo.",
        ],
        "groups": summary,
    }
