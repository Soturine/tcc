"""Adaptador KFall (Yu, Jang e Xiong, Front. Aging Neurosci. 13:692865, 2021).

Fatos verificados (PMC8322729 e pagina oficial do dataset): LPMS-B2 na regiao
lombar, 100 Hz; CSV com 11 colunas: TimeStamp(s), FrameCounter, aceleracao (g)
xyz, giroscopio (deg/s) xyz, angulos de Euler (deg) xyz; nomes
``SA06T01R01.csv`` (sujeito/tarefa/repeticao); rotulos por sujeito com
Task Code, Trial ID, Fall_onset_frame e Fall_Impact_frame.

Acesso controlado: cadastro aprovado pelo laboratorio e proibicao de repasse a
terceiros. Os dados NUNCA devem ser versionados neste repositorio.

Os rotulos sao distribuidos como .xlsx. Para manter o pacote sem dependencias,
este adaptador le uma exportacao CSV com cabecalho contendo ao menos
``subject_id,task_code,trial_id,fall_onset_frame,fall_impact_frame``.
Classe: linha com frames de onset/impacto -> queda; linha com frames vazios -> ADL;
trial sem linha -> excluido e reportado (nunca rotulado por suposicao).
"""

from __future__ import annotations

import csv
import re
from pathlib import Path
from typing import Dict, Iterator, List, Optional, Tuple

from ..model import DatasetFormatError, Sample, Trial
from . import sha256_file

DATASET = "kfall"
SOURCE_RATE_HZ = 100.0
SENSOR_POSITION = "low_back"
FILE_PATTERN = re.compile(r"^(?P<subject>S[A-Z]\d{2})T(?P<task>\d{2})R(?P<trial>\d{2})\.csv$")

LabelKey = Tuple[str, int, int]


class UnlabeledTrial(Exception):
    """Trial sem linha correspondente no arquivo de rotulos."""


def _optional_int(value: str) -> Optional[int]:
    value = (value or "").strip()
    if not value:
        return None
    try:
        return int(float(value))
    except ValueError as error:
        raise DatasetFormatError(f"frame invalido no arquivo de rotulos: {value!r}") from error


def load_labels(path: Path) -> Dict[LabelKey, Tuple[Optional[int], Optional[int]]]:
    required = {"subject_id", "task_code", "trial_id", "fall_onset_frame", "fall_impact_frame"}
    labels: Dict[LabelKey, Tuple[Optional[int], Optional[int]]] = {}
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise DatasetFormatError(f"rotulos sem colunas: {sorted(missing)}")
        for row in reader:
            task = int(str(row["task_code"]).strip().lstrip("Tt") or "0")
            trial = int(str(row["trial_id"]).strip().lstrip("Rr") or "0")
            key = (row["subject_id"].strip(), task, trial)
            onset = _optional_int(row["fall_onset_frame"])
            impact = _optional_int(row["fall_impact_frame"])
            if (onset is None) != (impact is None):
                raise DatasetFormatError(f"rotulo incompleto para {key}")
            if key in labels:
                raise DatasetFormatError(f"rotulo duplicado para {key}")
            labels[key] = (onset, impact)
    return labels


def parse_file(path: Path, labels: Dict[LabelKey, Tuple[Optional[int], Optional[int]]]) -> Trial:
    match = FILE_PATTERN.match(path.name)
    if not match:
        raise DatasetFormatError(f"{path.name}: nome fora do padrao KFall")
    subject = match["subject"]
    task = int(match["task"])
    trial_number = int(match["trial"])
    key = (subject, task, trial_number)
    if key not in labels:
        raise UnlabeledTrial(path.name)
    onset_frame, impact_frame = labels[key]

    samples: List[Sample] = []
    frame_times: Dict[int, float] = {}
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, None)
        if header is None or len(header) != 11:
            raise DatasetFormatError(f"{path.name}: cabecalho com 11 colunas esperado")
        first_timestamp = None
        for line_number, row in enumerate(reader, start=2):
            if not row:
                continue
            if len(row) != 11:
                raise DatasetFormatError(f"{path.name}:{line_number}: 11 colunas esperadas")
            try:
                values = [float(value) for value in row[:8]]
            except ValueError as error:
                raise DatasetFormatError(f"{path.name}:{line_number}: valor nao numerico") from error
            if first_timestamp is None:
                first_timestamp = values[0]
            t_ms = (values[0] - first_timestamp) * 1000.0
            frame_times[int(values[1])] = t_ms
            samples.append((t_ms, values[2], values[3], values[4], values[5], values[6], values[7]))

    onset_ms = impact_ms = None
    if onset_frame is not None:
        if onset_frame not in frame_times or impact_frame not in frame_times:
            raise DatasetFormatError(f"{path.name}: frame rotulado ausente na serie")
        onset_ms = frame_times[onset_frame]
        impact_ms = frame_times[impact_frame]

    return Trial(
        trial_id=path.stem,
        dataset=DATASET,
        subject_id=subject,
        activity_code=f"T{task:02d}",
        label="fall" if onset_frame is not None else "adl",
        samples=samples,
        source_rate_hz=SOURCE_RATE_HZ,
        sensor_position=SENSOR_POSITION,
        data_kind="real",
        source_file=str(path),
        source_sha256=sha256_file(path),
        fall_onset_ms=onset_ms,
        fall_impact_ms=impact_ms,
    )


def iter_trials(root: Path, subjects: Optional[set] = None, labels_path: Optional[str] = None,
                excluded: Optional[list] = None) -> Iterator[Trial]:
    if not labels_path:
        raise ValueError("KFall exige --labels com a exportacao CSV dos rotulos")
    labels = load_labels(Path(labels_path))
    for path in sorted(root.rglob("*.csv")):
        match = FILE_PATTERN.match(path.name)
        if not match:
            continue
        if subjects and match["subject"] not in subjects:
            continue
        try:
            yield parse_file(path, labels)
        except UnlabeledTrial:
            if excluded is not None:
                excluded.append({"trial_id": path.stem, "reason": "unlabeled"})
