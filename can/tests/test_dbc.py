"""The DBC checker must pass on the repo and must catch each class of mistake it claims to catch."""
from pathlib import Path

import pytest

import check_dbc
import dbc_util as u

VEH = (u.DBC_DIR / "veh.dbc").read_text()


def run(tmp_path, text, name="veh"):
    p = tmp_path / f"{name}.dbc"
    p.write_text(text)
    return check_dbc.check_file(p)


def test_repo_dbcs_pass():
    assert check_dbc.main() == 0


def test_baseline_copy_passes(tmp_path):
    assert run(tmp_path, VEH) == []


@pytest.mark.parametrize("old,new,expect", [
    # duplicate id
    ("BO_ 17 AMS_LIMITS: 8 AMS", "BO_ 16 AMS_LIMITS: 8 AMS", "duplicate frame id"),
    # sender in a range that belongs to someone else
    ("BO_ 48 DASH_IN: 4 DASH", "BO_ 48 DASH_IN: 4 AMS", "not allowed in range"),
    # id outside every allocated range
    ("BO_ 48 DASH_IN: 4 DASH", "BO_ 80 DASH_IN: 4 DASH", "outside every allocated"),
    # id inside the UDS/OBD2 range
    ("BO_ 1024 TIME_SYNC: 8 VCU", "BO_ 2019 TIME_SYNC: 8 VCU", "reserved UDS/OBD2"),
    # class A timeout must be 3 x period
    ('BA_ "RxTimeoutMs" BO_ 16 60;', 'BA_ "RxTimeoutMs" BO_ 16 100;', "3 x period"),
    # VCU receives a message but no timeout
    ('BA_ "RxTimeoutMs" BO_ 48 250;\n', "", "RxTimeoutMs must be > 0"),
    # class A without E2E header
    ('BA_ "DataId" BO_ 16 60;\n', "", "must carry the E2E header"),
    # DataId equal to low byte of the CAN id
    ('BA_ "DataId" BO_ 16 60;', 'BA_ "DataId" BO_ 16 16;', "low byte of the CAN id"),
    # duplicate DataId
    ('BA_ "DataId" BO_ 17 90;', 'BA_ "DataId" BO_ 17 60;', "already used"),
    # CRC signal in the wrong place
    ("SG_ AMS_STATE_CRC : 0|8@1+", "SG_ AMS_STATE_CRC : 56|8@1+", "must be bits 0..7"),
    # counter signal too wide
    ("SG_ AMS_LIM_CNT : 8|4@1+ (1,0) [0|15]", "SG_ AMS_LIM_CNT : 8|5@1+ (1,0) [0|31]", "must be bits 8..11"),
    # missing period
    ('BA_ "GenMsgCycleTime" BO_ 1024 1000;\n', "", "GenMsgCycleTime must be > 0"),
    # invalid safety class
    ('BA_ "SafetyClass" BO_ 1024 "C";', 'BA_ "SafetyClass" BO_ 1024 "X";', "must be one of A, B, C"),
])
def test_checker_catches(tmp_path, old, new, expect):
    assert old in VEH, f"mutation anchor missing: {old!r}"
    errors = run(tmp_path, VEH.replace(old, new, 1))
    assert any(expect in e for e in errors), (expect, errors)


def test_extended_id_rejected_on_veh(tmp_path):
    errors = run(tmp_path, VEH.replace("BO_ 1024 TIME_SYNC: 8 VCU", "BO_ 2147484672 TIME_SYNC: 8 VCU"))
    assert any("extended IDs are not allowed" in e for e in errors), errors


def test_placeholders_have_no_messages():
    for name in ("pt_dti_r4", "svc"):
        assert u.load(u.DBC_DIR / f"{name}.dbc").messages == []
