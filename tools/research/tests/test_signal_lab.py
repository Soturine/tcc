"""Testes do laboratorio de Nyquist com senoides conhecidas (sinais sinteticos)."""

import math
import sys
import unittest
from pathlib import Path

RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RESEARCH_ROOT))

from fallbench import signal_lab as lab  # noqa: E402

FS = 125.0
FIXTURE = ((1.0, 4.0), (0.5, 17.0))


class AliasFormulaTest(unittest.TestCase):
    def test_known_alias_values(self):
        self.assertAlmostEqual(lab.alias_frequency(17.0, 25.0), 8.0)
        self.assertAlmostEqual(lab.alias_frequency(17.0, 20.8333), 3.8333, places=3)
        self.assertAlmostEqual(lab.alias_frequency(17.0, 50.0), 17.0)  # abaixo de Nyquist
        self.assertAlmostEqual(lab.alias_frequency(30.0, 40.0), 10.0)
        self.assertAlmostEqual(lab.alias_frequency(40.0, 40.0), 0.0)
        with self.assertRaises(ValueError):
            lab.alias_frequency(1.0, 0.0)


class SpectrumTest(unittest.TestCase):
    def test_fft_finds_both_components_with_correct_amplitude(self):
        signal = lab.synthesize(FIXTURE, FS, 8.0)
        freqs, amps = lab.magnitude_spectrum(signal, FS)
        found = lab.peaks(freqs, amps)
        self.assertTrue(any(abs(f - 4.0) < 0.2 for f in found), found)
        self.assertTrue(any(abs(f - 17.0) < 0.2 for f in found), found)
        self.assertAlmostEqual(lab.amplitude_at(freqs, amps, 4.0), 1.0, delta=0.08)
        self.assertAlmostEqual(lab.amplitude_at(freqs, amps, 17.0), 0.5, delta=0.05)

    def test_resolution_matches_n_over_fs(self):
        signal = lab.synthesize(FIXTURE, FS, 8.0)  # 1000 amostras -> padding 1024
        freqs, _ = lab.magnitude_spectrum(signal, FS)
        self.assertAlmostEqual(freqs[1] - freqs[0], FS / 1024)
        self.assertAlmostEqual(freqs[-1], FS / 2)


class DecimationTest(unittest.TestCase):
    def setUp(self):
        self.reference = lab.synthesize(FIXTURE, FS, 8.0)

    def test_naive_decimation_to_25hz_aliases_17hz_into_8hz(self):
        out, fs_out, _ = lab.decimate(self.reference, 5, FS, antialias=False)
        freqs, amps = lab.magnitude_spectrum(out, fs_out)
        self.assertAlmostEqual(fs_out, 25.0)
        # Componente falsa em 8 Hz com a amplitude do 17 Hz original.
        self.assertGreater(lab.amplitude_at(freqs, amps, 8.0), 0.4)

    def test_antialias_filter_suppresses_alias_and_keeps_4hz(self):
        out, fs_out, info = lab.decimate(self.reference, 5, FS, antialias=True)
        freqs, amps = lab.magnitude_spectrum(out, fs_out)
        alias = lab.amplitude_at(freqs, amps, 8.0)
        self.assertLess(20 * math.log10(alias / 0.5), -30.0)  # >= 30 dB de atenuacao
        self.assertAlmostEqual(lab.amplitude_at(freqs, amps, 4.0), 1.0, delta=0.1)
        self.assertAlmostEqual(info["cutoff_hz"], 10.0)
        self.assertAlmostEqual(info["group_delay_s"], 50 / FS)

    def test_50hz_keeps_17hz_without_alias(self):
        out, fs_out, _ = lab.decimate(self.reference, 2, FS, antialias=True)
        freqs, amps = lab.magnitude_spectrum(out, fs_out)
        self.assertAlmostEqual(fs_out, 62.5)
        self.assertAlmostEqual(lab.amplitude_at(freqs, amps, 17.0), 0.5, delta=0.06)

    def test_upsampling_a_subsampled_signal_does_not_recover_17hz(self):
        low, fs_low, _ = lab.decimate(self.reference, 5, FS, antialias=True)
        restored = lab.upsample_linear(low, 5)
        freqs, amps = lab.magnitude_spectrum(restored, fs_low * 5)
        self.assertLess(lab.amplitude_at(freqs, amps, 17.0), 0.05)

    def test_filter_design_validation(self):
        kernel = lab.lowpass_fir(10.0, FS, 101)
        self.assertAlmostEqual(sum(kernel), 1.0)
        for a, b in zip(kernel, kernel[::-1]):  # simetrico = fase linear
            self.assertAlmostEqual(a, b, places=12)
        with self.assertRaises(ValueError):
            lab.lowpass_fir(10.0, FS, 100)
        with self.assertRaises(ValueError):
            lab.lowpass_fir(70.0, FS, 101)


class BandEdgeTest(unittest.TestCase):
    def test_energy_band_edge_tracks_components(self):
        signal = lab.synthesize(FIXTURE, FS, 8.0)
        # 4 Hz carrega 80% da energia (1^2 vs 0.5^2).
        self.assertLess(lab.band_edge_hz(signal, FS, 0.5), 5.0)
        self.assertGreater(lab.band_edge_hz(signal, FS, 0.95), 16.0)
        with self.assertRaises(ValueError):
            lab.band_edge_hz(signal, FS, 0.0)


class SyntheticReportTest(unittest.TestCase):
    def test_report_is_labeled_synthetic_and_flags_aliasing(self):
        report = lab.synthetic_condition_report()
        self.assertEqual(report["data_kind"], "synthetic")
        naive_25 = next(c for c in report["conditions"] if c["fs_out_hz"] == 25.0 and not c["antialias"])
        filtered_25 = next(c for c in report["conditions"] if c["fs_out_hz"] == 25.0 and c["antialias"])
        component = next(c for c in naive_25["components"] if c["true_hz"] == 17.0)
        self.assertTrue(component["aliased"])
        self.assertEqual(component["apparent_hz"], 8.0)
        self.assertGreater(component["measured_amplitude"], 0.4)
        filtered = next(c for c in filtered_25["components"] if c["true_hz"] == 17.0)
        self.assertLess(filtered["measured_amplitude"], 0.02)

    def test_report_only_uses_integer_factors_and_flags_ambiguous_alias(self):
        report = lab.synthetic_condition_report()
        rates = sorted({c["fs_out_hz"] for c in report["conditions"]})
        self.assertEqual(rates, [20.8333, 25.0, 31.25, 41.6667, 62.5, 125.0])
        self.assertNotIn(50.0, rates)
        at_20 = next(c for c in report["conditions"] if c["fs_out_hz"] == 20.8333 and not c["antialias"])
        alias = next(c for c in at_20["components"] if c["true_hz"] == 17.0)
        self.assertTrue(alias["ambiguous_with_other_component"])  # 3,83 Hz sobre o de 4 Hz
        self.assertIsNone(alias["measured_amplitude"])

    def test_transition_band_attenuates_component_just_below_nyquist(self):
        report = lab.synthetic_condition_report()
        at_41 = next(c for c in report["conditions"] if c["fs_out_hz"] == 41.6667 and c["antialias"])
        component = next(c for c in at_41["components"] if c["true_hz"] == 17.0)
        self.assertFalse(component["aliased"])
        self.assertLess(component["measured_amplitude"], 0.3)


if __name__ == "__main__":
    unittest.main()
