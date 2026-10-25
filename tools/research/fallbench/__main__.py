"""CLI: ``python -m fallbench evaluate|split`` (executar a partir de tools/research)."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path

from . import SCHEMA_VERSION
from .adapters import get_adapter
from .metrics import evaluate
from .model import DatasetFormatError
from .normalize import AxisMap
from .provenance import collect, dataset_fingerprint
from .replay import default_binary, evaluate_trial
from .splits import subject_split

FIRMWARE_INTERVAL_MS = {"normal": 50.0, "demo": 25.0}


def _subjects(value: str):
    return {item.strip() for item in value.split(",") if item.strip()} if value else None


def _load_trials(args, excluded):
    adapter = get_adapter(args.adapter)
    options = {}
    if args.adapter == "kfall":
        options = {"labels_path": args.labels, "excluded": excluded}
    if args.adapter == "weda_fall":
        options = {"rate_dir": args.rate_dir, "timebase": args.timebase, "excluded": excluded}
    if args.adapter == "sisfall":
        if args.accel_g_per_count:
            options["accel_g_per_count"] = args.accel_g_per_count
        if args.gyro_dps_per_count:
            options["gyro_dps_per_count"] = args.gyro_dps_per_count
    return adapter.iter_trials(Path(args.root), subjects=_subjects(args.subjects), **options)


def cmd_evaluate(args) -> int:
    binary = Path(args.replay_bin) if args.replay_bin else default_binary()
    if not binary.exists():
        print(f"binario de replay ausente: {binary} (rode tools/research/build_replay.sh)", file=sys.stderr)
        return 2

    axis_map = AxisMap.parse(args.axis_map)
    interval_ms = FIRMWARE_INTERVAL_MS[args.mode]
    excluded = []
    outcomes = []
    config = None
    for trial in _load_trials(args, excluded):
        try:
            outcome = evaluate_trial(trial, binary, args.mode, interval_ms, args.max_gap_ms, axis_map)
        except DatasetFormatError as error:
            excluded.append({"trial_id": trial.trial_id, "reason": str(error)})
            continue
        config = outcome.pop("_config")
        outcomes.append(outcome)
        if args.limit and len(outcomes) >= args.limit:
            break

    if not outcomes:
        print("nenhum trial avaliado", file=sys.stderr)
        return 3

    data_kinds = sorted({outcome["data_kind"] for outcome in outcomes})
    report = {
        "schema_version": SCHEMA_VERSION,
        "data_kind": data_kinds[0] if len(data_kinds) == 1 else "mixed",
        "disclaimer": (
            "Avaliacao offline do detector embarcado sobre dados de laboratorio. Nao e validacao "
            "clinica nem estimativa de desempenho em uso real."
        ),
        "dataset": {
            "adapter": args.adapter,
            "root_name": Path(args.root).name,
            "trials_evaluated": len(outcomes),
            "trials_excluded": excluded,
            "fingerprint_sha256": dataset_fingerprint(
                (outcome["trial_id"], outcome["source_sha256"]) for outcome in outcomes
            ),
        },
        "protocol": {
            "detector_mode": args.mode,
            "firmware_sample_interval_ms": interval_ms,
            "max_gap_ms": args.max_gap_ms,
            "axis_map": axis_map.spec,
            "weda_fall": (
                {"rate_dir": args.rate_dir, "timebase": args.timebase}
                if args.adapter == "weda_fall" else None
            ),
            "subjects_filter": sorted(_subjects(args.subjects) or []),
            "evaluation_unit": "trial",
            "detector_config": config,
            # Identidade semantica da baseline: muda somente se parametros efetivos mudarem.
            "detector_parameters_sha256": hashlib.sha256(
                json.dumps(config, sort_keys=True).encode()
            ).hexdigest(),
        },
        "provenance": collect(binary),
        "metrics": evaluate(outcomes),
        "trials": outcomes,
    }

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    with (out_dir / "per_trial.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["trial_id", "subject_id", "activity_code", "label", "detections",
                         "first_detection_ms", "fall_impact_ms", "duration_ms", "invalid_rows"])
        for outcome in outcomes:
            writer.writerow([outcome["trial_id"], outcome["subject_id"], outcome["activity_code"],
                             outcome["label"], outcome["detections"], outcome["first_detection_ms"],
                             outcome["fall_impact_ms"], round(outcome["duration_ms"], 3),
                             outcome["resample"]["invalid_rows"]])

    trial_level = report["metrics"]["trial_level"]
    print(json.dumps({
        "data_kind": report["data_kind"],
        "trials": len(outcomes),
        "excluded": len(excluded),
        "confusion": trial_level["confusion"],
        "recall_sensitivity": trial_level["recall_sensitivity"],
        "specificity": trial_level["specificity"],
        "out": str(out_dir),
    }, ensure_ascii=False))
    return 0


def cmd_nyquist_synthetic(args) -> int:
    from .signal_lab import synthetic_condition_report

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(synthetic_condition_report(), indent=2) + "\n", encoding="utf-8")
    print(str(out))
    return 0


def cmd_split(args) -> int:
    excluded = []
    subjects = {trial.subject_id for trial in _load_trials(args, excluded)}
    print(json.dumps(subject_split(subjects, args.seed, args.dev_fraction), indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="fallbench")
    sub = parser.add_subparsers(dest="command", required=True)

    def common(command):
        command.add_argument("--adapter", required=True, choices=["sisfall", "kfall", "normalized", "weda_fall"])
        command.add_argument("--root", required=True)
        command.add_argument("--labels", help="KFall: exportacao CSV dos rotulos")
        command.add_argument("--subjects", default="", help="lista separada por virgula")
        command.add_argument("--accel-g-per-count", type=float, default=None)
        command.add_argument("--gyro-dps-per-count", type=float, default=None)
        command.add_argument("--rate-dir", default="50Hz", help="WEDA-FALL: subdiretorio de taxa")
        command.add_argument("--timebase", default="recorded", choices=["recorded", "uniform_index"],
                             help="WEDA-FALL: timestamps como gravados ou suposicao uniforme declarada")

    evaluate_parser = sub.add_parser("evaluate")
    common(evaluate_parser)
    evaluate_parser.add_argument("--mode", choices=sorted(FIRMWARE_INTERVAL_MS), default="normal")
    evaluate_parser.add_argument("--axis-map", default="x=+x,y=+y,z=+z")
    evaluate_parser.add_argument("--max-gap-ms", type=float, default=150.0)
    evaluate_parser.add_argument("--replay-bin", default="")
    evaluate_parser.add_argument("--limit", type=int, default=0)
    evaluate_parser.add_argument("--out", required=True)
    evaluate_parser.set_defaults(func=cmd_evaluate)

    split_parser = sub.add_parser("split")
    common(split_parser)
    split_parser.add_argument("--seed", required=True)
    split_parser.add_argument("--dev-fraction", type=float, default=0.5)
    split_parser.set_defaults(func=cmd_split)

    nyquist_parser = sub.add_parser("nyquist-synthetic", help="artefato espectral sintetico 4+17 Hz @125 Hz")
    nyquist_parser.add_argument("--out", required=True)
    nyquist_parser.set_defaults(func=cmd_nyquist_synthetic)
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
