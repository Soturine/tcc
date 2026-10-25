"""Gravador de sessao com integridade (ML-1).

Estados: created -> recording <-> paused -> finalizing -> finalized; ou interrupted
(sem finalizacao, ex.: crash) / rejected (qualidade insuficiente). A sessao grava
frames brutos em chunks CSV e, ao finalizar, um manifest.json com SHA-256 de cada
arquivo, taxa efetiva, lacunas e duplicatas. Nada e descartado silenciosamente:
frames fora de ordem/duplicados sao contados e recusados.
"""

from __future__ import annotations

import hashlib
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

from . import FRAME_SCHEMA_VERSION, MANIFEST_SCHEMA_VERSION

FRAME_FIELDS = ("sample_seq", "device_time_us", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps", "valid")
CHUNK_HEADER = ",".join(FRAME_FIELDS)

_TRANSITIONS = {
    "created": {"recording"},
    "recording": {"paused", "finalizing", "interrupted"},
    "paused": {"recording", "finalizing", "interrupted"},
    "finalizing": {"finalized", "rejected"},
}


class SessionStateError(RuntimeError):
    """Comando invalido para o estado atual da sessao."""


class FrameError(ValueError):
    """Frame fora do contrato lab-frame-v1."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 16), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_frame(frame: Dict) -> None:
    if frame.get("schema_version") != FRAME_SCHEMA_VERSION:
        raise FrameError("schema_version nao suportado")
    for name in FRAME_FIELDS:
        if name not in frame:
            raise FrameError(f"campo ausente: {name}")
    if not isinstance(frame["sample_seq"], int) or frame["sample_seq"] < 0:
        raise FrameError("sample_seq deve ser inteiro >= 0")
    if not isinstance(frame["device_time_us"], int) or frame["device_time_us"] < 0:
        raise FrameError("device_time_us deve ser inteiro >= 0")
    for name in ("ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"):
        value = frame[name]
        if not isinstance(value, (int, float)) or not math.isfinite(value):
            raise FrameError(f"{name} nao finito")
    if not isinstance(frame["valid"], bool):
        raise FrameError("valid deve ser booleano")


@dataclass
class RecorderStats:
    accepted: int = 0
    duplicates: int = 0
    out_of_order: int = 0
    gaps: int = 0
    missing_frames: int = 0
    invalid_frames: int = 0
    first_time_us: Optional[int] = None
    last_time_us: Optional[int] = None
    last_seq: Optional[int] = None


@dataclass
class SessionRecorder:
    root: Path
    session_id: str
    participant_pseudonym: str
    protocol_version: str
    device: Dict
    chunk_frames: int = 500
    state: str = "created"
    stats: RecorderStats = field(default_factory=RecorderStats)
    chunks: List[str] = field(default_factory=list)
    marks: List[Dict] = field(default_factory=list)
    _buffer: List[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.directory = Path(self.root) / self.session_id
        self.directory.mkdir(parents=True, exist_ok=False)
        self._write_state()

    # ---------------------------------------------------------------- comandos
    def _transition(self, target: str) -> None:
        if target not in _TRANSITIONS.get(self.state, set()):
            raise SessionStateError(f"{self.state} -> {target} nao permitido")
        self.state = target
        self._write_state()

    def start(self) -> None:
        self._transition("recording")

    def pause(self) -> None:
        self._transition("paused")

    def resume(self) -> None:
        self._transition("recording")

    def mark(self, label: str, device_time_us: int, note: str = "") -> None:
        """Marca do observador (intencao/instante), NAO e ground truth revisado."""
        if self.state not in ("recording", "paused"):
            raise SessionStateError("mark exige sessao em gravacao")
        self.marks.append({"label": label, "device_time_us": device_time_us, "note": note})

    def append(self, frame: Dict) -> bool:
        """Retorna True se o frame foi gravado. Duplicado/fora de ordem e recusado e contado."""
        if self.state != "recording":
            raise SessionStateError("append exige estado recording")
        try:
            validate_frame(frame)
        except FrameError:
            self.stats.invalid_frames += 1
            raise
        seq = frame["sample_seq"]
        last = self.stats.last_seq
        if last is not None and seq == last:
            self.stats.duplicates += 1
            return False
        if last is not None and seq < last:
            self.stats.out_of_order += 1
            return False
        if last is not None and seq > last + 1:
            self.stats.gaps += 1
            self.stats.missing_frames += seq - last - 1
        self.stats.last_seq = seq
        if self.stats.first_time_us is None:
            self.stats.first_time_us = frame["device_time_us"]
        self.stats.last_time_us = frame["device_time_us"]
        self.stats.accepted += 1
        self._buffer.append(",".join(
            str(int(frame[name])) if name == "valid" else str(frame[name]) for name in FRAME_FIELDS))
        if len(self._buffer) >= self.chunk_frames:
            self._flush()
        return True

    def stop(self, min_frames: int = 1) -> Dict:
        self._transition("finalizing")
        self._flush()
        if self.stats.accepted < min_frames:
            self._transition("rejected")
        else:
            self._transition("finalized")
        return self._write_manifest()

    def interrupt(self) -> None:
        """Encerramento anormal (ex.: emergencia/crash simulado): nada e finalizado."""
        self._flush()
        self._transition("interrupted")

    # ---------------------------------------------------------------- arquivos
    def _flush(self) -> None:
        if not self._buffer:
            return
        name = f"imu.raw.{len(self.chunks):04d}.csv"
        path = self.directory / name
        tmp = path.with_suffix(".tmp")
        tmp.write_text(CHUNK_HEADER + "\n" + "\n".join(self._buffer) + "\n", encoding="utf-8")
        tmp.replace(path)
        self.chunks.append(name)
        self._buffer = []

    def _write_state(self) -> None:
        (self.directory / "state.json").write_text(
            json.dumps({"session_id": self.session_id, "state": self.state}), encoding="utf-8")

    def _write_manifest(self) -> Dict:
        span_us = (self.stats.last_time_us or 0) - (self.stats.first_time_us or 0)
        effective = None
        if self.stats.accepted > 1 and span_us > 0:
            effective = (self.stats.accepted - 1) / (span_us / 1e6)
        marks_path = self.directory / "marks.json"
        marks_path.write_text(json.dumps(self.marks, indent=2), encoding="utf-8")
        files = {name: sha256_file(self.directory / name) for name in self.chunks + ["marks.json"]}
        manifest = {
            "schema_version": MANIFEST_SCHEMA_VERSION,
            "data_kind": self.device.get("data_kind", "synthetic"),
            "session_id": self.session_id,
            "participant_pseudonym": self.participant_pseudonym,
            "protocol_version": self.protocol_version,
            "state": self.state,
            "device": self.device,
            "frames": {
                "accepted": self.stats.accepted,
                "duplicates_rejected": self.stats.duplicates,
                "out_of_order_rejected": self.stats.out_of_order,
                "invalid_rejected": self.stats.invalid_frames,
                "gaps": self.stats.gaps,
                "missing_frames": self.stats.missing_frames,
                "effective_rate_hz": effective,
            },
            "files": files,
        }
        (self.directory / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")
        return manifest
