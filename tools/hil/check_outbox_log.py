#!/usr/bin/env python3
"""Verificador de log serial para o ensaio HIL da entrega critica (docs/quality/hil-bench-checklist-2026-10-25.md).

Le o log do monitor serial (115200) de UM ensaio e verifica as linhas que o firmware
emite (`[outbox] ...`, `[boot] ...`). Nao substitui o observador: confirma que a
sequencia esperada apareceu e que nada proibido aconteceu. Uso:

    python tools/hil/check_outbox_log.py --scenario power_cut serial.log

Saida: JSON com cada verificacao (ok/falhou) e codigo de saida 0 somente se todas passarem.
Sem hardware, este script NAO produz evidencia HIL: os testes usam logs sinteticos.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, List

ENQUEUED = re.compile(r"\[outbox\] enqueued type=(?P<type>\S+) event_uuid=(?P<uuid>\S+) persisted=(?P<persisted>\d)")
SENT = re.compile(r"\[outbox\] sent_awaiting_ack event_uuid=(?P<uuid>\S+)")
ACK = re.compile(r"\[outbox\] ack event_uuid=(?P<uuid>\S+) .*outcome=(?P<outcome>\S+)")
RESULT = re.compile(r"\[outbox\] result event_uuid=(?P<uuid>\S+) status=(?P<status>\S+) reason=(?P<reason>\S+) outcome=(?P<outcome>\S+)")
BOOT = re.compile(r"\[boot\] boot_id=(?P<boot>\S+) .*restored=(?P<restored>\d+) corrupted=(?P<corrupted>\d+) pending=(?P<pending>\d+)")
OVERFLOW = re.compile(r"\[outbox\] overflow_rejected type=(?P<type>\S+) event_uuid=(?P<uuid>\S+)")
TMP_PROMOTED = re.compile(r"\[outbox\] tmp_promoted")
NOT_DELIVERED = re.compile(r"\[outbox\] critical_event_not_delivered event_uuid=(?P<uuid>\S+)")


def parse(lines: List[str]) -> Dict:
    events: Dict = {"enqueued": [], "sent": [], "acks": [], "results": [], "boots": [], "overflow": [],
                    "tmp_promoted": 0, "not_delivered": []}
    phase = 0  # quantos [boot] ja apareceram: separa antes/depois de um corte
    for line in lines:
        if m := ENQUEUED.search(line):
            events["enqueued"].append({**m.groupdict(), "phase": phase})
        elif m := SENT.search(line):
            events["sent"].append(m["uuid"])
        elif m := RESULT.search(line):
            events["results"].append({**m.groupdict(), "phase": phase})
        elif m := ACK.search(line):
            events["acks"].append({**m.groupdict(), "phase": phase})
        elif m := BOOT.search(line):
            phase += 1
            events["boots"].append({k: (int(v) if k != "boot" else v) for k, v in m.groupdict().items()})
        elif m := OVERFLOW.search(line):
            events["overflow"].append(m.groupdict())
        elif TMP_PROMOTED.search(line):
            events["tmp_promoted"] += 1
        elif m := NOT_DELIVERED.search(line):
            events["not_delivered"].append(m["uuid"])
    return events


def removed(events: Dict, phases=None) -> List[str]:
    keep = (lambda item: True) if phases is None else (lambda item: item["phase"] in phases)
    out = [a["uuid"] for a in events["acks"] if a["outcome"] in ("removed", "removed_store_error") and keep(a)]
    out += [r["uuid"] for r in events["results"] if r["outcome"] in ("removed", "removed_store_error") and keep(r)]
    return out


def check(scenario: str, events: Dict) -> List[Dict]:
    results: List[Dict] = []

    def add(ok: bool, message: str) -> None:
        results.append({"ok": bool(ok), "message": message})

    enqueued = [e["uuid"] for e in events["enqueued"]]
    persisted = {e["uuid"] for e in events["enqueued"] if e["persisted"] == "1"}
    gone = removed(events)

    # Invariantes validos para qualquer ensaio.
    add(all(u in enqueued for u in gone) or bool(events["boots"]),
        "nenhum UUID removido sem ter sido enfileirado (ou restaurado apos boot)")
    add(not [r for r in events["results"] if r["status"] == "rejected_permanent" and r["outcome"] == "removed"],
        "rejected_permanent nunca remove como entregue")
    add(len(gone) == len(set(gone)), "cada UUID removido no maximo uma vez")

    if scenario == "online":
        add(bool(enqueued), "evento enfileirado")
        add(set(enqueued) <= persisted, "todo evento persistido em flash antes do envio")
        add(set(enqueued) <= set(gone), "todo evento confirmado e removido")
    elif scenario == "power_cut":
        add(len(events["boots"]) >= 2, "houve reboot no ensaio (>= 2 linhas [boot])")
        last_phase = len(events["boots"])
        earlier = set(range(last_phase))
        before = ({e["uuid"] for e in events["enqueued"] if e["phase"] in earlier}
                  - set(removed(events, earlier)))
        add(bool(before), "havia evento pendente antes do corte")
        last_boot = events["boots"][-1] if events["boots"] else {"restored": 0, "corrupted": 0}
        add(last_boot["restored"] >= 1, f"evento(s) restaurado(s) no boot (restored={last_boot['restored']})")
        add(last_boot["corrupted"] == 0, "nenhum arquivo corrompido no boot")
        add(bool(before) and before <= set(removed(events, {last_phase})),
            "pendentes de antes do corte confirmados depois, com o MESMO UUID")
    elif scenario == "rejected":
        add(any(r["status"] == "rejected_permanent" and r["outcome"] == "quarantined" for r in events["results"]),
            "rejeicao permanente foi para quarentena")
        add(bool(events["not_delivered"]), "sinalizacao local critical_event_not_delivered emitida")
    elif scenario == "sos_under_load":
        sos = [e["uuid"] for e in events["enqueued"] if e["type"] == "sos_pressed"]
        add(bool(sos), "SOS aceito pela outbox mesmo com fila de quedas carregada")
        add(not [o for o in events["overflow"] if o["type"] == "sos_pressed"], "nenhum SOS recusado por overflow")
    else:
        raise ValueError(f"cenario desconhecido: {scenario}")
    return results


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", required=True, choices=["online", "power_cut", "rejected", "sos_under_load"])
    parser.add_argument("log")
    args = parser.parse_args(argv)
    lines = Path(args.log).read_text(encoding="utf-8", errors="replace").splitlines()
    results = check(args.scenario, parse(lines))
    print(json.dumps({"scenario": args.scenario, "results": results}, indent=2, ensure_ascii=False))
    return 0 if all(r["ok"] for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
