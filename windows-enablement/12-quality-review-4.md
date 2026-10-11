# 12 — Quality Review 4 (Stage 4, cycle 2 · second-pass review of the cycle-2 chain)

**Artifacts under review:** `09-cycle2-program-plan.md` (immutable, as scored by 10), `10-quality-review-3.md` (amendment layer A-1..A-7, B-1/B-2, M-1..M-4), `11-cycle2-test-plan.md` (Stage-3 response — the primary subject of this pass).
**Branch:** `feature/windows-enablement` @ `71be5a5` (= upstream `a5b82f6`, v0.6.0-28 + 10 commits). `git rev-list --count HEAD..origin/master` = 0 at review time (2026-10-10).
**Authority:** this document is the FINAL authority for the C2 Stage-5 implementer. Precedence on conflict: **12 §6 directives → 10 §8 amendments → 11 → 09 → 05 ID-layer → 04 → 06 → 02.** Artifacts 01–11 remain unmodified; nothing committed by this stage.

---

## 1. Review Method

1. **Full re-read** of 09, 10, 11; targeted re-reads of 02 (SC4 :40), 04 (§1 buckets/§1.2 exposure/§1.4 sockets/§2 tiers/§3 large-IO/§5 CI), 05 (ID-1..ID-14 headers), 06 (M2a evidence), 08 (D-flags/L-lessons/backlog/DPs).
2. **Independent re-derivation of every number 11 claims**, from the live tree — not from 11's text: `ctest --test-dir build-win -N` (**Total Tests: 91**, re-run by this review); `COMPILE_EXPOSURE.txt` (read in full — **35** names, `test_dflash2`/`test_loom_cache` present, `test_registration_smoke` absent); `COMPILE_EXCLUSIONS.md` (read in full — **2** data rows, both shaped `` | `test_x` | `` → the A-1 awk pattern matches exactly and nothing else); `build-win-notests/build.ninja` (**168** `FLAGS =` edges, **166** pinned with `ffp-contract=off`; the 2 unpinned edges attributed by name — they are the `CXX_EXECUTABLE_LINKER__lse` / `__lse-server` link edges, exactly as 11 claims); `cmake/LSECompilerFlags.cmake` (:75 `/clang:-fno-fast-math /clang:-ffp-contract=off`; :87-88 `_CRT_NONSTDC_NO_DEPRECATE`/`_CRT_SECURE_NO_WARNINGS`); `tests/CMakeLists.txt` (`lse_add_test` function :4-16; `if(NOT WIN32)` exactly 2 at :104/:195; dflash2 `else()` branch :210-213; raw `test_server_shutdown` target :169-175; tokenizer guards :137/:220/:238); `cmake/LSERust.cmake` (auto-clone at pinned `7973014e…` — 11's "no manual clone" deviation VERIFIED); `tests/harness.hpp` (skip mechanics :43/:98-106, `skip <case> (<reason>)` print, exit 0); vendored `httplib.h` (**0.18.3** — Windows AF_UNIX path sockets supported → row 2's `pipe_url()` H-port is feasible); `docs/WINDOWS.md` (:28-32 toolchain table, :124 dangling log cite, :160-170 "all will fail" warning, :190-195 proven targeted command); `.github/workflows/` (no `windows.yml`); `windows-enablement/` (gen_compile_exposure.sh **exists and is git-tracked**; `logs/` subdirectory **does NOT exist**); `BUILD_FROM_SOURCE.md` :49-55 (snippet confirmed lacking `-DLSE_BUILD_TESTS=OFF` and carrying `$env:CC`).
3. **Per-file POSIX symbol census** of all 17 `<unistd.h>`/`<sys/wait.h>` files (15 Windows-registered after removing HRX-gated `test_jit.cpp`/`test_capture_prefill.cpp` — count re-confirmed), and registration check of all 15 against the 91-name W list (13 present; `test_dflash2`/`test_loom_cache` absent = X-guards working).
4. **Port-table spot-check of 10 of 21 rows** against actual file contents (§4 below), including reading the exact cited lines (`test_graph.cpp:3155`, `test_model.cpp:310-330`, `test_tree_verify.cpp:698-710`, `test_dflash2_convert.cpp:30-50`, `test_server_shutdown.cpp:1-45`).
5. **Adversarial execution walk-through** of every command block in 11 (§2.1/2.2/2.3/3.1/5.2, §6 YAML) as if run verbatim in Git Bash / GitHub Actions on this tree, hunting silent-wrong-direction failure modes (the B-1/BL-3 class). This found N-1..N-4 below.
6. **Arithmetic reconciliation** of 04's W=92 prediction vs the measured 91 vs 11's 97/96+1 chain, line by line (§3).

---

## 2. Amendment Closure Matrix (10 §7–§8 → 11/09)

| Item | 10's demand | Where answered | Independent verification by this review | Status | Residual action |
|---|---|---|---|---|---|
| **B-1** false-green targeted ctest (pseudo-file `COMPILE_EXCLUSIONS-driven-list`; degenerate RE → 1/1 "100%") | A-1: proven command from `docs/WINDOWS.md:190-195`, ledger-derived EXCL, cardinality assertion | 11 §2.3 (Step R3) + I-1/I-2 invariants | Re-derived from real files: 35 exposure names (smoke NOT among them → +1 cannot double-count), 2 exclusion rows matching the awk shape → **|RE| = 35−2+1 = 34**, equals the proven M2a set (34/34 in `logs/m2a-build.txt`). Degeneration paths: awk-source missing → `set -euo pipefail` aborts; EXCL empty post-X=0 → `grep -vxF ""` under `-x` matches only empty lines → all 35 pass, NRE=36=35−0+1 ✓ (correct, though only because of `-x` — fragile-but-sound). `NRAN == NRE` "out of N" parse kills the 1/1 class outright. Triple assertion (`NEXPOSURE−NEXCL+1`, floor ≥30, smoke present) all present | **CLOSED** | None. Record |RE| in the sync note (already required) |
| **B-2** SYNC-1 doctrine gaps (a) conflict-set detection, (b) >50-edit threshold, (c) upstream delete/rename, (d) `cmake/*.cmake` incl. silent `-ffp-contract=off` drop, (e) new POSIX coupling | A-2a..A-2e adopted verbatim before `git merge` | 10 §8 A-2 (binding text) + 11 §2.2 (FP canary layers A/B/B′ operationalizing A-2d) + 11 §2.4 (R4 adopts A-2a..e by reference) | Layer B claim re-measured on the live tree: **168/166/2, unpinned = the two linker edges** — exact. Layer A pin at `LSECompilerFlags.cmake:75` — exact. B′ (`ninja -t commands`) logic valid. A-2b three-dot diff semantics after a merge are correct (merge-base = upstream tip → pure fork delta). Detection commands exist in A-2a text | **CLOSED** (via amendments; 11 adopts, does not restate) | Directive **ID-C2-1** consolidates A-2a..e into one executable checklist; **ID-C2-5** parameterizes the canary to BOTH build trees from G-C2-3 on (new/unlocked targets must be pin-covered too) |
| **M-1** Linux-parity evidence circularity; G-C2-0 Pass line omits step 4 | A-3: WSL go/no-go task in C2-WP1; hoist parity job on no-go; reconcile Pass line; +ID-11 smoke in parity job (m-6) | 11 §2.4 R4 (go/no-go before gate declaration; A-3.3 path), §5.2 step 7, §6 linux-parity job incl. `test_registration_smoke` run + artifact | Parity job spec present with ID-11 producer (m-6 closed); sequencing escape (hoist/descope/M3-partial) intact | **CLOSED** | **ID-C2-2** states G-C2-0 Pass = ALL of R1–R4 (supersedes 09's Pass line, which omits parity) |
| **M-2** false "21.1.8 matches docs §1" claim | A-4: docs row at WP6; STATUS.md records both patch levels; fix G-C2-5 comment | 11 §6 YAML comments carry A-4 verbatim ("pin RECORDED in docs/WINDOWS.md §1 alongside local 21.0.0git") | `docs/WINDOWS.md:30` re-verified: LLVM **21** major only, "any 21.x works" — 11's framing is now truthful | **CLOSED** | Docs row is a C2-WP6 deliverable (**ID-C2-12/14**) |
| **M-3** SC4 script undefined | A-5: `windows-enablement/sc4-script.md` with model ID, pull, generation invocation, server transcript, pass criteria — committed BEFORE G-C2-2 | 11 §7.5 (existence precondition + fixture suggestion + evidence joins M2b bundle) | Spec now executable-as-a-task; the file itself is a Stage-5 deliverable, correctly | **CLOSED** (as spec) | **ID-C2-13** mandates content list + budget line + commit-before-run |
| **M-4** K-count grep counts header row | A-6: data-row-only pattern frozen in ledger header | 11 §4: `K=$(grep -cE '^\| `' …)`; row format exact; pattern frozen in header comment; §5.2 step 5 + §8 assert ≤5 | Pattern verified against the specified row shape (data rows begin `` | ` ``, header row does not, `|---` does not) | **CLOSED** | NEW adjacent defect found: the skip-scrape cross-check is unmeasurable under `--output-on-failure` → **ID-C2-9** |
| **A-1** | — | 11 §2.3 (strengthened: adds exact-equality cardinality + NRAN parse beyond A-1's floor-only check) | See B-1 | **CLOSED** | — |
| **A-2a..e** | — | Adopted by reference (11 §2.4); A-2d strengthened by canary layers | See B-2 | **CLOSED** | ID-C2-1 consolidation |
| **A-3** | — | 11 §2.4/§5.2-7/§6 | See M-1 | **CLOSED** | ID-C2-2 Pass-line |
| **A-4** | — | 11 §6 comments | See M-2 | **CLOSED** | — |
| **A-5** | — | 11 §7.5 | See M-3 | **CLOSED** | — |
| **A-6** | — | 11 §3.4 step 2 (guard count → **0**), §4 | dflash2 `else()` branch (test_feature_taps ENVIRONMENT) confirmed at tests/CMakeLists.txt:210-213 — merge-don't-delete rule correct | **CLOSED** | — |
| **A-7** (m-1/m-2/m-4/m-5) | — | m-1: 11 §3.3 note ("inside the 15") ✓; m-2: §6 cron check via `gh workflow view --yaml` ✓; m-5: §3.4 "locate by target name" ✓; **m-4 ($env:CC drop): NOT restated in 11** — remains binding via 10 §8 | `BUILD_FROM_SOURCE.md:50` still has `$env:CC = "clang-cl"` | **CLOSED** (m-4 by carry-forward) | **ID-C2-14** folds the $env:CC drop into DOC-1 scope explicitly |

**Matrix verdict: 2/2 BLOCKING, 4/4 MAJOR, 7/7 amendments CLOSED.** No amendment regressed. All residual actions are directive-level operationalizations, none requires re-planning.

---

## 3. Numeric Reconciliation

### 3.1 The W chain — re-derived, line by line

| Step | 11's claim | This review's independent measurement | Verdict |
|---|---|---|---|
| Today (X=2, pre-P8) | W = **91** | `ctest --test-dir build-win -N` → `Total Tests: 91`; `test_dflash2`/`test_loom_cache` absent from the name list; `test_registration_smoke`, `test_tokenizer`, `test_server_cli`, `test_server_shutdown`, `test_dflash2_walk` present | **EXACT** |
| Targeted set | 35 − 2 + 1 = **34** | Ledger files read in full; smoke not in exposure; both X rows awk-matchable | **EXACT** |
| After X→0 (G-C2-3) | **93** + U | 91 + 2 restored | **CORRECT** |
| After P8 (G-C2-4) | **97** + U | 93 + 4 (`test_platform_shim`, `test_large_file_io`, `test_numerics_fingerprint`, `test_fp_model` — all confirmed ABSENT from `tests/` today) | **CORRECT** |
| M3 main run | `out of 96+U` + separate `out of 1` | 97 − 1 (LARGE_IO label) = 96 | **CORRECT — no off-by-one anywhere** |
| FP canary | 168 edges / 166 pinned / 2 linker | Re-measured identical; linker attribution verified by name | **EXACT** |
| unistd census | 17 files → 15 Windows-registered → 14 harness + shutdown | Re-grepped identical; all 15 confirmed in the 91-list except the 2 X-guarded | **EXACT** |

### 3.2 Cycle-1 W=92 vs cycle-2 W=91 — the reconciliation 11 omits

04 §1.1's **W = 92** was a *pre-fork prediction* at base `a5b82f6` ("90 `lse_add_test` + `test_server_shutdown` + `test_server_cli`"). The measured **91** = 92 **+ 1** (`test_registration_smoke`, fork-added in `9ce9287`) **− 2** (X-guards `if(NOT WIN32)` at `tests/CMakeLists.txt:104/:195`, landed in `fe6d522`). Therefore:
- **W=97 is RIGHT; 98 would be WRONG** (92+2+4 double-counts the smoke test, which is already inside the measured 91).
- 04's bucket totals remain internally consistent: 92 = 69(a) + 10(b) + 10(d) + 3(e-excl); the "110" universe = 102 `lse_add_test` + 8 raw `add_test`.
- **92 must never be used as a rehearsal anchor again** (stale prediction, not a measurement). 11 §5.1's anchors (91→93→97) are the binding ones, regenerated at each gate per I-2.
- Two truthfulness nits: (i) 11's G-C2-3 row says "diff vs the 91 **measured at M2a**" — `logs/m2a-build.txt` contains **no** `ctest -N` census (only the targeted 34/34 run); 91 was measured by Stage 3 at `71be5a5`. The value is unaffected (commits `fe6d522..71be5a5` are docs-only) but the attribution is false → correct in STATUS.md (**ID-C2-10**). (ii) The 92→91 reconciliation above must be recorded once in STATUS.md (**ID-C2-10**).

### 3.3 M3 pass criterion — identity check 09 vs 11

09 G-C2-4 states the criterion qualitatively (all W pass or configure-disabled-with-ledger; K ≤ 5 capability-only; zero bucket-(a); X=0; parity same SHA; bundle committed). 11 §8 states it numerically (W==97+U == committed `ci-expected-w.txt`; main run `100% … out of 96+U`; large-io `out of 1` <30 s; K = frozen-grep ≤ 5; skip-scrape == K; X=0 + guard-count 0; zero Windows KNOWN_FAILURES entries; canary C + HASH-EQ equality; parity + ID-11; goldens or INCOMPLETE stamp). **No contradiction** — 11 refines 09 within the A-layer precedence. **Binding M3 criterion = 11 §5.2 + §8, as amended by ID-C2-9/-10 below** (the skip-scrape needs `-V` visibility; the expected-W file needs a lifecycle rule).

---

## 4. Port-Table Audit (11 §3.3 — 10 of 21 rows spot-checked against real files)

**Threshold question first: is 11's correction of 04 §6 P1 right? YES — verified.** The 14 files contain a literal `#include <unistd.h>` directive; a macro defined by `harness.hpp` cannot satisfy an include of a *nonexistent* header — clang-cl fails at the directive itself. The shim include-dir (`tests/compat/unistd.h` + `tests/compat/sys/wait.h`, wired by one guarded `target_include_directories` inside `lse_add_test()` — function verified at `tests/CMakeLists.txt:4-16` — plus one on the raw `test_server_shutdown` target :169-175) is the only genuine zero-touch mechanism. `_CRT_NONSTDC_NO_DEPRECATE` (LSECompilerFlags.cmake:87) makes `<io.h>/<process.h>` export POSIX-named `read/write/close/pipe/access/getpid`, so the shim's content list covers the actual per-file symbol census I re-ran (getpid, setenv, unsetenv, access, mkdtemp, read/write/close; `sleep` appears only in `test_server_shutdown` [in-file port] and HRX-gated `test_capture_prefill` [irrelevant] — compat should still add `sleep()`→`Sleep(s*1000)` for census-robustness, see ID-C2-6).

| Row | Spot-check result |
|---|---|
| 1 `test_api_parity` (S+G) | Symbol census = getpid, write ✓; POLL define :33 ✓ (04/10 confirmed). **Sound** |
| 4 `test_dflash2_convert` (S) | Read :30-50: mkdtemp pattern **already** `fs::temp_directory_path()`-based (:39-41) — only the `::mkdtemp` function is missing → pure S works. **Sound** |
| 5 `test_graph` (S+P) | `:3155` exact (`"/tmp/lse-cold-spans-" + ::getpid()`), `::setenv` :3159 ✓. The `fork` grep-hits in this file are test *names* (`workgroup_diamond_is_a_fork` etc.) — no real fork. **Sound** |
| 6 `test_hub` (S+G) | access/close/getpid/setenv/unsetenv ✓; POLL :24 ✓. **Sound** |
| 8 `test_loom_cache` (S + X-removal) | getpid/setenv/unsetenv ✓; guard at :104 ✓. **Sound** |
| 3 `test_dflash2` (S+P + X-removal) | guard :195 with `else()` preserving `test_feature_taps` ENVIRONMENT (:210-213) ✓ — merge-don't-delete per A-6 is the correct instruction. **Sound** |
| 16 `test_model` (P) | :312-328 read: `ScopedEnv home("HOME","/tmp/lse-fake-home")` + **equality** asserts vs `"/tmp/lse-fake-home/hub"` → requires the ID-8 rewrite (temp-dir HOME + separator-tolerant compare), exactly as row says. **Sound** |
| 2 `test_communication` (S+H) | 11 `unix://@` sites (04/10-verified line list); no real `pipe()` call (word-hits only); httplib **0.18.3** vendored → Windows AF_UNIX *path* sockets supported → `pipe_url()` helper feasible; 108-byte guard + teardown specified. **Sound** |
| **13 `test_server_shutdown` (P2)** | Verified: own `main`, raw target, POLL :2, `pipe/fork/waitpid/WEXITSTATUS` :25-38, expects exit 1 + empty marker. Approach correct but **UNDER-SPECIFIED — will risk a vacuous pass**: CreateProcess children do not inherit CRT pipe fds like fork; `_pipe` handles are non-inheritable by default. Requires `SECURITY_ATTRIBUTES{bInheritHandle=TRUE}` + OS-handle value passed via `--child-stuck` argv + `_open_osfhandle` in the child (or temp-file marker). If wired wrong, "marker read empty" passes **vacuously** → false-green inside the shutdown-semantics test. → **ID-C2-8** (mandates mechanism + positive control) |
| **14 `test_tree_verify` (S)** | **WILL FAIL — approach wrong.** `:703` is `char dir[] = "/tmp/lse-trials-XXXXXX"` (22 bytes) + `::mkdtemp(dir)` (:704). mkdtemp mutates the template **in place**: a compat mkdtemp returning a real Windows temp path (~45+ chars: `C:\Users\...\AppData\Local\Temp\lse-trials-XXXXXX`) **overflows the 22-byte stack buffer**; honoring `/tmp` literally creates `C:\tmp` (drive-relative, permission-fragile, junk outside `%TEMP%`). "compat mkdtemp handles the template" is unsound for this row. → **ID-C2-7**: row 14 reclassified S→**P** (rewrite to `std::string` + `fs::temp_directory_path()`, mirroring `test_dflash2_convert`'s existing pattern; `::setenv` :705 stays S) |
| 17/18-21 | Loopback-TCP row and the 4 N-rows consistent with §7 specs (checked against 04 §1.3(d) and the absent-file census). **Sound** |

**Table-size claim** (14×S, 4×G, 4×P, 2×H, 4×N): after ID-C2-7 moves row 14 S→P it becomes 13×S / 5×P — the census (§3.1) overrides regardless. **One row (14) fails as written; one row (13) passes only with the mandated mechanism.** No other row shows a compile- or link-time defect. Note: `test_model_info_real` (registered executable, non-ctest, inside `if(TARGET lse_tokenizer)`) also compiles under `all` and is absent from the table — the census absorbs it by design (**ID-C2-3** names it so Stage 5 expects it).

---

## 5. Ambiguity Register + Mandated Resolutions

Every item a solo implementer would otherwise guess at across 09+10+11. Resolution column is binding (executed via the §6 directive of the same name).

| # | Ambiguity / contradiction (where) | Mandated resolution |
|---|---|---|
| R-1 | **Interim CI scope contradiction:** 09 C2-WP6 = tests-OFF full build + targeted-34; 11 §6 YAML = tests-ON + `all`; 11's own landing note recommends an early narrow landing but **specifies no narrow YAML**. Landing 11's YAML before WP12 = guaranteed red; landing only at WP6 = no push coverage during WP3-5 | **ID-C2-4**: two-phase spec. Phase 1 (may land at C2-WP1): windows job = §2.1 tests-OFF build + §2.2 canary + §2.3 targeted-34 blocks verbatim (shell: bash); linux-parity job lands in phase 1 **iff** A-3 no-go. Phase 2 (C2-WP6): switch to tests-ON `all` + §5.2-step-2 assertions. Baseline remains 09's CI-last ordering; phase-1 early landing is optional but if taken must be the narrow variant |
| R-2 | **Weekly/dispatch LARGE_IO semantics** (11 §6 env): `--label-include LARGE_IO` runs ONLY the large-io test → weekly R3 alarm + dispatch proof execute 1 test and report green | **ID-C2-4**: toggle = `''` (no filter; full suite incl. LARGE_IO) for schedule/dispatch vs `'--label-exclude LARGE_IO'` for PR/push. Dispatch proof = full-suite run whose log shows `test_large_file_io` executing |
| R-3 | **CI YAML mechanics** (11 §6): pwsh default shell expands `$( )` inside the `bash -c` canary step before bash sees it; `upload-artifact` `path:` uses single-quoted `'\n'` = literal backslash-n (step fails); main ctest step carries a *comment* where I-1 requires real assertions | **ID-C2-4**: `shell: bash` on every assertion step (Git Bash ships on windows-latest); `|` block scalars for artifact paths; §2.3/§5.2 assertion blocks inlined verbatim — a comment referencing them is not compliance |
| R-4 | **Census completeness** (11 §3.1): plain `cmake --build` = Ninja stop-on-first-error → harvested "complete" error list is partial; ≥14-file assertion usually trips, then Stage 5 improvises | **ID-C2-3**: census command is `cmake --build build-win -- -k 0` (keep-going); assertion stays ≥14 distinct failing files |
| R-5 | **K skip-scrape unmeasurable** (11 §4): skips print inside PASSING binaries; `--output-on-failure` suppresses their stdout → zero `skip … (windows:` lines in `logs/m3-ctest-windows.txt`; "distinct == K" fails for any K>0 | **ID-C2-9**: M3 evidence run adds `-V` (or a second `-V` run of the K-binaries only) tee'd to the committed log; scrape source = that log |
| R-6 | `ci-expected-w.txt` lifecycle (11 §5.2 step 1 / §6): who creates it, with what value, when? Consumed by CI on EVERY push; W legitimately changes 91→93→97 across WP4/WP5 | **ID-C2-10**: same-commit rule — the file is updated in every commit that changes test registration (G-C2-3 commit writes `93+U`; each P8 registration commit increments; value = output of `ctest -N` parse, never typed). CI W-assert consumes it from phase 2 onward (phase-1 narrow CI asserts |RE| instead) |
| R-7 | `windows-enablement/logs/` **does not exist**; §2.1's first `tee` aborts under `set -euo pipefail` | **ID-C2-2**: `mkdir -p windows-enablement/logs` is step 0 of R1 (and the dir is committed via its first artifact) |
| R-8 | Canary tree hardcoding (11 §2.2 = `build-win-notests` only): post-unlock, 17 new/re-enabled compile targets could escape the pin | **ID-C2-5**: layers A/B run against **both** trees at G-C2-3/G-C2-4 (B on `build-win` after its configure); B′ optional; C + one-time mutation transcript at G-C2-4 |
| R-9 | Evidence path conflict: 04 §3.5 says `windows-enablement/logs/m3-large-file-io.txt`; 09/11 say `logs/` | **ID-C2-11**: bind **root `logs/`** for all m3-* evidence (matches `m2a-build.txt` class, 09 §2 rule) |
| R-10 | "91 measured at M2a" attribution false (no `ctest -N` in M2a evidence); 92-vs-91 delta unexplained anywhere | **ID-C2-10**: STATUS.md records: 91 measured at `71be5a5` (2026-10-10); reconciliation 92+1(smoke)−2(X)=91; 92 banned as anchor |
| R-11 | SC4 script — **NOW specified** (A-5 content list + 11 §7.5): file `windows-enablement/sc4-script.md`, fixture suggestion, commit-before-G-C2-2, budget the download. Remaining choice (exact model) delegated deliberately | **ID-C2-13** restates the content list + adds: generation prompt fixed in the script; transcript must show non-empty generated text, not `--help` |
| R-12 | K-ledger location/name/format/count — **specified** (`windows-enablement/K-LEDGER.md`, frozen `'^\| `'` pattern in header comment). No ambiguity. Binary-level KEX generator present for the exceptional path | **ID-C2-9** (mechanics restated) |
| R-13 | STATUS.md location/schema — **specified** (09 §4 L7: `windows-enablement/STATUS.md`, fields enumerated). No ambiguity | — |
| R-14 | `gen_compile_exposure.sh` — **exists and tracked** (verified); regeneration is real, not aspirational | — |
| R-15 | The 4 P8 tests — all **fully spec'd** (file names, fixtures, assertions, labels, registration, platform gating: §7.1-7.4 incl. `test_fp_model` volatile-operand canary + mutation acceptance; fingerprint constexpr membership table + runtime print; shim API surface enumerated against the real `platform.hpp` — `make_temp_dir(std::string&)` signature verified) | **ID-C2-6** (execution order) |
| R-16 | `grep -qE "error"` on build logs can false-positive (any line containing "error") | **ID-C2-2**: use `grep -qE "error:|FAILED:"` |
| R-17 | compat.hpp content list omits `sleep()` (used by row 13 in-file; census could expose more) and the mkdtemp buffer contract | **ID-C2-6/7**: compat.hpp adds `sleep`; mkdtemp shim documented as "template dir MUST be a writable Windows path — callers with `/tmp` literals are P-rewrites, not shim consumers" |

---

## 6. Implementation Directives (ID-C2-*) — OVERRIDE 09/10/11 on conflict

### ID-C2-1 · BLOCKING — SYNC-1 execution checklist (consolidates A-2a..A-2e; run in order)
**What/Where:** C2-WP1, before and during `git merge origin/master`. **Why:** B-2 closure depends on one executable checklist, not cross-references.
```bash
git status -sb                                   # clean; chroma_data/ untouched
git fetch origin --tags
N=$(git rev-list --count HEAD..origin/master)    # record N; >150/week → stop, re-evaluate per L6
git tag pre-sync/$(date +%F) && git push fork pre-sync/$(date +%F)
git merge origin/master || {
  # A-2a detection FIRST, before reading any content:
  git diff --name-only --diff-filter=U | tee /tmp/conflicts.txt
  git status --porcelain | grep -E '^(DD|AU|UD|UA|DU|AA|UU)'
  grep -E 'include/lse/core/platform\.hpp|src/core/platform_(posix|win)\.cpp|src/communication/poller_winsock\.cpp|include/lse/communication/winsock_compat\.hpp' /tmp/conflicts.txt \
    && { git merge --abort; echo "ABORT §3.6.1: seam file in conflict set"; exit 1; }
}
```
Resolve per 09 §3.3/§3.4 + A-2c (deletes/renames: honor upstream; prune ledgers; re-place reroutes at successor) + A-2e (new POSIX coupling: src/** → guarded reroute; tests/** → census worklist or X-ledger; every occurrence named in sync note). **Post-resolution, pre-commit:** A-2b count (fork-added unguarded lines in upstream-owned files, `git diff origin/master...HEAD` restricted per A-2b's exclusion list): total > 50 → `git merge --abort`, restore tag, escalate; 20–50 → proceed, record in STATUS.md. **A-2d greps (mandatory, grep-based not build-based):** `ffp-contract` + `NONSTDC_NO_DEPRECATE` in `cmake/LSECompilerFlags.cmake`; raw-dylib filter + `ws2_32 bcrypt userenv` fallback in `cmake/LSERust.cmake`; HRX force-OFF guard in `cmake/LSEOptions.cmake`. **Acceptance:** sync note in STATUS.md contains N, conflict list, A-2b count, A-2d grep results, bucket classification of every new upstream test.

### ID-C2-2 · BLOCKING — G-C2-0 normative command block (B-1-safe; supersedes 09 §6 G-C2-0 steps 2-4 and defines the Pass line)
**What/Where:** C2-WP1 after merge. **Why:** the gate that certifies the base for the whole cycle must be copy-paste executable and silent-failure-proof.
Step 0: `mkdir -p windows-enablement/logs` (R-7). Then run **11 §2.1 (R1), §2.2 (R2), §2.3 (R3), §2.4 (R4) verbatim**, with two corrections: (a) R1's error grep becomes `grep -qE "error:|FAILED:"` (R-16); (b) R4 = A-3 go/no-go recorded in STATUS.md **before** the gate is declared. **Pass line (overrides 09, which omits parity):** ALL of R1 (0 errors, HRX-off line, [N/N] recorded) + R2 (canary A/B/B′ green) + R3 (`|RE|` = NEXPOSURE−NEXCL+1 = 34 today; 100% AND `out of 34`) + R4 (Linux-parity configure+build green on merged SHA **or** recorded no-go invoking A-3.3) + STATUS.md pushed. **Acceptance:** `logs/c2-sync-targeted-ctest.txt` shows `100% tests passed, 0 tests failed out of 34` (or drifted M with sync-note delta); any single assertion red = gate not declared.

### ID-C2-3 · BLOCKING — L1 census must be complete: keep-going build
**What/Where:** C2-WP4 first act (11 §3.1). **Why:** Ninja stops at the first failed edge by default; a partial census violates "the census file *is* the work list" and undercounts WP12 scope.
```bash
set -uo pipefail    # NOT -e
cmake --build build-win -- -k 0 2>&1 | tee windows-enablement/logs/c2-wp12-census.txt
grep -c "error:" windows-enablement/logs/c2-wp12-census.txt                      # ≥ 14
grep -oE "tests/[a-z_0-9]+\.cpp" windows-enablement/logs/c2-wp12-census.txt | sort -u
```
Expect the 15 unistd files **plus possibly** `test_model_info_real.cpp` (registered non-ctest executable inside `if(TARGET lse_tokenizer)` — compiles under `all`, absent from the 21-row table; census absorbs it). Commit the census with the WP12a commit. **Acceptance:** distinct failing-file set ⊇ the 15; census file tracked in git; worklist derived from the file, never from grep alone.

### ID-C2-4 · BLOCKING — Interim CI: two-phase scope, label semantics, YAML mechanics
**What/Where:** `.github/workflows/windows.yml` (C2-WP6; optional phase-1 early landing per R-1). **Why:** 11 §6 as written cannot run green once (pwsh `$( )` expansion; `'\n'` artifact paths; missing inlined assertions) and its weekly/dispatch mode silently degenerates to a 1-test run (B-1 class).
(a) **Phase 1 (narrow, if landed early):** windows job = ID-C2-2's R1+R2+R3 blocks verbatim with `shell: bash`; **Phase 2 (C2-WP6, per 09):** tests-ON configure + `cmake --build build` + 11 §5.2-step-1/2 assertions inlined (W==`ci-expected-w.txt`; `100%` AND `out of N` parse). (b) **Label toggle:** PR/push → `--label-exclude LARGE_IO`; schedule/dispatch(`run_large_io=true`) → **empty flag** (full suite INCLUDING large-io). `--label-include LARGE_IO` is FORBIDDEN. Dispatch proof for G-C2-5 = full-suite log showing `test_large_file_io … Passed`. (c) All assertion steps `shell: bash`; all `upload-artifact` `path:` values are `|` block scalars. (d) linux-parity job per ID-C2-12. (e) cron check: `gh workflow view windows.yml --repo antmikinka/LSE --yaml | grep "17 6 \* \* 1"`. **Acceptance:** 1× green, both jobs, same SHA; dispatch run executes the full suite; no step relies on pwsh string interpolation.

### ID-C2-5 · MAJOR — FP-canary execution order + both trees
**What/Where:** every sync and gate. **Why:** the pin degrades silently; coverage must include the post-unlock targets.
Order: **A** (source grep) immediately after merge resolution, before configure → **B** (build.ninja FLAGS: VIOL==0, PINNED≥100) after each configure, before building tests → **B′** (`ninja -t commands` per-target) optional spot-check → at **G-C2-3 and G-C2-4 run A+B against BOTH `build-win-notests` and `build-win`** (parameterize the tree in 11 §2.2's block) → **C** (`test_fp_model`) from C2-WP5 on, with the one-time ID-12 mutation transcript (remove pin locally → canary red → restore) committed into the census log. **Acceptance:** canary results (both trees) recorded in STATUS.md at each gate.

### ID-C2-6 · MAJOR — WP12 execution order (census-first per L1)
**What/Where:** C2-WP4. **Why:** deterministic order prevents mid-WP improvisation.
1. ID-C2-3 census (committed) → 2. `tests/compat.hpp` + `tests/compat/unistd.h` + `tests/compat/sys/wait.h` (content per 11 §3.2 **plus** `sleep()`→`Sleep(s*1000)`; doc-comment every semantic delta per R8; mkdtemp contract note per ID-C2-7) → 3. wiring: one guarded `target_include_directories(... /compat)` inside `lse_add_test()` + one on raw `test_server_shutdown` → 4. build the 13 S-rows (zero file edits) → 5. G/P rows: POLL guards (`test_api_parity:33`, `test_hub:24`), `/tmp` rewrites (`test_graph:3155`, `test_dflash2:1610`, `test_tree_verify:703` per ID-C2-7, `test_model:312-328` per ID-8) → 6. H rows: `pipe_url()` (11 sites + `kUnimplemented` case), `test_server_shutdown` P2 per ID-C2-8 → 7. X-guard removal (by target name; merge dflash2's `else()` branch) → 8. `cmake --build build-win` exit 0 → 9. same-commit docs rewrite (L4) → 10. W re-census 93+U + `ci-expected-w.txt`. **Acceptance:** each step's output in the WP12a commit stack; census file is the worklist of record.

### ID-C2-7 · MAJOR — Port-table row 14 reclassified S→P (`test_tree_verify`)
**What/Where:** 11 §3.3 row 14. **Why:** `char dir[]="/tmp/lse-trials-XXXXXX"` (22 bytes) + in-place mkdtemp → any Windows temp path overflows the buffer; literal `/tmp` → fragile `C:\tmp`. Portable rewrite: `std::string`/`std::vector<char>` sized at runtime + `fs::temp_directory_path()` template (copy the `test_dflash2_convert.cpp:39-41` pattern); `::setenv` :705 stays compat-S. **Acceptance:** no fixed-size char buffer holds a generated temp path; row 14 passes green on Windows and POSIX unchanged in behavior (separator-tolerant if asserted).

### ID-C2-8 · MAJOR — `test_server_shutdown` P2 port: handle inheritance + anti-vacuous marker
**What/Where:** 11 §3.3 row 13. **Why:** CreateProcess ≠ fork for fd inheritance; a mis-wired marker pipe makes the "marker empty" assert pass vacuously (false-green in the shutdown-semantics test).
Mechanism: `_pipe` + `SECURITY_ATTRIBUTES{bInheritHandle=TRUE}` on the write handle; pass `_get_osfhandle(marker[1])` value in the `--child-stuck` argv; child rewraps via `_open_osfhandle`; parent closes its copy of the write end before waiting (else EOF never arrives). Wait via `WaitForSingleObject` + `GetExitCodeProcess` (expect 1). **Positive control (mandatory):** a `--child-clean` argv mode whose child DOES write the marker byte before exiting 0, asserted in the same test — proves the pipe plumbing works, so the stuck-child's empty read is meaningful. Cases 2-4 stay shared. SIGTERM remains undeliverable (PR7): assert exit-code-1 + empty marker, never signal delivery. **Acceptance:** both child modes pass; a deliberately broken inheritance (local mutation) makes the positive control fail.

### ID-C2-9 · MAJOR — K/X ledger mechanics (binding)
**What/Where:** C2-WP3 creates `windows-enablement/K-LEDGER.md`; consumed at G-C2-4. **Why:** M-4 fix + the skip-scrape visibility defect (R-5).
Row format exactly per 11 §4; header comment freezes the count pattern; `K=$(grep -cE '^\| `' windows-enablement/K-LEDGER.md || true); [ "$K" -le 5 ]`. Case-group entries = `#ifdef _WIN32 LSE_SKIP("windows: <capability|flake> — <delta> — #<issue> — review-by <YYYY-MM-DD ≤90d>")`; ledger key mirrors the skip string. **Skip-evidence:** the M3 run that feeds the scrape must be `ctest -V` (or a second `-V` run of only the K-binaries) tee'd to a committed log; assert distinct scraped `binary:case-group` == K. Binary-level K entries only via the KEX generator (11 §4) with `W_run == W − |K_bin|` asserted. X: stub `COMPILE_EXCLUSIONS.md` to `X = 0`, `grep -c "if(NOT WIN32)" tests/CMakeLists.txt` == 0, KNOWN_FAILURES.md zero Windows entries (`grep -in windows` exits 1). **Acceptance:** all five numbers (W, K, X, skip-distinct, guard-count) machine-derived in the gate script; KNOWN_FAILURES diff in bundle.

### ID-C2-10 · MAJOR — M3 numbers + `ci-expected-w.txt` lifecycle + W-anchor truth
**What/Where:** G-C2-3/G-C2-4/CI. **Why:** R-6/R-10 — the count file is consumed on every CI push and W changes mid-cycle.
Binding anchors (regenerated at each gate, never hand-carried): post-SYNC-1 91+U → G-C2-3 **93+U** → G-C2-4 **97+U**; main run `out of 96+U`; large-io `out of 1`, <30 s wall. `windows-enablement/logs/ci-expected-w.txt` is written from the `ctest -N` parse **in the same commit as every registration change** (G-C2-3 commit: 93+U; each P8 test commit: +1). STATUS.md records: "91 measured at `71be5a5` 2026-10-10 (not at M2a — m2a-build.txt holds no `ctest -N` census); reconciliation: 04's predicted 92 + 1 smoke − 2 X-guards = 91; 92 is a stale prediction, never an anchor." **Acceptance:** at G-C2-4, `W == WE == 97+U` and the file's git history shows one touch per registration commit.

### ID-C2-11 · MAJOR — M3 evidence package (committed at G-C2-4; all paths root `logs/` per R-9)
`logs/m3-ctest-windows.txt` (with `-V` skip visibility per ID-C2-9), `logs/m3-large-file-io.txt`, `logs/m3-ctest-N.txt`, `logs/m3-fingerprint-windows.txt`, `logs/m3-ctest-linux.txt` (or CI URLs + "M3-partial" per A-3.3), `logs/m2b-acceptance.md` (SC4, cross-referenced), `windows-enablement/K-LEDGER.md`, `windows-enablement/COMPILE_EXCLUSIONS.md` (X=0 stub), `windows-enablement/logs/c2-wp12-census.txt`, `windows-enablement/logs/c2-w-census-g3.txt`, `ci-expected-w.txt`, KNOWN_FAILURES diff, HASH-EQ comparison vs linux-parity artifacts + TOLERANCE `max_rel` table, `tests/data/reference.safetensors` SHA256 (goldens materialized — else bundle stamped **INCOMPLETE**), STATUS.md W/K/X + gate table, `windows-enablement/sc4-script.md`. Every cited file `git ls-files`-tracked (L5). **Acceptance:** the bundle checklist re-run at the gate; M3 declared only with these links.

### ID-C2-12 · MAJOR — linux-parity job scope (A-3/A-4 encoded)
ubuntu-latest + apt.llvm.org **clang++-21** (same LLVM major as the windows job = T3 pair) + `-DLSE_ENABLE_HRX=OFF -DLSE_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off` (CI-level pin only, SC6); full ctest under the ID-C2-4 label toggle; `./build/tests/test_registration_smoke` run + `smoke-linux.txt` artifact (ID-11 baseline, m-6); header comment states the C2-4 scope limit (no ROCm/GPU; upstream `ci.yml` remains the authoritative GPU/GCC gate). Under A-3 no-go this job **hoists ahead of G-C2-4** (split from C2-WP6) so the parity evidence source exists when M3 needs it; M3 is never declared without same-SHA parity evidence. **Acceptance:** parity green (or recorded M3-partial + user decision) on the M3 SHA; artifacts downloadable.

### ID-C2-13 · MAJOR — SC4 script (A-5, binding content list)
First act of C2-WP3: commit `windows-enablement/sc4-script.md` containing: model ID (default suggestion `mlx-community/Qwen3.5-0.8B-4bit` per docs/WINDOWS.md §5, or a smaller fixture — choice recorded), exact `lse.exe` pull command, a **fixed prompt** generation invocation with pass criteria (exit 0 + non-empty generated text in transcript — `--help`/`--version` do NOT satisfy SC4), `lse-server.exe` launch + exact HTTP request/response transcript (tool, port, endpoint), and the network/disk budget line. G-C2-2 runs the script's commands **verbatim**; transcripts land in `logs/m2b-acceptance.md`. **Acceptance:** `git ls-files windows-enablement/sc4-script.md` non-empty BEFORE the sentinel run; transcript shows generated tokens.

### ID-C2-14 · MINOR — Doc fixes D-1/D-2/D-3 + toolchain-pin truth
**DOC-1 (D-1):** `BUILD_FROM_SOURCE.md` §Windows snippet gains `-DLSE_BUILD_TESTS=OFF` + "targeted tests only until WP12; canonical guide: docs/WINDOWS.md" — **and drops the `$env:CC = "clang-cl"` line** (`:50`, A-7/m-4; project is `LANGUAGES CXX` only). **DOC-2 (D-2):** commit `logs/m2a-configure.txt` (regenerate via the tests-OFF configure if the original `build-win-notests-configure.log` text — present untracked at repo root today — is unusable; label regenerated + date/SHA); re-point `docs/WINDOWS.md:124`. **D-3:** record-only — 06 stays frozen; STATUS.md carries live state (L7). **A-4:** at C2-WP6, add the CI pin row (`winget LLVM.LLVM 21.1.8 --exact`) to `docs/WINDOWS.md` §1 next to the local 21.0.0git host; STATUS.md records both patch levels. **Acceptance:** the four G-C2-1 greps (09 §6) flip green + `grep -c 'env:CC' BUILD_FROM_SOURCE.md` == 0.

**Directive tally: 14 — BLOCKING 4 (ID-C2-1..4), MAJOR 9 (ID-C2-5..13), MINOR 1 (ID-C2-14).** These directives OVERRIDE 09/10/11 wherever they conflict; 01–11 remain immutable.

---

## 7. Scores & Verdict

### 7.1 Component scores

| Artifact | Score | Basis |
|---|---|---|
| **09-cycle2-program-plan.md** | **7/10** (10's score, re-affirmed; immutable) | Scope/excellent, evidence/strong, gates mixed (B-1), doctrine incomplete (B-2) — all corrected by the amendment layer |
| **11-cycle2-test-plan.md** | **8/10** | Every re-derivable claim verified EXACT to the unit (91, 35−2+1=34, 168/166/2 with correct linker attribution, 17→15→14+1 census, all file:line refs, LSERust pinned-SHA auto-clone, harness skip mechanics, `lse_add_test` wiring feasibility, httplib 0.18.3 enabling row 2, `test_dflash2_convert`'s already-portable mkdtemp pattern). It correctly catches 04 §P1's impossible zero-touch claim and correctly fixes B-1/M-4 arithmetic. Loses points for: the CI-YAML layer (label-include degeneration = B-1 class recurring; pwsh/`$()` and `'\n'` mechanics; narrow-phase absence), the census keep-going omission, row-14's unsound S, the skip-scrape visibility gap, and two lifecycle ambiguities (`ci-expected-w.txt`, "measured at M2a" attribution) |
| **10-quality-review-3.md** | (process) | Both BLOCKING and all four MAJOR findings confirmed real by independent re-derivation; zero false positives; A-1..A-7 all adopted-or-carried. The amendment layer held |

### 7.2 CYCLE-2 CHAIN score: **8 / 10**

09 (sound plan, defective gate text) → 10 (caught exactly the right defects, binding fixes) → 11 (near-flawless local re-derivation, weaker CI-layer execution). The chain self-corrected every cycle-1 failure class it claimed to (L1-L8 operationalized; B-1's degenerate-regex class genuinely killed by the `out of N` parse) — and this review found the same class recurring once more, in the weekly-CI label toggle, which is precisely why second-pass review exists.

### 7.3 Verdict: **READY-WITH-DIRECTIVES**

Stage 5 may execute C2-WP1..C2-WP6 with: **09 as amended by 10 §8 (A-1..A-7), as operationally specified by 11 §2-§8, as OVERRIDDEN by 12 §6 (ID-C2-1..14).** Hard order constraints carried forward and re-affirmed: ID-C2-1 + ID-C2-2 in hand before `git merge`; A-3 go/no-go before G-C2-0 is declared; ID-C2-13 script before G-C2-2; ID-C2-3 census before any porting; ID-C2-4 semantics before the workflow's first push. No gate is declared on a command not run verbatim (L2). Escalation unchanged: any abort/gate miss → options + recommendation in STATUS.md → user decision, never silent.

*End of quality review 4. No source or prior-artifact files were modified; nothing committed. All verification performed against the working tree at `71be5a5`, 2026-10-10.*
