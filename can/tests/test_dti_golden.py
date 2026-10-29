"""Golden-capture test for the DTI status layout (spec 6.1.5, test T-DTI-1).

Skipped until the inverter team commits can/captures/dti_r4_golden.asc together with a filled
can/dbc/pt_dti_r4.dbc. Once the capture exists the test is mandatory: it fails if the DBC is empty or
if any frame in the capture does not decode.
"""
import json

import can
import pytest

import dbc_util as u

CAPTURE = u.ROOT / "can" / "captures" / "dti_r4_golden.asc"
EXPECTED = u.ROOT / "can" / "captures" / "dti_r4_expected.json"

pytestmark = pytest.mark.skipif(
    not CAPTURE.exists(),
    reason="no golden capture yet: inverter team delivers can/captures/dti_r4_golden.asc (T-DTI-1)")


def frames():
    return [m for m in can.ASCReader(str(CAPTURE)) if not m.is_error_frame]


def test_dbc_is_filled_in():
    assert u.load(u.DBC_DIR / "pt_dti_r4.dbc").messages, "capture exists but pt_dti_r4.dbc has no messages"


def test_every_frame_decodes_and_every_message_appears():
    db = u.load(u.DBC_DIR / "pt_dti_r4.dbc")
    seen = set()
    for f in frames():
        assert f.is_extended_id, f"non-extended frame in the PT-CAN capture: {f.arbitration_id:#x}"
        msg = db.get_message_by_frame_id(f.arbitration_id)   # KeyError = unknown id = test failure
        msg.decode(bytes(f.data))
        seen.add(msg.name)
    assert seen == {m.name for m in db.messages}


@pytest.mark.skipif(not EXPECTED.exists(), reason="no dti_r4_expected.json yet")
def test_expected_values():
    """dti_r4_expected.json: [{"frame_id": int, "occurrence": 0, "signals": {"name": value}, "tol": 0.01}, ...]"""
    db = u.load(u.DBC_DIR / "pt_dti_r4.dbc")
    allf = frames()
    for exp in json.loads(EXPECTED.read_text()):
        hits = [f for f in allf if f.arbitration_id == exp["frame_id"]]
        f = hits[exp.get("occurrence", 0)]
        decoded = db.decode_message(f.arbitration_id, bytes(f.data))
        for name, val in exp["signals"].items():
            assert abs(decoded[name] - val) <= exp.get("tol", 0.0), (exp["frame_id"], name)
