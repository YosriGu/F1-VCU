"""Shared helpers for the CAN tooling. The DBC files are the single source of truth."""
from __future__ import annotations

from pathlib import Path

import cantools

ROOT = Path(__file__).resolve().parents[2]
DBC_DIR = ROOT / "can" / "dbc"
GEN_DIR = ROOT / "can" / "gen"

# Nominal bit rate per bus (VCU CAN spec, section 2). The key is the DBC file stem.
BUS_BITRATE = {
    "veh": 1_000_000,
    "svc": 500_000,
    "pt_dti_r4": 500_000,
}

# Frame-protection layout (VCU CAN spec, section 5.1): byte 0 = CRC-8, byte 1 bits 0-3 = counter.
E2E_CRC_START, E2E_CRC_LEN = 0, 8
E2E_CNT_START, E2E_CNT_LEN = 8, 4


def dbc_files() -> list[Path]:
    return sorted(DBC_DIR.glob("*.dbc"))


def load(path: Path):
    """Load a DBC strictly (overlapping signals, bad ranges and similar are errors)."""
    return cantools.database.load_file(str(path), strict=True)


def attr(msg, name: str, default):
    """Return a message attribute. cantools only lists attributes that were set explicitly."""
    if msg.dbc is None or name not in msg.dbc.attributes:
        return default
    return msg.dbc.attributes[name].value


def cycle_ms(msg) -> int:
    return int(attr(msg, "GenMsgCycleTime", 0))


def safety_class(msg) -> str:
    return str(attr(msg, "SafetyClass", "C"))


def rx_timeout_ms(msg) -> int:
    return int(attr(msg, "RxTimeoutMs", 0))


def data_id(msg) -> int:
    return int(attr(msg, "DataId", 0))


def e2e_exempt(msg) -> str:
    """Reason string if the message is exempt from the E2E header, else ''."""
    return str(attr(msg, "E2EExempt", "")).strip()


def vcu_is_sender(msg) -> bool:
    return "VCU" in msg.senders


def vcu_is_receiver(msg) -> bool:
    return any("VCU" in (sig.receivers or []) for sig in msg.signals)


def frame_bits_worst_case(dlc: int, extended: bool) -> int:
    """Worst-case frame length in bits including inter-frame space and worst-case stuffing.

    Standard: 47 + 8*dlc nominal, stuffable bits 34 + 8*dlc  (8 bytes -> 111 nominal, 135 worst).
    Extended: 67 + 8*dlc nominal, stuffable bits 54 + 8*dlc  (8 bytes -> 131 nominal, 160 worst).
    """
    if extended:
        nominal, stuffable = 67 + 8 * dlc, 54 + 8 * dlc
    else:
        nominal, stuffable = 47 + 8 * dlc, 34 + 8 * dlc
    return nominal + (stuffable - 1) // 4
