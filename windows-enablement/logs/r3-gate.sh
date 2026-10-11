#!/usr/bin/env bash
# G-C2-0 Step R3 — targeted regression (B-1-safe; 11 §2.3 verbatim + ID-C2-2 corrections)
set -euo pipefail
cd /c/Users/antmi/LSE

EXCL=$(awk -F'|' '/^\| `test_/ {gsub(/[` ]/,"",$2); print $2}' windows-enablement/COMPILE_EXCLUSIONS.md)
NEXPOSURE=$(grep -c "" windows-enablement/COMPILE_EXPOSURE.txt)
NEXCL=$(printf '%s\n' "$EXCL" | grep -c . || true)
[ "$NEXPOSURE" -ge 30 ] || { echo "G-C2-0 ABORT: exposure list degenerate ($NEXPOSURE)"; exit 1; }

RE=$( (grep -vxF "$EXCL" windows-enablement/COMPILE_EXPOSURE.txt; \
       echo test_registration_smoke) | sort | paste -sd'|' -)
NRE=$(echo "$RE" | tr '|' '\n' | wc -l)

[ "$NRE" -eq $((NEXPOSURE - NEXCL + 1)) ] || { echo "G-C2-0 ABORT: |RE|=$NRE != $((NEXPOSURE - NEXCL + 1))"; exit 1; }
[ "$NRE" -ge 30 ]                          || { echo "G-C2-0 ABORT: targeted set degenerate ($NRE)"; exit 1; }
echo "$RE" | tr '|' '\n' | grep -qE "^test_registration_smoke$" || { echo "G-C2-0 ABORT: smoke missing"; exit 1; }
echo "R3-CARDINALITY: NEXPOSURE=$NEXPOSURE NEXCL=$NEXCL NRE=$NRE"

cmake -S . -B build-win -G Ninja -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSE_BUILD_TESTS=ON > windows-enablement/logs/c2-sync-configure-tests.txt 2>&1
cmake --build build-win --target $(echo "$RE" | tr '|' ' ') 2>&1 | tee logs/c2-sync-targeted-build.txt | tail -2
grep -qE "error:|FAILED:" logs/c2-sync-targeted-build.txt && { echo "G-C2-0 ABORT: targeted build errors"; exit 1; }

ctest --test-dir build-win -R "^($RE)$" --output-on-failure 2>&1 | tee logs/c2-sync-targeted-ctest.txt | tail -4
grep -q "100% tests passed" logs/c2-sync-targeted-ctest.txt || { echo "G-C2-0 FAIL: not 100%"; exit 1; }
NRAN=$(sed -nE 's/.*out of ([0-9]+).*/\1/p' logs/c2-sync-targeted-ctest.txt | tail -1)
[ "$NRAN" -eq "$NRE" ] || { echo "G-C2-0 FAIL: ctest ran $NRAN, expected $NRE — regex degenerated"; exit 1; }
echo "R3-GATE: PASS ($NRAN/$NRE)"
