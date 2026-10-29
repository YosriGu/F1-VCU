# Contributing

## Ground rules
1. **The DBC files are the single source of truth** for CAN IDs, signals, periods, timeouts, safety classes and DataIds. Do not copy those values by hand into code, dashboards or loggers.
2. `can/gen/` and `docs/generated/` are **generated**. Never edit them; run `python can/tools/generate.py`.
3. Nothing merges to `main` without a pull request, passing CI, and the reviewers CODEOWNERS asks for.
4. CAN may only **remove** torque, never add it. Safety functions are hardware, not CAN.

## Workflow
```bash
git switch -c feature/<area>-<short-name>      # e.g. feature/ams-limits-frame
# ... work ...
scripts/ci-local.sh                            # same checks as CI
git commit -m "can: set AMS_LIMITS timeout to 60 ms"
git push -u origin HEAD                         # then open a pull request
```
Branch names: `feature/...`, `fix/...`, `docs/...`. Commit messages: imperative, start with the area (`can:`, `fw:`, `hw:`, `docs:`, `ci:`).

## Changing a CAN message
1. Edit the DBC (`can/dbc/*.dbc`). For a new message set: `GenMsgCycleTime`, `SafetyClass`, `RxTimeoutMs` (if the VCU receives it) and `DataId` (if class A/B or VCU-owned).
2. Run `python can/tools/check_dbc.py` and fix what it reports. The rules come from the CAN spec (ID ranges, E2E layout, class A timeout = 3 x period).
3. Run `python can/tools/generate.py` and commit the regenerated files.
4. Run `scripts/ci-local.sh`.
5. In the PR, state the safety class, period and timeout.

## Test evidence
Large logs do not go into Git. Add one line to `test-evidence/INDEX.csv` per test run with the firmware hash, DBC hash, DTI firmware and CAN map version, and a link to the log.

## Do not commit
Vendor manuals, NXP SDK/RTD, DTI or Orion documents (licensed). Link them from `docs/references.md`.
