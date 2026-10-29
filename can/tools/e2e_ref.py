"""Python reference for the VCU frame protection (VCU CAN spec, section 5).

CRC-8/SAE-J1850: poly 0x1D, init 0xFF, xorout 0xFF, no reflection. Check value for b"123456789" is 0x4B.
Frame CRC = crc8( DATA_ID || byte1 .. byte7 ).  Counter: 0..14, wraps 14 -> 0, 15 is never valid.
"""
from __future__ import annotations


def crc8_j1850(data: bytes) -> int:
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def frame_crc(data_id: int, frame: bytes) -> int:
    assert len(frame) == 8
    return crc8_j1850(bytes([data_id]) + frame[1:8])


def protect(data_id: int, frame: bytearray, counter: int) -> None:
    """Write counter into byte1 bits 0..3, then the CRC into byte 0."""
    assert len(frame) == 8 and 0 <= counter <= 14
    frame[1] = (frame[1] & 0xF0) | counter
    frame[0] = frame_crc(data_id, bytes(frame))


def table() -> list[int]:
    """256-entry MSB-first table for poly 0x1D (without init / xorout)."""
    t = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = ((c << 1) ^ 0x1D) & 0xFF if c & 0x80 else (c << 1) & 0xFF
        t.append(c)
    return t
