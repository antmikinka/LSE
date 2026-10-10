# 07 — Documentation Package (Stage 7)

Index, audience map, maintenance rules and upstream-PR / release-notes drafts
for the Windows-enablement documentation. Companion to
`06-implementation-log.md` (implementation record). Style rule for this
package: every claim carries a runnable command or a file reference; no
marketing language.

---

## 1. What was written where

| # | Artifact | Path | Author stage | Kind |
|---|---|---|---|---|
| 1 | Planning / toolchain analysis | `windows-enablement/01-planning-analysis.md` | Stage 1 | frozen record |
| 2 | Program plan (WP0–WP15, milestones, risks) | `windows-enablement/02-program-plan.md` | Stage 2 | frozen record |
| 3 | Quality review 1 | `windows-enablement/03-quality-review-1.md` | Stage 3 | frozen record |
| 4 | Test strategy (gates, numerics, T3 fingerprint) | `windows-enablement/04-test-strategy.md` | Stage 4 | frozen record |
| 5 | Quality review 2 — directive layer **ID-1…ID-14** | `windows-enablement/05-quality-review-2.md` | Stage 5 | frozen record; IDs remain normative until superseded |
| 6 | Implementation log (M2a evidence, deviations §4, discrepancies §5) | `windows-enablement/06-implementation-log.md` | Stage 6 | frozen record |
| 7 | **This file** — docs-package index + maintenance map | `windows-enablement/07-documentation.md` | Stage 7 | living |
| 8 | **Windows build guide (user-facing, canonical)** | `docs/WINDOWS.md` | Stage 7 | living |
| 9 | Compile-exposure set (35, machine-generated) | `windows-enablement/COMPILE_EXPOSURE.txt` + `gen_compile_exposure.sh` | Stage 6 (WP10) | regenerated, not hand-edited |
| 10 | Exclusions ledger (X = 2: `test_dflash2`, `test_loom_cache`) | `windows-enablement/COMPILE_EXCLUSIONS.md` | Stage 6 (WP10) | living — shrinks at WP12 |
| 11 | M2a gate evidence (full build log, targeted build, 34/34 ctest transcript) | `logs/m2a-build.txt` | Stage 6 | frozen record |
| 12 | Configure transcript, tests-OFF tree | `build-win-notests-configure.log` | Stage 6 | frozen record |

**Deliberately untouched:** `README.md` (fork-branch mergeability with the
churning upstream — a future README PR adds one link line:
`Windows: see docs/WINDOWS.md`). `BUILD_FROM_SOURCE.md` §Windows was written at
WP1 (`9ce9287`) and is **stale**: its `cmake --build build` with default
`LSE_BUILD_TESTS=ON` builds `all`, which fails on the 15 unported `<unistd.h>`
test files (ID-3b; 06-log §5.26). `docs/WINDOWS.md` supersedes it and says so
in its preamble; the snippet becomes correct only after WP12 (see §3).

## 2. Audience map

| Reader | Wants | Serves them |
|---|---|---|
| Developer on Windows: build/test/run | exact prereqs, copy-pasteable commands, known limits, fixes for common failures | **`docs/WINDOWS.md`** (single entry point) |
| Fork maintainer / next pipeline stage (Phase 2: WP11–WP13, M3/M4) | what was decided and why; what is normative; what evidence exists | `01`–`06` (context), `07` (this file: what to update when), `COMPILE_EXCLUSIONS.md` (ledger state) |
| CI author (WP13) | gate commands to encode, parity requirements | `04-test-strategy.md` §5 (windows.yml + Linux parity), `docs/WINDOWS.md` §3–4 (the exact commands), `05` ID-10 (YAML fixes) |
| Upstream reviewer (Geramy/LSE) | scope, guarantees, evidence, honest gaps | PR description (§5 below) + `06` §4–5 + `logs/m2a-build.txt` |
| Fork release consumers | what changed, what works, what doesn't | release-notes snippet (§6) |

## 3. Maintenance notes (what changes when)

Rule: `docs/WINDOWS.md` is the **only** user-facing doc that changes per
milestone. `01`–`06` are frozen; corrections go into a new numbered artifact
(`08-…`), never by editing history. Ledger/`COMPILE_EXPOSURE.txt` changes are
regenerated/edited in place (they are working state, not records).

| Trigger | Required doc updates |
|---|---|
| **WP11 / M2b** (executables + tokenizer sentinel green) | `docs/WINDOWS.md` §5 "Running": replace the M2a scope note with the verified end-to-end run commands + expected output; start runtime-K accounting notes if any case is ledgered. §6 "Numerics" row unchanged. |
| **WP12** (test-source ports; `tests/compat.hpp`) | (a) Delete each ledger entry in `COMPILE_EXCLUSIONS.md` as its guard is removed; at X=0 the file becomes a stub saying so. (b) Rewrite `docs/WINDOWS.md` §4 "Testing": plain `cmake --build build-win` + full `ctest --test-dir build-win` replaces the targeted-34 gate; drop the `all`-fails warning and the `<unistd.h>` troubleshooting row. (c) `BUILD_FROM_SOURCE.md` §Windows snippet becomes truthful — or better, shrink it to a pointer at `docs/WINDOWS.md` (coordinate with the README PR; do not churn upstream-owned text unnecessarily). (d) Update §1 table row 10 (X value) here. |
| **WP13** (CI: `.github/workflows/windows.yml` + Linux-parity job) | (a) Add a CI status line to `docs/WINDOWS.md` header ("verified continuously by CI" — badge only once the future README PR can host it; branch docs don't render repo badges meaningfully). (b) New §"CI" in `docs/WINDOWS.md` or a short `windows-enablement/08-ci.md` recording the workflow's toolchain pins (must match §1 table). (c) If CI pins differ from the §1 verified versions, §1 gains a "CI-pinned" column. |
| **M3** (full-suite green, K ≤ 5 ledgered, T3 fingerprint) | `docs/WINDOWS.md`: §4 expected results → full-suite counts; §6 "Numerics" row → T3 hash-equality status (HASH-EQ vs TOLERANCE partition per ID-5). Evidence file `logs/m3-*.txt` referenced from §8 table. |
| **M4** (3× green release protocol) | `docs/WINDOWS.md` header banner: "experimental — Phase 1 / M2a" → "supported (CPU backend)". Remove "Honest scope note" language that no longer applies. |
| **Phase 3 / HRX on Windows** (GPU spike) | `docs/WINDOWS.md` §6: GPU row removed or replaced with the real backend status; §3 configure-summary block re-captured (HIP/comgr/loomc lines change); `cmake/LSEOptions.cmake` force-OFF message text changes — keep §3 transcript in sync. |
| **Upstream merge/rebase events** | Re-verify every command in `docs/WINDOWS.md` §3–4 against the new base (line refs like `CMakeLists.txt:18-31`, `LSEOptions.cmake:16-22` drift on upstream churn; prefer re-running the commands over trusting the refs). |

Standing invariants for edits to `docs/WINDOWS.md`: (1) no claim without a
command or file ref; (2) limitations section never shrinks without the
in-code deviation note also disappearing (06-log §4 is the audit trail);
(3) expected-output blocks are copied from real logs, not typed.

## 4. Known inconsistencies in the package (recorded, not hidden)

1. `BUILD_FROM_SOURCE.md` §Windows (WP1 vintage) vs the M2a test constraint —
   described in §1 above; superseded by `docs/WINDOWS.md`, fix scheduled at
   WP12.
2. Stage 6's configure passed `-DCMAKE_C_COMPILER` (logged as the benign
   "not used" warning in `build-win-notests-configure.log`); `docs/WINDOWS.md`
   omits it. Cosmetic.
3. `LSE_ENABLE_HRX` reads `ON` in both build trees' `CMakeCache.txt` while the
   configure summary prints `OFF` — normal CMake cache/shadow behavior of the
   force-OFF (`LSEOptions.cmake:16-22`); documented in `docs/WINDOWS.md` §3 so
   nobody "fixes" it.

## 5. Proposed upstream PR (Geramy/LSE ← antmikinka/LSE `feature/windows-enablement`)

**Title:** `Windows x64 support (clang-cl + MSVC STL, CPU backend): build, link, 34/34 targeted tests`

**Body:**

```markdown
## What

Native Windows x64 build of LSE with clang-cl 21 + MSVC 14.44 STL/UCRT +
lld-link + Ninja + Rust (x86_64-pc-windows-msvc). C++26 host / C++20 device.

- **CMake foundation**: clang-cl compiler-ID branch with a C++26 dialect
  override for CMake ≤ 3.31 (`CMAKE_CXX26_STANDARD_COMPILE_OPTION
  "-clang:-std=c++26"`); `/WHOLEARCHIVE` spelling in `lse_force_load()`
  (cmake/LSELink.cmake) so self-registering archives survive COFF
  symbol-driven extraction; MSVC spelling of the warning wall and
  `/clang:-fno-fast-math /clang:-ffp-contract=off` (numerics policy intact).
- **Platform seam** `lse::platform` (src/core/platform_{posix,win}.cpp):
  64-bit-safe positioned I/O (ReadFile+OVERLAPPED; never 32-bit `_pread`),
  `_wsopen_s(_SH_DENYNO)` + CreateFileMappingW mapping with a handle registry,
  stat/ino identity, clocks (QueryInterruptTime via GetProcAddress), env,
  pipes, file locks — POSIX side is inline pass-throughs.
- **Communication**: zero-macro winsock seam (`sock_*` wrappers, WSA*→errno
  table) + WSAPoll poller (ms granularity, documented in-file); httplib
  untouched (select path).
- **Rust FFI**: cargo staticlib on the MSVC triple; raw-dylib stub filtering
  (cmake/LSERust.cmake) so lld-link never opens un-materializable
  `windows.0.5x.lib` tokens.
- **HRX gated OFF on Windows** (cmake/LSEOptions.cmake): CPU reference backend
  only; loomc/hipc test TUs still compile and are test-gated.

## Verification (all on Windows x64; evidence in `logs/m2a-build.txt`)

- tests-OFF full build: **[192/192], 0 errors**; `lse.exe` + `lse-server.exe`
  link and run (`--help` exit 0).
- Targeted gate: **34/34 binaries compile+link, ctest 34/34 PASS (13.4 s)**,
  incl. registration smoke (force-load works: all 8 self-registering archives).
- Exclusions ledger X=2 (`test_dflash2`, `test_loom_cache`: own `.cpp` needs
  `<unistd.h>`); port scheduled via `tests/compat.hpp`.

## What this PR does NOT do (honest scope)

- No GPU backend on Windows (HRX/HIP/amd_comgr/loomc stay OFF).
- No CI workflow yet (windows.yml + Linux-parity job is the follow-up PR).
- 15 `<unistd.h>` test files not yet ported → tests-ON `all` build fails;
  the targeted gate above is the supported test path until then.
- Known deviations, all documented in code: WSAPoll ms granularity;
  `heap_stats()` zeroed (no mallinfo2); abstract UNIX sockets unimplemented
  (as on macOS).

## Governance

Additive-first: every change is `#ifdef _WIN32` / `if(WIN32)` /
`if(NOT WIN32)` or a seam call. No `__APPLE__` block touched, no upstream
symbol renamed. Linux/macOS behavior is byte-for-byte preserved (each Windows
branch is inert on POSIX). Full decision record: `windows-enablement/01–06`.

## Suggested review path

1. `cmake/LSELink.cmake` + `cmake/LSECompilerFlags.cmake` (linkage/flags policy)
2. `src/core/platform_win.cpp` header (I/O safety decisions, ID-1)
3. `windows-enablement/06-implementation-log.md` §4–5 (deviations + UCRT facts)
4. `logs/m2a-build.txt` (gate transcript)
```

## 6. Fork release-notes snippet (antmikinka/LSE)

```markdown
### Windows (experimental) — v0.6.0-28-win.1

Base: upstream v0.6.0-28. Branch: `feature/windows-enablement`.

- Native Windows x64 build with clang-cl 21 + MSVC STL/UCRT + Ninja + Rust
  (MSVC triple). C++26 host. CPU reference backend only — no GPU/HRX on
  Windows yet.
- `lse.exe` and `lse-server.exe` build and run (`--help`, `models list`
  verified; end-to-end generation is the next milestone, M2b).
- Test status: targeted gate 34/34 PASS (compile-exposure set + registration
  smoke), 13.4 s. Full suite blocked on porting 15 POSIX-include test files
  (WP12); 2 binaries ledgered out (`test_dflash2`, `test_loom_cache`).
- Known limitations: WSAPoll millisecond poll granularity; heap statistics
  report zero (no UCRT mallinfo2); large-checkpoint (>2 GB) I/O uses the
  64-bit ReadFile/OVERLAPPED + file-mapping seam (unit-verified, limited
  field mileage).
- Build instructions: `docs/WINDOWS.md`. Program record:
  `windows-enablement/`.
```

## 7. Stage-7 self-check

- [x] `docs/WINDOWS.md`: every command verified against branch state —
  configure flags vs `CMakeCache.txt` of both trees + `logs/m2a-build.txt:1808`;
  HRX gating vs `cmake/LSEOptions.cmake:16-22`; exclusion mechanism vs
  `tests/CMakeLists.txt:104,195`; targeted-34 regex dry-run
  (`ctest -N -R` → "Total Tests: 34"); `lse.exe --help`/`models list` and
  `lse-server.exe --help` executed (exit 0); toolchain versions re-run live
  (clang 21.0.0git, cmake 3.31.9, ninja 1.13.0, rustc/cargo 1.93.1, VS
  17.14.37628.2, SDK 10.0.26100, vcvars64.bat present).
- [x] No flag or option invented: only cache variables and options that exist
  in `CMakeLists.txt` / `cmake/*.cmake` are documented.
- [x] No file outside {`docs/WINDOWS.md`, `windows-enablement/07-documentation.md`}
  modified; `README.md` untouched; `chroma_data/` untouched (untracked).
