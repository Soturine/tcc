"""Laboratorio de amostragem/Nyquist (protocolo docs/research/wrist-sampling-nyquist-protocol.md, sec. 7).

Somente biblioteca padrao. Ferramentas para sinais JA digitalizados e uniformemente
amostrados:
- espectro de magnitude por FFT (janela Hann, zero padding ate potencia de 2);
- FIR passa-baixas por windowed-sinc (janela Hann) e atraso de grupo;
- decimacao com e sem filtro antialias;
- frequencia de alias prevista e borda de banda por energia acumulada.

Limites fisicos (nao contornaveis por software): a FFT de um sinal ja subamostrado nao
revela o que o ADC perdeu; filtragem digital so evita NOVO aliasing ao decimar; o filtro
antialias analogico/interno da IMU tem de ser conhecido pelo datasheet/configuracao.
Reamostrar para cima nao recupera informacao.
"""

from __future__ import annotations

import cmath
import math
from typing import Dict, List, Sequence, Tuple


def alias_frequency(frequency_hz: float, fs_hz: float) -> float:
    """Frequencia aparente apos amostrar ``frequency_hz`` a ``fs_hz`` (0..fs/2)."""
    if fs_hz <= 0:
        raise ValueError("fs_hz deve ser > 0")
    folded = math.fmod(abs(frequency_hz), fs_hz)
    return fs_hz - folded if folded > fs_hz / 2 else folded


def synthesize(components: Sequence[Tuple[float, float]], fs_hz: float, duration_s: float) -> List[float]:
    """Soma de senoides (amplitude, frequencia) amostrada a ``fs_hz``."""
    count = int(round(duration_s * fs_hz))
    return [sum(a * math.sin(2 * math.pi * f * n / fs_hz) for a, f in components) for n in range(count)]


def hann(length: int) -> List[float]:
    if length == 1:
        return [1.0]
    return [0.5 - 0.5 * math.cos(2 * math.pi * n / (length - 1)) for n in range(length)]


def _fft(values: List[complex]) -> List[complex]:
    n = len(values)
    if n == 1:
        return values
    even = _fft(values[0::2])
    odd = _fft(values[1::2])
    out = [0j] * n
    for k in range(n // 2):
        twiddle = cmath.exp(-2j * math.pi * k / n) * odd[k]
        out[k] = even[k] + twiddle
        out[k + n // 2] = even[k] - twiddle
    return out


def magnitude_spectrum(signal: Sequence[float], fs_hz: float, remove_mean: bool = True) -> Tuple[List[float], List[float]]:
    """(frequencias_hz, amplitude) de 0 a fs/2. Amplitude corrigida pelo ganho da janela."""
    if len(signal) < 4:
        raise ValueError("sinal curto demais para espectro")
    mean = sum(signal) / len(signal) if remove_mean else 0.0
    window = hann(len(signal))
    gain = sum(window) / len(window)
    size = 1
    while size < len(signal):
        size *= 2
    padded = [complex((x - mean) * w) for x, w in zip(signal, window)] + [0j] * (size - len(signal))
    spectrum = _fft(padded)
    half = size // 2
    freqs = [k * fs_hz / size for k in range(half + 1)]
    amps = [2 * abs(spectrum[k]) / (len(signal) * gain) for k in range(half + 1)]
    return freqs, amps


def amplitude_at(freqs: Sequence[float], amps: Sequence[float], target_hz: float, half_width_hz: float = 0.75) -> float:
    """Pico de amplitude perto de ``target_hz`` (tolera o espalhamento da janela)."""
    candidates = [a for f, a in zip(freqs, amps) if abs(f - target_hz) <= half_width_hz]
    return max(candidates) if candidates else 0.0


def peaks(freqs: Sequence[float], amps: Sequence[float], min_relative: float = 0.2) -> List[float]:
    """Frequencias de maximos locais com amplitude >= ``min_relative`` do maior pico."""
    top = max(amps)
    found = []
    for k in range(1, len(amps) - 1):
        if amps[k] >= amps[k - 1] and amps[k] > amps[k + 1] and amps[k] >= min_relative * top:
            found.append(freqs[k])
    return found


def lowpass_fir(cutoff_hz: float, fs_hz: float, taps: int) -> List[float]:
    """FIR windowed-sinc (Hann), ganho DC unitario. ``taps`` impar => fase linear."""
    if taps < 3 or taps % 2 == 0:
        raise ValueError("taps deve ser impar e >= 3")
    if not 0 < cutoff_hz < fs_hz / 2:
        raise ValueError("cutoff deve estar entre 0 e fs/2")
    middle = (taps - 1) / 2
    normalized = cutoff_hz / fs_hz
    kernel = []
    for n in range(taps):
        x = n - middle
        sinc = 2 * normalized if x == 0 else math.sin(2 * math.pi * normalized * x) / (math.pi * x)
        kernel.append(sinc)
    window = hann(taps)
    kernel = [k * w for k, w in zip(kernel, window)]
    total = sum(kernel)
    return [k / total for k in kernel]


def group_delay_s(taps: int, fs_hz: float) -> float:
    """Atraso de grupo de um FIR de fase linear: (N-1)/2 amostras."""
    return (taps - 1) / 2 / fs_hz


def convolve_same(signal: Sequence[float], kernel: Sequence[float]) -> List[float]:
    """Filtragem causal (como no dispositivo): y[n] = sum h[k] x[n-k]."""
    out = []
    for n in range(len(signal)):
        acc = 0.0
        for k, h in enumerate(kernel):
            if n - k < 0:
                break
            acc += h * signal[n - k]
        out.append(acc)
    return out


def decimate(signal: Sequence[float], factor: int, fs_hz: float, antialias: bool,
             taps: int = 101, cutoff_ratio: float = 0.8) -> Tuple[List[float], float, Dict]:
    """Reduz a taxa por ``factor`` inteiro. Com antialias, corta em ``cutoff_ratio`` x Nyquist novo."""
    if factor < 1:
        raise ValueError("factor deve ser >= 1")
    fs_out = fs_hz / factor
    info = {"factor": factor, "fs_out_hz": fs_out, "antialias": antialias}
    filtered = list(signal)
    if antialias and factor > 1:
        cutoff = cutoff_ratio * fs_out / 2
        kernel = lowpass_fir(cutoff, fs_hz, taps)
        filtered = convolve_same(signal, kernel)
        info.update({"cutoff_hz": cutoff, "taps": taps, "group_delay_s": group_delay_s(taps, fs_hz)})
        # Descarta o transitorio de partida do filtro causal.
        filtered = filtered[taps:]
    return filtered[::factor], fs_out, info


def upsample_linear(signal: Sequence[float], factor: int) -> List[float]:
    """Interpolacao linear (para demonstrar que NAO recupera banda perdida)."""
    out: List[float] = []
    for a, b in zip(signal, signal[1:]):
        out.extend(a + (b - a) * i / factor for i in range(factor))
    out.append(signal[-1])
    return out


def band_edge_hz(signal: Sequence[float], fs_hz: float, energy_fraction: float) -> float:
    """Menor f tal que a energia espectral acumulada (sem DC) atinge ``energy_fraction``."""
    if not 0 < energy_fraction <= 1:
        raise ValueError("energy_fraction deve estar em (0, 1]")
    freqs, amps = magnitude_spectrum(signal, fs_hz)
    power = [a * a for a in amps]
    total = sum(power[1:])
    if total == 0:
        return 0.0
    acc = 0.0
    for f, p in zip(freqs[1:], power[1:]):
        acc += p
        if acc >= energy_fraction * total:
            return f
    return freqs[-1]


def synthetic_condition_report(fs_reference_hz: float = 125.0,
                               components: Sequence[Tuple[float, float]] = ((1.0, 4.0), (0.5, 17.0)),
                               duration_s: float = 8.0) -> Dict:
    """Artefato reproduzivel: fixture 4 + 17 Hz a 125 Hz decimada para varias taxas."""
    reference = synthesize(components, fs_reference_hz, duration_s)
    resolution_guard_hz = 0.75
    conditions = []
    # Somente fatores inteiros: 125 -> 50 Hz exigiria reamostragem racional (L/M), nao implementada.
    for factor in (1, 2, 3, 4, 5, 6):
        for antialias in (False, True):
            if factor == 1 and antialias:
                continue
            out, fs_out, info = decimate(reference, factor, fs_reference_hz, antialias)
            freqs, amps = magnitude_spectrum(out, fs_out)
            entry = {
                **info,
                "fs_out_hz": round(fs_out, 4),
                "nyquist_hz": round(fs_out / 2, 4),
                "components": [],
            }
            apparent_all = [alias_frequency(frequency, fs_out) for _a, frequency in components]
            for index, (amplitude, frequency) in enumerate(components):
                apparent = apparent_all[index]
                # Alias que cai sobre outra componente nao pode ser medido separadamente.
                ambiguous = any(
                    other != index and abs(apparent - apparent_all[other]) <= 2 * resolution_guard_hz
                    for other in range(len(components))
                )
                entry["components"].append({
                    "true_hz": frequency,
                    "true_amplitude": amplitude,
                    "apparent_hz": round(apparent, 4),
                    "aliased": frequency > fs_out / 2,
                    "ambiguous_with_other_component": ambiguous,
                    "measured_amplitude": None if ambiguous else round(
                        amplitude_at(freqs, amps, apparent, resolution_guard_hz), 4),
                })
            conditions.append(entry)
    return {
        "data_kind": "synthetic",
        "disclaimer": "Fixture sintetica (senoides conhecidas). Nao e medicao de sinal humano nem da IMU.",
        "reference_fs_hz": fs_reference_hz,
        "components": [{"amplitude": a, "hz": f} for a, f in components],
        "duration_s": duration_s,
        "conditions": conditions,
        "notes": [
            "50 Hz nao e obtido de 125 Hz por fator inteiro; exige reamostragem racional L/M.",
            "Antialias com corte em 0,8 x Nyquist de saida: componentes logo abaixo de Nyquist "
            "sao atenuadas pela banda de transicao (fs > 2B nao basta sem margem).",
        ],
    }
