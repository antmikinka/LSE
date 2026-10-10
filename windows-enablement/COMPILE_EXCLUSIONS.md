# COMPILE_EXCLUSIONS.md — Windows compile-exposure ledger (X)

Authoritative record of COMPILE_EXPOSURE binaries that do **not** compile under
clang-cl at the M2a gate (WP10), dispositioned per 04-test-strategy.md §1.2
rule 2 and 05-quality-review-2.md ID-3(d)/ID-6.

Mechanism: additive `if(NOT WIN32)` guard around the `lse_add_test` call (and
its dependent `target_sources` / `target_compile_definitions` /
`set_tests_properties` lines) in `tests/CMakeLists.txt`. This is a
**compile-gate** exclusion — it never touches `src/backends/hrx/**` and never
uses KNOWN_FAILURES.md (which disables execution only and cannot absorb a
compile failure). POSIX configure is byte-for-byte unaffected.

- **X = 2** (target 0; X ≤ 5 without escalation per ID-6).
- Each is a whole-binary exclusion: the failure is in the test's own `.cpp`
  (`#include <unistd.h>`), not in the loomc/hipc translation units it compiles
  (those built clean under clang-cl).
- Both are ported at **WP12** via `tests/compat.hpp` (ID-14), which supplies
  portable `getpid`/`setenv`/`unsetenv`; at that point each guard is removed
  and the entry deleted here (driving X toward 0).

| Test | Guarded CMake lines (`tests/CMakeLists.txt`) | Reason | Issue / review-by |
|---|---|---|---|
| `test_loom_cache` | `lse_add_test(test_loom_cache …)` + `target_sources` + `target_compile_definitions` block, wrapped in `if(NOT WIN32)` | `windows: capability — test_loom_cache.cpp:19 includes <unistd.h> for ::getpid/::setenv/::unsetenv (UCRT has none)` | pending WP12 (tests/compat.hpp) — review-by 2027-01-08 |
| `test_dflash2` | `lse_add_test(test_dflash2 …)` + `if(TARGET lse_runtime)` + `if(NOT TARGET lse_backend_hrx) target_sources` + shared `set_tests_properties`, wrapped in `if(NOT WIN32)`/`else()` (else keeps `test_feature_taps` ENVIRONMENT on Windows) | `windows: capability — test_dflash2.cpp:30 includes <unistd.h> for getpid (UCRT has none)` | pending WP12 (tests/compat.hpp) — review-by 2027-01-08 |

## Not excluded (verified green under clang-cl at M2a)

The remaining 33 COMPILE_EXPOSURE binaries + `test_registration_smoke` compile
and link clean; see `logs/m2a-build.txt`. Note `test_dflash2_walk` and
`test_dflash2_convert` are **distinct** targets from `test_dflash2`:
`test_dflash2_walk` is in COMPILE_EXPOSURE and built green (no `<unistd.h>`);
`test_dflash2_convert` is outside COMPILE_EXPOSURE (compiles no hrx TU) and is
one of the 15 `<unistd.h>` files deferred wholesale to WP12, not ledgered here.
