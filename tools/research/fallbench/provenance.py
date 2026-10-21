"""Proveniencia minima para reproduzir uma execucao."""

from __future__ import annotations

import hashlib
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable

REPO_ROOT = Path(__file__).resolve().parents[3]
DETECTOR_SOURCES = (
    "include/app_config.h",
    "include/models.h",
    "src/angle_math.cpp",
    "src/fall_detector.cpp",
    "src/sensor_signal.cpp",
    "tools/research/replay/fall_replay.cpp",
)


def _git(*args: str) -> str:
    try:
        return subprocess.run(
            ["git", *args], cwd=REPO_ROOT, capture_output=True, text=True, check=True
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


def _sha256_path(path: Path) -> str:
    # Normaliza fim de linha para que checkout CRLF (Windows) e LF gerem o mesmo hash.
    data = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def detector_fingerprint() -> Dict[str, str]:
    files = {name: _sha256_path(REPO_ROOT / name) for name in DETECTOR_SOURCES}
    combined = hashlib.sha256("".join(f"{k}={v}\n" for k, v in sorted(files.items())).encode()).hexdigest()
    return {"combined_sha256": combined, "files": files}


def dataset_fingerprint(trial_hashes: Iterable[tuple]) -> str:
    digest = hashlib.sha256()
    for trial_id, file_hash in sorted(trial_hashes):
        digest.update(f"{trial_id}={file_hash}\n".encode())
    return digest.hexdigest()


def collect(replay_binary: Path) -> Dict:
    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "git_commit": _git("rev-parse", "HEAD"),
        "git_dirty": _git("status", "--porcelain") not in ("", "unavailable"),
        "detector": detector_fingerprint(),
        "replay_binary_sha256": hashlib.sha256(replay_binary.read_bytes()).hexdigest(),
        "python": sys.version.split()[0],
        "platform": platform.platform(),
    }
