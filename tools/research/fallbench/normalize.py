"""Mapeamento de eixos e reamostragem para a cadencia de leitura do firmware."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Sequence, Tuple

from .model import Sample

AXES = ("x", "y", "z")


@dataclass(frozen=True)
class AxisMap:
    """Eixo do dispositivo <- eixo do dataset, com sinal.

    Especificacao textual: ``"x=+x,y=+y,z=+z"`` (identidade) ou, por exemplo,
    ``"x=+x,y=-z,z=+y"``. O mesmo mapeamento e aplicado a acelerometro e giroscopio
    porque ambos compartilham o referencial do sensor.
    """

    mapping: Tuple[Tuple[int, float], Tuple[int, float], Tuple[int, float]]
    spec: str

    @classmethod
    def parse(cls, spec: str) -> "AxisMap":
        parts: Dict[str, Tuple[int, float]] = {}
        for item in spec.split(","):
            item = item.strip()
            if "=" not in item:
                raise ValueError(f"mapeamento de eixo invalido: {item!r}")
            target, source = (piece.strip().lower() for piece in item.split("=", 1))
            sign = -1.0 if source.startswith("-") else 1.0
            source = source.lstrip("+-")
            if target not in AXES or source not in AXES:
                raise ValueError(f"eixo desconhecido em {item!r}")
            if target in parts:
                raise ValueError(f"eixo de destino repetido: {target}")
            parts[target] = (AXES.index(source), sign)

        if set(parts) != set(AXES):
            raise ValueError("mapeamento precisa definir x, y e z")
        sources = sorted(index for index, _ in parts.values())
        if sources != [0, 1, 2]:
            raise ValueError("mapeamento precisa ser uma permutacao dos eixos de origem")
        return cls((parts["x"], parts["y"], parts["z"]), spec)

    def apply(self, vector: Sequence[float]) -> Tuple[float, float, float]:
        return tuple(sign * vector[index] for index, sign in self.mapping)  # type: ignore[return-value]


IDENTITY_AXIS_MAP = AxisMap.parse("x=+x,y=+y,z=+z")


@dataclass
class ResampleStats:
    source_samples: int
    output_rows: int
    invalid_rows: int
    gaps: List[Tuple[float, float]]
    accel_saturated_samples: int
    gyro_saturated_samples: int


def map_axes(samples: Sequence[Sample], axis_map: AxisMap) -> List[Sample]:
    mapped: List[Sample] = []
    for t_ms, ax, ay, az, gx, gy, gz in samples:
        max_, may, maz = axis_map.apply((ax, ay, az))
        mgx, mgy, mgz = axis_map.apply((gx, gy, gz))
        mapped.append((t_ms, max_, may, maz, mgx, mgy, mgz))
    return mapped


def resample_to_firmware_cadence(
    samples: Sequence[Sample],
    interval_ms: float,
    max_gap_ms: float,
    accel_range_g: float,
    gyro_range_dps: float,
) -> Tuple[List[Tuple[float, int, float, float, float, float, float, float]], ResampleStats]:
    """Amostra a serie a cada ``interval_ms`` como o loop do firmware faria.

    Cada tick usa a amostra mais recente com timestamp <= tick (zero-order hold).
    Se essa amostra for mais antiga que ``max_gap_ms``, o tick vira ``valid=0``:
    lacunas nao sao interpoladas nem preenchidas com valores fabricados.
    Valores alem da faixa do sensor do firmware sao saturados para a faixa,
    reproduzindo o limite fisico do MPU configurado, e contabilizados.
    """
    if interval_ms <= 0 or max_gap_ms < interval_ms:
        raise ValueError("interval_ms deve ser > 0 e max_gap_ms >= interval_ms")

    gaps: List[Tuple[float, float]] = []
    for previous, current in zip(samples, samples[1:]):
        if current[0] - previous[0] > max_gap_ms:
            gaps.append((previous[0], current[0]))

    start = samples[0][0]
    end = samples[-1][0]
    rows = []
    invalid = 0
    accel_saturated = 0
    gyro_saturated = 0
    cursor = 0
    tick_index = 0

    while True:
        tick = start + tick_index * interval_ms
        if tick > end:
            break
        while cursor + 1 < len(samples) and samples[cursor + 1][0] <= tick:
            cursor += 1
        source = samples[cursor]
        relative_tick = tick - start

        if tick - source[0] > max_gap_ms:
            rows.append((relative_tick, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
            invalid += 1
        else:
            accel = [_clamp(value, accel_range_g) for value in source[1:4]]
            gyro = [_clamp(value, gyro_range_dps) for value in source[4:7]]
            if any(abs(value) > accel_range_g for value in source[1:4]):
                accel_saturated += 1
            if any(abs(value) > gyro_range_dps for value in source[4:7]):
                gyro_saturated += 1
            rows.append((relative_tick, 1, *accel, *gyro))
        tick_index += 1

    stats = ResampleStats(
        source_samples=len(samples),
        output_rows=len(rows),
        invalid_rows=invalid,
        gaps=gaps,
        accel_saturated_samples=accel_saturated,
        gyro_saturated_samples=gyro_saturated,
    )
    return rows, stats


def _clamp(value: float, limit: float) -> float:
    return max(-limit, min(limit, value))
