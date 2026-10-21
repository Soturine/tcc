"""Adaptadores de datasets. Cada adaptador apenas le e converte; nao rotula por heuristica."""

from __future__ import annotations

import hashlib
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def get_adapter(name: str):
    if name == "sisfall":
        from . import sisfall

        return sisfall
    if name == "kfall":
        from . import kfall

        return kfall
    if name == "normalized":
        from . import normalized_csv

        return normalized_csv
    raise ValueError(f"adaptador desconhecido: {name}")
