"""Execucao do binario ``fall_replay`` (detector real do firmware) por trial."""

from __future__ import annotations

import io
import json
import subprocess
from pathlib import Path
from typing import Dict, List

from .model import Trial
from .normalize import AxisMap, map_axes, resample_to_firmware_cadence

REPLAY_HEADER = "t_ms,valid,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps"

# Configuracao do MPU no firmware (sensor_mpu6050: +/-8 g e +/-500 deg/s solicitados).
FIRMWARE_ACCEL_RANGE_G = 8.0
FIRMWARE_GYRO_RANGE_DPS = 500.0


class ReplayError(RuntimeError):
    pass


def default_binary() -> Path:
    from .provenance import REPO_ROOT

    name = "fall_replay.exe" if (REPO_ROOT / "build/research/fall_replay.exe").exists() else "fall_replay"
    return REPO_ROOT / "build" / "research" / name


def run_replay(binary: Path, rows, mode: str) -> Dict:
    buffer = io.StringIO()
    buffer.write(REPLAY_HEADER + "\n")
    for row in rows:
        t_ms, valid, *values = row
        buffer.write(f"{t_ms:.3f},{valid}," + ",".join(f"{value:.6f}" for value in values) + "\n")

    completed = subprocess.run(
        [str(binary), f"--mode={mode}"],
        input=buffer.getvalue(),
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise ReplayError(completed.stderr.strip() or f"fall_replay saiu com {completed.returncode}")

    config = None
    summary = None
    detections: List[Dict] = []
    for line in completed.stdout.splitlines():
        record = json.loads(line)
        if record["type"] == "config":
            config = record
        elif record["type"] == "detection":
            detections.append(record)
        elif record["type"] == "summary":
            summary = record
    if config is None or summary is None:
        raise ReplayError("saida do replay incompleta")
    return {"config": config, "detections": detections, "summary": summary}


def evaluate_trial(trial: Trial, binary: Path, mode: str, interval_ms: float, max_gap_ms: float,
                   axis_map: AxisMap) -> Dict:
    mapped = map_axes(trial.samples, axis_map)
    rows, stats = resample_to_firmware_cadence(
        mapped, interval_ms, max_gap_ms, FIRMWARE_ACCEL_RANGE_G, FIRMWARE_GYRO_RANGE_DPS
    )
    result = run_replay(binary, rows, mode)
    offset = trial.samples[0][0]
    detections = result["detections"]
    return {
        "trial_id": trial.trial_id,
        "dataset": trial.dataset,
        "data_kind": trial.data_kind,
        "subject_id": trial.subject_id,
        "activity_code": trial.activity_code,
        "label": trial.label,
        "duration_ms": trial.duration_ms,
        "detections": len(detections),
        "first_detection_ms": detections[0]["t_ms"] if detections else None,
        "detection_times_ms": [record["t_ms"] for record in detections],
        "fall_onset_ms": None if trial.fall_onset_ms is None else trial.fall_onset_ms - offset,
        "fall_impact_ms": None if trial.fall_impact_ms is None else trial.fall_impact_ms - offset,
        "pending_candidate_at_end": result["summary"]["pending_candidate"],
        "resample": {
            "source_samples": stats.source_samples,
            "rows": stats.output_rows,
            "invalid_rows": stats.invalid_rows,
            "gaps": len(stats.gaps),
            "accel_saturated_samples": stats.accel_saturated_samples,
            "gyro_saturated_samples": stats.gyro_saturated_samples,
        },
        "source_sha256": trial.source_sha256,
        "notes": trial.notes,
        "_config": result["config"],
    }
