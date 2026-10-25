"""ML-0/ML-1/ML-3 com fixtures sinteticas: nenhum dado humano."""

import json
import math
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from motionlab import bundle  # noqa: E402
from motionlab.recorder import FrameError, SessionRecorder, SessionStateError  # noqa: E402


def frame(seq, t_us=None, **overrides):
    value = {"schema_version": 1, "sample_seq": seq, "device_time_us": seq * 20000 if t_us is None else t_us,
             "ax_g": 0.0, "ay_g": 0.0, "az_g": 1.0, "gx_dps": 0.0, "gy_dps": 0.0, "gz_dps": 0.0, "valid": True}
    value.update(overrides)
    return value


class RecorderTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def recorder(self, session="s1", chunk=4):
        return SessionRecorder(self.root, session, "p-0001", "pilot-v0",
                               {"data_kind": "synthetic", "hw": "fixture"}, chunk_frames=chunk)

    def test_state_machine_rejects_invalid_commands(self):
        rec = self.recorder()
        with self.assertRaises(SessionStateError):
            rec.append(frame(0))  # antes de start
        rec.start()
        rec.pause()
        with self.assertRaises(SessionStateError):
            rec.append(frame(0))  # pausado nao grava
        rec.resume()
        rec.append(frame(0))
        rec.stop()
        with self.assertRaises(SessionStateError):
            rec.start()

    def test_duplicates_out_of_order_and_gaps_are_counted_not_hidden(self):
        rec = self.recorder()
        rec.start()
        self.assertTrue(rec.append(frame(0)))
        self.assertTrue(rec.append(frame(1)))
        self.assertFalse(rec.append(frame(1)))   # duplicado
        self.assertTrue(rec.append(frame(5)))    # lacuna de 3
        self.assertFalse(rec.append(frame(4)))   # fora de ordem
        with self.assertRaises(FrameError):
            rec.append(frame(6, ax_g=math.nan))
        manifest = rec.stop()
        frames = manifest["frames"]
        self.assertEqual((frames["accepted"], frames["duplicates_rejected"], frames["out_of_order_rejected"]), (3, 1, 1))
        self.assertEqual((frames["gaps"], frames["missing_frames"], frames["invalid_rejected"]), (1, 3, 1))
        self.assertEqual(manifest["state"], "finalized")
        self.assertEqual(manifest["data_kind"], "synthetic")

    def test_export_reimport_roundtrip_and_effective_rate(self):
        rec = self.recorder(chunk=3)
        rec.start()
        for seq in range(10):
            rec.append(frame(seq))
        rec.mark("sit_to_stand", 60000)
        manifest = rec.stop()
        self.assertAlmostEqual(manifest["frames"]["effective_rate_hz"], 50.0)
        imported = bundle.import_session(rec.directory)
        self.assertEqual(imported.state, "finalized")
        self.assertEqual([f["sample_seq"] for f in imported.frames], list(range(10)))
        self.assertEqual(len([n for n in manifest["files"] if n.startswith("imu.raw.")]), 4)

    def test_tampered_or_missing_chunk_is_detected(self):
        rec = self.recorder()
        rec.start()
        for seq in range(8):
            rec.append(frame(seq))
        rec.stop()
        chunk = rec.directory / "imu.raw.0000.csv"
        chunk.write_text(chunk.read_text(encoding="utf-8").replace("1.0", "9.0", 1), encoding="utf-8")
        with self.assertRaises(bundle.BundleError):
            bundle.import_session(rec.directory)
        chunk.unlink()
        with self.assertRaises(bundle.BundleError):
            bundle.import_session(rec.directory)

    def test_injected_chunk_outside_manifest_is_rejected(self):
        rec = self.recorder()
        rec.start()
        rec.append(frame(0))
        rec.stop()
        (rec.directory / "imu.raw.9999.csv").write_text("x\n", encoding="utf-8")
        with self.assertRaises(bundle.BundleError):
            bundle.import_session(rec.directory)

    def test_interrupted_session_is_never_reported_as_finalized(self):
        rec = self.recorder(chunk=2)
        rec.start()
        for seq in range(5):
            rec.append(frame(seq))
        rec.interrupt()  # ex.: emergencia tem prioridade e o laboratorio para
        imported = bundle.import_session(rec.directory)
        self.assertEqual(imported.state, "interrupted")
        self.assertEqual(imported.manifest, {})
        self.assertEqual(len(imported.frames), 5)

    def test_empty_session_is_rejected(self):
        rec = self.recorder()
        rec.start()
        self.assertEqual(rec.stop()["state"], "rejected")


class AnnotationTest(unittest.TestCase):
    def annotation(self, **overrides):
        value = {"schema_version": 1, "label": "sit_to_stand", "source": "observer", "time_start_us": 0,
                 "time_end_us": 1_000_000, "uncertainty_us": 250_000, "review_status": "draft"}
        value.update(overrides)
        return value

    def test_valid_and_unknown_labels(self):
        bundle.validate_annotation(self.annotation())
        bundle.validate_annotation(self.annotation(label="unknown"))
        bundle.validate_annotation(self.annotation(label="ambiguous", source="video_review"))

    def test_model_prediction_is_not_ground_truth(self):
        with self.assertRaises(bundle.BundleError):
            bundle.validate_annotation(self.annotation(source="model"))

    def test_rejects_fall_label_and_bad_interval(self):
        with self.assertRaises(bundle.BundleError):
            bundle.validate_annotation(self.annotation(label="fall"))  # fora do estudo
        with self.assertRaises(bundle.BundleError):
            bundle.validate_annotation(self.annotation(time_end_us=-1))
        with self.assertRaises(bundle.BundleError):
            bundle.validate_annotation(self.annotation(uncertainty_us=None))


class TransitionMatchingTest(unittest.TestCase):
    def test_one_to_one_matching_does_not_double_count_overlapping_windows(self):
        truth = [{"label": "sit_to_stand", "time_us": 1_000_000}, {"label": "stand_to_sit", "time_us": 5_000_000}]
        predicted = [
            {"label": "sit_to_stand", "time_us": 1_100_000},
            {"label": "sit_to_stand", "time_us": 1_200_000},   # janela sobreposta: FP, nao 2 TP
            {"label": "stand_to_sit", "time_us": 9_000_000},   # fora da tolerancia
        ]
        result = bundle.match_transitions(truth, predicted, tolerance_us=500_000)
        self.assertEqual(result["sit_to_stand"], {"tp": 1, "fp": 1, "fn": 0})
        self.assertEqual(result["stand_to_sit"], {"tp": 0, "fp": 1, "fn": 1})


class ContractExamplesTest(unittest.TestCase):
    def test_example_frame_matches_recorder_contract(self):
        repo = Path(__file__).resolve().parents[3]
        example = json.loads((repo / "docs/contracts/motionlab/examples/lab-frame-v1.synthetic.json").read_text(encoding="utf-8"))
        from motionlab.recorder import validate_frame

        validate_frame(example)


if __name__ == "__main__":
    unittest.main()
