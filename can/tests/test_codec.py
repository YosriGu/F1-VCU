"""Every message in every DBC must encode and decode losslessly at its limits."""
import pytest

import dbc_util as u


def all_messages():
    return [pytest.param(m, id=f"{path.stem}:{m.name}")
            for path in u.dbc_files() for m in u.load(path).messages]


@pytest.mark.parametrize("msg", all_messages())
def test_round_trip_min_max_mid(msg):
    for pick in ("min", "max", "mid"):
        values = {}
        for s in msg.signals:
            lo = s.minimum if s.minimum is not None else 0
            hi = s.maximum if s.maximum is not None else (2 ** s.length - 1) * (s.scale or 1)
            values[s.name] = {"min": lo, "max": hi, "mid": (lo + hi) / 2}[pick]
        data = msg.encode(values, scaling=True, strict=True)
        assert len(data) == msg.length
        back = msg.decode(data, decode_choices=False, scaling=True)
        for s in msg.signals:
            tol = (s.scale or 1) / 2 + 1e-9
            assert abs(back[s.name] - values[s.name]) <= tol, (msg.name, s.name, pick)


@pytest.mark.parametrize("msg", all_messages())
def test_no_signal_leaves_the_frame(msg):
    for s in msg.signals:
        assert s.start + s.length <= msg.length * 8


def test_vcu_state_bit_layout_matches_spec():
    """Spec 5.1: byte 0 = CRC, byte 1 low nibble = counter, bits 12-13 carry the fault class."""
    m = u.load(u.DBC_DIR / "veh.dbc").get_message_by_name("VCU_STATE")
    sig = {s.name: s for s in m.signals}
    assert (sig["VCU_STATE_CRC"].start, sig["VCU_STATE_CRC"].length) == (0, 8)
    assert (sig["VCU_STATE_CNT"].start, sig["VCU_STATE_CNT"].length) == (8, 4)
    assert (sig["VCU_FaultClass"].start, sig["VCU_FaultClass"].length) == (12, 2)
