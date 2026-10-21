import json
import math
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RESEARCH_ROOT))

from fallbench import metrics  # noqa: E402
from fallbench.adapters import kfall, sisfall  # noqa: E402
from fallbench.model import DatasetFormatError, Trial  # noqa: E402
from fallbench.normalize import AxisMap, map_axes, resample_to_firmware_cadence  # noqa: E402
from fallbench.replay import default_binary  # noqa: E402
from fallbench.splits import subject_split  # noqa: E402


def outcome(label, detections, subject="S1", activity="A", duration_ms=1000.0, **extra):
    return {"label": label, "detections": detections, "subject_id": subject,
            "activity_code": activity, "duration_ms": duration_ms, **extra}


class MetricsTest(unittest.TestCase):
    def test_confusion_and_rates(self):
        outcomes = [outcome("fall", 1), outcome("fall", 0), outcome("fall", 2),
                    outcome("adl", 1), outcome("adl", 0), outcome("adl", 0), outcome("adl", 0)]
        summary = metrics.summarize_confusion(metrics.confusion_from_outcomes(outcomes))
        self.assertEqual(summary["confusion"], {"tp": 2, "fn": 1, "fp": 1, "tn": 3})
        self.assertAlmostEqual(summary["recall_sensitivity"], 2 / 3)
        self.assertAlmostEqual(summary["specificity"], 3 / 4)
        self.assertAlmostEqual(summary["precision"], 2 / 3)
        self.assertAlmostEqual(summary["f1"], 2 / 3)
        self.assertEqual(summary["unit"], "trial")

    def test_missing_class_is_undefined_not_perfect(self):
        summary = metrics.summarize_confusion(metrics.confusion_from_outcomes([outcome("adl", 0)]))
        self.assertIsNone(summary["recall_sensitivity"])
        self.assertIsNone(summary["precision"])
        self.assertIsNone(summary["f1"])
        self.assertIsNone(summary["recall_ci95_wilson"])
        self.assertEqual(summary["specificity"], 1.0)

    def test_zero_true_positives_gives_zero_f1(self):
        summary = metrics.summarize_confusion(
            metrics.confusion_from_outcomes([outcome("fall", 0), outcome("adl", 1)]))
        self.assertEqual(summary["precision"], 0.0)
        self.assertEqual(summary["recall_sensitivity"], 0.0)
        self.assertEqual(summary["f1"], 0.0)

    def test_wilson_reference_values(self):
        low, high = metrics.wilson_interval(0, 10)
        self.assertEqual(low, 0.0)
        self.assertAlmostEqual(high, 0.27753, places=4)
        low, high = metrics.wilson_interval(10, 10)
        self.assertAlmostEqual(low, 0.72247, places=4)
        self.assertAlmostEqual(high, 1.0, places=12)
        self.assertIsNone(metrics.wilson_interval(0, 0))
        with self.assertRaises(ValueError):
            metrics.wilson_interval(11, 10)

    def test_false_alarms_use_all_adl_detections_and_duration(self):
        result = metrics.false_alarm_rate([
            outcome("adl", 2, duration_ms=1_800_000.0),
            outcome("adl", 0, duration_ms=1_800_000.0),
            outcome("fall", 5, duration_ms=10_000.0),
        ])
        self.assertEqual(result["adl_detections"], 2)
        self.assertAlmostEqual(result["false_alarms_per_hour"], 2.0)
        self.assertIsNone(metrics.false_alarm_rate([outcome("fall", 1)])["false_alarms_per_hour"])

    def test_latency_only_from_annotated_detected_falls(self):
        result = metrics.latency_summary([
            outcome("fall", 1, first_detection_ms=3000.0, fall_impact_ms=1000.0, fall_onset_ms=900.0),
            outcome("fall", 0, first_detection_ms=None, fall_impact_ms=1000.0),
            outcome("fall", 1, first_detection_ms=2000.0, fall_impact_ms=None),
            outcome("adl", 1, first_detection_ms=100.0),
        ])
        self.assertEqual(result["from_impact"]["n"], 1)
        self.assertEqual(result["from_impact"]["median_ms"], 2000.0)
        self.assertEqual(result["from_onset"]["min_ms"], 2100.0)
        self.assertIsNone(metrics.latency_summary([outcome("fall", 1)])["from_impact"])


class NormalizeTest(unittest.TestCase):
    def test_axis_map_parse_and_apply(self):
        axis_map = AxisMap.parse("x=+x,y=-z,z=+y")
        self.assertEqual(axis_map.apply((1.0, 2.0, 3.0)), (1.0, -3.0, 2.0))
        for invalid in ("x=+x,y=+y", "x=+x,y=+x,z=+z", "x=+w,y=+y,z=+z", "x=+x,x=+y,z=+z"):
            with self.assertRaises(ValueError):
                AxisMap.parse(invalid)

    def test_map_axes_rotates_accel_and_gyro_together(self):
        mapped = map_axes([(0.0, 1.0, 2.0, 3.0, 10.0, 20.0, 30.0)], AxisMap.parse("x=+z,y=+x,z=+y"))
        self.assertEqual(mapped[0], (0.0, 3.0, 1.0, 2.0, 30.0, 10.0, 20.0))

    def test_resample_uses_zero_order_hold_at_firmware_interval(self):
        samples = [(float(t), t / 1000.0, 0.0, 1.0, 0.0, 0.0, 0.0) for t in range(0, 201, 10)]
        rows, stats = resample_to_firmware_cadence(samples, 50.0, 150.0, 8.0, 500.0)
        self.assertEqual([row[0] for row in rows], [0.0, 50.0, 100.0, 150.0, 200.0])
        self.assertAlmostEqual(rows[1][2], 0.05)
        self.assertEqual(stats.invalid_rows, 0)

    def test_gap_becomes_invalid_rows_without_interpolation(self):
        samples = [(0.0, 0, 0, 1, 0, 0, 0), (10.0, 0, 0, 1, 0, 0, 0), (500.0, 0, 0, 1, 0, 0, 0)]
        rows, stats = resample_to_firmware_cadence(samples, 50.0, 150.0, 8.0, 500.0)
        self.assertEqual(len(stats.gaps), 1)
        invalid_ticks = [row[0] for row in rows if row[1] == 0]
        self.assertEqual(invalid_ticks, [200.0, 250.0, 300.0, 350.0, 400.0, 450.0])
        self.assertTrue(all(value == 0.0 for row in rows if row[1] == 0 for value in row[2:]))

    def test_values_beyond_firmware_range_are_clamped_and_counted(self):
        rows, stats = resample_to_firmware_cadence(
            [(0.0, 12.0, 0, 0, 900.0, 0, 0), (50.0, 0, 0, 1, 0, 0, 0)], 50.0, 150.0, 8.0, 500.0)
        self.assertEqual(rows[0][2], 8.0)
        self.assertEqual(rows[0][5], 500.0)
        self.assertEqual(stats.accel_saturated_samples, 1)
        self.assertEqual(stats.gyro_saturated_samples, 1)


class ModelTest(unittest.TestCase):
    def _trial(self, **overrides):
        values = dict(trial_id="t", dataset="d", subject_id="s", activity_code="a", label="adl",
                      samples=[(0.0, 0, 0, 1, 0, 0, 0), (10.0, 0, 0, 1, 0, 0, 0)], source_rate_hz=100.0,
                      sensor_position="waist", data_kind="real")
        values.update(overrides)
        return Trial(**values)

    def test_rejects_non_monotonic_or_non_finite_samples(self):
        with self.assertRaises(DatasetFormatError):
            self._trial(samples=[(10.0, 0, 0, 1, 0, 0, 0), (10.0, 0, 0, 1, 0, 0, 0)])
        with self.assertRaises(DatasetFormatError):
            self._trial(samples=[(0.0, math.nan, 0, 1, 0, 0, 0)])

    def test_rejects_fall_annotation_on_adl_and_unknown_kind(self):
        with self.assertRaises(ValueError):
            self._trial(fall_impact_ms=5.0)
        with self.assertRaises(ValueError):
            self._trial(data_kind="simulated")


class SisFallAdapterTest(unittest.TestCase):
    def test_parses_counts_with_explicit_conversion_and_label_from_name(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "F05_SA03_R02.txt"
            path.write_text("10,-20,256,100,0,-100,1,2,3;\n 11, -21, 257, 0, 0, 0, 1, 2, 3;\n", encoding="utf-8")
            trial = sisfall.parse_file(path, accel_g_per_count=0.01, gyro_dps_per_count=0.5)
        self.assertEqual(trial.label, "fall")
        self.assertEqual(trial.subject_id, "SA03")
        self.assertEqual(trial.activity_code, "F05")
        self.assertEqual(trial.samples[1][0], 5.0)
        self.assertAlmostEqual(trial.samples[0][3], 2.56)
        self.assertAlmostEqual(trial.samples[0][6], -50.0)
        self.assertIsNone(trial.fall_impact_ms)

    def test_rejects_unexpected_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "D01_SE01_R01.txt"
            path.write_text("1,2,3;\n", encoding="utf-8")
            with self.assertRaises(DatasetFormatError):
                sisfall.parse_file(path)
        self.assertIsNone(sisfall.parse_name("F05_XX03_R02.txt"))
        self.assertEqual(sisfall.parse_name("D19_SE15_R01.txt")["kind"], "D")


class KFallAdapterTest(unittest.TestCase):
    HEADER = "TimeStamp(s),FrameCounter,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,EulerX,EulerY,EulerZ\n"

    def _write(self, directory, name, frames):
        lines = [self.HEADER]
        for index, frame in enumerate(frames):
            lines.append(f"{100 + index * 0.01:.2f},{frame},0,0,1,0,0,0,0,0,0\n")
        path = Path(directory) / name
        path.write_text("".join(lines), encoding="utf-8")
        return path

    def test_labels_drive_class_and_annotation_times(self):
        with tempfile.TemporaryDirectory() as directory:
            self._write(directory, "SA06T20R01.csv", range(1, 301))
            self._write(directory, "SA06T01R01.csv", range(1, 51))
            self._write(directory, "SA06T02R01.csv", range(1, 51))
            labels = Path(directory) / "labels.csv"
            labels.write_text(
                "subject_id,task_code,trial_id,fall_onset_frame,fall_impact_frame\n"
                "SA06,20,1,130,208\nSA06,T01,R01,,\n", encoding="utf-8")
            excluded = []
            trials = {t.trial_id: t for t in kfall.iter_trials(Path(directory), labels_path=str(labels),
                                                              excluded=excluded)}
        self.assertEqual(trials["SA06T20R01"].label, "fall")
        self.assertAlmostEqual(trials["SA06T20R01"].fall_onset_ms, 1290.0, places=3)
        self.assertAlmostEqual(trials["SA06T20R01"].fall_impact_ms, 2070.0, places=3)
        self.assertEqual(trials["SA06T01R01"].label, "adl")
        self.assertEqual(excluded, [{"trial_id": "SA06T02R01", "reason": "unlabeled"}])

    def test_incomplete_label_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            labels = Path(directory) / "labels.csv"
            labels.write_text("subject_id,task_code,trial_id,fall_onset_frame,fall_impact_frame\n"
                              "SA06,20,1,130,\n", encoding="utf-8")
            with self.assertRaises(DatasetFormatError):
                kfall.load_labels(labels)


class SplitTest(unittest.TestCase):
    def test_subject_split_is_deterministic_and_disjoint(self):
        subjects = [f"SA{index:02d}" for index in range(1, 24)]
        first = subject_split(subjects, "tg1", 0.5)
        second = subject_split(list(reversed(subjects)), "tg1", 0.5)
        self.assertEqual(first, second)
        self.assertFalse(set(first["dev"]) & set(first["test"]))
        self.assertEqual(sorted(first["dev"] + first["test"]), subjects)
        self.assertNotEqual(subject_split(subjects, "other", 0.5)["dev"], first["dev"])
        with self.assertRaises(ValueError):
            subject_split(["only"], "tg1", 0.5)


class ReplayEndToEndTest(unittest.TestCase):
    """Caracterizacao do detector real sobre fixtures SINTETICAS (nao e resultado experimental)."""

    def setUp(self):
        self.binary = Path(os.environ.get("FALLBENCH_REPLAY_BIN") or default_binary())
        if not self.binary.exists():
            if os.environ.get("FALLBENCH_REQUIRE_REPLAY") == "1":
                self.fail(f"binario de replay ausente: {self.binary}")
            self.skipTest("binario de replay nao compilado")

    def _evaluate(self, out_dir):
        completed = subprocess.run(
            [sys.executable, "-m", "fallbench", "evaluate", "--adapter", "normalized",
             "--root", str(RESEARCH_ROOT / "fixtures" / "synthetic"), "--replay-bin", str(self.binary),
             "--out", out_dir],
            cwd=RESEARCH_ROOT, capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        return json.loads((Path(out_dir) / "results.json").read_text(encoding="utf-8"))

    def test_synthetic_characterization_is_deterministic_and_labeled(self):
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first = self._evaluate(first_dir)
            second = self._evaluate(second_dir)

        self.assertEqual(first["data_kind"], "synthetic")
        self.assertEqual(first["metrics"], second["metrics"])
        detections = {trial["trial_id"]: trial["detection_times_ms"] for trial in first["trials"]}
        self.assertEqual(detections["SYN_FALL_LYING_01"], [4700])
        self.assertEqual(detections["SYN_ADL_JUMP_01"], [])
        self.assertEqual(detections["SYN_ADL_LIE_DOWN_01"], [])
        # Caracterizacao: a FSM nao verifica postura final; ficar imovel apos levantar ainda confirma.
        self.assertEqual(detections["SYN_FALL_RECOVERS_01"], [5700])
        self.assertEqual(first["protocol"]["detector_config"]["algorithm_version"],
                         "threshold_fsm_v2_time_features_v1")


if __name__ == "__main__":
    unittest.main()
