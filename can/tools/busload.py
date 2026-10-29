#!/usr/bin/env python3
"""Worst-case bus load per DBC, from GenMsgCycleTime. Fails above the limit (default 40 %)."""
from __future__ import annotations

import argparse
import sys

import dbc_util as u


def bus_load(path) -> tuple[int, float, int]:
    db = u.load(path)
    bitrate = u.BUS_BITRATE[path.stem]
    bits_per_s = 0.0
    for m in db.messages:
        cyc = u.cycle_ms(m)
        if cyc <= 0:
            raise ValueError(f"{path.name}: {m.name} has no GenMsgCycleTime")
        bits_per_s += u.frame_bits_worst_case(m.length, m.is_extended_frame) * (1000.0 / cyc)
    return len(db.messages), 100.0 * bits_per_s / bitrate, bitrate


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=float, default=40.0, help="max worst-case load in percent")
    args = ap.parse_args()
    failed = False
    print(f"{'bus':<12}{'msgs':>5}{'rate':>11}{'worst-case load':>18}")
    for f in u.dbc_files():
        if f.stem not in u.BUS_BITRATE:
            print(f"ERROR: no bit rate configured for {f.name} in dbc_util.BUS_BITRATE")
            return 1
        n, pct, rate = bus_load(f)
        flag = "  <-- OVER LIMIT" if pct > args.limit else ""
        print(f"{f.stem:<12}{n:>5}{rate:>11}{pct:>17.2f}%{flag}")
        failed |= pct > args.limit
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
