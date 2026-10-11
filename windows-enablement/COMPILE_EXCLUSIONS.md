# COMPILE_EXCLUSIONS.md — Windows compile-exposure ledger (X)

Authoritative record of COMPILE_EXPOSURE binaries that do **not** compile under
clang-cl. Dispositioned per 04-test-strategy.md §1.2 rule 2 and
05-quality-review-2.md ID-3(d)/ID-6.

Mechanism (kept for any future entry): additive `if(NOT WIN32)` guard around
the `lse_add_test` call (and its dependent `target_sources` /
`target_compile_definitions` / `set_tests_properties` lines) in
`tests/CMakeLists.txt`. This is a **compile-gate** exclusion — it never touches
`src/backends/hrx/**` and never uses KNOWN_FAILURES.md (which disables
execution only and cannot absorb a compile failure). POSIX configure is
byte-for-byte unaffected.

- **X = 0** (2026-10-10, C2-WP4/WP12). All three cycle-1/sync entries
  (`test_loom_cache`, `test_dflash2`, `test_launch_template`) were ported via
  `tests/compat.hpp` + the `tests/compat/` shim include-dir and their guards
  removed; `grep -c "if(NOT WIN32)" tests/CMakeLists.txt` == 0.

| Test | Guarded CMake lines (`tests/CMakeLists.txt`) | Reason | Issue / review-by |
|---|---|---|---|
