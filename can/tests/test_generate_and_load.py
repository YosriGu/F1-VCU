import subprocess
import sys

import dbc_util as u
import busload


def test_generated_code_is_up_to_date():
    r = subprocess.run([sys.executable, str(u.ROOT / "can" / "tools" / "generate.py"), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_frame_bit_formula_matches_spec():
    assert u.frame_bits_worst_case(8, False) == 135
    assert u.frame_bits_worst_case(8, True) == 160
    assert 47 + 8 * 8 == 111 and 67 + 8 * 8 == 131          # nominal lengths from the spec


def test_bus_load_within_limit():
    for path in u.dbc_files():
        n, pct, rate = busload.bus_load(path)
        assert pct <= 40.0, (path.name, pct)
