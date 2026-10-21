"""Metricas de deteccao com unidade de avaliacao explicita: o TRIAL (gravacao).

- Trial de queda com >= 1 deteccao: TP; sem deteccao: FN.
- Trial de ADL com >= 1 deteccao: FP; sem deteccao: TN.
- Alarmes falsos por hora contam TODAS as deteccoes em ADL e dividem pela duracao
  total das gravacoes ADL; e outra granularidade (deteccao/tempo) e e reportada
  separadamente, nunca misturada com a matriz por trial.
- Latencia so existe quando o dataset traz onset/impacto anotados.

Denominador zero resulta em ``None`` (indefinido), nunca em 0 ou 1.
"""

from __future__ import annotations

import math
import statistics
from collections import defaultdict
from typing import Dict, Iterable, List, Optional

Z_95 = 1.959963984540054


def ratio(numerator: int, denominator: int) -> Optional[float]:
    return None if denominator == 0 else numerator / denominator


def wilson_interval(successes: int, total: int, z: float = Z_95) -> Optional[List[float]]:
    """Intervalo de Wilson para proporcao binomial; ``None`` sem observacoes."""
    if total == 0:
        return None
    if successes < 0 or successes > total:
        raise ValueError("successes fora de [0, total]")
    p = successes / total
    denominator = 1 + z * z / total
    center = (p + z * z / (2 * total)) / denominator
    margin = (z / denominator) * math.sqrt(p * (1 - p) / total + z * z / (4 * total * total))
    return [max(0.0, center - margin), min(1.0, center + margin)]


def confusion_from_outcomes(outcomes: Iterable[Dict]) -> Dict[str, int]:
    matrix = {"tp": 0, "fn": 0, "fp": 0, "tn": 0}
    for outcome in outcomes:
        detected = outcome["detections"] > 0
        if outcome["label"] == "fall":
            matrix["tp" if detected else "fn"] += 1
        elif outcome["label"] == "adl":
            matrix["fp" if detected else "tn"] += 1
        else:
            raise ValueError(f"label desconhecido: {outcome['label']!r}")
    return matrix


def summarize_confusion(matrix: Dict[str, int]) -> Dict:
    tp, fn, fp, tn = matrix["tp"], matrix["fn"], matrix["fp"], matrix["tn"]
    precision = ratio(tp, tp + fp)
    recall = ratio(tp, tp + fn)
    specificity = ratio(tn, tn + fp)
    f1 = None
    if precision is not None and recall is not None and (precision + recall) > 0:
        f1 = 2 * precision * recall / (precision + recall)
    elif precision is not None and recall is not None:
        f1 = 0.0
    return {
        "unit": "trial",
        "confusion": dict(matrix),
        "falls": tp + fn,
        "adls": fp + tn,
        "precision": precision,
        "recall_sensitivity": recall,
        "specificity": specificity,
        "f1": f1,
        "accuracy": ratio(tp + tn, tp + tn + fp + fn),
        "recall_ci95_wilson": wilson_interval(tp, tp + fn),
        "specificity_ci95_wilson": wilson_interval(tn, tn + fp),
        "precision_ci95_wilson": wilson_interval(tp, tp + fp),
        "notes": [
            "IC de Wilson assume trials independentes; trials do mesmo participante nao sao "
            "independentes, portanto o IC e otimista. Ver breakdown por participante.",
        ],
    }


def false_alarm_rate(outcomes: Iterable[Dict]) -> Dict:
    adl_detections = 0
    adl_duration_ms = 0.0
    for outcome in outcomes:
        if outcome["label"] == "adl":
            adl_detections += outcome["detections"]
            adl_duration_ms += outcome["duration_ms"]
    hours = adl_duration_ms / 3_600_000.0
    return {
        "unit": "detection_per_hour_of_adl_recording",
        "adl_detections": adl_detections,
        "adl_recording_hours": hours,
        "false_alarms_per_hour": None if hours == 0 else adl_detections / hours,
        "caveat": "Gravacoes ADL de laboratorio sao curtas e roteirizadas; nao extrapolar para uso continuo.",
    }


def latency_summary(outcomes: Iterable[Dict]) -> Dict:
    from_impact: List[float] = []
    from_onset: List[float] = []
    for outcome in outcomes:
        first = outcome.get("first_detection_ms")
        if outcome["label"] != "fall" or first is None:
            continue
        if outcome.get("fall_impact_ms") is not None:
            from_impact.append(first - outcome["fall_impact_ms"])
        if outcome.get("fall_onset_ms") is not None:
            from_onset.append(first - outcome["fall_onset_ms"])

    def describe(values: List[float]) -> Optional[Dict]:
        if not values:
            return None
        ordered = sorted(values)
        return {
            "n": len(ordered),
            "min_ms": ordered[0],
            "median_ms": statistics.median(ordered),
            "max_ms": ordered[-1],
            "p95_ms": _nearest_rank(ordered, 0.95),
        }

    return {
        "definition": "primeira deteccao confirmada - instante anotado (ms) em trials de queda detectados",
        "from_impact": describe(from_impact),
        "from_onset": describe(from_onset),
    }


def _nearest_rank(ordered: List[float], quantile: float) -> float:
    rank = max(1, math.ceil(quantile * len(ordered)))
    return ordered[rank - 1]


def breakdown(outcomes: List[Dict], key: str) -> Dict[str, Dict]:
    groups: Dict[str, List[Dict]] = defaultdict(list)
    for outcome in outcomes:
        groups[outcome[key]].append(outcome)
    return {
        group: summarize_confusion(confusion_from_outcomes(items))["confusion"]
        for group, items in sorted(groups.items())
    }


def evaluate(outcomes: List[Dict]) -> Dict:
    return {
        "trial_level": summarize_confusion(confusion_from_outcomes(outcomes)),
        "false_alarms": false_alarm_rate(outcomes),
        "latency": latency_summary(outcomes),
        "by_subject": breakdown(outcomes, "subject_id"),
        "by_activity": breakdown(outcomes, "activity_code"),
    }
