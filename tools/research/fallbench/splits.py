"""Particao deterministica por participante (sem vazamento entre dev e teste)."""

from __future__ import annotations

import hashlib
from typing import Dict, Iterable, List


def subject_split(subject_ids: Iterable[str], seed: str, dev_fraction: float) -> Dict[str, List[str]]:
    """Ordena participantes por SHA-256(seed:subject) e corta na fracao pedida.

    Independe da ordem de entrada e do Python (sem ``random``/hash salgado).
    Todo trial de um participante fica do mesmo lado.
    """
    if not 0.0 < dev_fraction < 1.0:
        raise ValueError("dev_fraction deve estar em (0, 1)")
    unique = sorted(set(subject_ids))
    if len(unique) < 2:
        raise ValueError("sao necessarios ao menos 2 participantes para separar dev/teste")

    ranked = sorted(unique, key=lambda subject: hashlib.sha256(f"{seed}:{subject}".encode()).hexdigest())
    cut = min(len(ranked) - 1, max(1, round(len(ranked) * dev_fraction)))
    return {
        "seed": seed,
        "dev_fraction": dev_fraction,
        "dev": sorted(ranked[:cut]),
        "test": sorted(ranked[cut:]),
    }
