# 10 — Quality Review 3 (Stage 2, cycle 2 · quality review of `09-cycle2-program-plan.md`)

**Artifact under review:** `windows-enablement/09-cycle2-program-plan.md` (untracked working copy, reviewed as written; 09 remains immutable — all corrections are issued as Amendments in §8).
**Branch:** `feature/windows-enablement` @ `71be5a5` = upstream `a5b82f6` (v0.6.0-28, `git describe` re-verified) + 10 commits. Remotes re-verified: `origin` = `https://github.com/Geramy/LSE.git`, `fork` = `https://github.com/antmikinka/LSE.git`. `git rev-list --count HEAD..origin/master` = **0** at review time.
**Reviewer inputs:** 01–08 (04 §5–§7 and 05 §7 ID-layer treated as the binding correction substrate), `docs/WINDOWS.md`, `BUILD_FROM_SOURCE.md`, `tests/CMakeLists.txt`, `tests/KNOWN_FAILURES.md`, `windows-enablement/COMPILE_EXPOSURE.txt` (35 lines, `test_attention_short` present), `windows-enablement/COMPILE_EXCLUSIONS.md` (X=2), `.gitignore`, `logs/` (contains only `m2a-build.txt` — tracked), direct source greps, and `git diff a5b82f6..HEAD --stat` for the cmake/ delta surface.
**Constraints honored:** no source or prior artifact modified; nothing committed.

---

## 1. Review Method

1. **Full read of 09**, then full re-reads of 08 (backlog/D-flags/lessons/DPs), 04 (buckets, CI design, M3 exit criteria), 05 (ID-1..ID-14), 06 (proven commands, deviations) — the four references 09's claims must reconcile with.
2. **Independent repo re-verification of every checkable claim in 09** (~20 discrete checks, results in §6): unistd census, exposure/exclusion ledgers, CMake guard positions and counts, doc citations, `.gitignore`, executable existence, remote URLs, commit topology, P4/P5 file:line refs, fork-test content.
3. **Gate-by-gate implementability walk-through** (the 05 §1.5 method that caught BL-3): for each G-C2-0..5 command, asked "run verbatim in Git Bash on this tree — does it succeed, fail, or fail *silently in the wrong direction*?" This found the false-green defect in G-C2-0 (§3, B-1).
4. **SYNC-1 doctrine adversarial stress-test** against the five scenarios the merge can actually produce (§4), including a `git diff --stat` audit of which fork-modified files the §3.4 hotspot table covers — it does not cover `cmake/LSECompilerFlags.cmake`, `cmake/LSERust.cmake`, `cmake/LSEOptions.cmake` (all fork-modified upstream files).
5. **Sequencing/dependency analysis** (§5): WP3∥WP4 feasibility checked against the actual registration guard for `test_tokenizer` and the unistd grep; the C2-7 descope path traced against gate order; GitHub runner semantics for mixed-OS workflows.
6. **Completeness matrix** built from 08 §4 (items 1–11), 08 §1.3 (D-1..D-5), 08 §2.3 (L1..L8), and the 05 §7 IDs still binding at M3 (§2).
7. Severity discipline: BLOCKING = would corrupt a gate result or leave the first WP ungoverned; MAJOR = criterion unmeasurable/false as written, or a dependency cycle that can stall the cycle; MINOR = wording/staleness that the census or a diff-check absorbs.

---

## 2. Completeness Matrix

### 2.1 08 §4 backlog coverage

| 08 backlog item | Where in 09 | Verdict |
|---|---|---|
| 1 SYNC-1 + first STATUS.md | C2-WP1, §3 protocol, G-C2-0, SC-C2-1/9 | **COVERED** (gate command defect → B-1; doctrine gaps → B-2) |
| 2 DOC-1 (D-1) | C2-WP2 bullet 1, G-C2-1 check 3 | **COVERED** (verified D-1 real: `BUILD_FROM_SOURCE.md:49-55` snippet has no `-DLSE_BUILD_TESTS=OFF`; snippet-quality note m-4) |
| 3 DOC-2 (D-2) | C2-WP2 bullet 2, G-C2-1 checks 1–2 | **COVERED** (verified D-2 real: `docs/WINDOWS.md:124` cites `build-win-notests-configure.log`; `.gitignore:23` = `*.log`; file absent from `logs/` and from git) |
| 4 WP11/M2b sentinel + SC4 + K-ledger | C2-WP3, G-C2-2, SC-C2-4 | **COVERED** (SC4 script undefined → M-3) |
| 5 WP12a unlock (probe → compat.hpp → 15 files + fork test → X=0 → `all`) | C2-WP4 steps 1–7, G-C2-3, SC-C2-5 | **COVERED** (double-count wording m-1; hardcoded exclusion pair m-3) |
| 6 WP13a interim CI | C2-WP6, G-C2-5, SC-C2-8 | **COVERED** (false doc-pin claim M-2; cron check unmeasurable m-2) |
| 7 M3 tail (P2–P5/P7/P8, ID-5 partition, >2 GB test, goldens, parity, ID-11, bundle) | C2-WP5, G-C2-4, SC-C2-6/7 | **COVERED** (parity-evidence circularity M-1; ID-11-in-CI artifact gap m-6) |
| 8 M4 3×, 9/10 PR batches, 11 WP15 | §1.2 out-of-scope, DP2=NO consequence recorded | **CORRECTLY EXCLUDED** per user decision; R1′ honestly re-scored H/H |

### 2.2 Drift-flag closure (08 §1.3)

| Flag | Closed by 09? | Evidence |
|---|---|---|
| D-1 | **YES** — C2-WP2/DOC-1 now (DP6=a honored); L4 rationale ("true now") correct | G-C2-1 grep measurable, currently 0 hits |
| D-2 | **YES** — DOC-2: commit `logs/m2a-configure.txt` (.txt is ignore-proof), re-point link, `git ls-files` acceptance | G-C2-1 checks 1–2 measurable, currently red |
| D-3 | **YES (record-only, per 08 disposition)** — L7 STATUS.md mechanism; 06 stays frozen | §4 L7 row |
| D-4 | **YES** — L6 re-measurement at every sync, >150/wk re-evaluation trigger; §3.2 cadence rule | G-C2-0 step 1 records N |
| D-5 | **YES** — §3.4 adopts 08 §3.2's evidence-derived hotspot table (incl. `tests/CMakeLists.txt`, `scheduler.cpp`, `api/lse.cpp`, `server_main.cpp`; no `hrx_backend.cpp`) | **Gap:** table omits the fork-modified `cmake/*.cmake` modules → B-2(d) |

### 2.3 Cycle-1 lessons operationalized (visible, enforceable mechanisms)

| Lesson | 09 mechanism | Verdict |
|---|---|---|
| L1 compiler-as-census | C2-WP4 step 1 probe is the WP's first act; census file *is* the work list; extended grep pattern set matches 08's list exactly (`fsync|fdatasync|mallinfo|aligned_alloc|pipe|dup2|fnmatch|isatty|unistd|sys/wait`) | **OPERATIONALIZED** |
| L2 gate rehearsal | PROVEN/SPEC-NOW/EXPECT-FAIL tags on all 9 SCs and all 6 gates; EXPECT-FAIL used honestly for the tests-ON probe | **OPERATIONALIZED — but tag integrity broken in one place:** G-C2-0 step 3 carries a PROVEN tag on a command that is *not* the proven command and fails silently (B-1) |
| L4 docs land when true | DOC-1 now; WP4 step 7 same-commit doc rewrite; G-C2-3 greps the docs in-gate | **OPERATIONALIZED** |
| L5 cited evidence in git | DOC-2; "no artifact may cite an untracked file" review rule; all cycle-2 evidence paths committed | **OPERATIONALIZED** (note: 09 itself complies — every file it cites exists in git except the pseudo-file in B-1) |
| L3/L6/L7/L8 | §4 rows: machine-derived counts, churn measurement, STATUS.md schema, physics line | **OPERATIONALIZED** |

### 2.4 Binding ID directives at M3

| ID | Present in 09? | Where / caveat |
|---|---|---|
| ID-3 (targeted gate, never `all` pre-WP12) | **YES** | G-C2-0 step 3 targeted build; C2-WP4 step 1 probe is the sanctioned first `all` attempt — **but the step-3 command text is the B-1 defect** |
| ID-4 (tokenizer guards) | **YES** | §3.4 tests/CMakeLists row ("re-apply … :129/:130/:135/:196-equivalents"). Verified current reality: one guard block at `tests/CMakeLists.txt:137` wrapping `test_server`/`test_chat_protocol`/`test_api_parity` + one at `:220` (`test_memory_lifecycle`) + `:238` (`test_tokenizer`) = 3 blocks / 4 protected registrations. Stale line refs → m-5 |
| ID-5 (fingerprint partition) | **YES** | C2-WP5 `test_numerics_fingerprint` bullet is a verbatim-faithful partition (HASH-EQ transcendental-free only; TOLERANCE goldens; membership printed); L8 row forbids cross-OS bit-exact transcendental hashes "ever" |
| ID-6 (W/K/X units) | **YES** | SC-C2-6 reports the three numbers separately; G-C2-4 arithmetic; K reason format exact |
| ID-8 (HOME policy) | **YES** | C2-WP5 P5 bullet cites ID-8 explicitly |
| ID-10 (YAML fixes) | **YES** | C2-WP6 (a)(b)(c); SC-C2-8 dispatch proof |
| ID-11 (smoke census, K-accounting) | **YES** | G-C2-0 includes `test_registration_smoke`; C2-WP5 "ID-11 Linux census baseline captured in the same parity run" — **artifact gap under the C2-7 descope → m-6** |
| ID-12 (anti-folding canary) | **YES** | C2-WP5 `test_fp_model` bullet |
| ID-13 (clang++-21 mandatory from M3) | **YES** | SC-C2-7, G-C2-4 parity block, C2-7 risk row — **sequencing circularity → M-1** |

**Completeness verdict: COMPLETE.** No backlog item, drift flag, lesson, or binding ID is missing from 09. All defects found are in *gate-command text*, *doctrine coverage*, and *sequencing of evidence sources* — not in scope.

---

## 3. Gate Audit Table

"Measurable" = a third party can compute pass/fail from the stated command output alone. "Currently" = state of the tree at review time.

| Gate | Command-by-command audit | PROVEN tag justified? | Measurable? | Findings |
|---|---|---|---|---|
| **G-C2-0** | (1) sync ritual: valid; `rev-list` verified = 0 today. (2) tests-OFF configure+build: **byte-identical to the cycle-1 proven command** (`docs/WINDOWS.md` §3; evidence `logs/m2a-build.txt`). (3) targeted set: `bash windows-enablement/gen_compile_exposure.sh` exists ✓ — **but the `RE=` pipeline greps `windows-enablement/COMPILE_EXCLUSIONS-driven-list`, a file that does not exist.** The proven command (`docs/WINDOWS.md:190-195`) greps `COMPILE_EXPOSURE.txt`. (4) Linux-parity configure+build: listed as a command but **absent from the Pass line** (see M-1). (5) STATUS.md + push: fine. | Step 2: **YES**. Step 3: **NO — mislabeled.** The command as written was never run in cycle 1 and cannot run now. | Step 3 as written is *worse than unmeasurable*: grep fails (missing file), `RE` collapses to `test_registration_smoke`, the targeted build succeeds, `ctest -R "^(test_registration_smoke)$"` passes **1/1 = "100%"** → **silent false green at the gate that certifies the base for the entire cycle.** | **B-1 (BLOCKING)**, m-3 (hardcoded exclusion pair violates 09's own L3), M-1 (Pass-line/command mismatch) |
| **G-C2-1** | `git ls-files logs/m2a-configure.txt` — currently empty (red, correct pre-WP2). `grep build-win-notests-configure.log docs/WINDOWS.md` — currently hits `:124` (red, correct). `grep LSE_BUILD_TESTS=OFF BUILD_FROM_SOURCE.md` — currently 0 hits (red, correct). Snippet re-run: transcript requirement stated. | SPEC-NOW correctly tagged. | **YES**, all three binary. | Sound. m-4: the DOC-1 snippet fix must also drop `$env:CC="clang-cl"` (`BUILD_FROM_SOURCE.md:50`) — the project is `LANGUAGES CXX` only and `docs/WINDOWS.md` §3 warns passing a C compiler produces "Manually-specified variables were not used"; a verbatim-snippet-green claim with that warning contradicts the canonical guide. |
| **G-C2-2** | `cmake --build build-win --target test_tokenizer`: registration verified under `if(TARGET lse_tokenizer)` (`tests/CMakeLists.txt:238-240`); `test_tokenizer.cpp` is unistd-free (not in the 17-file grep) → builds pre-compat.hpp ✓. `ctest -R "^test_tokenizer$"` ✓. SC4: **`<generation invocation per SC4 script>` references a script that exists nowhere in the repo or artifacts**; 02's SC4 (:40) is the weaker `--version`/list-backends definition — 09/08 correctly tighten it to real generation, but real generation needs a model checkpoint (download, disk, network: unbudgeted, unspecified). `ls K-LEDGER.md` ✓. Executables verified present (`build-win-notests/lse.exe`, `lse-server.exe`). | SPEC-NOW correctly tagged. | SC4 clause **not executable as written** (placeholder). | **M-3 (MAJOR)** |
| **G-C2-3** | Probe command valid (Ninja `all` build; failure is the deliverable — honest EXPECT-FAIL). Post-port: reconfigure + `cmake --build build-win` exit 0 ✓ measurable. `grep -c "if(NOT WIN32)" tests/CMakeLists.txt` — currently exactly 2 (`:104`, `:195`, both X-ledger ✓); **pass expectation fuzzy** ("only non-X guards may remain; diff-verified") with no number. `grep -n "X = 0" COMPILE_EXCLUSIONS.md` — currently no hit (ledger says `**X = 2**`) ✓ measurable. `ctest -N` census diff ✓. `grep "will fail" docs/WINDOWS.md` — currently hits §4 (`:165`) ✓. | EXPECT-FAIL/SPEC-NOW correctly tagged. | Mostly yes; the guard-count check needs an expected value (0, or a named exception list in STATUS.md). | Amendment A-6. |
| **G-C2-4** | Main ctest flags valid (`--timeout 300`, `--label-exclude LARGE_IO`) ✓. Large-IO local run ✓ mandatory per S4 §3. `ctest -N` final W ✓. Parity block: **evidence source can be unreachable as sequenced → M-1**. `grep -c "^| " K-LEDGER.md` for K ≤ 5: **counts the markdown header row** (`| binary:case-group | reason | …` starts with `| `; only `|---` separators are excluded) → true K=5 reads as ≥6, K=0 with a header reads as 1. Criterion arithmetically wrong as written. `grep -in windows KNOWN_FAILURES.md` — verified currently **zero hits** ✓ measurable. Bundle contents enumerated ✓. | SPEC-NOW correctly tagged. | K-count **NO**; rest yes. | **M-4 (MAJOR)**, M-1, m-6 |
| **G-C2-5** | `gh workflow run`/`gh run watch`/`gh run list` ✓ valid. Dispatch proof `-f run_large_io=true` ✓ (assuming ID-10(a) fix landed). **`gh api repos/antmikinka/LSE/actions/workflows/windows.yml` does not return schedule/cron data** (workflow metadata only: name/path/state/URLs) → "weekly schedule cron present" is unmeasurable with this command; use `gh workflow view windows.yml --yaml` + grep, or grep the committed YAML. `grep -n "21.1.8" .github/workflows/windows.yml # toolchain pin == docs/WINDOWS.md §1` — the grep is measurable, but **the comment is false**: `docs/WINDOWS.md:30` pins only the LLVM **21 major** ("verification host: 21.0.0git … any LLVM 21.x works"); "21.1.8" appears nowhere in docs/WINDOWS.md (it originates in S4 §5.1's YAML, and — confusingly — in `BUILD_FROM_SOURCE.md`'s *macOS* row). | SPEC-NOW correctly tagged. | Cron clause **NO**; pin clause yes-but-mislabeled. | **M-2 (MAJOR)**, m-2 |

---

## 4. SYNC-1 Doctrine Stress-Test + Amendments

### 4.1 Scenario walk-through

| # | Scenario | Covered by 09 §3.3–3.6? | Analysis |
|---|---|---|---|
| (a) | **Abort-criterion detectability** — "seam files appear in the conflict set" | **PARTIALLY.** No detection command given. | The five seam files are fork-only *new* files; they can enter the conflict set only via add/add (upstream created same-path files — exactly the "upstream shipped its own Windows port" case §3.6.1 fears, so the criterion is coherent). Detection command must be stated: `git diff --name-only --diff-filter=U` plus `git status --porcelain` conflict codes (`AA/DU/UD/AU/UA/DD`). → **A-2a** |
| (b) | **>~50-unguarded-edits threshold** (§3.6.2) | **NOT OPERATIONAL.** No definition of "unguarded", no measurement command, no file scope. | Unmeasurable abort criteria are worse than none: under merge pressure the implementer will self-judge. → **A-2b** (formula + command) |
| (c) | **Upstream DELETES (or renames) a file we ported/guarded** (e.g. `test_dflash2.cpp`, `jit_cache.cpp`) | **NOT COVERED.** §3.3 assumes conflicts are "upstream moved/rewrote the region" — deletion produces a modify/delete conflict, which has no "take upstream wholesale + re-place guard" resolution. | Undefined behavior at the exact moment doctrine is most needed. Correct disposition: honor the deletion (upstream intent wins); if the file was X-ledgered or exposure-listed, delete its ledger/exposure entries and re-run `gen_compile_exposure.sh`; if it carried seam reroutes, locate the successor (`git log --follow`, upstream rename detection) and re-route there; if no successor, record in STATUS.md + fold the lost coverage into the next census. Renames must be tracked explicitly (`git status` R-records / `git diff -M`). → **A-2c** |
| (d) | **Upstream touches `cmake/LSECompilerFlags.cmake`, `cmake/LSERust.cmake`, `cmake/LSEOptions.cmake`** | **NOT COVERED by §3.4's hotspot table** — verified these are fork-**modified upstream files** (`git diff a5b82f6..HEAD --stat -- cmake/`: LSECompilerFlags +89/-…, LSERust +50, LSEOptions +8; only `LSELink.cmake` is fork-new/zero-conflict, added in `9ce9287`). §3.6.3 protects *one* clause of LSEOptions (HRX force-OFF) and nothing else. | These files carry: the Windows compiler-ID flag branch incl. `/clang:-ffp-contract=off` (R7/ID-14 — silently dropping it degrades numerics with **no compile error**), `_CRT_NONSTDC_NO_DEPRECATE`/`_CRT_SECURE_NO_WARNINGS`, the raw-dylib `^windows\.[0-9]` filter + fallback lib set `ws2_32 bcrypt userenv` (N4), and the C++26 dialect override keying. Post-merge verification must be grep + configure-summary based, not build-success based. → **A-2d** (new §3.4 rows + mandatory post-merge greps) |
| (e) | **Upstream adds NEW POSIX-only code to files we never touched** (new coupling appearing post-merge) | **NOT COVERED.** §3.5's tests-OFF regression catches *src-side* breakage but gives no disposition (abort? guard? census item?); *tests-side* new POSIX code escapes G-C2-0 entirely (the targeted-34 never compiles it) and surfaces only at C2-WP4's probe — silently inflating that WP without a traceability link back to the sync. | → **A-2e**: classify-and-route rule (src/** → guarded seam reroute per §3.3, counted toward the PR1 trigger; tests/** → appended to the committed census worklist or X-ledgered; every occurrence named in the sync note). |
| (f) | Upstream refactors a function we `#ifdef`'d inside | **COVERED** — §3.3's re-place rule handles it. | No amendment. |
| (g) | Upstream deletes a test in `COMPILE_EXPOSURE.txt` / adds HRX-tainted tests | Additions **COVERED** (§3.4 tests row + C2-2); deletions **NOT** explicitly. | Folded into A-2c (regenerate exposure list, prune ledgers). |

### 4.2 Verdict

The general rule (§3.3) and abort criteria (§3.6.1/3/4) are sound in spirit; the doctrine is **incomplete in four of seven stress scenarios and non-operational in a fifth**. Because C2-WP1 executes first and gates all other work under a self-declared H/H risk (R1′), this is **BLOCKING (B-2)** — the amendment clauses A-2a..A-2e below are normative additions to §3 and must be adopted by the implementer before touching `git merge`.

---

## 5. Sequencing Findings

1. **C2-WP3 ∥ C2-WP4 steps 1/2 — SOUND (verified).** `test_tokenizer.cpp` does not include `<unistd.h>`/`<sys/wait.h>` (absent from the 17-file grep), its registration is guarded by `if(TARGET lse_tokenizer)` (`tests/CMakeLists.txt:238-240`) which is satisfied (Rust FFI links since `96c32c4`), and Ninja per-target builds never touch the 15 broken files — so the sentinel does **not** depend on `compat.hpp`. Operational note for the implementer: both WPs share the `build-win` tree; WP4's intentionally-failing `all` probe and WP3's targeted builds interleave safely under Ninja but will look alarming — record the transient-red state in STATUS.md rather than "fixing" it.
2. **C2-WP6-last vs 04's CI design — ACCEPTABLE, deviation honestly recorded.** 04 §5 mandates no landing order; 08 DP3 wanted CI-now; 09 bounds the unmonitored window with weekly merges + the full M2a regression re-run at every sync and says so (R3′ row). No violation. Recommendation (non-binding): *author* `windows.yml` during C2-WP1 and land the file early with the narrow tests-OFF gate — push-triggered runs and the weekly R3 alarm then cover WP3–WP5 itself; only the tests-ON widening waits for C2-WP5. (A workflow only fires on pushes made after it exists on the branch.)
3. **C2-7 descope circularity — MAJOR DEFECT (M-1).** The Linux-parity evidence fallback ("take parity + ID-11 census from the C2-WP6 `linux-parity` CI job") is **unreachable when needed**: G-C2-0 step 4 and G-C2-4's parity clause both fire *before* C2-WP6 lands by design. The CI substitution itself is technically sound — a `linux-parity` job on `ubuntu-latest` inside the same workflow is a separate job on a separate runner; the windows-latest-can't-run-Linux limitation is irrelevant to it — but only if the job *exists*. Compounding this, **G-C2-0's Pass line silently omits step 4** while §3.5 calls Linux parity "mandatory" — an internal contradiction that invites dropping the clause without a recorded decision (exactly the "never silent" rule 09 itself states in §7). Fix per A-3: WSL clang++-21 provisioning becomes a hard C2-WP1 task with an explicit go/no-go in STATUS.md; on no-go, either hoist the linux-parity job alone ahead of G-C2-4 (split C2-WP6) or defer the parity clause to G-C2-5 with an "M3-partial" declaration and a user decision — and G-C2-0's Pass line must be reconciled with its command list either way.
4. **Critical path C2-WP1 → WP4 → WP5 → (G-C2-4=M3) → WP6 (G-C2-5)** is consistent with 08's critical path (1→4→5→7→8 truncated at the user's M3 horizon). The WP3-soft-dep rationale (K-accounting order before WP12 consumes K budget) matches 08 item 5's dep note. No cycle found other than M-1.
5. **Same-SHA discipline** for SC-C2-7/8 (both jobs green on identical SHA) mirrors S4 §5.2 correctly.

---

## 6. Spot-Check Results

| # | Claim in 09 | Verdict | Actual (re-derived this review) |
|---|---|---|---|
| 1 | Header: `71be5a5` = `a5b82f6` (v0.6.0-28) + 10 commits `9ce9287..71be5a5`; remotes origin/fork as stated; pushed | **EXACT** | `git log`: 10 commits listed match; `git describe --tags a5b82f6` = `v0.6.0-28-ga5b82f6`; `git remote -v` = Geramy/LSE + antmikinka/LSE; `rev-list --count HEAD..origin/master` = 0 |
| 2 | "15 `<unistd.h>` files" + compat.hpp "fixes 14 of 15" | **COUNT EXACT / wording flaw** | grep = 17 files total; minus HRX-gated `test_jit.cpp`, `test_capture_prefill.cpp` = **15** ✓. The 15th that compat.hpp cannot fix is `test_server_shutdown.cpp` — it is *inside* the 15, not additional to it (own `main`, not harness-based, so the `harness.hpp` include never reaches it). "15 files + fork test" (C2-WP4 step 3) double-counts → m-1 |
| 3 | Fork test identity/semantics: `test_server_shutdown` fork/waitpid → CreateProcess self-relaunch + GetExitCodeProcess; SIGTERM undeliverable (PR7) | **EXACT** | `tests/test_server_shutdown.cpp:24-37`: `pipe`/`fork`/`waitpid`/`WEXITSTATUS(status)==1`; registered ungated at `tests/CMakeLists.txt:168-174`; matches S4 §6 P2 (argv `--child-stuck` self-relaunch design) |
| 4 | exposure = 35, script-generated; X = 2 (`test_dflash2`, `test_loom_cache`) via two `if(NOT WIN32)` guards | **EXACT** | `wc -l COMPILE_EXPOSURE.txt` = 35 (incl. `test_attention_short`); ledger states X=2 with review-by 2027-01-08; `grep -n "if(NOT WIN32)" tests/CMakeLists.txt` = exactly `:104`, `:195`, both X-ledger |
| 5 | G-C2-0 step-2 configure/build commands are the PROVEN cycle-1 commands | **EXACT** | Byte-identical to `docs/WINDOWS.md` §3 (`:89-94`); evidence `logs/m2a-build.txt` tracked (`git ls-files logs/`) |
| 6 | G-C2-0 step-3 targeted-set command (PROVEN) | **FALSE — the file `windows-enablement/COMPILE_EXCLUSIONS-driven-list` does not exist**; the proven command (`docs/WINDOWS.md:190-195`) greps `COMPILE_EXPOSURE.txt` | → **B-1** |
| 7 | "winget LLVM 21.1.8 exact pin **matching docs/WINDOWS.md §1**" (C2-WP6) and G-C2-5 comment "toolchain pin == docs/WINDOWS.md §1" | **FALSE** | `docs/WINDOWS.md:30`: "LLVM **21** (verification host: 21.0.0git from AMD ROCm 7.1; **any LLVM 21.x works**)" + unversioned `winget install LLVM.LLVM`. "21.1.8" appears only in S4 §5.1's draft YAML (and `BUILD_FROM_SOURCE.md:7`'s *macOS* row). → **M-2** |
| 8 | D-2 artifacts: `docs/WINDOWS.md` §3 cites `build-win-notests-configure.log`; file gitignored, never committed | **EXACT** | cite at `:124`; `.gitignore:23` = `*.log`; `logs/` holds only `m2a-build.txt`; `git ls-files logs/m2a-configure.txt` empty |
| 9 | D-1: `BUILD_FROM_SOURCE.md` §Windows snippet lacks `-DLSE_BUILD_TESTS=OFF` and would fail verbatim | **EXACT** | `:49-55`: `$env:CC/CXX` + plain `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` + `cmake --build build` (tests default ON per `cmake/LSEOptions.cmake:3` → hits the 15 unistd files). Bonus finding: the `$env:CC` line itself contradicts docs §3 ("do not pass a C compiler") → m-4 |
| 10 | C2-WP5 P4/P5 file:line refs (`test_api_parity.cpp:33`, `test_hub.cpp:24`; `test_graph.cpp:3155`, `test_dflash2.cpp:1610`, `test_tree_verify.cpp:703`, `test_model.cpp:312-328`) | **EXACT** | `CPPHTTPLIB_USE_POLL` at precisely `:33`/`:24` (plus `test_server_shutdown.cpp:2`, `http_server.cpp:17`); `/tmp/lse-cold-spans-` at `test_graph.cpp:3155`; `/private/tmp/lse-dflash2-split-ring-` at `test_dflash2.cpp:1610` — 05 §6 rows 1-4/13 re-confirmed on the current tree |
| 11 | "both `if(NOT WIN32)` ledger guards removed from `tests/CMakeLists.txt`" as SC-C2-5 criterion | **EXACT & measurable** | currently 2, both X-ledger; note `test_dflash2`'s guard has an `else()` branch preserving `test_feature_taps` ENVIRONMENT (per COMPILE_EXCLUSIONS.md) — guard removal must merge that branch, not delete it |
| 12 | `grep -in windows KNOWN_FAILURES.md` → zero Windows entries (ID-6) | **EXACT (currently green)** | grep exits 1 (no hits) — the invariant 09 wants to *keep*, correctly chosen as a gate check |
| 13 | Weekly cron `17 6 * * 1` matches S4; ID-10(a) dead expression `${{ inputs.run_large_io && '' || '' }}` correctly identified for replacement | **EXACT** | S4 §5.1 `:155`/`:178` |
| 14 | SC4 executables exist to rehearse against | **EXACT** | `build-win-notests/lse.exe`, `lse-server.exe` present on disk |
| 15 | §3.4 ID-4 refs ":129/:130/:135/:196-equivalents" | **STALE-BUT-SAVED** | current guard positions `:137` (block of 3), `:220`, `:238`; "-equivalents" qualifier prevents error → m-5 |

**Tally: 15 checks, 12 exact, 1 stale-but-hedged, 2 materially false (#6 → B-1, #7 → M-2).** 09's evidence discipline is otherwise the strongest yet in the chain — every P4/P5/ledger/count claim re-verified clean.

---

## 7. Issue Register

### BLOCKING (2)

| # | Issue | Where | Corrective instruction |
|---|---|---|---|
| **B-1** | **G-C2-0 step 3 is a silent-false-green gate command, mislabeled PROVEN.** It greps a nonexistent pseudo-file (`COMPILE_EXCLUSIONS-driven-list`); on failure `RE` degenerates to `test_registration_smoke` and the gate "passes" 1/1 without building or running the 33 other targeted binaries. This is the BL-3 defect class (unexecutable gate text) recurring in the one gate that certifies the base for the whole cycle. | 09 §6 G-C2-0 step 3 | Adopt **A-1**: replace with the exact proven command from `docs/WINDOWS.md:190-195` (source `COMPILE_EXPOSURE.txt`), add a cardinality assertion, and re-tag PROVEN only on the corrected text. |
| **B-2** | **SYNC-1 doctrine incomplete at the moment of first execution.** Uncovered: conflict-set detection command (a); operational definition of the >~50-unguarded-edits abort threshold (b); upstream delete/rename of files carrying our guards/ports/reroutes (c); the three fork-modified `cmake/*.cmake` modules — incl. the silently-degradable `-ffp-contract=off` pin and the raw-dylib filter (d); new POSIX-only coupling in never-touched files post-merge (e). | 09 §3.3–3.6 | Adopt **A-2a..A-2e** verbatim as doctrine amendments before running `git merge` in C2-WP1. |

### MAJOR (4)

| # | Issue | Where | Corrective instruction |
|---|---|---|---|
| **M-1** | **Linux-parity evidence circularity + G-C2-0 internal contradiction.** The C2-7 descope fallback (CI parity job) lands *after* the two gates that may need it (G-C2-0 step 4, G-C2-4); and G-C2-0's Pass line omits step 4 that §3.5 calls mandatory — inviting a silent drop. | 09 §5 C2-7, §6 G-C2-0/G-C2-4 | Adopt **A-3** (hard provisioning go/no-go in C2-WP1; hoist parity job on no-go; reconcile the Pass line). |
| **M-2** | **False doc-equality claim:** "winget LLVM 21.1.8 exact pin matching docs/WINDOWS.md §1" — docs §1 pins only the LLVM 21 major ("any 21.x works", verified host 21.0.0git). G-C2-5's pin check comment inherits the falsehood; local (21.0.0git) ≠ CI (21.1.8) patch level goes unrecorded. | 09 C2-WP6, G-C2-5 | Adopt **A-4** (record CI pin in docs §1 during C2-WP6 — living-doc edit, L4-legal; note both patch levels in STATUS.md; fix the G-C2-5 comment). |
| **M-3** | **SC4 script does not exist.** G-C2-2's generation invocations are placeholders; real generation requires a model checkpoint (choice, download size, network, disk) that no WP budgets; 02's original SC4 (:40) was the weaker `--version` definition — 09's tightened intent is right but unexecutable as written (L2 violation). | 09 C2-WP3, G-C2-2 | Adopt **A-5** (define and commit the concrete SC4 script — model ID, pull command, CLI prompt invocation, server HTTP request transcript, pass criteria — as the first act of C2-WP3). |
| **M-4** | **K-count gate arithmetic wrong:** `grep -c "^| " K-LEDGER.md` counts the table header row; K=5 measures as 6, K=0 as ≥1. Criterion not measurable as written. | 09 G-C2-4 | Adopt **A-6** (count data rows only; assert K ≤ 5 against that number). |

### MINOR (6)

| # | Issue | Correction |
|---|---|---|
| **m-1** | "15 `<unistd.h>` files + `test_server_shutdown`" double-counts (the fork test is one of the 15; verified). | Say "15 files, of which `test_server_shutdown` additionally needs the P2 fork/waitpid port (compat.hpp cannot reach it — own `main`)". Census overrides regardless. |
| **m-2** | G-C2-5's `gh api …/actions/workflows/windows.yml` does not return schedule/cron data — "weekly schedule cron present" unmeasurable with it. | Use `gh workflow view windows.yml --repo antmikinka/LSE --yaml` and grep for `schedule:`/`cron: '17 6 * * 1'`. |
| **m-3** | G-C2-0's hardcoded `test_dflash2\|test_loom_cache` exclusion pair contradicts 09's own L3 ("no hand-maintained test-name lists"); it also silently rots when X changes. | Derive the exclusion set from `COMPILE_EXCLUSIONS.md` table rows at run time (folded into A-1). |
| **m-4** | DOC-1's fixed snippet must drop `$env:CC = "clang-cl"` (`BUILD_FROM_SOURCE.md:50`) — project is `LANGUAGES CXX` only; keeping it makes "verbatim snippet green" true only with a CMake warning the canonical guide calls out as user error. | One-line addition to C2-WP2's DOC-1 scope. |
| **m-5** | §3.4's ID-4 line refs (":129/:130/:135/:196") are pre-cycle-1 positions; current guards are `tests/CMakeLists.txt:137` (block covering `test_server`/`test_chat_protocol`/`test_api_parity`), `:220` (`test_memory_lifecycle`), `:238` (`test_tokenizer`). | Re-anchor at execution (post-SYNC-1 the lines move again anyway — instruct "locate by target name, not line number"). |
| **m-6** | Under the C2-7 descope, the ID-11 Linux registration-smoke census (`logs/m2a-smoke-linux.txt`-class baseline, still outstanding per 06 §7.3) has no producer: WP6's linux-parity artifact list names fingerprints/T1 hashes only. | If descope is invoked, add "run `test_registration_smoke` on Linux and upload the census log" to the linux-parity job spec (folded into A-3/A-7). |

---

## 8. Amendments for Implementation

**Binding on the C2 Stage-4 implementer. 09 remains immutable; where an amendment conflicts with 09's text, the amendment wins (same contract shape as 05 §7 over S1/S2).**

### A-1 (closes B-1, m-3) — Replace G-C2-0 step 3 with the proven command + guards

```bash
bash windows-enablement/gen_compile_exposure.sh                       # regenerate, never hand-edit (L3)
# exclusion set derived from the ledger, not hardcoded:
EXCL=$(awk -F'|' '/^\| `test_/ {gsub(/[` ]/,"",$2); print $2}' windows-enablement/COMPILE_EXCLUSIONS.md)
RE=$( (grep -vxF "$EXCL" windows-enablement/COMPILE_EXPOSURE.txt; \
       echo test_registration_smoke) | sort | paste -sd'|' -)
# cardinality assertion — refuses the silent-false-green degeneration:
test "$(echo "$RE" | tr '|' '\n' | wc -l)" -ge 30 || { echo "G-C2-0 ABORT: targeted set degenerate"; exit 1; }
cmake --build build-win --target $(echo "$RE" | tr '|' ' ')
ctest --test-dir build-win -R "^($RE)$" --output-on-failure            # expect 100% of |RE| (34 today, M after drift)
```
Record `|RE|` in the sync note. The PROVEN tag applies only to this corrected text (whose substance matches `docs/WINDOWS.md:190-195` and `logs/m2a-build.txt` PART 3). After C2-WP4 lands X=0, `EXCL` derives to empty automatically — no gate rewrite needed. (If the awk field-split proves brittle against the ledger's exact table format at execution time, any machine derivation from the ledger is acceptable; hand-listing is not.)

### A-2 (closes B-2) — SYNC-1 doctrine amendments (normative additions to 09 §3)

- **A-2a · Conflict-set detection (before any resolution):** immediately after `git merge` stops, run and commit into the sync note:
  `git diff --name-only --diff-filter=U` **and** `git status --porcelain | grep -E '^(DD|AU|UD|UA|DU|AA|UU)'`.
  If any of the five §3.6.1 seam paths appears (in any conflict code, incl. AA add/add), **abort per §3.6.1** — do not inspect contents first.
- **A-2b · Operational threshold for §3.6.2:** "unguarded edits" = lines *added by the fork* (i.e., present in `git diff origin/master...HEAD` after resolution, restricted to upstream-owned files: exclude `windows-enablement/**`, `docs/WINDOWS.md`, `BUILD_FROM_SOURCE.md`, `.github/workflows/windows.yml`, `logs/**`, and fork-new files) that do **not** sit inside an `#ifdef _WIN32` / `#if defined(_WIN32)` / `if(WIN32)` / `if(NOT WIN32)` / `elseif(WIN32)` block. Count them per file and in total; **total > 50 → abort per §3.6.2**; 20–50 → proceed but record the count and file list in STATUS.md as a PR1-trigger datapoint. The pre-existing `__builtin_memcpy → std::memcpy` class (08 §1.2 observation) counts toward the total.
- **A-2c · Upstream deletion/rename of a file we touched:** honor the deletion/rename (upstream intent wins; never keep a fork-only copy of a file upstream deleted). Then: (i) if the file was in `COMPILE_EXPOSURE.txt` or `COMPILE_EXCLUSIONS.md`, regenerate/prune both; (ii) if it carried seam reroutes or guards, locate the successor (`git log --follow`, rename detection in the merge output) and re-apply per §3.3; if no successor exists, record the lost coverage in the sync note and add it to the next census; (iii) if a *seam* file's call-site file was deleted wholesale, re-run the L1 probe (tests-ON or tests-OFF as appropriate) to re-census before proceeding. Renames appear as R-records — treat as delete+add at the new path.
- **A-2d · `cmake/` module doctrine (new §3.4 rows):** `cmake/LSECompilerFlags.cmake` (+89 fork lines), `cmake/LSERust.cmake` (+50), `cmake/LSEOptions.cmake` (+8) are fork-modified upstream files: accept upstream structure, re-place the guarded branches. **Mandatory post-merge verification (grep, not build success — these pins degrade silently):**
  `grep -n "ffp-contract" cmake/LSECompilerFlags.cmake` (Windows branch pin present — R7/ID-14);
  `grep -n "NONSTDC_NO_DEPRECATE\|CRT_SECURE_NO_WARNINGS" cmake/LSECompilerFlags.cmake`;
  `grep -n "windows\.\\\\?\[0-9\]\|native-static-libs" cmake/LSERust.cmake` (raw-dylib filter + fallback set `ws2_32 bcrypt userenv` intact — N4);
  `grep -n "forced OFF\|WIN32" cmake/LSEOptions.cmake` (HRX guard — §3.6.3 tripwire);
  configure summary still prints `HRX backend: OFF` and `host std: C++26` (dialect override keys on Clang).
  Any check failing that cannot be re-placed additively → §3.6 escalation.
- **A-2e · New-coupling rule (upstream POSIX-only code in files we never touched):** if the §3.5 regression build fails on such code, it is **not** automatically abort-4: disposition = (i) `src/**` → guarded seam reroute per §3.3 (counts toward the PR1 trigger and A-2b totals); (ii) `tests/**` → do not fix at SYNC-1 time beyond a ledger guard if it breaks the targeted set; append the file to the C2-WP4 census worklist (or open an X-ledger entry with `if(NOT WIN32)` if it breaks the *targeted* set); (iii) every occurrence is named in the sync note. Only if the breakage cannot be fixed within §4.3 rules does §3.6.4 abort apply.

### A-3 (closes M-1, m-6) — Parity-evidence sequencing fix

1. C2-WP1 gains an explicit task: provision WSL clang++-21 (apt.llvm.org) and record **go/no-go** in STATUS.md before G-C2-0 is attempted.
2. G-C2-0's Pass line is amended to match its command list: either step 4 (Linux-parity configure+build on the merged SHA) passes, or the no-go path below is invoked — never silently omitted.
3. **No-go path:** hoist *only* the `linux-parity` job (split from C2-WP6; the windows job stays last) and land it before G-C2-4 so the descope evidence source exists; or, if even that is impossible, declare "M3-partial (parity pending)" at G-C2-4 with a user decision recorded — M3 is not declared without parity evidence on the same SHA.
4. If the descope is invoked, the linux-parity job spec must additionally run `test_registration_smoke` and upload the Linux census log (ID-11 baseline, m-6).

### A-4 (closes M-2) — Toolchain-pin truth

In C2-WP6, add one row/line to `docs/WINDOWS.md` §1 recording the CI pin (`winget LLVM.LLVM 21.1.8 --exact`, source S4 §5.1) alongside the local verification host (21.0.0git) — living-doc edit, L4-legal. G-C2-5's grep comment becomes "CI pin recorded in docs/WINDOWS.md §1"; STATUS.md records both patch levels. Same-LLVM-major (21) across the windows and linux-parity jobs remains the T3 requirement — that part of 09 is sound.

### A-5 (closes M-3) — Concrete SC4 script, committed before execution

First act of C2-WP3: write `windows-enablement/sc4-script.md` defining: model ID (suggest the `docs/WINDOWS.md` §5 example `mlx-community/Qwen3.5-0.8B-4bit` or a smaller fixture), the `lse.exe pull` command, a fixed-prompt generation invocation with expected non-empty output + exit 0, and the `lse-server.exe` launch + HTTP request/response transcript (curl or Invoke-WebRequest, exact port/endpoint). Budget the download (network + disk) in the WP estimate. G-C2-2 then references the script's commands verbatim; `logs/m2b-acceptance.md` contains their real transcripts (L5).

### A-6 (closes M-4, G-C2-3 fuzz) — Ledger-count arithmetic

- K count: `grep -cE '^\| `' windows-enablement/K-LEDGER.md` (data rows begin with a backticked `binary:case-group`; adjust once to the ledger's actual row shape at creation, then freeze the pattern in the file's header comment). Assert ≤ 5 on that number.
- G-C2-3 guard count: expected `grep -c "if(NOT WIN32)" tests/CMakeLists.txt` = **0** after X-guard removal; any surviving occurrence must be listed in STATUS.md with a one-line justification (currently none besides the two X-ledger guards at `:104`/`:195`). When removing the `test_dflash2` guard, preserve its `else()` branch behavior (`test_feature_taps` ENVIRONMENT on Windows — see COMPILE_EXCLUSIONS.md).

### A-7 (closes m-1, m-2, m-4, m-5) — Wording/measurement corrections

- C2-WP4 step 3 reads: "the **15** Windows-registered `<unistd.h>` files — including `test_server_shutdown`, which additionally needs the P2 fork/waitpid port because it is not harness-based and `compat.hpp` cannot reach it".
- G-C2-5 cron check: `gh workflow view windows.yml --repo antmikinka/LSE --yaml | grep -n "cron"` (expect `17 6 * * 1`).
- C2-WP2/DOC-1 scope += delete the `$env:CC = "clang-cl"` line from the `BUILD_FROM_SOURCE.md` snippet.
- §3.4 ID-4 re-application: locate the tokenizer guards **by target name** (`test_server`, `test_chat_protocol`, `test_api_parity`, `test_memory_lifecycle`, `test_tokenizer`), not by line number.

---

## 9. Score & Verdict

### Score: **09-cycle2-program-plan.md — 7 / 10**

| Dimension | Assessment |
|---|---|
| Scope & completeness (§2) | **Excellent** — full backlog/D-flag/lesson/ID coverage; out-of-scope honestly derived from user decisions with the R1′ consequence recorded rather than hidden |
| Evidence discipline (§6) | **Strong** — 12/15 spot-checks exact to the file:line; the plan's own L5 rule is satisfied by the plan itself |
| Gate quality (§3) | **Mixed** — one silent-false-green command mislabeled PROVEN (B-1, the BL-3 class recurring), two unmeasurable criteria (M-2/M-4), one unexecutable placeholder (M-3), else sound and honestly tagged |
| Merge doctrine (§4) | **Incomplete** — right general rule, four uncovered scenarios + one non-operational threshold, at the highest-scored risk of the cycle (B-2) |
| Sequencing (§5) | **Good** — WP3∥WP4 verified sound; WP6-last deviation bounded and recorded; one real circularity in the parity-evidence fallback (M-1) |
| Risk register | **Excellent** — C2-1..C2-7 are the right risks with the right contingencies; C2-7's contingency is the only one that cannot execute as sequenced |

The two BLOCKING defects sit precisely in the failure classes cycle 1 paid for (gate text that cannot be trusted at face value; planning structure that outran measurement). Both are text-level, fully corrected by §8, and require no re-architecture — the WP structure, ordering, gates-as-designed, and doctrine-in-spirit survive intact.

### Verdict: **APPROVED WITH BINDING AMENDMENTS**

C2 Stage 4 may begin **only after adopting §8 (A-1..A-7)**. Hard order constraints added by this review:
1. **A-1 and A-2 must be in hand before `git merge` runs in C2-WP1** (B-1/B-2 gate the first WP).
2. **A-3's WSL go/no-go must be recorded in STATUS.md before G-C2-0 is declared** (M-1).
3. **A-5's SC4 script must exist before G-C2-2 is attempted** (M-3).
4. No gate may be declared on a command that was not run verbatim — including the corrected A-1 text (L2, re-affirmed).

Escalation path unchanged from 09 §7: any §3.6 abort or gate miss → options + recommendation in STATUS.md → user decision, never silent.

*End of quality review 3. No source or prior-artifact files were modified; nothing committed. All verification performed against the working tree at `71be5a5`, 2026-10-10.*
