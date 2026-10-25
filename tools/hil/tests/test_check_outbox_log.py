"""Logs SINTETICOS no formato emitido pelo firmware; nao e evidencia HIL."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_outbox_log as hil  # noqa: E402

U1 = "esp32-aa:b00000003-1234abcd:00000001"


def run(scenario, text):
    return hil.check(scenario, hil.parse(text.strip().splitlines()))


class HilLogCheckTest(unittest.TestCase):
    def test_power_cut_passes_only_when_same_uuid_is_confirmed_after_reboot(self):
        log = f"""
[boot] boot_id=b00000002-1 counter_persisted=1 outbox_store=1 restored=0 corrupted=0 pending=0
[outbox] enqueued type=fall_detected event_uuid={U1} persisted=1 bytes=900 pending=1/16
[outbox] sent_awaiting_ack event_uuid={U1} pending=1
[boot] boot_id=b00000003-2 counter_persisted=1 outbox_store=1 restored=1 corrupted=0 pending=1
[outbox] result event_uuid={U1} status=duplicate reason=- outcome=removed pending=0 quarantined=0
"""
        self.assertTrue(all(r["ok"] for r in run("power_cut", log)))
        broken = log.replace("restored=1", "restored=0").replace("outcome=removed", "outcome=unknown_uuid")
        self.assertFalse(all(r["ok"] for r in run("power_cut", broken)))

    def test_rejected_permanent_must_quarantine_and_signal(self):
        good = f"""
[outbox] enqueued type=fall_detected event_uuid={U1} persisted=1 bytes=900 pending=1/16
[outbox] result event_uuid={U1} status=rejected_permanent reason=event_uuid_conflict outcome=quarantined pending=0 quarantined=1
[outbox] critical_event_not_delivered event_uuid={U1} reason=event_uuid_conflict
"""
        self.assertTrue(all(r["ok"] for r in run("rejected", good)))
        bad = good.replace("outcome=quarantined", "outcome=removed")
        self.assertFalse(all(r["ok"] for r in run("rejected", bad)))

    def test_sos_overflow_fails_the_scenario(self):
        log = """
[outbox] enqueued type=sos_pressed event_uuid=s1 persisted=1 bytes=500 pending=13/16
[outbox] overflow_rejected type=sos_pressed event_uuid=s2 capacity=16 overflow_count=1
"""
        checks = run("sos_under_load", log)
        self.assertFalse(all(r["ok"] for r in checks))

    def test_online_requires_persist_before_send_and_removal(self):
        log = f"""
[outbox] enqueued type=fall_detected event_uuid={U1} persisted=0 bytes=900 pending=1/16
[outbox] ack event_uuid={U1} event_id=7 status=committed outcome=removed pending=0
"""
        checks = run("online", log)
        self.assertFalse(next(r for r in checks if "persistido" in r["message"])["ok"])


if __name__ == "__main__":
    unittest.main()
