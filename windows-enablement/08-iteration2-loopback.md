# 08 — Iteration-2 Loopback Analysis (planning-analysis, cycle 2)

**Mandate.** Critically re-plan the Windows-enablement program after one full
pipeline cycle (Stages 1–7, WP0–WP10 landed, M2a SATISFIED). This artifact is
the cycle-2 planning record: it audits the branch against the cycle-1 claims,
extracts process lessons, re-quantifies upstream-churn exposure against
measurements taken today, and issues the prioritized cycle-2 backlog that the
next pipeline pass executes. Per the Stage-7 maintenance rule (07 §3),
corrections live here — artifacts 01–07 remain frozen.

**Audit basis.** All statements below were re-verified against the working tree
and git history on 2026-10-10, not taken from the artifacts:
`git log a5b82f6..HEAD` (9 commits), `git diff a5b82f6..HEAD --stat`
(55 files, +6,956 / −324), direct reads of the seam sources, CMake files,
ledgers, and both doc surfaces, plus a live `git fetch origin`.

---

## 1. Reality Audit Results

### 1.1 Verified-clean claims (artifacts ↔ repo match)

| Claim (source) | Verification |
|---|---|
| 9 commits, WP1–WP10 + docs, on base `a5b82f6` = v0.6.0-28 (06 §2) | `git log`: `9ce9287, 7a9ac0e, 4dded6b, fa443f7, d3085ec, 96c32c4, fe6d522, f2540f4, a6a288d` — messages match WP claims exactly; `git describe a5b82f6` = `v0.6.0-28-ga5b82f6` |
| Platform seam exists as specified (ID-1/ID-2) | `include/lse/core/platform.hpp` (260 ln), `src/core/platform_posix.cpp` (408 ln, entire file inside `#ifndef _WIN32`), `src/core/platform_win.cpp` (948 ln, `#ifdef`-empty off Windows). Both `.cpp` files are listed unconditionally in `lse_core` — the "one source set, #ifdef-empty" rule stated in `CMakeLists.txt` comments holds |
| 64-bit-safe positioned I/O (BL-2 fix is real) | `file_read.cpp:22-27` now calls `platform::pread64(fd, …, int64_t)`; the truncating `static_cast<off_t>` is gone; `platform_posix.cpp:68-71` wraps `::pread` with 64-bit `off_t`; `platform_win.cpp:299` uses ReadFile+OVERLAPPED |
| Zero-macro socket seam (06 §5.21) | `winsock_compat.hpp` documents and implements the no-function-like-macro rule; `socket.cpp` call sites all renamed to `sock_*` once per platform; POSIX side = exact pass-throughs |
| X = 2 exclusions ledger (06 §3e) | `windows-enablement/COMPILE_EXCLUSIONS.md` present, X=2 (`test_dflash2`, `test_loom_cache`), mechanism (`if(NOT WIN32)` in `tests/CMakeLists.txt`) matches the diff; `COMPILE_EXPOSURE.txt` (35) + `gen_compile_exposure.sh` present |
| `docs/WINDOWS.md` commands vs CMake reality | `LSE_BUILD_TESTS` default ON (`cmake/LSEOptions.cmake:3`) — doc correctly mandates `-DLSE_BUILD_TESTS=OFF` for the full build and documents the targeted-34 gate + `all`-fails warning; HRX force-OFF matches `LSEOptions.cmake` WIN32 guard; poller selection matches the `elseif(WIN32)` branch in `CMakeLists.txt` |
| M2a evidence committed | `logs/m2a-build.txt` (1,859 lines) tracked in git |
| Governance: `chroma_data/` untouched | untracked, unmodified |

### 1.2 Governance spot-check (#ifdef discipline, Linux behavior)

Eight core-file diffs inspected line-by-line: `file_read.cpp`, `weights.cpp`,
`socket.cpp`, `graph.cpp`, `scheduler.cpp`, `companions.cpp`, `dtype.hpp`,
`hash.hpp`.

- Every Windows behavioral delta is either a `#ifndef _WIN32` include guard, an
  `#ifdef _WIN32` branch, or a call rerouted through `lse::platform` /
  `sock_*` whose POSIX implementation is a pass-through (`platform_posix.cpp`
  header states the exact-call preservation rule and the code matches).
- `scheduler.cpp`: pure addition (`device_set_factory_registered()`), inert
  unless called. `socket.cpp`: the abstract-namespace rejection mirrors the
  existing `__APPLE__` guard idiom precisely. No `__APPLE__` block was touched
  anywhere in the diff.
- **Observation (not a violation):** `__builtin_memcpy → std::memcpy` in
  `dtype.hpp` / `hash.hpp` / `graph.cpp` is an *unguarded shared-code edit*
  (identical codegen on GCC/Clang). It is authorized by WP9/E2 "portable
  builtins" and §4.3 rule 2's spirit, but it is the one class of change in the
  branch that alters POSIX-side source text without a guard — keep it in the
  Batch-1 upstream PR so upstream owns it, rather than growing this class
  fork-side.
- `src/backends/hrx/**`: only the two `CMakeLists.txt` guard lines; no source
  touched (PR5 clean).

**Governance verdict: COMPLIANT.** Minimal-diff, additive-first, no
Linux/macOS behavior change; the ~2,500 lines of new files carry most of the
delta (zero-conflict surface).

### 1.3 Drift flags

| # | Drift | Severity | Disposition |
|---|---|---|---|
| **D-1** | `BUILD_FROM_SOURCE.md` §Windows (WP1 vintage) documents a default tests-ON `cmake --build build` that **fails** pre-WP12 (contradicts ID-3b and `docs/WINDOWS.md`). Known since Stage 7, still unfixed. | **Medium — actively misleads any new reader today** | Hoist the fix out of WP12 into a standalone XS item (cycle-2 backlog #2). One-line `-DLSE_BUILD_TESTS=OFF` + pointer to `docs/WINDOWS.md`, or shrink §Windows to a pointer now |
| **D-2** | `docs/WINDOWS.md` §3 cites `build-win-notests-configure.log` as evidence (07 §1 row 12 calls it a frozen record) — but the file is **gitignored** (`.gitignore:23 *.log`) and never committed. Dangling evidence link in every clone. | Medium (evidence-integrity) | Backlog #3: rename/commit under `logs/…txt` (pattern: `m2a-build.txt`) or re-point the doc link into `logs/m2a-build.txt`'s configure section |
| **D-3** | 06 §"Honest status" says "the branch was **not pushed**" — false since Stage 7 (`fork/feature/windows-enablement` = `a6a288d`). Frozen artifact; record-only. | Low | No action on 06 (frozen). Lesson L7: live state lives in living files only |
| **D-4** | Stage-2 churn assumption "~80 commits/week" is stale: measured **534 commits in the last 30 days ≈ 125/week** (§3). Plan inputs, not plan structure, are wrong. | Medium (planning input) | Sync protocol re-derived from measurement (§3); re-measure at every sync (lesson L6) |
| **D-5** | Stage-2 §4.2 hot-file watchlist (`graph.cpp`, `jit_cache.cpp`, `hrx_backend.cpp`, root `CMakeLists.txt`) diverges from the measured hotspots among files we actually touched: it missed `tests/CMakeLists.txt`, `scheduler.cpp`, `api/lse.cpp`, `server_main.cpp` and listed `hrx_backend.cpp`, which our diff never touches. | Low | Watchlist superseded by §3.2 (evidence-derived) |

No other drift found: commit hashes, gate numbers (192/192, 34/34, 13.4 s),
ledger contents, deviation log, and doc claims all reconcile with the repo.

---

## 2. Cycle-1 Retrospective + Process Lessons

### 2.1 What the pipeline got right

1. **The two-review chain earned its cost.** Stage 1 missed the 32-bit `_pread`
   trap entirely; Stage 3 (BL-2) caught silent >2 GB checkpoint corruption
   *and* the two follow-on traps (`_lseeki64`+`_read` races on shared fds,
   `FILE_SHARE_READ` requirement), and Stage 5 turned them into the normative
   ID-1 spec that the shipped `platform_win.cpp` implements. Same for BL-3
   (M2a gate infeasible as written → ID-3 targeted build) and the T3
   bit-exactness impossibility (→ ID-5 re-partition).
2. **The directive layer (ID-1…ID-14) was the right contract shape** — Stage 6
   implemented against it with only 8 deliberate deviations, each documented
   in-code, and a 26-entry discrepancy log that "repo beats directive"
   governance demanded.
3. **Probe-first (WP0) paid off**: NASM-not-needed, bare `.lib` tokens, the
   CMake 3.31 C++26 dialect gap, and the `QueryInterruptTime` import-thunk
   absence were all environment facts no artifact could have predicted.
4. **Additive-first seam design held**: 81% of touched files sit in upstream
   hot zones (§3), yet most delta mass is in new files — the R1 mitigation is
   measurably working.

### 2.2 What went wrong

- **Census incompleteness**: Stage 1/2's grep-based inventory missed `fsync`,
  `fnmatch`, `isatty`, `std::aligned_alloc` (06 §5.4–7) — found only at
  compile time. Grep patterns were also wrong on counts (19 vs 17 vs 15
  `<unistd.h>` files; 35 vs a 34-name list, `test_attention_short` omitted).
- **Gates shipped unexecuted**: Stage 2's M2a demanded a tests-ON build that
  could not exist before WP12; only Stage 3's re-derivation caught it.
- **Docs written before their truth-conditions**: `BUILD_FROM_SOURCE.md`
  §Windows (WP1) became false the moment ID-3b landed and stayed false through
  Stage 7 (D-1).
- **Evidence hygiene**: a cited evidence file never committed (D-2); a frozen
  artifact asserting a live-state fact that immediately went stale (D-3).
- **Stale planning inputs**: churn rate (D-4) and hotspot list (D-5) were
  assumptions, not measurements.

### 2.3 Process lessons binding on cycle 2

| # | Lesson | Cycle-2 mechanism |
|---|---|---|
| **L1** | **Compiler-as-census.** Never size remaining port surface by grep alone. | WP12's first act is a tests-ON `all` build; the harvested error list *is* the work list. Any future grep census must include `fsync/fdatasync/mallinfo/aligned_alloc/pipe/dup2/fnmatch/isatty` |
| **L2** | **Gate rehearsal.** Every acceptance command must be dry-run (or proven infeasible) before a plan freezes. | Each cycle-2 backlog item's acceptance criteria below were checked against current CMake/repo state; the next implementation stage re-runs them before declaring |
| **L3** | **Generate all counts.** Hand lists rot (35/34, 19/17/15). | W re-census = committed `ctest -N` diff; X and K ledgers are machine-derivable; `COMPILE_EXPOSURE.txt` pattern extends to everything enumerated |
| **L4** | **Docs land at the gate that makes them true.** | D-1 fix ships as its own XS commit now; WP12's doc rewrites ship inside WP12's commit, not before |
| **L5** | **Cited evidence must be in git.** | D-2 fix; CI evidence (M4) committed as `logs/m4-ci-evidence.md` with run URLs |
| **L6** | **Measure churn at every sync.** | `git rev-list --count <last-sync>..origin/master` recorded in each sync note; protocol re-evaluated if > 150/week |
| **L7** | **Frozen artifacts never track live state.** | A short living `windows-enablement/STATUS.md` (branch pushed? base age? X/K/W values?) updated at each gate |
| **L8** | **Feasibility-review acceptance criteria against platform physics.** | ID-5 partition applies at M3; any new bit-exactness or sub-ms-latency criterion gets an explicit "is this physically attainable on MSVC-STL/WSAPoll?" line in review |

---

## 3. Churn Exposure & Sync Protocol

### 3.1 Measured state (2026-10-10, post-`git fetch origin`)

- `origin/master` == `a5b82f6` == our base: **0 commits behind right now.**
  The last upstream commit (width-tiles merge) landed 2026-10-10 08:01. The
  merge window is as cheap as it will ever be.
- Velocity: **534 upstream commits since 2026-09-10** (30 days) ≈ **125/week**
  — ~1.6× the Stage-2 planning figure (D-4).

### 3.2 Merge exposure quantified

Of our **43** non-doc changed files, **35 (81%)** appear in upstream commits
since 2026-07-01. Upstream commit counts since 2026-08-01 for the files we
also touched (evidence-derived hotspot watchlist, superseding Stage-2 §4.2):

| Upstream commits | File | Our delta shape | Conflict risk |
|---|---|---|---|
| 94 | `CMakeLists.txt` | guarded hunks (compiler branch, seam sources, poller select, ws2_32) | **HIGH** — top concentrator |
| 81 | `tests/CMakeLists.txt` | +97 ln (WIN32 guards, force-load, 2 exclusions) | **HIGH** |
| 62 | `src/graph/scheduler.cpp` | +4 ln pure addition | Low-Med |
| 33 | `include/lse/graph/graph.hpp` | +6 ln | Med |
| 32 | `src/cli/server_main.cpp`, `src/api/lse.cpp` | seam reroutes (+21 / ±26) | Med |
| 22 | `src/server/http_server.cpp` | +5 ln | Low |
| 17 | `src/graph/jit_cache.cpp` | seam reroutes (±35) | Med |
| 16/15/12/11 | `hrx/CMakeLists.txt`, `devices.cpp`, `graph.cpp`, `weights.cpp` | small guarded hunks | Low-Med |

Mitigating fact: ~2,500 of our +6,956 lines are brand-new files (seam,
winsock compat, poller, smoke test, docs, ledgers) — **zero-conflict
surface**. Conflicts will concentrate in small `#ifdef`/`if(WIN32)` hunks
inside the two CMakeLists files.

### 3.3 Sync protocol (cycle-2 normative)

1. **Merge, not rebase.** `git merge origin/master` into
   `feature/windows-enablement`. Rationale: (a) the branch is **published** on
   `fork` — rebase means force-push and rewrites the commit IDs
   (`9ce9287..fe6d522`) that frozen artifacts 06/07 and `logs/m2a-build.txt`
   cite as evidence; (b) merge resolves each hot-file conflict **once per
   sync** instead of once per commit × 9; (c) a linear PR history, if ever
   wanted, is producible at PR time (squash or one-off rebase onto the
   then-tip) without disturbing the working branch. This **supersedes**
   Stage-2 §4.1's "weekly rebase".
2. **Cadence**: weekly (fixed day) + **mandatory pre-gate** — a gate
   certifies only against a base ≤ 7 days old (unchanged from §4.1). Never
   let two weeks pass at 125/week.
3. **Ritual**: push tag `pre-sync/YYYY-MM-DD` to fork → merge → conflicts
   resolved per §4.3 rules still in force → tests-OFF full build + targeted-34
   ctest + Linux-parity configure → sync note (commits absorbed, conflicts
   touched, hotspot status, `rev-list` count per L6) appended to living
   `STATUS.md` (L7).
4. **SYNC-1 immediately**: merging today costs ~nothing (0 behind) and
   rehearses the ritual before WP12 enlarges the delta and before CI exists.
5. **Upstreaming is the permanent churn fix** (§4.4 batches). Batch 1
   (build-system hygiene: `lse_force_load`, compiler-ID flag branch,
   `std::memcpy` conversions, C++26 dialect override) is **overdue** — §4.4
   scheduled it pre-M2 — and targets exactly the #1 conflict file. Recommend
   proposing it now (DP2). The Stage-7-drafted full-seam PR follows M4.

---

## 4. Cycle-2 Backlog (prioritized, dependency-ordered)

Sizes: XS < ½ day · S ≤ 1 day · M ≤ 3 days · L ≤ 2 weeks. "Deps" = must land
first. Acceptance criteria follow L2 (checked against current repo state) and
L3 (machine-verifiable).

| # | Item | WP ref | Acceptance criteria | Size | Deps |
|---|---|---|---|---|---|
| 1 | **SYNC-1**: tag `pre-sync/2026-10-10`, merge `origin/master` (currently a no-op merge or fast-forward-adjacent — rehearse the ritual regardless), first `STATUS.md` | §3.3 protocol (supersedes S2 §4.1) | Merge commit (or recorded no-op) on branch; tests-OFF build still **[192/192], 0 errors**; targeted **ctest 34/34 PASS**; sync note + base-age in `STATUS.md`; pushed to fork | S | — |
| 2 | **DOC-1**: fix `BUILD_FROM_SOURCE.md` §Windows (drift D-1) | 07 §3 trigger table (hoisted from WP12) | Fresh-clone reader following the snippet verbatim gets a green build on Windows: snippet carries `-DLSE_BUILD_TESTS=OFF` + "targeted tests only until WP12; canonical guide: `docs/WINDOWS.md`" (or is shrunk to a pointer). Committed evidence: the snippet's exact commands re-run | XS | rides with #1 |
| 3 | **DOC-2**: fix dangling configure-log evidence link (drift D-2) | 07 §1 row 12 | Configure transcript committed under `logs/` with an ignore-proof extension (e.g. `logs/m2a-configure.txt`), `docs/WINDOWS.md` + 07-maintenance-note link updated **in the living doc only**; `git ls-files` proves it tracked | XS | — |
| 4 | **WP11 / M2b**: tokenizer sentinel + executables gate | WP11, ID-6 | `test_tokenizer` green on Windows (or a **ledgered K entry** opening the runtime-K account with reason + review-by); SC4 demonstrated: real end-to-end transcripts of `lse.exe` and `lse-server.exe` (generation, not just `--help`) committed as `m2b-acceptance.md`; K ledger file created (machine-readable, L3) | M | 1 |
| 5 | **WP12**: test-source porting; X→0; unlock tests-ON `all` | WP12, ID-14, L1 | (a) *Probe first*: tests-ON `all` build attempted, error census harvested and committed (this — not grep — is the work list); (b) `tests/compat.hpp` lands portable `getpid/setenv/unsetenv/…`; (c) all **15** `<unistd.h>` files + `test_server_shutdown` (`fork`/`waitpid` → Windows process-termination strategy, PR7) compile; (d) both `if(NOT WIN32)` ledger guards removed, `COMPILE_EXCLUSIONS.md` → stub, **X = 0**; (e) `cmake --build build-win` with `LSE_BUILD_TESTS=ON`, target `all`: **0 errors**; (f) W re-censused: `ctest -N` diff committed; (g) zero Windows entries in `KNOWN_FAILURES.md` (K is the runtime ledger, ID-6); (h) doc rewrites per 07 §3 inside the same commit (L4) | L | 4 (soft: compat.hpp is independent; serialized to keep K-accounting order) |
| 6 | **WP13a (interim CI)**: `windows.yml` on fork — tests-OFF full build + targeted-34 ctest + weekly `schedule` | WP13, ID-10 | Workflow green on fork; weekly schedule active (lights up the R3 drift alarm **before** WP12 finishes — DP3); ID-10 YAML defects fixed (`run_large_io` no-op expression; `--label-exclude LARGE_IO` on both jobs); toolchain pins match `docs/WINDOWS.md` §1 table | M | 1 (full-tests version re-triggers after 5) |
| 7 | **M3**: local full-suite gate | WP12 tail, ID-5, ID-13 | ≥ (W − K) of W pass on Windows, **K ≤ 5** ledgered case-groups, each a documented platform-capability gap; Linux parity suite unchanged on the same SHA (ID-13 toolchain per its disposition); ID-5 fingerprint partition applied: HASH-EQ only for transcendental-free ops, TOLERANCE goldens for `exp/tanh/sin/cos` callers; >2 GB acceptance test (Stage-4 design) executed; Linux census baseline (ID-11) captured in a Linux/WSL environment; evidence `logs/m3-*.txt` committed | M | 5 |
| 8 | **WP13b / M4**: CI green ×3 + parity | WP13 | `windows.yml` (now full tests-ON suite) green **3 consecutive runs** + Linux-parity job green on the same SHA; duration < 45 min (PR3 budget); weekly schedule retained; `logs/m4-ci-evidence.md` with 3 run URLs + durations committed (L5) | S–M | 6, 7 |
| 9 | **PR-BATCH1 (upstream)**: build-hygiene PR(s) to Geramy/LSE | S2 §4.4 Batch 1 | ≤ ~300 diff lines per PR: `lse_force_load` + `/WHOLEARCHIVE` spelling, compiler-ID flag branch (incl. `-ffp-contract=off`), `std::memcpy` conversions (§1.2 observation), C++26 dialect override. Filed with the 06-log evidence links. Never blocks a milestone (§4.4 rule) | S | none — parallel any time (DP2) |
| 10 | **PR-BATCH2/3 (upstream)**: platform seam + test ports — the Stage-7-drafted PR | S2 §4.4, 07 §5 | Filed after M4 against a ≤ 7-day-old base; body = 07 §5 draft updated with M3/M4 evidence; ≤ 300-line split if upstream prefers | M | 8 (+9 merged or filed) |
| 11 | **WP15 (stretch)**: HRX compile-only spike | WP15, Q8 | Entry gate first: 1-day probe that the installed Windows HIP SDK ships `amd_comgr` import lib + headers (Q8 — still an unverified assumption). Only then: compile-only `LSE_ENABLE_HRX=ON` behind its own guard; capture-target tests compile; **no dispatch claimed** | L (probe: S) | 8 + DP4 |

**Critical path:** 1 → 4 → 5 → 7 → 8. Items 2, 3, 6, 9 float in parallel.
Tests-ON `all` unlock (item 5e) is **required for M3** (the W census is
meaningless without it) but **not** for interim CI (item 6 runs the targeted
gate) — see DP4.

---

## 5. Revised Risk Register

Re-scored against cycle-1 evidence. Likelihood/Impact: L/M/H.

### 5.1 Inherited risks (Stage-1 §8 / Stage-2 §5)

| # | Risk | Cycle-1 outcome | L | I | Cycle-2 posture |
|---|---|---|---|---|---|
| R1 | Upstream churn vs fork | **Materialized in measurement** (125/wk; 81% of touched files in hot zones; hotspots ≠ S2 watchlist) but **not yet in harm** (0 behind today) | **H** | **H** | Still the top risk. Merge protocol §3.3; SYNC-1 rehearsal; Batch-1 PR removes the hottest deltas permanently; L6 re-measurement each sync |
| R2 | Silent whole-archive/registration loss | **Evaporated** — smoke census 8/8 archives, counts > 0 (06 §3d) | L | H | Closed; CI smoke job keeps it closed |
| R3 | C++26 feature drift upstream | **Open and growing** — the mitigation (weekly-scheduled CI) does not exist yet; every week without WP13a is unmonitored exposure at 125 commits/wk | **M→H** | H | Interim CI (backlog #6) is the earliest-possible mitigation; SYNC-1 cadence bounds silent breakage to ≤ 1 week |
| R4 | Rust chain on windows-msvc | **Mostly evaporated** — `fastokens_ffi.lib` links; `ring` built without NASM; fallback lib set documented | L | M | Residual = raw-dylib filter fragility → **N4** |
| R5 | HRX has no Windows runtime | Unchanged (certain for dispatch) | Certain | M | Phase-3 gated; Q8 probe before commitment (DP4) |
| R6 | AF_UNIX abstract namespace absent | **Closed by design** — `kUnimplemented` mirrors the macOS guard exactly (verified in `socket.cpp` diff) | — | — | Residual only: WP12 test migrations to path/TCP endpoints per Stage-4 T-plan |
| R7 | Numerics parity | **Partial** — `-ffp-contract=off` pinned in the flag branch (verified); ID-5 partition designed but **unexercised** until M3 | M | M | M3 acceptance executes the partition; L8 review of any new bit-exact criterion |
| R8 | flock → LockFileEx semantics | Documented in the seam; **not yet field-tested** under concurrent hub-pull on Windows | L-M | L-M | WP12's hub tests are the first real exercise; watch at M3 |
| R9 | Long paths (MAX_PATH) | Shim home-resolution landed (`\\?\` policy per ID-2/ID-8); runner policy pending | L-M | L | WP13a: enable long-path policy on the runner image |
| R10 | No AMD GPU on CI runners | By design; existing guards verified skipping | — | L | Unchanged |
| PR1 | Divergence outpaces sync capacity | Trigger not tripped (0 behind, merge ritual untested) | M | H | Watchlist = §3.2 table; trigger: > 2 conflicting files in 2 consecutive syncs → escalate Batch-1/2 PR urgency |
| PR2 | Registration loss escapes smoke | Smoke censuses all 8 archives → mitigated as designed | L | H | CI re-runs smoke weekly |
| PR3 | CI minutes overrun | Unexercised | M | M | Interim CI is small; caching per S2; 45-min gate at M4 |
| PR4 | Single-maintainer bus factor | Unchanged; handoff package = `windows-enablement/` (this file extends it) | M | M | `STATUS.md` (L7) makes live state resumable by any agent |
| PR5 | Scope creep into `hrx/**` | **Verified clean** — audit found zero hrx source deltas | L | H | Review checklist item stays |
| PR6 | clang-cl 21 instability | No ICE/miscompile observed through 192-target build | L | H | CI pins exact toolchain (backlog #6) |
| PR7 | Test semantic drift on Windows | **Elevated — now has named vectors**: WSAPoll ms granularity, SIGTERM-undeliverable (`test_server_shutdown`), mandatory LockFileEx. First contact at WP12/M3 | **M→H** | M | WP12 probe-first (L1) surfaces these as compiler/test errors early; K ≤ 5 budget is the containment; each K entry needs an upstream issue describing the delta |

### 5.2 New risks discovered in implementation (cycle-1 §4/§5 deviations)

| # | Risk | Evidence | L | I | Mitigation |
|---|---|---|---|---|---|
| **N1** | **WSAPoll millisecond granularity** → latency-sensitive comm/server tests flaky or slow on Windows (epoll contract is ns; poller rounds up) | 06 §4.1–4.2; `poller_winsock.cpp` header deviations | M | M | WP12 audits comm/server tests for sub-ms timing assumptions; tolerance-aware ports; K entry only as last resort (counts against ≤ 5) |
| **N2** | **`heap_stats()` returns zeros** → `host_memory_json` API consumers see plausible-looking zeros; any test asserting non-zero heap fields fails silently-meaningfully | 06 §4.4 | M | L-M | WP12 greps tests for heap assertions (add `mallinfo/heap_stats` to the L1 pattern list); `docs/WINDOWS.md` limitation row already published; future: `_HEAPINFO`-based estimate |
| **N3** | **`QueryInterruptTime` via `GetProcAddress`** — on a kernel32 without the export the boottime clock silently falls back to the unbiased counter (drifts across sleep) | 06 §4.6; `platform_win.cpp:877-885` | L | L | Declare minimum supported Windows (10+) in `docs/WINDOWS.md`; fallback is already graceful + commented |
| **N4** | **raw-dylib stub filtering fragility** — `LSERust.cmake` drops `^windows\.[0-9].*\.lib`; a cargo/rustc upgrade could rename tokens or a genuine lib could match the pattern | 06 §4.7 | M | M | Pin Rust toolchain version in CI (with docs §1 table); link stage fails loudly if a real lib is missing; documented fallback set `ws2_32 bcrypt userenv` |
| **N5** | **File-mapping handle registry** — Windows keeps section+file handles keyed by view pointer (because `weights.cpp` closes its fd post-mmap); move/copy or double-unmap paths could leak or double-close | 06 §5.15 | L-M | H | Unit coverage exists (M2a); the Stage-4 >2 GB acceptance test at M3 is the real exercise; registry audit in WP15-adjacent review if leaks appear |
| **N6** | **Docs/evidence drift** — stale doc snippets (D-1), uncommitted cited evidence (D-2), frozen artifacts asserting live state (D-3) | §1.3 | **H** | L | DOC-1/DOC-2 backlog items now; L4/L5/L7 mechanisms make recurrence a review-checklist failure |

---

## 6. Decision Points for the User

| # | Decision | Options | Recommendation |
|---|---|---|---|
| **DP1** | Sync mechanic for `feature/windows-enablement` | (a) weekly **merge** of `origin/master`; (b) weekly **rebase** (Stage-2 original) | **(a) Merge.** Branch is published; frozen artifacts and `logs/m2a-build.txt` cite commit IDs that a rebase rewrites; merge = one conflict pass per sync. Produce linear history only at PR time if upstream asks. Supersedes S2 §4.1 |
| **DP2** | Upstream Batch-1 PR timing (§4.4 said pre-M2 — now overdue) | (a) file **now**, parallel with WP11/WP12; (b) wait until M4 and file one big PR (Stage-7 draft) | **(a) Now.** It targets `CMakeLists.txt`/flags — the #1 and #2 conflict concentrators (94/81 upstream commits) — and every merged batch permanently removes rebase/merge cost. Small, idiom-matched, high acceptance odds (`if(APPLE)` precedent) |
| **DP3** | Interim CI before WP12 completes | (a) land `windows.yml` with the targeted-34 gate + weekly schedule **now**; (b) wait for tests-ON `all` | **(a) Now.** R3 (C++26 drift at 125 commits/wk) is currently unmonitored; the interim workflow is small, and WP12/13 later widen it. Cost: one extra workflow edit |
| **DP4** | Is tests-ON `all` unlock (WP12) required for M3? for M4? | — | **Required for M3** (the W census and "≥ W−K pass" are meaningless without the full suite built) and therefore for M4. **Not** required for interim CI (DP3a runs the targeted gate). No decision needed beyond confirming this reading of ID-6/SC5 |
| **DP5** | Phase-3 HRX spike (WP15) | (a) attempt after M4; (b) defer indefinitely; (c) **1-day Q8 SDK probe now**, commit only if `amd_comgr` import lib + headers are present | **(c) Probe-then-decide.** Q8 is still an unverified assumption from the original brief; the probe is cheap and converts DP5 from speculation into a fact-based choice at M4 |
| **DP6** | D-1 fix timing | (a) standalone XS commit now (backlog #2); (b) keep Stage-7's plan (fix at WP12) | **(a) Now.** The snippet is actively false today; L4 says docs land when true — the honest interim state ("targeted tests only") is documentable in one line |

---

*End of iteration-2 loopback. Next pipeline pass executes §4 in order; §3.3 is
normative for syncs; §5 supersedes the Stage-1/Stage-2 registers; decisions
DP1–DP6 resolve before backlog items 1, 6, 9, 11 start.*
