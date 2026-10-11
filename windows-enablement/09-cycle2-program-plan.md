# 09 — Cycle-2 Program Plan (Stage 1, cycle 2 · program management)

**Branch:** `feature/windows-enablement` @ `71be5a5` = upstream `a5b82f6` (v0.6.0-28) + **10 commits** (`9ce9287..71be5a5`), pushed to `fork/feature/windows-enablement` (antmikinka/LSE). Remotes verified 2026-10-10: `origin` = Geramy/LSE (upstream), `fork` = antmikinka/LSE.
**Primary input:** `08-iteration2-loopback.md` (reality audit D-1..D-5, lessons L1–L8, sync protocol §3.3, backlog ①–⑤, DP1–DP6). Supporting: 01–07 (frozen; ID-1..ID-14 directives in 03/05 remain the binding correction layer), `docs/WINDOWS.md`, `logs/m2a-build.txt`.
**User decisions (fixed, override artifact recommendations):** run full cycle 2 targeting **M3**; upstream PRs to Geramy/LSE **DEFERRED until M4** → DP2 resolves **NO** (08 §6's "file Batch-1 now" is overridden).
**Status:** Plan only. No source modified, nothing committed by this stage.

---

## 1. Cycle-2 Charter

### 1.1 Objective

Reach **M3 on Windows**: the full locally-registered test suite (W) builds with
`LSE_BUILD_TESTS=ON` target `all` (0 errors, X = 0 compile exclusions), passes
locally with **K ≤ 5** ledgered case-group quarantines (each a documented
platform-capability gap, never a product defect), M2b's tokenizer sentinel is
green with SC4 end-to-end evidence, and the **interim CI** (`windows.yml` on the
fork: tests-OFF full build + targeted-34 ctest + weekly schedule + Linux-parity
job) is running green — all against a freshly merged upstream base (SYNC-1)
with the M2a gate re-proven as regression evidence.

### 1.2 Scope

**In scope** — 08 §4 backlog items ①–⑤ plus the M3 gate (item ⑦ is "WP12
tail" and the cycle objective):

| ① | SYNC-1 merge + first `STATUS.md` (08 §3.3 protocol, supersedes S2 §4.1 weekly rebase) |
|---|---|
| ② | DOC-1 (D-1 `BUILD_FROM_SOURCE.md` §Windows) + DOC-2 (D-2 dangling configure-log evidence) |
| ③ | WP11/M2b: `test_tokenizer` sentinel, SC4 real end-to-end transcripts, K-ledger creation |
| ④ | WP12: L1 compiler census → `tests/compat.hpp` → 15 `<unistd.h>` files + `test_server_shutdown` fork/waitpid port → X=0 → tests-ON `all` unlock; then the M3 suite tail (S4 §6 ports P2–P5, new tests P8, ID-5 partition, >2 GB test, Linux parity, evidence bundle) |
| ⑤ | WP13a interim CI: `windows.yml` + `linux-parity` job + weekly schedule + ID-10 YAML fixes |

**Out of scope (cycle 2):** all upstream PRs (Batch-1/2/3 — deferred to M4 per
user decision; DP2=NO), WP13b/M4 3× green protocol (cycle 3; G-C2-5 needs only
1× green), WP15 HRX spike + Q8 probe (post-M4, DP5), DLL/ARM64/MinGW, any
change to Linux/macOS behavior (§4.3 rules remain binding), `chroma_data/`
(untouched, governance).

**Consequence of DP2=NO (recorded honestly):** the #1/#2 conflict
concentrators (`CMakeLists.txt` 94 upstream commits since Aug,
`tests/CMakeLists.txt` 81) stay fork-owned for the whole cycle. R1/PR1 load is
carried entirely by the §3 merge protocol and weekly cadence — never let two
weeks pass at ~125 commits/week (D-4 measurement; re-measure per L6).

### 1.3 Success criteria (measurable; per L2 every criterion ships its rehearsal command)

Rehearsal-status tags: **PROVEN** = executed in cycle 1 with committed evidence
(`logs/m2a-build.txt`); **SPEC-NOW** = command valid against the current tree,
outcome pending; **EXPECT-FAIL** = infeasible today by design (the failure is
the deliverable — L1 probe).

| # | Criterion | Measurement / rehearsal command | Status |
|---|---|---|---|
| SC-C2-1 | Branch merged with upstream, base ≤ 7 days old at every gate | `git fetch origin && git rev-list --count HEAD..origin/master` → 0 (or merge landed); tag `pre-sync/<date>` pushed to fork | SPEC-NOW |
| SC-C2-2 | M2a regression green post-merge | tests-OFF build `[N/N] 0 errors` + targeted-set `ctest 100% pass` incl. `test_registration_smoke` (exact commands §6 G-C2-0) | PROVEN (pre-merge) |
| SC-C2-3 | Docs drift D-1/D-2 closed | `git ls-files logs/m2a-configure.txt` non-empty; `grep -n "build-win-notests-configure.log" docs/WINDOWS.md` empty; DOC-1 snippet carries `-DLSE_BUILD_TESTS=OFF` | SPEC-NOW |
| SC-C2-4 | M2b satisfied | `ctest --test-dir build-win -R "^test_tokenizer$" --output-on-failure` green **or** ledgered K entry; SC4 generation transcripts committed `logs/m2b-acceptance.md` | SPEC-NOW |
| SC-C2-5 | Tests-ON unlock, X=0 | `cmake --build build-win` (no `--target`) exits 0; `COMPILE_EXCLUSIONS.md` stub X=0; both `if(NOT WIN32)` ledger guards removed from `tests/CMakeLists.txt` | EXPECT-FAIL (pre-WP12 → the L1 probe; rehearsed at execution) |
| SC-C2-6 | M3 arithmetic (ID-6 units) | Full `ctest` run reports **W, K, X as three numbers**: all W entries pass or are configure-disabled with ledgered reason; K ≤ 5 case-groups, each `windows: <capability\|flake> — <issue #N> — review-by <date ≤ 90d>`; zero bucket-(a) entries in K/X ever | SPEC-NOW |
| SC-C2-7 | Linux parity unchanged on same SHA | `linux-parity` CI job (clang++-21, `-DLSE_ENABLE_HRX=OFF`, `-DCMAKE_CXX_FLAGS=-ffp-contract=off`) green on the M3 SHA, or local WSL equivalent per ID-13 disposition | SPEC-NOW |
| SC-C2-8 | Interim CI running | `windows.yml` green 1× on fork (both jobs, same SHA); weekly `schedule` cron active; ID-10 fixes demonstrated (`run_large_io` dispatch toggles the LARGE_IO label; `--label-exclude LARGE_IO` on both PR/push jobs) | SPEC-NOW |
| SC-C2-9 | Living state resumable (L7) | `windows-enablement/STATUS.md` exists and is current at cycle end: branch/push state, base SHA + age, last-sync `rev-list` count, W/X/K, gate statuses G-C2-0..5 | SPEC-NOW |

---

## 2. Work Packages (C2-WP1 … C2-WP6)

Sizes per 08 §4: XS < ½ day · S ≤ 1 day · M ≤ 3 days · L ≤ 2 weeks.
One WP = one commit (or tight stack), `C2-WPn:` message prefix, §4.3 diff rules
binding, evidence to `logs/` or `windows-enablement/logs/` (match existing
location per file class; m2a evidence lives in root `logs/`).

### C2-WP1 — SYNC-1: merge `origin/master`, first `STATUS.md` · **S** · deps: none · *gates everything*

Execute the §3 protocol verbatim. Deliverables: tag `pre-sync/2026-10-<DD>`
pushed to fork; merge commit (or recorded no-op — at cycle-1 fetch the branch
was 0 behind; re-measure, ritual runs regardless per 08 backlog #1); full M2a
regression re-run green; `windows-enablement/STATUS.md` created (schema §4,
L7) with the first sync note (commits absorbed, conflicts touched, hotspot
status, `git rev-list --count <last-sync>..origin/master` per L6); branch +
tag pushed to fork. If upstream added tests/targets, regenerate
`COMPILE_EXPOSURE.txt` (`gen_compile_exposure.sh`) and re-derive the targeted
set before running the regression gate (§3.5). **Gate: G-C2-0.**

### C2-WP2 — DOC-1 + DOC-2 drift fixes · **XS** · deps: none · *floats; rides with C2-WP1's push*

- **DOC-1 (D-1):** fix `BUILD_FROM_SOURCE.md` §Windows so a fresh-clone reader
  following the snippet verbatim gets a green build: snippet carries
  `-DLSE_BUILD_TESTS=OFF` + one line "targeted tests only until WP12;
  canonical guide: `docs/WINDOWS.md`" — or shrink §Windows to a pointer
  (08 DP6=a; L4: this states the honest *current* truth, so it lands now).
  Evidence: the snippet's exact commands re-run, transcript appended to the
  commit description or `STATUS.md` note.
- **DOC-2 (D-2):** commit the configure transcript as
  `logs/m2a-configure.txt` (ignore-proof `.txt`, pattern of `m2a-build.txt`;
  regenerate by re-running the tests-OFF configure if the original text is
  unrecoverable — label it as regenerated with date/SHA); re-point
  `docs/WINDOWS.md` §3's `build-win-notests-configure.log` link to it
  (living doc only; 07 stays frozen, its maintenance-note row updated per
  07 §3 mechanism). Acceptance: `git ls-files logs/m2a-configure.txt`
  non-empty; no dangling `.log` citation remains in living docs.
**Gate: G-C2-1.**

### C2-WP3 — WP11/M2b: tokenizer sentinel + SC4 + K-ledger · **M** · deps: C2-WP1 · *partial parallel with C2-WP4 (census/compat.hpp half is independent)*

- Build + run the bucket-(e) sentinel: `cmake --build build-win --target
  test_tokenizer` then `ctest -R "^test_tokenizer$"` (tree configured per
  `docs/WINDOWS.md` §4). Green → M2b sentinel satisfied. Red → diagnose;
  a runtime failure opens **K-ledger entry #1** (reason format ID-6) rather
  than silently deferring (08 backlog #4 fallback); a link/build failure is
  a defect → fix (Rust FFI already links `fastokens_ffi.lib` per cycle 1).
- **SC4 demonstrated for real:** end-to-end transcripts of `lse.exe` and
  `lse-server.exe` performing *generation* (not just `--help`/`--version`),
  committed as `logs/m2b-acceptance.md` (L5: cited evidence in git).
- Create the machine-readable K ledger `windows-enablement/K-LEDGER.md`
  (L3): columns `binary:case-group | reason | issue # | review-by`; starts
  at K=0 or K=1; KNOWN_FAILURES.md itself gets **zero** Windows entries
  (ID-6/S4 §5.3 rule 1 — platform-agnostic file).
**Gate: G-C2-2.**

### C2-WP4 — WP12a: census → compat.hpp → X=0 → tests-ON `all` unlock · **L** · deps: C2-WP1; soft-dep C2-WP3 (K-accounting order per 08 item 5) · **CRITICAL PATH**

1. **Probe first (L1, non-negotiable):** configure `build-win` tests-ON
   (`docs/WINDOWS.md` §4) and run `cmake --build build-win` (target `all`);
   it **will fail** — harvest the complete compiler-error list to
   `windows-enablement/logs/c2-wp12-census.txt` and commit it. *This file,
   not grep, is the work list.* Supplementary grep (only to cross-check the
   census) must use the extended pattern set:
   `unistd|sys/wait|fsync|fdatasync|mallinfo|aligned_alloc|pipe|dup2|fnmatch|isatty`.
2. `tests/compat.hpp` lands (S4 §6 P1): portable `getpid/setenv/unsetenv/
   usleep/access/ssize_t/...`, included from `tests/harness.hpp` under
   `#ifdef _WIN32` (3-line guarded edit) — fixes 14 of 15 unistd files
   without touching them.
3. Port the census surface: 15 `<unistd.h>` files + `test_server_shutdown`
   fork/waitpid → `CreateProcess` self-relaunch + `GetExitCodeProcess`
   (S4 §6 P2; PR7 vector — SIGTERM undeliverable). If the census exposes
   deeper POSIX in shared utilities (`harness.hpp`, `reference.hpp`), port
   those too — scope grows by census, not by assumption (risk C2-3).
4. Remove both `if(NOT WIN32)` X-ledger guards (`test_dflash2`,
   `test_loom_cache`) from `tests/CMakeLists.txt`; stub
   `COMPILE_EXCLUSIONS.md` to **X=0** (keep the file + mechanism doc; the
   ledger stays machine-derivable).
5. `cmake --build build-win` (tests-ON, `all`): **0 errors** (SC-C2-5).
6. W re-census (L3): `ctest --test-dir build-win -N` output committed +
   diffed vs M1/M2a captures (S4 MJ-4).
7. Doc rewrites ship **inside this commit** (L4): `docs/WINDOWS.md` §4's
   "tests-ON `all` will fail" warning is deleted/replaced in the same
   commit that makes `all` build; `BUILD_FROM_SOURCE.md` pointer updated if
   its "targeted tests only" line becomes false.
**Gate: G-C2-3.**

### C2-WP5 — WP12b/M3: full-suite ports, new tests, ID-5 partition, M3 evidence · **M** · deps: C2-WP4 · **CRITICAL PATH**

- S4 §6 ports not compile-blocking but M3-blocking: P3 `test_communication`
  `pipe_url()` helper (11 abstract-socket sites → path AF_UNIX in `%TEMP%`,
  108-byte guard, teardown remove, one `kUnimplemented` assertion case);
  P4 `CPPHTTPLIB_USE_POLL` guards (`test_api_parity.cpp:33`,
  `test_hub.cpp:24`); P5 `/tmp` ports (`test_graph.cpp:3155`,
  `test_dflash2.cpp:1610`, `test_tree_verify.cpp:703` mkdtemp,
  `test_model.cpp:312-328` per ID-8 HOME policy); P7 `test_server_cli.py`
  audit (port only if audit fails).
- P8 new additive tests: `test_platform_shim` (S4 §4 table + ID-2 additions,
  green on **both** platforms), `test_large_file_io` (S4 §3: 4 GiB+1 MiB
  sparse, label LARGE_IO, **mandatory locally at M3** →
  `logs/m3-large-file-io.txt`), `test_numerics_fingerprint` (**ID-5
  partition: HASH-EQ only for transcendental-free kernels; TOLERANCE goldens
  for exp/tanh/sin/cos/log/pow callers; test prints set membership**),
  `test_fp_model` canary (ID-12 anti-constant-folding).
- `test_reference` runs with **materialized** `tests/data/reference.safetensors`
  (S4 §2.3: generate once on Linux, SHA256 recorded); a run without goldens
  is labeled *incomplete evidence* in the M3 bundle.
- Full local ctest (G-C2-4 commands); W/K/X arithmetic per ID-6; every K
  entry gets an upstream issue describing the platform delta (PR7
  containment). Linux parity on the same SHA per ID-13 disposition (local
  WSL clang++-21 if provisioned; else CI parity job evidence — see §5 C2-7
  descope). ID-11 Linux census baseline captured in the same parity run.
- Evidence bundle: `logs/m3-ctest-windows.txt`, `logs/m3-ctest-linux.txt`
  (or CI run URLs), KNOWN_FAILURES diff (must show zero Windows entries),
  K-LEDGER.md, fingerprint/T1 hashes. **Gate: G-C2-4 (= M3).**

### C2-WP6 — WP13a interim CI: `windows.yml` + linux-parity + weekly alarm · **M** · deps: C2-WP1 hard; lands **after** C2-WP5 (stable local state; task sequencing constraint) · *DP3 note below*

- New purely-additive `.github/workflows/windows.yml` on the fork, per
  S4 §5.1 skeleton **with ID-10 fixes**: (a) real `run_large_io` conditional
  (dead `${{ inputs.run_large_io && '' || '' }}` expression replaced);
  (b) `--label-exclude LARGE_IO` on **both** jobs' PR/push ctest; weekly
  schedule includes LARGE_IO ≥1×/platform; (c) 3× protocol text kept for M4.
- Jobs: `windows` (windows-latest; winget LLVM 21.1.8 exact pin matching
  `docs/WINDOWS.md` §1; dtolnay rust stable-msvc + Swatinem cache;
  `reference/fastokens` at pinned SHA; tests-OFF full build + targeted-34
  ctest via the committed `COMPILE_EXPOSURE.txt` mechanism; `ctest -N` W
  baseline tee'd; artifacts: build log, LastTest.log, ctest-N, fingerprint
  hashes; `timeout-minutes: 60`; concurrency cancel). After C2-WP5 lands,
  **widen the windows job to tests-ON full suite in the same cycle** (the
  workflow edit is trivial; DP4: interim CI never *required* the unlock, but
  post-unlock there is no reason to keep the narrow gate).
- `linux-parity` job (S4 §5.2, **ID-5/ID-13 encoded**): ubuntu-latest +
  apt.llvm.org clang++-21 (same LLVM major = T3 controlled pair) +
  `-DCMAKE_CXX_COMPILER=clang++ -DLSE_ENABLE_HRX=OFF -DLSE_BUILD_TESTS=ON
  -DCMAKE_CXX_FLAGS=-ffp-contract=off` (CI-level only, SC6); strict ctest
  (no `--repeat` on PR/push; weekly adds repeat-classification per S4 §5.1
  flake policy); fingerprint + T1 hash artifacts; **no ROCm/GPU scope —
  upstream `ci.yml` remains the authoritative GCC+GPU gate** (risk C2-4).
- Weekly `schedule` (cron `17 6 * * 1`) active from landing — the R3 C++26
  drift alarm. DP3 wanted this earlier; the task sequences CI last, and the
  residual unmonitored window is bounded by C2-WP1's merge cadence (§3.2
  weekly rule) — recorded as an accepted, deliberate deviation.
- Acceptance: green 1× on fork, both jobs, same SHA; dispatch test proves
  the LARGE_IO toggle. **Gate: G-C2-5.** (M4's 3× green is cycle 3.)

### 2.1 Dependency graph & ordering

```
C2-WP1 (SYNC-1) ──┬─► C2-WP3 (M2b sentinel) ──soft──┐
                  │                                 ▼
                  ├─► C2-WP4 (unlock, X=0) ──► C2-WP5 (M3 suite) ──► [G-C2-4 = M3]
                  │            ▲ census half independent of WP3      │
C2-WP2 (DOC fixes, floats; rides WP1 push) ─ [G-C2-1]                ▼
                  └────────────────────────────────► C2-WP6 (interim CI, LAST) ─► [G-C2-5]
Critical path: C2-WP1 → C2-WP4 → C2-WP5 → C2-WP6.
Parallel sets: {WP2 anytime}; {WP3 ∥ WP4-steps-1/2}; WP6 authored early, merged last.
```

Ordering rationale: (1) SYNC-1 first — every other WP's evidence is worthless
against a stale base, and merging at the measured 0-behind window rehearses
the ritual before WP12 enlarges the delta (08 §3.3.4); (2) M2a regression
re-run is the **entry proof** that the merge changed nothing — no new work
lands on an unproven base; (3) M2b sentinel before the suite tail so K
accounting exists before WP12 can consume K budget; (4) unlock before suite
(W census meaningless without `all` built — DP4); (5) CI last per task
constraint (stable local state; encode the final ID-5 partition and W into
the workflow once, not twice).

---

## 3. SYNC-1 Merge Protocol (normative for C2-WP1 and every future sync)

### 3.1 Pre-conditions

1. Working tree clean (`git status -sb`); untracked `chroma_data/` stays
   untouched (governance).
2. Branch `feature/windows-enablement` pushed and in sync with `fork`
   (verified true at planning time).
3. Remotes: `origin` = upstream (Geramy/LSE), `fork` = antmikinka/LSE.

### 3.2 Ritual (in order; every output recorded in the STATUS.md sync note)

```bash
git fetch origin --tags
git rev-list --count HEAD..origin/master          # L6 churn measurement — record N
# if N/week > 150: re-evaluate protocol per L6 before merging
git tag pre-sync/$(date +%F) && git push fork pre-sync/$(date +%F)
git merge origin/master                            # MERGE. NEVER rebase.
# ...resolve per §3.3–3.4...
# §3.5 regression gate (full M2a re-run) — mandatory, before ANY new work
# STATUS.md: sync note + base age + W/X/K + rev-list count
git push fork feature/windows-enablement
```

**Why never rebase (08 §3.3.1, DP1=a):** the branch is published on `fork`;
frozen artifacts 06/07/08 and `logs/m2a-build.txt` cite commit IDs
`9ce9287..71be5a5` that a rebase rewrites; merge resolves each hot-file
conflict once per sync instead of once per commit × 10. Linear history, if
upstream ever wants it, is produced at PR time (M4+, squash or one-off
rebase onto then-tip) without disturbing this branch.

**Cadence:** weekly (fixed day) + mandatory pre-gate (a gate certifies only
against a base ≤ 7 days old). At ~125 commits/week, two missed weeks ≈ 250
commits of drift — never let it happen.

### 3.3 Conflict doctrine — general rule

**Take upstream content wholesale, then re-apply our Windows guards
additively at the new location. Never delete an upstream line to preserve a
fork line; preserve BOTH upstream intent AND Windows gating.** Our delta is
by construction small guarded hunks (`#ifdef _WIN32`, `elseif(WIN32)`,
compiler-ID branches) inside upstream-owned files + ~2,500 lines of brand-new
files (zero-conflict surface: seam, winsock compat, poller, ledgers, docs).
A conflict is therefore almost always "upstream moved/rewrote the region our
guard sits in" → the resolution is to *re-place* the guard around upstream's
new code, idiom-matched (`if(APPLE)` precedent), not to choose sides.
§4.3 diff-minimization rules remain binding on every resolution hunk.

### 3.4 Per-hotspot resolution doctrine (08 §3.2 exposure table)

| Hotspot (upstream commits since Aug) | Our delta shape | Doctrine |
|---|---|---|
| `CMakeLists.txt` (**94 — HIGH**) | compiler-ID flag branch, seam sources `elseif(WIN32)`, poller select, ws2_32 link, `lse_force_load` sites | Accept upstream's new structure verbatim; re-insert each guarded branch at the semantically corresponding new location; verify the configure summary still prints `HRX backend: OFF` and the C++26 dialect override still keys on Clang; re-check `-ffp-contract=off` survives in the Windows flag branch (R7). |
| `tests/CMakeLists.txt` (**81 — HIGH**) | +97 ln: WIN32 guards, X-ledger `if(NOT WIN32)` ×2, ID-4 `if(TARGET lse_tokenizer)` guards ×4 | Accept every new/changed upstream registration. **For each NEW upstream test, classify immediately into Stage-4 buckets (a)–(e)** and record the classification in the sync note. If the test is **HRX-tainted** (its `target_sources` reference `src/backends/hrx/loomc|hipc` TUs, incl. via `if(NOT TARGET lse_backend_hrx)` else-paths): regenerate `windows-enablement/COMPILE_EXPOSURE.txt` with `gen_compile_exposure.sh`, add it to compile-exposure gating — targeted-set membership if it compiles under clang-cl, else COMPILE_EXCLUSIONS ledger entry (X budget review) with `if(NOT WIN32)` guard + in-test `LSE_SKIP` per S4 §1.2.2. Re-apply ID-4 tokenizer guards if upstream touched lines :129/:130/:135/:196-equivalents. |
| `src/graph/scheduler.cpp` (62) | +4 ln pure addition | Re-place the addition; trivially additive. |
| `include/lse/graph/graph.hpp` (33), `src/graph/jit_cache.cpp` (17), `src/graph/graph.cpp` (12) | +6 ln / seam reroutes (±35) / guarded hunks | Take upstream logic; re-route the same calls through `lse::platform` (flock→`try_lock_exclusive`, setenv→`set_env`) with the existing idiom; no new unguarded edits. |
| `src/cli/server_main.cpp`, `src/api/lse.cpp` (32) | seam reroutes (+21 / ±26) | Same re-route doctrine; `SetConsoleCtrlHandler` and `_pipe`-based LogCapture arms must survive; JSON key-presence contract (N2) unchanged. |
| `src/server/http_server.cpp` (22), `devices.cpp` (15), `weights.cpp` (11), `hrx/CMakeLists.txt` (16) | small guarded hunks | Re-place guards; `hrx/CMakeLists.txt`: our only delta is the 2 guard lines — anything else there is a PR5 tripwire. |

### 3.5 Post-merge regression gate (mandatory — full M2a re-run)

No new work (C2-WP3..6) lands until this is green. Exact commands in §6
G-C2-0. If upstream added targets/tests, counts legitimately drift
(`[192/192]`→`[N/N]`, `34`→`M`): pass criterion is **0 build errors + 100%
of the (regenerated) targeted set + registration smoke green**, with the
delta recorded in the sync note. Plus Linux-parity configure+build on the
merged SHA (ID-13 disposition compiler).

### 3.6 Abort criteria (stop → `git merge --abort` → restore `pre-sync` tag → STATUS.md incident note → escalate to user with options)

1. **Platform-seam files appear in the conflict set** — any of
   `include/lse/core/platform.hpp`, `src/core/platform_posix.cpp`,
   `src/core/platform_win.cpp`, `src/communication/poller_winsock.cpp`,
   `include/lse/communication/winsock_compat.hpp`. Upstream has no reason to
   touch these; if it does, upstream has likely shipped its own Windows port
   and the program needs a dedup decision, not a merge resolution.
2. **Destructive seam-call-site churn:** an upstream rewrite of
   `file_read.cpp`/`weights.cpp`/`socket.cpp`/`jit_cache.cpp` etc. removes
   our reroutes *and* correct re-application would require >~50 lines of
   unguarded edits to upstream-owned files (violates §4.3 minimal-diff).
3. **HRX gating breach:** the merge would alter `src/backends/hrx/**`
   sources or defeat the force-OFF WIN32 guard in `LSEOptions.cmake` (PR5).
4. Regression gate (§3.5) red and root cause is upstream-introduced breakage
   not fixable within §4.3 rules → pin base to last-green SHA, report, await
   decision (never silently proceed).

---

## 4. Cycle-1 Process Lessons — Binding Mechanisms

| Lesson | Mechanism in this plan (enforceable, not aspirational) |
|---|---|
| **L1 compiler-as-census** | C2-WP4 step 1: tests-ON `all` build is the WP's **first act**; harvested error list committed as `windows-enablement/logs/c2-wp12-census.txt` — that file *is* the work list. Grep only cross-checks, and must use the extended pattern set (`fsync|fdatasync|mallinfo|aligned_alloc|pipe|dup2|fnmatch|isatty|unistd|sys/wait`). Scope growth is census-driven (risk C2-3). |
| **L2 gate rehearsal** | Every SC (§1.3) and gate (§6) carries its exact command + rehearsal-status tag (PROVEN / SPEC-NOW / EXPECT-FAIL). EXPECT-FAIL is used honestly for the tests-ON `all` build: its failure today is the L1 probe; the gate is rehearsed by executing the probe. The implementing stage re-runs every command before declaring its gate. |
| **L3 generate all counts** | W = committed `ctest -N` diffs (M1/M2a/G-C2-3/G-C2-4); `COMPILE_EXPOSURE.txt` regenerated by script, never hand-edited; X and K live in machine-readable ledgers (`COMPILE_EXCLUSIONS.md`, new `K-LEDGER.md`); no hand-maintained test-name lists anywhere. |
| **L4 docs land when true** | DOC-1 ships now because "targeted tests only" is *true now* (XS commit, C2-WP2). C2-WP4 step 7: the `docs/WINDOWS.md` §4 "all will fail" warning is deleted **in the same commit** that makes `all` build. M3/M5 doc surfaces ship with their gates, never ahead. |
| **L5 cited evidence in git** | DOC-2 fixes D-2 (`logs/m2a-configure.txt` committed, link re-pointed, `git ls-files` acceptance). From cycle 2 on: **no artifact, doc, or ledger may cite a file not tracked in git** — review-checklist item; SC4 transcripts, census log, M3 bundle, and (cycle 3) `logs/m4-ci-evidence.md` all committed. |
| **L6 measure churn every sync** | §3.2 records `git rev-list --count <last-sync>..origin/master` in every sync note; >150/week triggers protocol re-evaluation before merging. |
| **L7 living state file** | `windows-enablement/STATUS.md` (created by C2-WP1, updated at every gate) — schema: branch + pushed?; base SHA + age (days); last-sync rev-list count; W / X / K current values; gate table G-C2-0..5 with dates + evidence links; open incidents. Frozen artifacts (01–08) never edited for live state; corrections append here or to 08's successor. |
| **L8 physics-feasibility line** | Any new acceptance criterion added mid-cycle requires an explicit attainability line (MSVC-STL/WSAPoll physics). ID-5 partition is pre-applied to M3 (C2-WP5): **no cross-OS bit-exact hashes for transcendental callers, ever**; T1 bit-exact set is limited to IEEE-exact ops. |

---

## 5. Cycle-2 Risk Register

Inherits and re-scores 08 §5 (which supersedes S1 §8/S2 §5); rows below are
the cycle-2 deltas and new risks. Full inherited table remains authoritative
for R2/R4/R5/R6/R9/R10/N1–N6 (unchanged postures, owners as mapped there).

| # | Risk | L | I | Posture / mitigation / contingency |
|---|---|---|---|---|
| **R1′** | Upstream churn vs fork — **posture worsened by DP2=NO**: Batch-1 PR (which 08 §3.3.5 calls the permanent fix for the #1 conflict file) is deferred to M4, so `CMakeLists.txt`/`tests/CMakeLists.txt` deltas stay fork-owned through cycle 2 | **H** | **H** | Mitigation: SYNC-1 executed first in the measured 0-behind window; weekly cadence + pre-gate syncs (§3.2); §3.3–3.4 doctrine; pre-sync tags. Contingency: PR1 trigger (>2 conflicting files in 2 consecutive syncs) → escalate Batch-1 filing to the user as a decision item (deferral is a user decision — this plan reports, it does not override). |
| **R3′** | C++26 feature drift upstream — interim CI (the mitigation) lands **last** this cycle per task sequencing, not "now" per DP3 | **H** | H | Mitigation: weekly merges bound silent breakage to ≤1 week; the full M2a regression re-run at every sync is a drift detector; C2-WP6's weekly cron keeps the alarm on after landing. Accepted deviation from DP3, recorded. |
| **PR7′** | Test semantic drift (WSAPoll ms granularity, SIGTERM-undeliverable, mandatory LockFileEx, heap_stats zeros N2) — first real contact at C2-WP4/5 | **M→H** | M | Mitigation: L1 probe surfaces vectors as compiler/test errors early; K ≤ 5 budget is containment; every K entry needs an upstream issue; tolerance-aware ports preferred over skips (S4 N1 guidance). Contingency: if K budget exhausts pre-M3 → gate review with user (add budget vs. descope test) — never silent. |
| **C2-1** | **Merge conflicts in CMakeLists hotspots** materialize at SYNC-1 (94/81 upstream commits since Aug; 81% of our touched files in hot zones) | M (at execution: depends on N measured) | M | Doctrine §3.3–3.4 (re-place guards, never delete upstream); conflicts concentrate in small `#ifdef` hunks; ~2,500 lines of our delta are zero-conflict new files. Contingency: abort criteria §3.6; pre-sync tag restore. |
| **C2-2** | **Fresh HRX-tainted upstream tests** added since the last fetch: they compile loomc/hipc TUs, silently invalidating `COMPILE_EXPOSURE.txt` (35) and the targeted-34 gate | **M** | M | §3.4 tests/CMakeLists doctrine: classify every new test into buckets a–e at merge time; regenerate exposure list by script (L3); compile-check new exposures before the regression gate. Contingency: ledger entry (X review) — but X must return to 0 at C2-WP4. |
| **C2-3** | **WP12 port uncovers deeper POSIX usage** in shared test utilities (`tests/harness.hpp`, `tests/reference.hpp`, fixtures) beyond the 15-file census → unlock slips, M3 slips | M | M | Mitigation: census-first (L1) sizes the *real* surface on day 1 of C2-WP4, not day 10; `compat.hpp` is extensible by design; G-C2-3/G-C2-4 split so CI (G-C2-5) can still land on the targeted gate if the tail slips (DP4 decoupling). Contingency: re-plan mid-cycle from the committed census; K-budget for runtime-only residuals. |
| **C2-4** | **CI runners lack ROCm/AMD GPU** → linux-parity job cannot mirror upstream's GPU/HRX coverage; T3 pair is CPU-only | Certain | L | Scope limit stated in the workflow header + STATUS.md: parity job = clang++-21, `LSE_ENABLE_HRX=OFF`, CPU backend; upstream `ci.yml` (self-hosted GPU, g++-16) remains the authoritative GPU/GCC gate, untouched (SC6). No mitigation needed beyond honest labeling. |
| **C2-5** | **Interim CI minutes budget** overrun (windows-latest multiplier; cargo/ring cold ≈12 min; S4 est. ~40 min cold full suite) | M | M | Mitigation: interim gate is small (tests-OFF build + 34 binaries ≈ well under budget); rust-cache + fastokens pin caching; concurrency-cancel; public repo → hosted minutes free. The <45-min criterion binds only at M4 (cycle 3). Trigger: any run >45 min → path filters/sccache evaluation. |
| **C2-6** | **M2b sentinel fails** (`test_tokenizer` runtime red on Windows — bucket-(e) is exactly the untested surface) | L-M | M | Mitigation: C2-WP3 early in the cycle; failure opens K-entry #1 with review-by date instead of blocking (08 backlog #4 fallback) — K budget then has 4 left for M3. Contingency: if it's a product defect (not capability gap), it blocks per S4 §7 rule 1 and gets fixed in-cycle. |
| **C2-7** | **Local WSL clang++-21 unavailable** for Linux parity/census at G-C2-4 (ID-13 makes clang++-21 *mandatory* from M3 for the T3 pair) | M | M | Mitigation: provision during C2-WP1 (apt.llvm.org, cheap); **descope option (recommended if provisioning fails): take Linux-parity + ID-11 census evidence from the C2-WP6 `linux-parity` CI job on the same SHA** — same compiler, same artifact class, no local dependency; record the substitution in STATUS.md/DECISIONS. |
| **N6′** | Docs/evidence drift recurrence (D-class) after WP12's doc churn | M | L | L4/L5 mechanisms above; review checklist: every gate PR asks "does any living doc now say something false?" and "is every cited file in `git ls-files`?" |

Register reviewed at every gate G-C2-0..5; PR1/R3′ triggers checked at each
sync note; new risks appended C2-8+ in STATUS.md.

---

## 6. Milestones / Gates (each with exact commands)

Shell assumptions for every command below: MSVC env entered per
`docs/WINDOWS.md` §2 (vcvars64 / DevShell / inherited Git Bash); clang-cl 21
on PATH; repo root = `C:\Users\antmi\LSE`. Commands marked PROVEN were
executed verbatim in cycle 1 (evidence `logs/m2a-build.txt`).

### G-C2-0 — SYNC-1 merged + M2a regression green *(gate of C2-WP1; blocks all new work)*

```bash
# 1. sync ritual (§3.2)
git fetch origin --tags
git rev-list --count HEAD..origin/master                  # record N in sync note
git tag pre-sync/$(date +%F) && git push fork pre-sync/$(date +%F)
git merge origin/master                                    # resolve per §3.3–3.4 or abort per §3.6

# 2. regression: tests-OFF full build (PROVEN)
cmake -S . -B build-win-notests -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=OFF
cmake --build build-win-notests        # expect [N/N], 0 errors; summary prints "HRX backend: OFF"

# 3. regression: targeted test set (PROVEN; regenerate exposure list first if upstream added tests)
bash windows-enablement/gen_compile_exposure.sh            # refresh COMPILE_EXPOSURE.txt (L3)
cmake -S . -B build-win -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=ON
RE=$( (grep -vxE "test_dflash2|test_loom_cache" windows-enablement/COMPILE_EXCLUSIONS-driven-list; \
       echo test_registration_smoke) | sort | paste -sd'|' -)   # exclusion list per current ledger
cmake --build build-win --target $(echo "$RE" | tr '|' ' ')
ctest --test-dir build-win -R "^($RE)$" --output-on-failure     # expect 100% pass (34/34 or drifted M/M)

# 4. Linux parity configure+build on merged SHA (ID-13 compiler, WSL)
# 5. STATUS.md created/updated; push
git push fork feature/windows-enablement
```
**Pass:** merge (or recorded no-op) on branch; builds 0 errors; targeted ctest
100%; smoke green; sync note + base age + N in `STATUS.md`; pushed.
**Fail → §3.6 abort/escalate. No C2-WP3..6 work before this gate.**

### G-C2-1 — Docs drift closed *(gate of C2-WP2)*

```bash
git ls-files logs/m2a-configure.txt                        # non-empty (D-2 fixed, L5)
grep -n "build-win-notests-configure.log" docs/WINDOWS.md  # zero hits (link re-pointed)
grep -n "LSE_BUILD_TESTS=OFF" BUILD_FROM_SOURCE.md         # ≥1 hit in §Windows (D-1 fixed)
# DOC-1 evidence: re-run the fixed snippet verbatim; green configure+build
```
**Pass:** all four checks hold; snippet re-run transcript recorded.

### G-C2-2 — M2b sentinel passes *(gate of C2-WP3)*

```bash
cmake --build build-win --target test_tokenizer
ctest --test-dir build-win -R "^test_tokenizer$" --output-on-failure
# SC4: real generation transcripts (not --help) for both executables → logs/m2b-acceptance.md
./build-win-notests/lse.exe <generation invocation per SC4 script>
./build-win-notests/lse-server.exe <end-to-end request transcript>
ls windows-enablement/K-LEDGER.md                          # exists; K=0 or 1 (ledgered, ID-6 format)
```
**Pass:** sentinel green (or K-entry #1 ledgered with reason+review-by);
SC4 transcripts committed; K ledger machine-readable.

### G-C2-3 — X=0 + tests-ON `all` builds *(gate of C2-WP4; unlocks the W census)*

```bash
# Step 1 was the L1 probe (EXPECT-FAIL pre-port; its log is committed):
cmake --build build-win 2>&1 | tee windows-enablement/logs/c2-wp12-census.txt   # pre-port failure census
# After ports:
cmake -S . -B build-win -G Ninja -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSE_BUILD_TESTS=ON
cmake --build build-win                                    # NO --target: expect exit 0, 0 errors (SC-C2-5)
grep -c "if(NOT WIN32)" tests/CMakeLists.txt               # X-ledger guards gone (only non-X guards may remain; diff-verified)
grep -n "X = 0" windows-enablement/COMPILE_EXCLUSIONS.md   # ledger stubbed
ctest --test-dir build-win -N > windows-enablement/logs/c2-w-census-g3.txt      # W re-census (L3), diff vs M2a committed
grep -n "will fail" docs/WINDOWS.md                        # §4 warning removed in the SAME commit (L4)
```
**Pass:** `all` builds 0 errors; X=0 ledger; W census committed; docs updated
in-commit.

### G-C2-4 — M3: full local ctest green, K ≤ 5 *(gate of C2-WP5 = program milestone M3)*

```bash
ctest --test-dir build-win --output-on-failure --timeout 300 --label-exclude LARGE_IO 2>&1 \
  | tee logs/m3-ctest-windows.txt
ctest --test-dir build-win -R "test_large_file_io" --output-on-failure 2>&1 \
  | tee logs/m3-large-file-io.txt                          # mandatory locally (S4 §3/§7)
ctest --test-dir build-win -N | tee logs/m3-ctest-N.txt    # final W; diff vs G-C2-3 + M2a
# Linux parity, same SHA (ID-13 clang++-21; or C2-7 descope → linux-parity CI job evidence):
#   WSL: cmake -S . -B build-lin -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DLSE_ENABLE_HRX=OFF \
#        -DLSE_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off
#   cmake --build build-lin && ctest --test-dir build-lin --output-on-failure --label-exclude LARGE_IO \
#        | tee logs/m3-ctest-linux.txt
#   + ID-11 Linux census baseline captured in the same run
grep -c "^| " windows-enablement/K-LEDGER.md               # K ≤ 5, each: windows: <capability|flake> — issue #N — review-by
grep -in "windows" KNOWN_FAILURES.md                       # ZERO Windows entries (ID-6)
# Evidence bundle committed: m3-* logs, K-LEDGER, fingerprint hashes (ID-5 partition printout:
# HASH-EQ equal cross-platform; TOLERANCE within golden max_rel), reference.safetensors SHA256
# (goldens materialized — else bundle labeled INCOMPLETE per S4 §2.3)
```
**Pass:** all W entries pass or configure-disabled-with-ledger; K ≤ 5
capability gaps only; zero bucket-(a) failures (any = product defect = gate
blocked); X=0; parity green same SHA; bundle committed. **M3 declared in
DECISIONS/STATUS only with these links.**

### G-C2-5 — Interim CI green 1× *(gate of C2-WP6; M4 needs 3× later)*

```bash
# workflow + parity job pushed to fork; then:
gh workflow run windows.yml --repo antmikinka/LSE --ref feature/windows-enablement
gh run watch --repo antmikinka/LSE            # both jobs green on the SAME SHA
gh run list --repo antmikinka/LSE --workflow windows.yml
# ID-10 dispatch proof:
gh workflow run windows.yml --repo antmikinka/LSE -f run_large_io=true   # log shows LARGE_IO tests RUNNING
gh api repos/antmikinka/LSE/actions/workflows/windows.yml                # weekly schedule cron present (R3 alarm)
grep -n "21.1.8" .github/workflows/windows.yml                           # toolchain pin == docs/WINDOWS.md §1
```
**Pass:** 1× green (windows + linux-parity, same SHA); schedule active;
run_large_io toggle demonstrated; pins match docs. Record run URL in
STATUS.md (L5). **Not** an M4 declaration (3× consecutive + <45 min is
cycle 3, 08 backlog #8).

---

## 7. Handoff to cycle-2 execution stages

- **Implementer:** execute C2-WP1..6 in §2.1 order; §4.3 diff rules and
  §3.3 doctrine binding on every hunk; one WP = one commit stack with
  `C2-WPn:` prefix; evidence per §6; STATUS.md updated at every gate.
- **Quality reviewer:** check L1 (census before porting), L4/L5 (docs-truth +
  evidence-in-git checklist), K/X/W unit discipline (ID-6), and that no
  gate is declared on unrehearsed commands (L2).
- **Escalation:** gate miss or §3.6 abort → options + recommendation logged
  in STATUS.md, user decision before proceeding — never silent scope change.

*End of cycle-2 program plan. Artifacts 01–08 remain frozen; corrections and
live state live in this file's gates and `STATUS.md` (L7). No source
modified; nothing committed by Stage 1.*
