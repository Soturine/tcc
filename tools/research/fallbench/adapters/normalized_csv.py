"""Formato normalizado do projeto: coletas proprias em bancada e fixtures sinteticas.

Diretorio com ``manifest.json``::

    {"dataset": "...", "data_kind": "real|synthetic", "sensor_position": "...",
     "trials": [{"file": "t1.csv", "trial_id": "...", "subject_id": "...",
                 "activity_code": "...", "label": "fall|adl",
                 "fall_onset_ms": null, "fall_impact_ms": null}]}

Cada CSV: ``t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps``.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path
from typing import Iterator, Optional

from ..model import DatasetFormatError, Trial
from . import sha256_file

HEADER = ["t_ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"]


def iter_trials(root: Path, subjects: Optional[set] = None, **_unused) -> Iterator[Trial]:
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    data_kind = manifest["data_kind"]
    for entry in manifest["trials"]:
        if subjects and entry["subject_id"] not in subjects:
            continue
        path = root / entry["file"]
        with path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            header = next(reader, None)
            if header != HEADER:
                raise DatasetFormatError(f"{path.name}: cabecalho esperado {','.join(HEADER)}")
            try:
                samples = [tuple(float(value) for value in row) for row in reader if row]
            except ValueError as error:
                raise DatasetFormatError(f"{path.name}: valor nao numerico") from error
        yield Trial(
            trial_id=entry["trial_id"],
            dataset=manifest["dataset"],
            subject_id=entry["subject_id"],
            activity_code=entry["activity_code"],
            label=entry["label"],
            samples=samples,  # type: ignore[arg-type]
            source_rate_hz=manifest.get("source_rate_hz"),
            sensor_position=manifest["sensor_position"],
            data_kind=data_kind,
            source_file=str(path),
            source_sha256=sha256_file(path),
            fall_onset_ms=entry.get("fall_onset_ms"),
            fall_impact_ms=entry.get("fall_impact_ms"),
        )
