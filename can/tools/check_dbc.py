#!/usr/bin/env python3
"""Static checks on the DBC files. Exit code 1 if any rule is violated.

Rules come from the VCU CAN spec: ID ranges (4.1), frame protection (5), timeouts (5.3).
"""
from __future__ import annotations

import sys

import dbc_util as u

# VEH-CAN ID ranges: (low, high, allowed sender nodes)   -- spec section 4.1
VEH_RANGES = [
    (0x001, 0x00F, {"VCU"}),
    (0x010, 0x01F, {"AMS"}),
    (0x020, 0x02F, {"VCU"}),
    (0x030, 0x03F, {"DASH"}),
    (0x100, 0x1FF, {"VCU"}),
    (0x200, 0x2FF, {"SENSOR"}),
    (0x300, 0x3FF, {"PDU"}),
    (0x400, 0x4FF, {"LOG", "VCU"}),   # 0x400 TIME_SYNC is sent by the VCU
]
# Reserved for the Orion BMS OBD2 default ECU ID (0x7E3 request / 0x7EB response) and UDS.
RESERVED_RANGE = (0x7E0, 0x7FF)

VALID_CLASSES = {"A", "B", "C"}

# Buses whose receiver is a third-party device that cannot carry the E2E header.
E2E_EXEMPT_BUSES = {"pt_dti_r4"}


def check_file(path) -> list[str]:
    errors: list[str] = []
    name = path.stem

    def err(msg, text):
        errors.append(f"{path.name}: {msg.name if msg is not None else '-'}: {text}")

    try:
        db = u.load(path)
    except Exception as exc:  # parse errors are rule violations too
        return [f"{path.name}: cannot be parsed strictly: {exc}"]

    ids: dict[int, str] = {}
    names: set[str] = set()
    data_ids: dict[int, str] = {}

    for m in db.messages:
        # --- identity
        if m.frame_id in ids:
            err(m, f"duplicate frame id {m.frame_id:#x} (also {ids[m.frame_id]})")
        ids[m.frame_id] = m.name
        if m.name in names:
            err(m, "duplicate message name")
        names.add(m.name)

        if len(m.senders) != 1:
            err(m, f"must have exactly one sender, has {m.senders}")
        if not 1 <= m.length <= 8:
            err(m, f"DLC {m.length} outside 1..8 (classic CAN)")

        # --- bus rules
        if name in ("veh", "svc"):
            if m.is_extended_frame:
                err(m, "extended IDs are not allowed on VEH/SVC-CAN (11-bit only)")
            lo, hi = RESERVED_RANGE
            if lo <= m.frame_id <= hi:
                err(m, f"id {m.frame_id:#x} is in the reserved UDS/OBD2 range {lo:#x}-{hi:#x}")
        if name == "veh" and not m.is_extended_frame:
            owners = [r for r in VEH_RANGES if r[0] <= m.frame_id <= r[1]]
            if not owners:
                err(m, f"id {m.frame_id:#x} is outside every allocated VEH-CAN range")
            elif m.senders and not (set(m.senders) & owners[0][2]):
                err(m, f"sender {m.senders} not allowed in range "
                       f"{owners[0][0]:#x}-{owners[0][1]:#x} (allowed {sorted(owners[0][2])})")

        # --- attributes
        cyc = u.cycle_ms(m)
        cls = u.safety_class(m)
        rxt = u.rx_timeout_ms(m)
        did = u.data_id(m)
        exempt = u.e2e_exempt(m)

        if cyc <= 0:
            err(m, "GenMsgCycleTime must be > 0 (every message needs a period for the bus-load check)")
        if cls not in VALID_CLASSES:
            err(m, f"SafetyClass '{cls}' must be one of A, B, C")
        if u.vcu_is_receiver(m) and not u.vcu_is_sender(m):
            if rxt <= 0:
                err(m, "VCU receives this message, so RxTimeoutMs must be > 0")
            elif cls == "A" and rxt != 3 * cyc:
                err(m, f"class A timeout must be 3 x period ({3 * cyc} ms), is {rxt} ms (spec 5.3)")
            elif cls != "A" and cyc > 0 and rxt < 2 * cyc:
                err(m, f"timeout {rxt} ms is shorter than 2 periods ({2 * cyc} ms)")
        elif rxt != 0:
            err(m, "RxTimeoutMs is only meaningful for messages the VCU receives")

        # --- frame protection
        if exempt:
            if name not in E2E_EXEMPT_BUSES:
                err(m, "E2EExempt is only allowed on third-party buses "
                       f"({', '.join(sorted(E2E_EXEMPT_BUSES))})")
            if cls == "C":
                err(m, "E2EExempt is pointless on class C")
            if did != 0:
                err(m, "E2EExempt and DataId are mutually exclusive")
        elif cls in ("A", "B") and did == 0:
            err(m, "class A/B messages must carry the E2E header (set DataId 1..255)")
        if did != 0:
            if m.length != 8:
                err(m, "E2E messages must have DLC 8")
            if did in data_ids:
                err(m, f"DataId {did} already used by {data_ids[did]}")
            data_ids[did] = m.name
            if did == (m.frame_id & 0xFF):
                err(m, "DataId must not equal the low byte of the CAN id (spec 5.2)")
            crc = [s for s in m.signals if s.name.endswith("_CRC")]
            cnt = [s for s in m.signals if s.name.endswith("_CNT")]
            if len(crc) != 1 or len(cnt) != 1:
                err(m, "E2E message needs exactly one *_CRC and one *_CNT signal")
            else:
                c, n = crc[0], cnt[0]
                if (c.start, c.length, c.byte_order, c.is_signed) != (
                        u.E2E_CRC_START, u.E2E_CRC_LEN, "little_endian", False):
                    err(m, f"{c.name} must be bits 0..7, unsigned, little endian")
                if (n.start, n.length, n.byte_order, n.is_signed) != (
                        u.E2E_CNT_START, u.E2E_CNT_LEN, "little_endian", False):
                    err(m, f"{n.name} must be bits 8..11, unsigned, little endian")
    return errors


def main() -> int:
    errors: list[str] = []
    files = u.dbc_files()
    if not files:
        errors.append("no DBC files found in can/dbc")
    for f in files:
        errors += check_file(f)
    if errors:
        print("DBC CHECK FAILED")
        for e in errors:
            print("  -", e)
        return 1
    print(f"DBC check OK ({len(files)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
