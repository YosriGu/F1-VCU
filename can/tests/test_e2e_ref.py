"""Python side of the frame-protection tests. The C side (firmware/test/test_crc.c) reads the same vectors."""
import csv
import random
from pathlib import Path

import e2e_ref as r

VECTORS = Path(__file__).parent / "vectors" / "e2e_vectors.csv"


def load_vectors():
    rows = []
    with VECTORS.open() as fh:
        for row in csv.reader(line for line in fh if not line.startswith("#") and line.strip()):
            rows.append((int(row[0], 16), bytes.fromhex(row[1]), int(row[2], 16)))
    return rows


def test_crc_check_value():
    assert r.crc8_j1850(b"123456789") == 0x4B


def test_table_matches_bitwise():
    t = r.table()
    rnd = random.Random(1)
    for _ in range(2000):
        data = bytes(rnd.randrange(256) for _ in range(rnd.randrange(0, 33)))
        crc = 0xFF
        for b in data:
            crc = t[crc ^ b]
        assert (crc ^ 0xFF) == r.crc8_j1850(data)


def test_vectors_present_and_consistent():
    vectors = load_vectors()
    assert len(vectors) >= 6
    for data_id, frame, crc in vectors:
        assert frame[0] == crc
        assert r.frame_crc(data_id, frame) == crc
        rebuilt = bytearray(frame)
        rebuilt[0] = 0
        r.protect(data_id, rebuilt, frame[1] & 0x0F)
        assert bytes(rebuilt) == frame


def test_protect_covers_state_nibble_and_data_id():
    f = bytearray(8)
    f[1] = 0xA0
    r.protect(0x5A, f, 3)
    assert f[1] == 0xA3
    g = bytearray(f)
    g[1] = 0xB3
    assert r.frame_crc(0x5A, bytes(g)) != f[0]          # state nibble is covered
    assert r.frame_crc(0x5B, bytes(f)) != f[0]          # DataId is covered
