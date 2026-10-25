"""Contrato do adaptador WEDA-FALL e da metrica com janela.

Todas as fixtures sao SINTETICAS, geradas em diretorio temporario no layout documentado
do WEDA-FALL; nenhum dado real do dataset e lido ou versionado aqui.
"""

import math
import sys
import tempfile
import unittest
from pathlib import Path

RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RESEARCH_ROOT))

from fallbench import metrics  # noqa: E402
from fallbench.adapters import get_adapter, weda_fall  # noqa: E402
from fallbench.model import DatasetFormatError  # noqa: E402


def write_series(path: Path, prefix: str, rows):
    lines = [f"{prefix}_time_list,{prefix}_x_list,{prefix}_y_list,{prefix}_z_list"]
    lines += [",".join(str(value) for value in row) for row in rows]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def bursty_times(count=12):
    # Rajadas como as observadas: 4 amostras proximas e um intervalo de ~120 ms.
    times, t = [], 0.0
    for index in range(count):
        times.append(round(t, 3))
        t += 0.002 if (index % 4) != 3 else 0.115
    return times


class WedaFixture:
    def __init__(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name) / "dataset"
        (self.root / "50Hz" / "F01").mkdir(parents=True)
        (self.root / "50Hz" / "D01").mkdir(parents=True)
        times = bursty_times()
        accel = [(t, 0.0, 0.0, 9.80665) for t in times]
        gyro = [(t + 0.001, 0.0, 0.0, math.pi) for t in times]  # pi rad/s = 180 dps
        for code, user in (("F01", 1), ("D01", 1), ("D01", 22)):
            write_series(self.root / "50Hz" / code / f"U{user:02d}_R01_accel.csv", "accel", accel)
            write_series(self.root / "50Hz" / code / f"U{user:02d}_R01_gyro.csv", "gyro", gyro)
        # Trial com sensor faltando deve ser excluido e reportado.
        write_series(self.root / "50Hz" / "D01" / "U02_R01_accel.csv", "accel", accel)
        (self.root / "fall_timestamps.csv").write_text(
            "﻿filename,start_time,end_time\nF01/U01_R01,0.1,0.3\n", encoding="utf-8")

    def cleanup(self):
        self._tmp.cleanup()


class WedaFallAdapterTest(unittest.TestCase):
    def setUp(self):
        self.fixture = WedaFixture()
        self.addCleanup(self.fixture.cleanup)

    def load(self, **options):
        excluded = []
        trials = list(weda_fall.iter_trials(self.fixture.root, excluded=excluded, **options))
        return {trial.trial_id: trial for trial in trials}, excluded

    def test_registered_and_labels_from_activity_code(self):
        self.assertIs(get_adapter("weda_fall"), weda_fall)
        trials, excluded = self.load()
        self.assertEqual(sorted(trials), ["D01/U01_R01", "D01/U22_R01", "F01/U01_R01"])
        self.assertEqual(trials["F01/U01_R01"].label, "fall")
        self.assertEqual(trials["D01/U01_R01"].label, "adl")
        self.assertEqual(trials["F01/U01_R01"].data_kind, "real")
        self.assertEqual(trials["F01/U01_R01"].sensor_position, "wrist")
        self.assertEqual(excluded, [{"trial_id": "D01/U02_R01", "reason": "sensores incompletos: ['accel']"}])

    def test_units_converted_to_g_and_dps(self):
        trials, _ = self.load()
        sample = trials["D01/U01_R01"].samples[-1]
        self.assertAlmostEqual(sample[3], 1.0, places=6)    # 9.80665 m/s^2 -> 1 g
        self.assertAlmostEqual(sample[6], 180.0, places=6)  # pi rad/s -> 180 dps

    def test_merge_starts_when_both_sensors_exist_and_is_strictly_increasing(self):
        trials, _ = self.load()
        samples = trials["D01/U01_R01"].samples
        self.assertEqual(samples[0][0], 1.0)  # primeiro gyro em 0.001 s
        self.assertTrue(all(b[0] > a[0] for a, b in zip(samples, samples[1:])))

    def test_fall_window_and_population_are_carried(self):
        trials, _ = self.load()
        fall = trials["F01/U01_R01"]
        self.assertEqual(fall.fall_onset_ms, 100.0)
        self.assertIsNone(fall.fall_impact_ms)
        self.assertIn("population=young", fall.notes)
        self.assertIn("population=elder", trials["D01/U22_R01"].notes)

    def test_bursty_recorded_timebase_is_reported_and_uniform_is_explicit(self):
        recorded, _ = self.load(timebase="recorded")
        uniform, _ = self.load(timebase="uniform_index")
        recorded_note = next(n for n in recorded["D01/U01_R01"].notes if n.startswith("accel_dt_max_ms="))
        self.assertGreater(float(recorded_note.split("=")[1]), 100.0)
        self.assertIn("timebase=uniform_index", uniform["D01/U01_R01"].notes)
        steps = [b[0] - a[0] for a, b in zip(uniform["D01/U01_R01"].samples, uniform["D01/U01_R01"].samples[1:])]
        self.assertLess(max(steps), 100.0)

    def test_subject_filter_and_invalid_options(self):
        trials, _ = self.load(subjects={"U22"})
        self.assertEqual(list(trials), ["D01/U22_R01"])
        with self.assertRaises(ValueError):
            self.load(timebase="interpolated")
        with self.assertRaises(ValueError):
            self.load(rate_dir="100Hz")

    def test_wrong_header_is_excluded_not_guessed(self):
        bad = self.fixture.root / "50Hz" / "D01" / "U01_R01_gyro.csv"
        bad.write_text("t,x,y,z\n0,0,0,0\n0.1,0,0,0\n", encoding="utf-8")
        trials, excluded = self.load()
        self.assertNotIn("D01/U01_R01", trials)
        self.assertTrue(any("cabecalho" in item["reason"] for item in excluded))

    def test_missing_rate_directory_fails_explicitly(self):
        with self.assertRaises(DatasetFormatError):
            list(weda_fall.iter_trials(self.fixture.root.parent))


class WindowedMetricTest(unittest.TestCase):
    def test_early_detection_does_not_confirm_fall(self):
        outcomes = [
            {"label": "fall", "detection_times_ms": [100.0], "fall_onset_ms": 5000.0},   # cedo demais
            {"label": "fall", "detection_times_ms": [4600.0], "fall_onset_ms": 5000.0},  # dentro da tolerancia
            {"label": "fall", "detection_times_ms": [], "fall_onset_ms": 5000.0},
            {"label": "fall", "detection_times_ms": [9000.0], "fall_onset_ms": None},    # sem anotacao
            {"label": "adl", "detection_times_ms": [10.0]},
            {"label": "adl", "detection_times_ms": []},
        ]
        summary = metrics.windowed_confusion(outcomes)
        self.assertEqual(summary["confusion"], {"tp": 1, "fn": 2, "fp": 1, "tn": 1})
        self.assertEqual(summary["early_detections"], 1)
        self.assertEqual(summary["unannotated_falls"], 1)
        self.assertEqual(summary["early_tolerance_ms"], 500.0)

    def test_population_breakdown_from_notes(self):
        outcomes = [
            {"label": "adl", "detections": 1, "subject_id": "U22", "activity_code": "D01",
             "duration_ms": 1000.0, "notes": ["population=elder"], "detection_times_ms": [1.0]},
            {"label": "fall", "detections": 1, "subject_id": "U01", "activity_code": "F01",
             "duration_ms": 1000.0, "notes": ["population=young"], "detection_times_ms": [1.0],
             "fall_onset_ms": None, "fall_impact_ms": None, "first_detection_ms": 1.0},
        ]
        result = metrics.evaluate(outcomes)
        self.assertEqual(result["by_population"]["elder"], {"tp": 0, "fn": 0, "fp": 1, "tn": 0})
        self.assertEqual(result["by_population"]["young"], {"tp": 1, "fn": 0, "fp": 0, "tn": 0})


class SubjectBootstrapTest(unittest.TestCase):
    def outcomes(self):
        items = []
        # S1 detecta tudo, S2 nada: o IC por participante tem de ser largo.
        for subject, detected in (("S1", 1), ("S2", 0)):
            for _ in range(10):
                items.append({"label": "fall", "detections": detected, "subject_id": subject})
                items.append({"label": "adl", "detections": 0, "subject_id": subject})
        return items

    def test_deterministic_and_wider_than_trial_independence(self):
        first = metrics.subject_bootstrap(self.outcomes(), resamples=500)
        second = metrics.subject_bootstrap(self.outcomes(), resamples=500)
        self.assertEqual(first, second)
        low, high = first["recall_ci95"]
        self.assertLessEqual(low, 0.0 + 1e-9)
        self.assertGreaterEqual(high, 1.0 - 1e-9)
        wilson = metrics.wilson_interval(10, 20)
        self.assertGreater(high - low, wilson[1] - wilson[0])
        self.assertEqual(first["specificity_ci95"], [1.0, 1.0])
        self.assertEqual(first["fall_subjects"], 2)

    def test_no_falls_gives_undefined_recall_interval(self):
        result = metrics.subject_bootstrap([{"label": "adl", "detections": 0, "subject_id": "S1"}])
        self.assertIsNone(result["recall_ci95"])


if __name__ == "__main__":
    unittest.main()


class SpectralProfileTest(unittest.TestCase):
    def test_high_frequency_trial_shows_energy_above_firmware_nyquist(self):
        from fallbench import spectral_profile

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "dataset"
            for code, freq in (("F01", 15.0), ("D01", 2.0)):
                folder = root / "50Hz" / code
                folder.mkdir(parents=True)
                times = [n / 50.0 for n in range(400)]
                accel = [(t, 0.0, 0.0, 9.80665 * (1 + 0.5 * math.sin(2 * math.pi * freq * t))) for t in times]
                gyro = [(t, 0.0, 0.0, math.sin(2 * math.pi * freq * t)) for t in times]
                write_series(folder / "U01_R01_accel.csv", "accel", accel)
                write_series(folder / "U01_R01_gyro.csv", "gyro", gyro)
            report = spectral_profile.profile_dataset(root)
        fall = report["groups"]["fall/young"]["accel"]
        adl = report["groups"]["adl/young"]["accel"]
        self.assertGreater(fall["energy_above_10hz"]["median"], 0.9)
        self.assertLess(adl["energy_above_10hz"]["median"], 0.05)
        self.assertAlmostEqual(fall["fs_hz"]["median"], 50.0, places=1)
        self.assertEqual(report["firmware_nyquist_hz"], 10.0)
