# can/captures

Put **bench captures** here. Currently expected:

- `dti_r4_golden.asc`  Vector ASC log of the DTI HV-550 status frames (test T-DTI-1, spec 6.1.5).
- `dti_r4_expected.json`  optional known values for specific frames (format in `test_dti_golden.py`).

Every capture needs a header comment in the first lines with: DTI firmware version, **CAN map version**,
status rate, node ID, date, who captured it, and how the values were cross-checked (DTI Tool screenshot, lab supply voltage, etc.).

Until `dti_r4_golden.asc` exists, `can/tests/test_dti_golden.py` is skipped. After it exists, it is mandatory and a stale `pt_dti_r4.dbc` fails CI.
