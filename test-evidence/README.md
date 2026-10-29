# test-evidence

One line per test run in `INDEX.csv`. The raw logs (`.asc`, `.blf`, scope screenshots) are **not** committed (large, binary); store them as release assets or on the team drive and put the link in `log_location`.

Test IDs come from `docs/VCU_CAN_Implementation_Spec.md` (T-DTI-1..7, P-1..5, S-1..13). `dbc_hash` = `sha256sum can/dbc/*.dbc | sha256sum`.
