#!/usr/bin/env bash
# Run the same checks as CI on your machine.
#   Requires: python3 with can/tools/requirements.lock.txt installed, cmake, gcc.
#   Optional: gcc-arm-none-eabi (ARM job), cppcheck (static analysis). Missing optional tools are reported, not hidden.
set -euo pipefail
cd "$(dirname "$0")/.."

step() { printf '\n==> %s\n' "$1"; }
skipped=()

step "DBC rules";            python3 can/tools/check_dbc.py
step "Generated files";      python3 can/tools/generate.py --check
step "Bus load";             python3 can/tools/busload.py
step "Python tests";         python3 -m pytest -q

step "Firmware host build + tests"
( cd firmware && cmake --preset host-debug && cmake --build --preset host-debug && ctest --preset host-debug )

step "Firmware sanitizer build + tests"
( cd firmware && cmake --preset host-sanitize && cmake --build --preset host-sanitize && ctest --preset host-sanitize )

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  step "ARM cross-compile (libraries)"
  ( cd firmware && cmake --preset arm-libs && cmake --build --preset arm-libs )
else
  skipped+=("ARM cross-compile (arm-none-eabi-gcc not found)")
fi

if command -v cppcheck >/dev/null 2>&1; then
  step "cppcheck"
  cppcheck -q --std=c11 --enable=warning,style,performance,portability \
    --inline-suppr --error-exitcode=1 --suppress=missingIncludeSystem \
    -I firmware/lib/can_e2e -I firmware/lib/can_dti -DE2E_VECTORS_CSV='"x.csv"' \
    firmware/lib firmware/test
else
  skipped+=("cppcheck (not found)")
fi

printf '\nALL CHECKS PASSED\n'
if [ "${#skipped[@]}" -gt 0 ]; then
  printf 'NOT RUN on this machine:\n'; printf '  - %s\n' "${skipped[@]}"
fi
