# 13 — Cycle 2 implementation log (C2-WP3 … C2-WP6, G-C2-3, M3)

*Living log for cycle-2 implementation stages. Frozen artifacts 01–12 are not
edited; everything corrective lands here and in STATUS.md. Two authorship
hands are distinguished throughout: **[prior]** = the truncated Stage-5
session (C2-WP12a, ended mid-flight), **[cont]** = the continuation session
that finished WP12a, WP12b, G-C2-3, M3 and C2-WP6 (this hand also owns this
file). Dates are local; evidence paths are repo-relative.*

---

## 1. Scope and entry state

Entry (continuation hand): branch `feature/windows-enablement`, 11 commits
ahead of fork `antmikinka/LSE` (unpushed), last commits `91a4fdf` (C2-WP4
census), `8732b3d` (C2-WP3 product defects + M2b), `c73c6bb` (C2-WP3 SC4 +
K-ledger). The working tree carried the **uncommitted C2-WP12a stack** from
the prior session (27 modified files + new `tests/compat*`), i.e. WP12a was
assessed, not started from scratch.

Work order executed: finish WP12a → G-C2-3 → WP12b (four P8 tests) → M3
(G-C2-4) → C2-WP6 (`windows.yml`, G-C2-5 = PENDING-PUSH) → STATUS.md +
docs/WINDOWS.md → this log → commit + push.

## 2. Prior session vs continuation session (explicit split)

### 2.1 [prior] — committed before the continuation (in `c73c6bb`…`91a4fdf`)

- SYNC-1 merge of upstream v0.6.1 (`5a99199`), pre-sync tag, G-C2-0 evidence,
  STATUS.md, doc-drift fixes D-1/D-2 (`4e584aa`, `d3c2535`).
- SC4 gate script + K-LEDGER creation (`c73c6bb`).
- Product defect #1: `src/hub/pull.cpp` — Windows cannot delete/rename a file
  with an open handle; close-fd-before-remove pattern.
- Product defect #2: `src/communication/winsock_compat.hpp` — `WSASendMsg` is
  broken on Windows stream sockets (WSAEINVAL); `sock_sendmsg` remapped to
  `WSASend`.
- M2b acceptance: executables + tokenizer sentinel end-to-end (`8732b3d`).
- C2-WP4: L1 keep-going tests-ON build census of record (`91a4fdf`).

### 2.2 [prior] — uncommitted WP12a stack, assessed and finished by [cont]

- `tests/compat.hpp` + `tests/compat/` shim include-dir (WIN32-only; POSIX
  never adds the directory): `<unistd.h>`, `<sys/wait.h>`, `<dlfcn.h>`,
  `setenv`/`usleep`/`mkdtemp`/… — every semantic delta documented at its
  definition. `harness.hpp` includes `compat.hpp` under `#ifdef _WIN32`.
- Ports of ~15 `<unistd.h>` test files to the compat layer; X driven 3 → **0**
  (`COMPILE_EXCLUSIONS.md`, `grep -c 'if(NOT WIN32)' tests/CMakeLists.txt`==0);
  `COMPILE_EXPOSURE.txt` regenerated (40 names).
- `tests/CMakeLists.txt` restructure: full registration (W = 94 pre-P8),
  P8 block for the four new tests (env `LSE_BACKEND=cpu;LSE_POOL=cpu` for the
  fingerprint, `LABELS "LARGE_IO"` + `TIMEOUT 120` for large-io).
- `.gitattributes`: `tests/fixtures/** -text` (CRLF checkout corrupted
  byte-exact fixtures; blobs are LF so POSIX checkouts are unchanged).
- `include/lse/backends/hrx/compiler_image_identity.hpp` — **KEEP** (see §3).
- `tests/test_server_shutdown.cpp` port (+102 lines; Row-13), assorted small
  test fixes (test_api_parity, test_dflash2, test_graph, test_hub,
  test_loom_cache, test_model, test_model_info, test_q4_gemm,
  test_tree_verify, test_variant_book, test_compiler_identity,
  test_flash_prefill, test_gdn_rows).

### 2.3 [cont] — continuation-session work

1. **K-quarantine completion**: settled skip format
   (`windows: capability — [binary:case-group] <delta> — #N — review-by
   2027-01-08`), applied 4 skips (3 loopback-backpressure cases in
   test_communication, 1 p2996 case in test_reflect), wrote both K-LEDGER
   rows, verified binaries exit 0 and the frozen count pattern yields
   **K = 2 ≤ 5**. Fixed the frozen-pattern bug (§4 D-1).
2. **Product defects #3–#4** (found in the continuation):
   - **#3**: `src/core/platform_win.cpp` `map_discard` — probe-verified
     `DiscardVirtualMemory`/`MEM_RESET` semantics and rewrote map_discard as a
     hybrid (§4 D-2). Probe sources: `%TEMP%/commprobe/discard.cpp`,
     `discard2.cpp` (results in §4 D-2).
   - **#4**: `src/probe/profile_store.cpp` `write_text_atomic` — the temp name
     was keyed by PID only, so the two concurrent pool-qualification ranks in
     `test_probe` (same fingerprint → same profile path) shared one `.tmp<pid>`
     file; Windows rename/remove sharing semantics then intermittently left the
     final `.profile` unwritten (`qualifying_a_pool_...` read-back failed). Fixed
     with a per-call unique temp name (§4 D-14). Repro **3/50** standalone
     pre-fix → **0/100** post-fix. Surfaced only during the M3 full-suite run.
3. **WP12b — the four P8 tests** (11 §7):
   - `tests/test_fp_model.cpp` — ID-12 layer-C canary, 2/2; mutation
     acceptance run (§4 D-8), transcript:
     `windows-enablement/logs/id12-fp-canary-mutation.txt`.
   - `tests/test_platform_shim.cpp` — 04 §4 coverage, 20/20 (§4 D-3…D-7).
   - `tests/test_large_file_io.cpp` — >2 GiB sparse NTFS fixture
     (4 GiB + 1 MiB), probes at 0 / 2^31−512K / 2^31+1M / 4G−1M, straddle
     read, EOF short-read → kIoError, safetensors with data past 2^31;
     LARGE_IO label; local run **2/2 in 0.17 s** (spec budget <30 s).
   - `tests/test_numerics_fingerprint.cpp` — ID-5 partition (gemv, rms_norm,
     rope_apply = HASH_EQ; softmax, attention_step = TOLERANCE); fixed-seed
     splitmix64 fixtures; embedded cross-platform SHA constants + f32-bit
     goldens; 3/3 via ctest; two consecutive full-binary runs byte-identical
     (`diff` of stdout: IDENTICAL).
4. **test_model heavy real-model path** (the census's only model-gated slow
   case): two findings. (a) ctest `TIMEOUT`-property beats `--timeout`
   (test_memory_lifecycle with `TIMEOUT 60` ran 29.08 s to PASS under
   `ctest --timeout 5`) — proven, but NOT the resolution used here (see D-11).
   (b) This workstation's real HF cache holds `mlx-community/Qwen3.5-0.8B-4bit`,
   so test_model's heavy cases (`long_prefill`/`a_paged`/`a_split`) drive the
   CPU fp64 interpreter for **>2 h**; a standalone timed run was ABORTED at
   120 m39 s wall (still computing, no verdict) AND was running the STALE
   pre-relink test_model.exe (the tests-ON 'all' build relinked every other edge
   but lld-link hit "permission denied" on test_model.exe while the process held
   it) — non-evidence on both counts. Resolution (D-11): M3 runs with
   `HF_HUB_CACHE` pointed at an empty `.../hub` dir (CI no-cache parity); the
   heavy cases self-skip and test_model is green **55/55 in ~1 s**. Analysis:
   `windows-enablement/logs/test-model-heavy-path-note.txt`.
5. **C2-WP6**: `.github/workflows/windows.yml` authored per 11 §6 (new file,
   purely additive; existing ci.yml untouched), PyYAML-validated (jobs,
   triggers, cron `17 6 * * 1`). Deviations §4 D-10. **G-C2-5 = PENDING-PUSH.**
6. **G-C2-3 execution** (numbers §5) and **M3 execution** per 11 §5.2
   (numbers §6), evidence to `logs/` (root) and `windows-enablement/logs/`.
7. `ci-expected-w.txt` regenerated 94 → **98** (generated from `ctest -N`
   parse, never typed, per ID-C2-10).
8. STATUS.md refresh + docs/WINDOWS.md coherence pass (§7).

## 3. `compiler_image_identity.hpp` — disposition: KEEP

Scrutiny item from the work order. The change is **additive `#if
defined(_WIN32)`** branches only; POSIX and Apple paths are byte-identical:

- `WinFileTime` shim: UCRT `struct stat` has second-granularity `time_t`
  members (no `st_mtimespec`/`st_mtim`); the shim keeps `auto` deduction
  uniform across the three platforms.
- `same_compiler_file` on Windows keys on **size + mtime + ctime** and drops
  `st_dev`/`st_ino`: UCRT populates `st_dev` only for `stat()` (drive number,
  0 from `fstat()`) and `st_ino` is always 0 — comparing them would spuriously
  mismatch the *same* file seen via `fstat` vs `stat`. The remaining keys
  still reject a concurrent replacement, which is the function's contract.
  Empirically exercised by `test_compiler_identity` (green in the census).

Alternative rejected: excluding the header's consumers on Windows — that
would re-open an X-ledger entry against a target of X = 0 and lose real
coverage. KEEP stands.

## 4. Deviations (every one justified; numbered for citation)

- **D-1 — K-LEDGER frozen-pattern fix.** The row-format example inside the
  ledger's code fence starts with `| \`` and was counted by the frozen pattern
  `grep -cE '^\| \`'` (K showed 3 with 2 real rows). The example line is now
  indented one space + an explanatory note. Latent since C2-WP3; would have
  corrupted every gate K-check. Ledger is otherwise append-only.
- **D-2 — `map_discard` product change (spec alignment, not a new
  behavior).** 04 §4 / 05 :144 require discard → read-back zeros for anon
  memory; the shipped `MEM_RESET`-only implementation is *lazy* for private
  pages (probe: anon content survived). Probe results (10.0.26100):
  `DiscardVirtualMemory` on anon, page-aligned base, page-multiple size →
  ret 0, immediate zeros; unaligned base or non-multiple size → error 87,
  nothing discarded; **RO file-backed view → error 87 even aligned/rounded**;
  RW dirty file view → error 1224 (`ERROR_USER_MAPPED_FILE`). `MEM_RESET` on
  a RO file view correctly re-faults content from the file. Implementation:
  try `DiscardVirtualMemory(base, round_up(bytes))` when page-aligned, fall
  back to `MEM_RESET` — which preserves the only engine caller's contract
  (`weights.cpp:209 release_pages` on *file* mappings must re-fault file
  content, not zero it). Probe sources kept at `%TEMP%/commprobe/discard*.cpp`
  (not repo artifacts; results recorded here).
- **D-3 — shim clock-drift window 500 ms → 2 s.** `GetTickCount64` has
  ~15.6 ms quantization; a 1 % drift bound is unverifiable inside 500 ms.
- **D-4 — boottime ≥ monotonic asserted with 100 ms tolerance** (the two
  Windows clock sources can disagree by a scheduling quantum at read time).
- **D-5 — heap_stats JSON-key presence not asserted in the shim test.**
  `host_memory_json` is internal to `src/api/lse.cpp:215`; asserting its keys
  from a platform test would require linking `lse_api` (scope creep). The
  Windows contract actually specified (zeros, keys unchanged) is asserted:
  shim checks `heap_stats` returns zeros; key presence is covered by
  test_api_parity.
- **D-6 — no double-unmap assertion** (POSIX-undefined behavior cannot be a
  contract on either platform; commented in-test).
- **D-7 — `dl_close` not exercised**: POSIX `dlclose(NULL)` is UB and the
  Windows self-handle uses `GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT`
  (closing it would be wrong); commented in-test.
- **D-8 — FP canary hardened during mutation acceptance.** The mutation run
  exposed that the original canary could not fail on this toolchain: the
  project builds for baseline x86-64 (no `/arch:`/`-march` anywhere in
  cmake/), where the backend has **no FMA instruction to contract into** —
  pin removed, `plain` still rounded twice. Fix: the canary expressions live
  in helpers carrying `__attribute__((target("avx,fma")))` (clang-cl accepts
  it; note clang-cl does **not** define `__GNUC__`, only `__clang__`/
  `__x86_64__`/`_M_X64` — macro-dump verified). Disassembly probe (clang-cl
  21.1.8, /O2, no /arch): attributed `a*b+c` → `vfmadd213ss` by default;
  → `vmulss+vaddss` with `/clang:-ffp-contract=off`. With the hardened
  canary the acceptance sequence is exactly what ID-12 demands: pin removed →
  **0/2, plain=2^-24/2^-68, exit 1**; pin restored → **2/2, exit 0**
  (transcript: `windows-enablement/logs/id12-fp-canary-mutation.txt`;
  `cmake/LSECompilerFlags.cmake` verified byte-identical after restore,
  `git diff` empty).
- **D-9 — fingerprint `sha_of` bug (own code, fixed).** First embedding of
  the HASH-EQ constants produced per-process-varying SHAs: the helper built
  `std::span(v.data(), v.size() * sizeof(float))` — span counts are
  *elements*, so it hashed 4× the element count (16× the bytes), reading
  heap garbage past the vector. Fixed to element-count + `as_bytes`; the two
  post-fix runs are byte-identical and the constants were re-embedded from
  them. The TOLERANCE f32-bit goldens were never affected (pure float bits;
  `max_rel = 0.000e+00` against them in every run). Recorded because the
  first (garbage-mixed) constants briefly existed in the file.
- **D-10 — `windows.yml` deviations from the 11 §6 template** (all recorded
  in the file header too): (a) `shell: bash` everywhere per the WP6 work
  order — PowerShell-only idioms replaced by bash equivalents, same commands
  and assertions (L2 preserved); (b) artifact `path:` as a block scalar (the
  template's single-quoted `'a\nb\n'` passes literal backslash-n);
  (c) linux-parity gains `-DCMAKE_BUILD_TYPE=RelWithDebInfo` (template left
  it empty ⇒ -O0; both halves of the T3 pair should run the same
  optimization class); (d) linux-parity excludes `^test_reflect$` with
  adjusted count arithmetic — LLVM 21 has no `-freflection` at all (probe:
  clang-cl "unknown argument ignored"; `clang --driver-mode=g++` "error:
  unknown argument"), so the test's `#else` hard-fail branch compiles under
  clang++-21; upstream `ci.yml` (g++-16) remains the authoritative P2996
  gate, and on Windows the same gap is K-ledger #2 — the exclusion is
  labeled in the step summary, never silent; (e) weekly-schedule flake
  classification runs as a SECOND non-gating ctest invocation
  (`--repeat until-pass:3 --repeat until-fail:0`, `continue-on-error`) —
  the gating run stays strict on every event; (f) after `winget install
  LLVM.LLVM`, `C:\Program Files\LLVM\bin` is published to `GITHUB_PATH`
  (installer PATH changes are invisible to the running session) and ninja
  presence is checked with a winget fallback; (g) `ilammy/msvc-dev-cmd@v1`
  (x64) added before configure — clang-cl needs the MSVC `INCLUDE`/`LIB`
  environment (docs/WINDOWS.md §2), which a bare runner shell does not have;
  the template omitted it and the CMake compiler probe would have failed;
  (h) two path corrections: `LastTest.log` artifacts read from
  `build/Testing/Temporary/` (ctest `--test-dir` puts `Testing/` at the
  binary-dir root — verified locally, the template's
  `build/tests/Testing/...` never exists) and the I-5 KNOWN_FAILURES grep
  targets `tests/KNOWN_FAILURES.md` (the spec's root-level spelling would
  exit 2 on the missing file and silently skip the check).
- **D-11 — `test_model` heavy path: hide the model, do NOT add a TIMEOUT
  property (REVISED).** The inherited plan was a per-test `TIMEOUT ≈ 2×` the
  measured heavy-path duration (precedence proven: a `TIMEOUT` property beats
  `ctest --timeout`; test_memory_lifecycle ran 29.08 s to PASS under
  `--timeout 5`). Measurement killed that plan: the heavy real-model path runs
  **>2 h** on this machine's CPU (aborted at 120 m39 s, still computing), so a
  faithful `TIMEOUT` would be ~4 h — an absurd, hang-masking value — and the M3
  main run would burn hours on one test. It is also not CI-gated (hosted runners
  have no cache → the cases self-skip in seconds) and not Windows-specific (the
  same fp64 interpreter path runs on Linux CPU). The plan's §5.2 `--timeout 300`
  and its expected-count arithmetic already ASSUME the heavy path does not run.
  Resolution: reproduce the plan's assumed CI-like condition by exporting
  `HF_HUB_CACHE` = an empty `.../hub` dir for the M3 run. `hf_cache_root()`
  (src/model/weights.cpp:521) reads `HF_HUB_CACHE` first, so `resolve_model`
  finds no snapshot and every heavy case hits its `if (!paths.ok()) return;`
  self-skip. Light cases are untouched (each pins its own `HF_HUB_CACHE` via
  ScopedEnv/FakeHub, test_model.cpp:169); the dir must end in `/hub` because
  `hf_cache_root_honours_env` (:116) asserts the ambient root contains
  "hub"/"huggingface". Result: test_model 55/55 in ~1 s. **No TIMEOUT property
  added** (the plan does not request one; a multi-hour value would be
  counterproductive). The Windows seams the heavy path would touch (map-discard,
  >2 GiB I/O) are covered green by test_platform_shim 20/20 and
  test_large_file_io 2/2. Evidence:
  `windows-enablement/logs/test-model-heavy-path-note.txt`.
- **D-12 — M3 step 7 (linux parity) via A-3.3 hoist**: WSL has no
  clang++-21/ninja (C2-7 materialized at G-C2-0); parity evidence is the
  `linux-parity` CI job on the SAME pushed SHA. M3 is declared
  **M3-partial (parity pending CI)** until that job is green, exactly as
  G-C2-0 recorded.
- **D-13 — linux-parity W offset +2, asserted structurally**: UNIX registers
  `compiler_identity_default`/`compiler_identity_override` (tests/CMakeLists
  `if(UNIX)` block) which Windows does not; the parity job asserts
  `W_linux == ci-expected-w.txt + 2` with the reason commented in-step. Any
  other delta fails the job (B-1).
- **D-14 — 4th product defect: `write_text_atomic` needs a per-CALL temp name
  (src/probe/profile_store.cpp).** The M3 full-suite run surfaced an
  intermittent test_probe failure: the pool profile saved by the concurrent
  ranks test was missing on the read-back (test_probe.cpp:1133
  `fs::exists(profile_path(...))` false). Root cause: the temp name was keyed
  only by PID (`path + ".tmp" + pid()`), so two THREADS of one process saving
  the same fingerprint — exactly what the concurrent ranks of a pool
  qualification do (every rank computes the same fingerprint, pool.cpp:205,
  and each calls `save_pool_profile`, pool.cpp:290) — shared one temp file.
  POSIX hides the collision (rename(2) overwrites atomically; unlinking an
  open inode is harmless), but Windows refuses to rename over or remove a file
  another thread holds open, so the loser's error-path `fs::remove(tmp)` could
  delete the winner's temp mid-flight and the final profile never landed.
  Repro: **3 failures in 50** standalone runs pre-fix (iterations 7, 39, 40),
  plus the one under full-suite load during M3 attempt 3. Fix: append a
  process-wide `std::atomic<unsigned long long>` sequence to the temp name —
  unique per call, not merely per process. Temps are then disjoint on every
  platform, and the first rename to a not-yet-existing target always succeeds,
  so the profile is guaranteed to land; POSIX/macOS behavior is identical
  (minimal, platform-neutral diff). Verification: **0 failures in 100** runs
  post-fix and the M3 main run green 97/97 (attempt 4). Counted as cycle-2
  product defect #4 (§2.3).

## 5. G-C2-3 (tests-ON 'all' builds; X = 0) — results

**PASS — 2026-10-11** (captured after the D-14 profile_store fix and final
relink; all evidence regenerated against the final tree).

- **Builds**: tests-ON (`build-win`) `ninja all` exit **0** ("no work to do"
  at capture — tree fully linked); tests-OFF (`build-win-notests`) `ninja all`
  exit **0**, linking `lse.exe` + `lse-server.exe`.
- **X accounting**: COMPILE_EXCLUSIONS.md declares **"X = 0"**;
  `grep -c 'if(NOT WIN32)' tests/CMakeLists.txt` = **0**.
- **FP canary (ID-5/ID-12)** — layer A: source pin present at
  LSECompilerFlags.cmake:75 (`/clang:-fno-fast-math /clang:-ffp-contract=off`).
  Layer B: FLAGS violations = **0** in both trees (pinned rules: **476** in
  build-win, **167** in build-win-notests). Layer B′: `ninja -t commands
  lse_core` shows **11/11** TUs carrying the pinned flags in both trees.
- **Census**: `ctest -N` **Total Tests: 98**; W = 98 == `ci-expected-w.txt`
  (98); K = **2** (ledger, ceiling 5).
- **P8 tests**: **4/4 PASS** — test_fp_model (0.01 s), test_platform_shim
  (2.14 s), test_numerics_fingerprint (0.01 s), test_large_file_io (0.06 s),
  "100% tests passed, 0 tests failed out of 4".

Evidence (committed): `windows-enablement/logs/g-c2-3-builds.txt`,
`g-c2-3-x.txt`, `g-c2-3-canary.txt`, `g-c2-3-census.txt`, `g-c2-3-p8.txt`,
`id12-fp-canary-mutation.txt`, `ci-expected-w.txt`,
`test-model-heavy-path-note.txt`. (The intermediate `c2-wp12-*` captures were
DELETED before commit as stale: they recorded a superseded pre-relink state —
lld-link LoomEmitter errors, W = 94, 10/93 failures — that would contradict
the final green tree.)

## 6. M3 / G-C2-4 — results

**== M3 COMPLETE 2026-10-11T09:16:06Z — W=98 NRAN=97 K=2 X=0 ==**, declared
**M3-PARTIAL** (linux parity pending the `linux-parity` CI job on the same
pushed SHA, per D-12/A-3.3). Protocol: 11-cycle2-test-plan §5.2 verbatim,
B-1-safe count assertions (expected counts read from generated files, never
typed twice).

- **Step 1 census**: `ctest -N` Total Tests **98** == `ci-expected-w.txt`
  (98) — silent test loss/gain assertion passed.
- **Step 2 main run** (`--timeout 300 --label-exclude LARGE_IO`): **"100%
  tests passed, 0 tests failed out of 97"**, total wall **76.25 s**;
  NRAN = 97 == W − 1 asserted. test_model finished **55/55 in ~1 s** under
  the D-11 hide-cache protocol (`HF_HUB_CACHE` → empty `.../hub`; CI parity).
  The main run's LastTest.log was snapshotted to
  `logs/m3-maintest-LastTest.log` immediately after step 2 — every ctest
  invocation rewrites the live file, and steps 3–4 would otherwise erase the
  K-skip evidence (this ordering bug made the first attempt exit 1 at step 5
  with KD = 0 despite a green main run).
- **Step 3 LARGE_IO** (S4 §3/§7 mandatory local run): test_large_file_io
  **"out of 1"** PASS (0.06 s).
- **Step 4 FP canary layer C + fingerprint partition** (ID-5/ID-12):
  test_fp_model + test_numerics_fingerprint **"out of 2"** PASS.
- **Step 5 K accounting**: ledger **K = 2** (frozen pattern
  `grep -cE '^\| \`'`, ≤ 5); distinct bracketed skip keys scraped from the
  main-run snapshot = **2** — `[test_communication:loopback-backpressure]`
  and `[test_reflect:p2996]`; no "windows" entry in tests/KNOWN_FAILURES.md.
- **Step 6 X accounting**: "X = 0" present in COMPILE_EXCLUSIONS.md;
  **0** `if(NOT WIN32)` guards in tests/CMakeLists.txt.
- **Step 7**: M3-PARTIAL declaration (linux parity via A-3.3 hoist; the CI
  `linux-parity` job on the same SHA is the evidence).

The only failure met en route was the D-14 test_probe race (attempt 3, exit
8: 96/97 with test_probe failing at :1133); after the fix and relink,
attempt 4 ran the whole protocol green end-to-end.

Evidence (committed to root `logs/`, L4/L5): `m3-ctest-N.txt`,
`m3-ctest-windows.txt`, `m3-large-file-io.txt`, `m3-fingerprint-windows.txt`,
`m3-k-skips.txt` (the 4 skip lines + 2 distinct keys extracted from the
main-run LastTest.log snapshot; the raw `.log` stays local per the repo's
`*.log` gitignore convention — same precedent as `m2b-sc4-server.log`/`.txt`).
The K/X checks and the M3 COMPLETE line are emitted by the protocol script
itself.

## 7. Documentation updates

- **STATUS.md**: gate-table rows G-C2-1…G-C2-5 set from real outcomes;
  W/X/K current block refreshed (W = 98, X = 0, K = 2); new dated section
  for C2-WP12a/b + G-C2-3 + M3; A-4 toolchain pin (official LLVM 21.1.8,
  ROCm clang-cl 21.0.0git constexpr bug) recorded.
- **docs/WINDOWS.md**: header/status lifted from M2a/34-binary language to
  the tests-ON full-suite state; §1 toolchain row records the 21.1.8 pin
  (CI `--exact`); §4 count 94 → 98 + LARGE_IO label instructions;
  pre-WP12 troubleshooting entries removed/updated; §6 limitations table
  refreshed (tests row → full suite, K = 2 with review-by dates; numerics
  row → fingerprint landed, T3 pair via CI); §5 scope note updated for
  M2b/M3; §8 roadmap updated (WP12a/b done, WP6 landed pending push,
  M3-partial); product defects #1–#4 summarized with file pointers.

## 8. Commits and push

Continuation-session commits (explicit paths only; `chroma_data/` junk never
staged), on top of the prior-session `4e584aa`…`91a4fdf`:

- `7d6e230` — C2-WP12: four more Windows product defects + compiler-image
  identity port (pull.cpp, winsock_compat.hpp, platform_win.cpp,
  profile_store.cpp, compiler_image_identity.hpp).
- `77f1256` — C2-WP12: full test-tree Windows port — X 3→0, W→98 (P8 block)
  (compat layer, 16 ports, K skips, 4 P8 tests, .gitattributes).
- `122f802` — C2-WP12: ledgers to X=0/K=2, G-C2-3 PASS evidence, stale
  census removed (`c2-wp12-census.txt` deleted; superseded, history retains).
- `fcc6295` — C2-WP6: interim Windows CI gate + linux-parity hoist (A-3.3)
  (`.github/workflows/windows.yml`).
- C5 (this file's commit) — C2-WP5: M3 protocol green locally (M3-PARTIAL);
  M3 evidence to root `logs/`, STATUS.md refresh, docs/WINDOWS.md coherence,
  this log. Its own hash is necessarily self-referential — `git log`
  immediately after it.

Push target: `fork feature/windows-enablement` (antmikinka/LSE), executed
immediately after C5. **G-C2-5 stays PENDING-PUSH** until the workflow is
observed on the pushed SHA: first run green on BOTH jobs at the same SHA
(which also converts M3-PARTIAL → M3 per D-12), dispatch proof with
`run_large_io=true` (only if the runner disk guard passes — otherwise the
schedule run is the LARGE_IO proof), and the cron line verified via
`gh workflow view windows.yml --repo antmikinka/LSE --yaml | grep cron`.

## 9. Remaining work toward M4

1. **G-C2-5**: push; observe first `windows` workflow run green on BOTH jobs
   at the same SHA (that also converts M3-partial → M3, D-12); dispatch run
   with `run_large_io=true` as the LARGE_IO-on-hosted proof (only if the
   runner disk guard passes — otherwise the schedule run is the proof);
   cron verification line captured.
2. **test_model duration on CI vs local**: hosted runners have no model
   cache → heavy cases self-skip (fast). Local machines WITH the cache face a
   >2 h CPU-interpreter run in the heavy real-model cases; the local M3
   protocol therefore hides the cache (`HF_HUB_CACHE` → an empty `.../hub`
   dir, D-11 — no TIMEOUT property was added). If CI shows anything else,
   triage before M4.
3. **M4 release protocol** (02-program-plan): 3× release-candidate runs on
   Windows artifacts (`lse.exe`, `lse-server.exe`), version pinning, and the
   same-SHA rule (11 §6: both jobs green on the identical commit counts).
4. **K-ledger reviews** by 2027-01-08 (#1 loopback backpressure — retest
   against a future Windows networking change or refile; #2 P2996 — recheck
   when a pinned LLVM ships `-freflection`).
5. **Toolchain watch**: the ROCm clang-cl 21.0.0git constexpr bug forced the
   official-LLVM 21.1.8 pin (A-4/ID-C2-14); revisit at LLVM 22 — the
   weekly cron job is the drift alarm until then.
6. **cl.exe path** remains planned-not-landed (01 §5); PR1-trigger tracking
   continues at the next upstream sync (N = 4 baseline).
