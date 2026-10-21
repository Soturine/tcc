"""Adaptador SisFall (Sucerquia, Lopez e Vargas-Bonilla, Sensors 17(1):198, 2017).

Fatos verificados na publicacao (PMC5298771): cintura (fivela de cinto), 200 Hz,
ADXL345 +/-16 g 13 bits, ITG3200 +/-2000 deg/s 16 bits, MMA8451Q +/-8 g 14 bits;
z para frente, y na direcao da gravidade, x para a direita do participante;
19 ADL (D01-D19) e 15 quedas (F01-F15); idosos nao executaram quedas, exceto SE06.

Formato de arquivo esperado (verificar contra o Readme distribuido com o dataset):
uma linha por amostra com 9 inteiros separados por virgula, opcionalmente
terminada por ';', na ordem ADXL345 xyz, ITG3200 xyz, MMA8451Q xyz. O arquivo e
recusado se nao seguir esse formato; nada e inferido silenciosamente.

Os fatores de conversao padrao sao DERIVADOS de faixa/resolucao declaradas no
artigo, nao copiados do Readme. Confirme-os antes de reportar resultados e
registre o valor usado (ele vai para o manifesto de proveniencia).
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterator, List, Optional

from ..model import DatasetFormatError, Sample, Trial
from . import sha256_file

DATASET = "sisfall"
SOURCE_RATE_HZ = 200.0
SENSOR_POSITION = "waist"
# +/-16 g em 13 bits -> 32 g / 8192 contagens
DEFAULT_ACCEL_G_PER_COUNT = 32.0 / 8192.0
# +/-2000 deg/s em 16 bits -> 4000 deg/s / 65536 contagens
DEFAULT_GYRO_DPS_PER_COUNT = 4000.0 / 65536.0

FILE_PATTERN = re.compile(r"^(?P<kind>[DF])(?P<code>\d{2})_(?P<subject>S[AE]\d{2})_R(?P<trial>\d{2})\.txt$")


def parse_name(name: str):
    match = FILE_PATTERN.match(name)
    if not match:
        return None
    return match.groupdict()


def parse_file(
    path: Path,
    accel_g_per_count: float = DEFAULT_ACCEL_G_PER_COUNT,
    gyro_dps_per_count: float = DEFAULT_GYRO_DPS_PER_COUNT,
) -> Trial:
    meta = parse_name(path.name)
    if meta is None:
        raise DatasetFormatError(f"{path.name}: nome fora do padrao SisFall")

    samples: List[Sample] = []
    period_ms = 1000.0 / SOURCE_RATE_HZ
    with path.open("r", encoding="utf-8", errors="strict") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip().rstrip(";").strip()
            if not line:
                continue
            fields = [field.strip() for field in line.split(",")]
            if len(fields) != 9:
                raise DatasetFormatError(
                    f"{path.name}:{line_number}: esperados 9 campos, encontrados {len(fields)}"
                )
            try:
                counts = [int(field) for field in fields]
            except ValueError as error:
                raise DatasetFormatError(f"{path.name}:{line_number}: valor nao inteiro") from error

            index = len(samples)
            samples.append(
                (
                    index * period_ms,
                    counts[0] * accel_g_per_count,
                    counts[1] * accel_g_per_count,
                    counts[2] * accel_g_per_count,
                    counts[3] * gyro_dps_per_count,
                    counts[4] * gyro_dps_per_count,
                    counts[5] * gyro_dps_per_count,
                )
            )

    activity = f"{meta['kind']}{meta['code']}"
    return Trial(
        trial_id=path.stem,
        dataset=DATASET,
        subject_id=meta["subject"],
        activity_code=activity,
        label="fall" if meta["kind"] == "F" else "adl",
        samples=samples,
        source_rate_hz=SOURCE_RATE_HZ,
        sensor_position=SENSOR_POSITION,
        data_kind="real",
        source_file=str(path),
        source_sha256=sha256_file(path),
        notes=[
            "timestamps reconstruidos a partir da taxa nominal de 200 Hz",
            "sem anotacao de onset/impacto: latencia de deteccao indisponivel",
            f"accel_g_per_count={accel_g_per_count!r}",
            f"gyro_dps_per_count={gyro_dps_per_count!r}",
        ],
    )


def iter_trials(root: Path, subjects: Optional[set] = None, **options) -> Iterator[Trial]:
    for path in sorted(root.rglob("*.txt")):
        meta = parse_name(path.name)
        if meta is None:
            continue
        if subjects and meta["subject"] not in subjects:
            continue
        yield parse_file(path, **options)
