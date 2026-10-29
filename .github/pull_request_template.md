## What and why
<!-- One or two sentences. Link the issue or test ID. -->

## Checklist
- [ ] `scripts/ci-local.sh` passes on my machine (or I say below what could not be run)
- [ ] If a **DBC** changed: `python can/tools/generate.py` was run and the regenerated files are in this PR
- [ ] If a **DBC** changed: safety class (A/B/C), period and timeout for every new or changed message are stated below
- [ ] If a rule or requirement is affected: `docs/traceability.csv` updated
- [ ] Test added or updated; evidence recorded in `test-evidence/INDEX.csv` (if this is a hardware or bench result)
- [ ] No node was added to PT-CAN, no SDC element is routed through the VCU PCB, no CAN input can raise torque

## Safety class and timing of changed messages
<!-- e.g. AMS_LIMITS: class A, 20 ms, timeout 60 ms (3 x period) -->

## Not verified / needs confirmation
<!-- Anything inferred rather than measured. Mark it as such. -->
