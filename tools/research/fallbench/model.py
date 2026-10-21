"""Representacao comum de um ensaio (trial) independente do dataset de origem."""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

LABELS = ("fall", "adl")
DATA_KINDS = ("real", "synthetic")

# (t_ms, ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps)
Sample = Tuple[float, float, float, float, float, float, float]


class DatasetFormatError(ValueError):
    """Arquivo nao corresponde ao formato documentado do adaptador."""


@dataclass
class Trial:
    trial_id: str
    dataset: str
    subject_id: str
    activity_code: str
    label: str
    samples: List[Sample]
    source_rate_hz: Optional[float]
    sensor_position: str
    data_kind: str
    source_file: str = ""
    source_sha256: str = ""
    fall_onset_ms: Optional[float] = None
    fall_impact_ms: Optional[float] = None
    notes: List[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        if self.label not in LABELS:
            raise ValueError(f"label invalido para {self.trial_id}: {self.label!r}")
        if self.data_kind not in DATA_KINDS:
            raise ValueError(f"data_kind invalido para {self.trial_id}: {self.data_kind!r}")
        if not self.samples:
            raise DatasetFormatError(f"{self.trial_id}: nenhuma amostra")

        previous = None
        for sample in self.samples:
            if len(sample) != 7 or not all(math.isfinite(value) for value in sample):
                raise DatasetFormatError(f"{self.trial_id}: amostra nao finita ou incompleta")
            if previous is not None and sample[0] <= previous:
                raise DatasetFormatError(f"{self.trial_id}: timestamps nao estritamente crescentes")
            previous = sample[0]

        for name in ("fall_onset_ms", "fall_impact_ms"):
            value = getattr(self, name)
            if value is not None and self.label != "fall":
                raise ValueError(f"{self.trial_id}: {name} so se aplica a trials de queda")

    @property
    def duration_ms(self) -> float:
        return self.samples[-1][0] - self.samples[0][0]
