"""Gera fixtures SINTETICAS deterministicas no formato normalizado.

Servem apenas para testar adaptador -> normalizacao -> replay -> metricas.
Nao representam movimento humano e nunca devem ser reportadas como resultado.
Uso: python tools/research/fixtures/make_synthetic.py
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

OUT = Path(__file__).resolve().parent / "synthetic"
RATE_HZ = 100.0
PERIOD_MS = 1000.0 / RATE_HZ


def _gravity(angle_deg: float):
    """Gravidade girando do eixo z para o eixo x."""
    angle = math.radians(angle_deg)
    return (math.sin(angle), 0.0, math.cos(angle))


def _segment(samples, duration_ms, accel_fn, gyro_fn):
    start = samples[-1][0] + PERIOD_MS if samples else 0.0
    steps = int(round(duration_ms / PERIOD_MS))
    for step in range(steps):
        fraction = step / max(1, steps - 1)
        ax, ay, az = accel_fn(fraction)
        gx, gy, gz = gyro_fn(fraction)
        samples.append((start + step * PERIOD_MS, ax, ay, az, gx, gy, gz))


def still(angle_deg):
    return lambda _f: _gravity(angle_deg), lambda _f: (0.0, 0.0, 0.0)


def impact_rotation(from_deg, to_deg, peak_g, gyro_dps):
    def accel(fraction):
        gx, gy, gz = _gravity(from_deg + (to_deg - from_deg) * fraction)
        scale = peak_g
        return gx * scale, gy * scale, gz * scale

    return accel, lambda _f: (0.0, gyro_dps, 0.0)


def build(segments):
    samples = []
    for duration, (accel_fn, gyro_fn) in segments:
        _segment(samples, duration, accel_fn, gyro_fn)
    return samples


TRIALS = [
    {
        "trial_id": "SYN_FALL_LYING_01",
        "subject_id": "SYN01",
        "activity_code": "SYN_FALL",
        "label": "fall",
        "segments": [(2000, still(0)), (300, impact_rotation(0, 90, 4.0, 300.0)), (4000, still(90))],
        "fall_onset_ms": 1990.0,
        "fall_impact_ms": 2150.0,
    },
    {
        "trial_id": "SYN_FALL_RECOVERS_01",
        "subject_id": "SYN02",
        "activity_code": "SYN_FALL_GETS_UP",
        "label": "fall",
        "segments": [
            (2000, still(0)),
            (300, impact_rotation(0, 90, 4.0, 300.0)),
            (600, still(90)),
            (500, impact_rotation(90, 0, 1.6, 120.0)),
            (3000, still(0)),
        ],
        "fall_onset_ms": 1990.0,
        "fall_impact_ms": 2150.0,
    },
    {
        "trial_id": "SYN_ADL_JUMP_01",
        "subject_id": "SYN01",
        "activity_code": "SYN_JUMP_UPRIGHT",
        "label": "adl",
        "segments": [(2000, still(0)), (300, impact_rotation(0, 5, 4.0, 300.0)), (4000, still(0))],
    },
    {
        "trial_id": "SYN_ADL_LIE_DOWN_01",
        "subject_id": "SYN02",
        "activity_code": "SYN_LIE_DOWN_SLOW",
        "label": "adl",
        "segments": [(2000, still(0)), (2000, impact_rotation(0, 90, 1.05, 40.0)), (4000, still(90))],
    },
]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "dataset": "synthetic_fixtures",
        "data_kind": "synthetic",
        "sensor_position": "synthetic",
        "source_rate_hz": RATE_HZ,
        "generator": "tools/research/fixtures/make_synthetic.py",
        "trials": [],
    }
    for spec in TRIALS:
        name = f"{spec['trial_id']}.csv"
        with (OUT / name).open("w", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle, lineterminator="\n")
            writer.writerow(["t_ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"])
            for sample in build(spec["segments"]):
                writer.writerow([f"{value:.4f}" for value in sample])
        manifest["trials"].append({
            "file": name,
            "trial_id": spec["trial_id"],
            "subject_id": spec["subject_id"],
            "activity_code": spec["activity_code"],
            "label": spec["label"],
            "fall_onset_ms": spec.get("fall_onset_ms"),
            "fall_impact_ms": spec.get("fall_impact_ms"),
        })
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
