"""Reimportacao verificada de sessoes e ground truth independente (ML-1/ML-3)."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

from . import ANNOTATION_SCHEMA_VERSION, GT_SOURCES, LABELS, TRANSITION_LABELS
from .recorder import CHUNK_HEADER, FRAME_FIELDS, sha256_file


class BundleError(ValueError):
    """Sessao corrompida, adulterada ou incompleta."""


@dataclass
class ImportedSession:
    manifest: Dict
    frames: List[Dict]
    state: str


def import_session(directory: Path) -> ImportedSession:
    """Valida hashes e retorna os frames. Sessao sem manifest = interrompida (nunca 'ok')."""
    directory = Path(directory)
    manifest_path = directory / "manifest.json"
    if not manifest_path.exists():
        state = json.loads((directory / "state.json").read_text(encoding="utf-8")).get("state", "unknown")
        # Chunks gravados ate o crash continuam legiveis, mas a sessao nao e finalizada.
        frames = _read_chunks(directory, sorted(p.name for p in directory.glob("imu.raw.*.csv")))
        return ImportedSession(manifest={}, frames=frames, state=state if state == "interrupted" else "interrupted")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for name, expected in manifest["files"].items():
        path = directory / name
        if not path.exists():
            raise BundleError(f"arquivo ausente: {name}")
        if sha256_file(path) != expected:
            raise BundleError(f"hash divergente: {name}")
    chunks = sorted(name for name in manifest["files"] if name.startswith("imu.raw."))
    extra = sorted(p.name for p in directory.glob("imu.raw.*.csv") if p.name not in manifest["files"])
    if extra:
        raise BundleError(f"chunks fora do manifest: {extra}")
    frames = _read_chunks(directory, chunks)
    if len(frames) != manifest["frames"]["accepted"]:
        raise BundleError("contagem de frames diverge do manifest")
    return ImportedSession(manifest=manifest, frames=frames, state=manifest["state"])


def _read_chunks(directory: Path, names: List[str]) -> List[Dict]:
    frames: List[Dict] = []
    for name in names:
        with (directory / name).open("r", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            if ",".join(next(reader, [])) != CHUNK_HEADER:
                raise BundleError(f"{name}: cabecalho invalido")
            for row in reader:
                if not row:
                    continue
                frame = dict(zip(FRAME_FIELDS, row))
                frames.append({
                    "sample_seq": int(frame["sample_seq"]),
                    "device_time_us": int(frame["device_time_us"]),
                    **{k: float(frame[k]) for k in ("ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps")},
                    "valid": frame["valid"] == "1",
                })
    return frames


def validate_annotation(annotation: Dict) -> None:
    """Rotulo de GT: separado da intencao e de predicoes."""
    if annotation.get("schema_version") != ANNOTATION_SCHEMA_VERSION:
        raise BundleError("annotation schema_version nao suportado")
    if annotation.get("label") not in LABELS:
        raise BundleError(f"label fora do vocabulario: {annotation.get('label')!r}")
    if annotation.get("source") not in GT_SOURCES:
        # Inclui "model": predicao nao vira rotulo automaticamente.
        raise BundleError(f"source nao aceito como ground truth: {annotation.get('source')!r}")
    start, end = annotation.get("time_start_us"), annotation.get("time_end_us")
    if not isinstance(start, int) or not isinstance(end, int) or end < start:
        raise BundleError("intervalo invalido")
    if not isinstance(annotation.get("uncertainty_us"), int) or annotation["uncertainty_us"] < 0:
        raise BundleError("uncertainty_us obrigatorio e >= 0")
    if annotation.get("review_status") not in ("draft", "reviewed", "rejected"):
        raise BundleError("review_status invalido")


def match_transitions(truth: List[Dict], predicted: List[Dict], tolerance_us: int) -> Dict:
    """Pareamento guloso 1:1 por evento (tolerancia pre-declarada), por classe de transicao.

    Uma deteccao so pareia com um evento de GT do mesmo rotulo, dentro da tolerancia,
    e cada um so e usado uma vez: janelas sobrepostas nao viram eventos independentes.
    """
    result = {}
    for label in TRANSITION_LABELS:
        gt = sorted(e["time_us"] for e in truth if e["label"] == label)
        pr = sorted(e["time_us"] for e in predicted if e["label"] == label)
        used = [False] * len(pr)
        tp = 0
        for t in gt:
            best: Optional[int] = None
            for index, p in enumerate(pr):
                if used[index] or abs(p - t) > tolerance_us:
                    continue
                if best is None or abs(p - t) < abs(pr[best] - t):
                    best = index
            if best is not None:
                used[best] = True
                tp += 1
        fn = len(gt) - tp
        fp = used.count(False)
        result[label] = {"tp": tp, "fp": fp, "fn": fn}
    result["tolerance_us"] = tolerance_us
    return result
