# 15 — Iteration-3 Loopback Analysis (planning-analysis, cycle 3)

**Branch:** `feature/windows-enablement` @ `209d025` (C10) = upstream `90d66ab`
(v0.6.1, merged at SYNC-1 `5a99199`) + 17 fork commits. Fork =
`antmikinka/LSE`; upstream = `Geramy/LSE`.
**Date:** 2026-10-11 (Sunday). **Author:** Stage 1 (planning & analysis),
iteration 3 — consumed by: PM/runbook stage, thin implementation stage,
quality reviewers, tech writer.
**Status:** Analysis only. No source modified; this is the only file written;
nothing committed or pushed by this stage. Frozen artifacts 01–12 untouched;
per 07 §3 corrections and live planning land here, in 13-log and STATUS.md.

**Audit basis (all re-measured today, not taken from artifacts):**
`git log`/`git diff 90d66ab..HEAD` (118 files, **+15,884 / −370** total;
non-evidence slice **71 files, +5,465 / −370** excluding `windows-enablement/`,
`logs/`, `docs/`); `git fetch origin` → **N = 8** (`origin/master` =
`61f1cf0`, 2026-10-10 21:53 −0700); `gh run list --workflow windows.yml`
(C10 push run **38141203856** IN PROGRESS at `209d025`, started 12:56:39Z;
dispatch **38138800778** completed FAILURE = the D-19 false-red; run 5
**38137423880** SUCCESS, **20 m 24 s**); `gh repo view antmikinka/LSE
--json defaultBranchRef` → **`master`**; direct reads of STATUS.md, 13-log,
14, 09, 11, 02, 08, `.github/workflows/windows.yml`, `K-LEDGER.md`,
`ci-expected-w.txt` (98), `docs/WINDOWS.md`, `BUILD_FROM_SOURCE.md`.

---

## 1. Measured state snapshot (2026-10-11)

| Fact | Value | How verified |
|---|---|---|
| HEAD | `209d025` (C10, pushed; push run in flight) | `git log`, `gh run list` |
| Gates | G-C2-0…G-C2-5 **all PASS**; **M3 ACHIEVED** 2026-10-11 | STATUS.md gate table |
| W / X / K | **98 / 0 / 2** (ceiling 5; review-by 2027-01-08) | `ci-expected-w.txt`, `COMPILE_EXCLUSIONS.md`, `K-LEDGER.md` |
| Banked both-jobs green | **1** (run 5, 38137423880 @ `5b16e94`, 20 m 24 s < 45 min) | `gh run list` |
| Runs since | dispatch 38138800778 @ `5b16e94` FAILED (D-19 false-red, workflow-only); push 38141203856 @ `209d025` IN PROGRESS | `gh run list` |
| Upstream drift | **N = 8** behind `origin/master` (`61f1cf0`); base age 1 day | `git fetch` + `rev-list --count` |
| Fork default branch | **`master`** (windows.yml lives only on the feature branch) | `gh repo view` |
| Next cron window | Monday **2026-10-12 06:17 UTC** (`17 6 * * 1`) | windows.yml:62; weekday computed |
| PR-A slice sizes | core **220** changed diff lines; +LSERust **272**; +HRX guard **280** | `git diff 90d66ab..HEAD` per-file, `grep -c '^[+-]'` |
| PR-B slice size | **71 files, +5,465/−370** (code+tests+workflow, excl. evidence/docs-dirs) | `git diff --shortstat` with excludes |

---

## 2. Findings §A — M4 readiness: what stands between `209d025` and M4

M4's canonical criteria, consolidated (02 §6 SC7/M4 row · 08 backlog #8 ·
11 §6 same-SHA rule · 13-log §9): **3× green CI** (both jobs, same SHA,
each < 45 min, weekly schedule active) + **release protocol** on Windows
artifacts + **version pinning** + evidence committed (`logs/m4-ci-evidence.md`).
Everything else (M3, gates, ledgers, docs refresh at C10/`1df3eec`) is done.

- **A1 — Evidence accumulation is the only unfinished *passive* item; state
  is exactly 1 banked green + 1 in flight.** Run 5 is banked for G-C2-5/M3
  but sits at `5b16e94` ≠ HEAD (see B3 for whether/how it counts toward 3×).
  The C10 push run (38141203856) is the first candidate green at `209d025`.
  The failed dispatch (38138800778) is a diagnosed workflow-only false-red
  (D-19) at the *previous* SHA — it does not taint `209d025`.
  **Owner:** orchestrator (watch runs). **Sequencing:** first; nothing else
  in §A should commit while this run is in flight (concurrency cancel, C6).

- **A2 — NEW DEFECT (blocker): the weekly schedule is very likely DISARMED —
  fork default branch is `master` and GitHub fires `on.schedule` only from
  the workflow file on the DEFAULT branch.** `windows.yml` (cron
  `17 6 * * 1`) exists only on `feature/windows-enablement`, so the Monday
  06:17 UTC run will not fire. Consequences: (i) 02 §6's M4 criterion
  "weekly schedule active" is unmet; (ii) the LLVM-21.1.8 drift alarm
  (STATUS.md "Toolchain pin" row) is silent; (iii) 13-log §8's fallback
  evidence path ("otherwise the weekly schedule run is the LARGE_IO proof")
  does not exist; (iv) the cheap cron green that could be counted-run #2/#3
  (B3) is unavailable. The recorded verifications (`gh workflow view
  --ref feature/windows-enablement --yaml | grep cron`, 13-log §8;
  STATUS.md G-C2-5 "Cron verified") prove only that the YAML *text* exists —
  they structurally cannot detect disarming (F2). **Fix (XS, zero repo
  diff):** set the fork's default branch to `feature/windows-enablement`
  (`gh repo edit antmikinka/LSE --default-branch
  feature/windows-enablement`), or carry `windows.yml` on fork `master`.
  **Verify:** a `schedule`-event run appears after Monday 06:17 UTC.
  **Owner:** user/orchestrator (repo settings). **Deadline: before Monday
  06:17 UTC** to harvest the first cron run.

- **A3 — Release protocol is required but never specified to executable
  precision (blocker).** 13-log §9.3: "3× release-candidate runs on Windows
  artifacts (`lse.exe`, `lse-server.exe`), version pinning"; 02 §6 M4
  evidence: run URLs + durations in `logs/m4-ci-evidence.md` (**not yet
  created**). No artifact defines what an "RC run" is or what "version
  pinning" means. **Recommended minimal definition (zero product diff, no
  fork-side versioning scheme invented — versioning is upstream's call at PR
  time):** RC SHA = the declared M4 candidate (A4); RC record =
  `--version` transcripts of both executables built at the RC SHA + the
  sc4-script.md generation transcript + toolchain identities (clang-cl
  21.1.8, Rust stable-msvc, CMake/Ninja versions from `toolchain.txt`
  artifacts) committed as `logs/m4-release-protocol.md`; "3× RC runs"
  satisfied by the 3 counted CI runs at the RC SHA (they build and exercise
  the artifacts) **plus** one local artifact-run record. `m4-ci-evidence.md`
  created at the first counted run (14 §4 trigger row already says this).
  **Owner:** thin-cycle implementation stage; user ratifies the definition
  (DP-C). **Sequencing:** definition written BEFORE the RC SHA is declared.

- **A4 — Ordering constraint: SYNC-2 and the release-protocol commit must
  land BEFORE the RC SHA is declared, or the 3× evidence is orphaned.**
  Measured N = 8 today; the PR-filing base-freshness rule (≤ 7 days,
  08 §3.3 / 14 §5.3) forces a sync near filing anyway, and every sync moves
  the SHA. Recommended order (also the thin-cycle work order):
  **(1)** write the counting rule (B2/B3) into STATUS.md + open
  `logs/m4-ci-evidence.md` — before any further run lands; **(2)** arm the
  schedule (A2) — before Monday 06:17 UTC; **(3)** SYNC-2 ritual (09 §3.2:
  tag `pre-sync/2026-10-12`, merge, classification of any new upstream
  tests, G-C2-0 regression re-run, regenerate `ci-expected-w.txt` IN the
  merge commit if registrations changed — C4, re-derive the D-13 +2 offset);
  **(4)** release-protocol commit(s) (A3); **(5)** local M3 protocol re-run
  at the final tree (C8 precedent: product-line changes invalidate evidence
  carry-over by argument); **(6)** declare **RC SHA**, push, enter the
  evidence window: zero commits, 3 consecutive counted greens (push run +
  Monday cron — which doubles as LARGE_IO-on-hosted proof since schedule
  sets `LARGE_IO_ON=true` — + one dispatch if needed); **(7)** M4 declared;
  **(8)** doc pass (F1/F4/F5) + PR-A hand-craft + PR-B refresh, filed from
  the RC SHA. Elapsed ≈ 2–4 days. **Owner:** thin-cycle implementation
  stage, orchestrator sequencing.
  *Rejected alternative (recorded):* bank 3 greens at `209d025` now, sync
  after M4 — a post-M4 sync would still move the PR head and orphan the
  evidence, re-raising the carry-forward question at the worst moment;
  sync-first is strictly cheaper.

---

## 3. Findings §B — the 3×-green counting question

- **B1 — The acceptance rule is genuinely ambiguous in the frozen record;
  four sub-questions are unresolved.** 02 SC7 says "green **3 consecutive
  runs**"; 08 #8 says "3 consecutive green runs + Linux-parity green on the
  same SHA"; 11 §6 defines the same-SHA rule for a single run ("both jobs
  green on the identical commit counts"); 13-log §9.3 restates both.
  Unresolved: (i) does run 5 @ `5b16e94` count as #1 after HEAD moved?
  (ii) does a workflow-only false-red (the D-19 dispatch) break a chain?
  (iii) may the 3 greens share one SHA (cron/dispatch reruns)? (iv) which
  triggers count? The project already has a *precedent* bearing on (ii):
  the D-19 false-red explicitly did NOT reopen the banked G-C2-5 gate
  ("the gate is NOT reopened", 13-log D-19) — but that doctrine was applied
  to a gate, never written as a counting rule. **Recommendation: adopt B2
  and record it in STATUS.md + the header of `logs/m4-ci-evidence.md`
  BEFORE the C10 run lands — a rule written after results is rule-shopping.**

- **B2 — Recommended rule (defensible interpretation).**
  **Counted green** = one `windows.yml` run, any trigger (push / schedule /
  dispatch), in which BOTH jobs pass strictly (cardinality asserts included)
  at the run's SHA, duration < 45 min, recorded in `m4-ci-evidence.md`
  (URL + SHA + trigger + duration) within 24 h. **M4 requires 3 counted
  greens on 3 CONSECUTIVE runs at ONE declared RC SHA** — no failed
  countable run in between. **Chain-break exception:** a failed run
  classified as *workflow-only false-red* (zero product/test failures;
  defect D-numbered; diagnosis recorded BEFORE the next run starts — never
  retroactively) is NON-COUNTABLE: it neither adds a green nor breaks the
  chain. Rationale: (a) same-SHA repetition is the *right* measurement —
  D-15…D-19 proved runner nondeterminism (PATH shadows, drive layout,
  action env exports, tool-version drift) is the dominant failure class, and
  repeats at one SHA are exactly what sample it; (b) a distinct-SHA rule is
  operationally a Zeno trap (every workflow/doc fix commit restarts the
  counter) and adds no product evidence (each green already includes the
  full suite + parity); (c) the false-red exception codifies the doctrine
  the project already applied to run 5, with an anti-abuse clause
  (pre-recorded diagnosis).

- **B3 — Application to the runs on the board.** Under B2: **run 5 does NOT
  count toward the 3×** — it is `5b16e94`-scoped and stays banked as the
  G-C2-5/M3 acceptance evidence (declaring it "#1 of 3" would mix SHAs and
  re-open exactly the ambiguity B2 removes). The D-19 dispatch failure sat
  at `5b16e94` too (the pre-C10 workflow), so it breaks no `209d025`/RC
  chain. The C10 push run, if green, is a *rehearsal-class* run under D3-iii
  (first run after a workflow-only edit) UNLESS the RC SHA is declared as
  `209d025` before it lands — which A4 argues against (SYNC-2 first).
  Practical consequence: the counting starts at the post-SYNC-2 RC push run;
  realistic completion = RC push run (Mon) + cron (Tue 06:17 UTC, armed per
  A2) + one dispatch (Tue) → **M4 by ~Wed 2026-10-14**, no product work
  required. If the user prefers to skip SYNC-2 (N = 8 is trivially small and
  the base is 1 day old — the ≤ 7-day rule does not force a sync until
  ~Oct 17), RC = `209d025`, the in-flight push run counts as rehearsal, and
  cron + 2 dispatches complete the window Monday–Tuesday; the sync then
  happens post-M4 pre-filing with a one-confirming-green carry-forward rule
  (local M3 re-run at the merged tree per C8 precedent + 1 green at the new
  SHA). Both paths satisfy B2; the A4 path is recommended because it files
  the PR from the same SHA that carries the evidence.

---

## 4. Findings §C — residual risks

- **C1 — Drift-alarm dependency is now single-threaded through A2.** Until
  the default-branch fix lands and a schedule-event run is OBSERVED, treat
  "weekly cron = LLVM-drift alarm + LARGE_IO fallback proof" as **unarmed**
  in every plan. Secondary trap: GitHub skips a scheduled start while a run
  is in flight on the ref — if the RC push run is still active at Monday
  06:17 UTC the cron tick is missed; recover with a manual dispatch (counts
  identically under B2).
- **C2 — Toolchain supply risk at the pins.** `winget install --id LLVM.LLVM
  --version 21.1.8 --exact` can fail if the winget source retires the
  version; the linux-parity clang++-21 install keys on
  `$(lsb_release -cs)`, which breaks when `ubuntu-latest` transitions images
  (22.04 → 24.04 codename change) or apt.llvm.org reshuffles. Both fail
  LOUDLY at install steps (acceptable: named failure, no silent drift);
  mitigation is the existing LLVM-22 revisit note (STATUS.md) + cron alarm.
  No pre-M4 action.
- **C3 — SYNC-2 merge exposure.** N = 8 today; at the measured ~125
  commits/week (08 §3.1, re-measure per L6) a slip to Oct 17 means N ≈ 100+.
  Hotspots unchanged: `CMakeLists.txt` (94 upstream commits since Aug),
  `tests/CMakeLists.txt` (81), `scheduler.cpp` (62), `graph.hpp` (33).
  Doctrine is proven (SYNC-1: 0 conflicts, all four hotspots auto-resolved
  by ort). PR1 trigger (> 2 conflicting files in 2 consecutive syncs): one
  datapoint (0 conflicts); SYNC-2 is the second — record it either way.
  Abort criteria 09 §3.6 unchanged (seam files in conflict set = upstream
  may have shipped its own Windows port → dedup decision, not a merge).
- **C4 — `ci-expected-w.txt` lifecycle on registration change.** Any sync
  that brings new upstream tests must regenerate the file (from `ctest -N`
  parse, never typed — ID-C2-10) **inside the merge commit**, and re-derive
  the D-13 structural +2 parity offset (if upstream adds/removes a UNIX-only
  registration, `WE + 2` breaks). Forgetting is loud, not silent (both jobs
  fail the census assert — B-1 by design) but it burns a chain run under B2.
  Add both lines to the sync runbook (A4 step 3). X-budget watch: a new
  HRX-tainted upstream test that clang-cl cannot compile re-opens the
  X ledger against the X = 0 invariant that CI asserts — classify every new
  test at merge time per 09 §3.4 (the `test_launch_template` precedent,
  STATUS.md SYNC-1 note, shows the ritual works).
- **C5 — K-ledger clock.** Reviews due **2027-01-08** (~89 days): #1
  loopback-backpressure (retest against then-current Windows networking),
  #2 p2996 (recheck when a pinned LLVM ships `-freflection`). No M4/PR
  impact at current pace; if the program stalls past the date, rows must be
  retested and re-dated (append-only) BEFORE PR-B filing, since the PR body
  cites K = 2 + review dates as the honest-limitations exhibit.
- **C6 — Concurrency foot-gun during the evidence window.**
  `win-${{ github.ref }}` + cancel-in-progress means ANY push or dispatch
  while a counted run is in flight cancels the evidence run. 13-log §8
  already noted this operationally; promote it to a written window rule:
  *between RC declaration and the 3rd green — no commits, no dispatches
  while a run is in flight; dispatch only into an idle queue.*
- **C7 — Runner-image drift between counted runs.** The D-15/D-17 classes
  (PATH order, coreutils shadows, checkout-drive layout) are properties of
  the hosted image, which GitHub updates continuously. Keeping the 3× window
  temporally tight (A4: ~2 days) bounds the exposure; post-M4 the cron alarm
  (armed per A2) is the standing detector. No further action; do NOT
  over-mitigate (see D3).

---

## 5. Findings §D — the D-15…D-19 lesson pattern and its cheapest fix

- **D1 — Pattern.** Five CI defects in five runs; **four were
  workflow-environment issues visible only on the runner** (D-15: Git
  coreutils `link(1)` PATH shadow + rust-cache's global `CARGO_TERM_COLOR`
  ANSI leak; D-16: the D-15 sink missed the Configure step + LSERust's
  *silent fallback* masked it; D-18: ctest summary-spelling drift between
  local 3.31.9 and the runner's newer ctest; D-19: `--label-include` SELECTS
  rather than adds + GitHub-expression `''` falsy trap), and one was a
  genuine PRODUCT defect (D-17 cross-drive — CI doing exactly its job,
  catching what a single-volume dev box never could). Classes: (i)
  runner-env ≠ local-env; (ii) third-party action side effects; (iii)
  tool-version-dependent output parsing; (iv) workflow-language semantics.
  Root cause in one line: **assertions and environment assumptions were
  authored against the locally observable world; L2 rehearsal cannot reach
  the hosted runner.** The class inventory is now largely exhausted — but
  every future `windows.yml` edit re-opens it.

- **D2 — Cheapest structural mitigation for M4 (four items, ~15 lines +
  one tool, no new jobs).**
  **(i)** `actionlint` (with its shellcheck integration) on every
  `windows.yml` edit, pre-commit — targets class (iv) directly; the D-19
  falsy-`''` expression is precisely what it flags. **(ii)** Codify the two
  already-learned assertion rules as a workflow-review checklist: every grep
  on tool output is format-tolerant (D-18), and every ctest invocation keeps
  its cardinality assert (B-1/I-1 — the reason D-19 failed loudly in one run
  instead of silently passing). **(iii)** Rehearsal doctrine: the first run
  after any workflow-only edit is a **non-countable rehearsal** (already de
  facto practice: run 5 banked the gate while the C10 fix ran as
  supplementary) — now load-bearing because B2 makes runs countable.
  **(iv)** A ~10-line **env-probe step** at the top of the windows job:
  record `command -v link.exe` (must resolve under VC\Tools), the effective
  `CARGO_TERM_COLOR`, and `ctest/cmake/clang-cl --version` into the evidence
  artifact — converts the next D-15-class drift into a *named step-1
  failure* instead of a mid-run mystery. All four are workflow-only or
  process-only; zero product diff.

- **D3 — Explicitly rejected as over-engineering.** `act`/local runner
  emulation (cannot emulate the `windows-latest` image — the very thing that
  bites); a parallel staging workflow (double maintenance, halves the signal
  per run); automatic false-red exemption (B2 deliberately keeps a human
  diagnosis in the loop with a pre-record requirement). The D2 set covers
  four of the five observed classes; the fifth (D-17, product) needs no
  workflow mitigation — surfacing it IS the workflow's purpose.

---

## 6. Findings §E — upstream-PR readiness (critical review of 14 §5)

- **E1 — PR-A ≤ 300 lines: STILL TRUE, measured — but the slice should be
  re-cut.** `git diff 90d66ab..HEAD` today: `CMakeLists.txt` 51+/39−,
  `cmake/LSECompilerFlags.cmake` ~89 changed, `cmake/LSELink.cmake` +28
  (new), `std::memcpy` conversions (`hash.hpp`, `dtype.hpp`, `graph.cpp`)
  ~13 → **pure-hygiene core = 220 changed lines**; adding `LSERust.cmake`
  (52) → 272; adding the `LSEOptions.cmake` HRX force-OFF guard (8) → 280.
  **C8/C9/C10 touched none of these files** (C8 = product/test, C9/C10 =
  workflow/docs), so 14 §5.2's ≤300 claim survived the product fixes with
  20–80 lines of headroom depending on slice. **Recommended re-cut
  (supersedes 14 §5.2's PR-A contents):** PR-A = the 220-line
  cross-platform core only (force-load helper + call-site refactor,
  compiler-ID flag branch incl. FP pins, C++26 dialect override, memcpy
  conversions); move the LSERust windows-msvc branch (the raw-dylib filter
  lives *inside* it — extracting it alone means hand-splitting one hunk for
  zero review value) and the WIN32-only HRX guard to PR-B. PR-A then rides
  upstream's own `ci.yml` with no Windows dependency at all, exactly the
  14 §5.2 rationale #4.

- **E2 — PR-A is a hand-craft, not a subtree diff — budget for it.** The
  branch's `CMakeLists.txt` delta MIXES hygiene hunks with
  Windows-enablement hunks (`elseif(WIN32)` seam sources, poller select,
  `ws2_32` link edges) that are meaningless upstream without PR-B's files.
  PR-A must be authored as a fresh commit on an upstream-master base
  containing only the hygiene hunks, idiom-matched to the existing
  `if(APPLE)` precedent. Size: **S** (~half day) including watching upstream
  `ci.yml` green on the PR branch. Additionally, every line ref in 14 §5.1
  (`CMakeLists.txt:18-31`, `LSEOptions.cmake:16-22`, …) must be re-verified
  **after SYNC-2** per 07 §3's invariant (re-run commands, don't trust line
  refs). None of this was invalidated by C8–C10; it simply was never started
  (correctly — the user deferred filing to M4).

- **E3 — PR-B refresh list (the branch moved since 14 was written at
  `5b16e94`).** PR-B's real size today: **71 files, +5,465/−370**
  (code+tests+workflow; excludes the ~10k lines of evidence/decision-record
  in `windows-enablement/`+`logs/`) — 14's "~7k-line" figure was
  conservative-high; the split rationale is unchanged at 5.8k. Refresh work
  at filing: swap the run-5 URL for the 3× M4 run URLs at the RC SHA
  (14 §5.3); add the D-19/C10 record to the CI narrative; re-verify ledger
  states (X = 0, K = 2 + review dates, C5) and all line refs at the filing
  SHA; attach the base-freshness sync note (≤ 7 days + N); decide the
  `windows-enablement/` curated-subset question with the maintainer
  (14 §5.1 item 8).

- **E4 — Filing mechanics.** Stacked PRs: cut the PR-A branch from upstream
  `master` (hand-crafted commit per E2), file PR-A; base PR-B on the PR-A
  branch (or file against master with a stacking note), rebase PR-B when A
  merges. The fork default-branch change (A2) does not affect PR mechanics.
  Keep the 14 §5.2 caveat honest: deferral decays PR-A's conflict-reduction
  value at ~125 upstream commits/week on the two hot CMakeLists — filing
  promptly AT M4 matters; the PR1 trigger (C3) remains the escalation path
  if syncs start conflicting before then.

---

## 7. Findings §F — stale / internally inconsistent records (FLAGGED, not fixed)

- **F1 — `docs/WINDOWS.md:371` is stale in name AND semantics.** It still
  documents the workflow env as `LSE_LARGE_IO_FLAG` — renamed to
  `LARGE_IO_ON` by C10 — and describes the include/exclude flag semantics
  that D-19 disproved (`--label-include` selects; ON now means *no* label
  filter). C10 updated `windows.yml`, 13-log, STATUS.md and
  `BUILD_FROM_SOURCE.md` but not this line. Fix belongs to the M4 doc pass
  (14 §4 trigger table); the frozen 11 §6 template keeps its historical
  spelling (never edited).
- **F2 — "Cron verified" overclaims in two living files.** STATUS.md G-C2-5
  row ("Cron verified (`17 6 * * 1`)") and 13-log §8's verification line
  equate YAML-text presence with an ARMED schedule; per A2 the schedule
  almost certainly cannot fire (fork default branch = `master`). Reword to
  "cron line present; arming pending default-branch fix + observed schedule
  run" when A2 lands.
- **F3 — STATUS.md "Branch / push state" drift (minor, living-doc).**
  "base age 0 days" is now 1+; measured N = 8 is unrecorded (the sync note
  still shows SYNC-1's N = 4). Normal between-gate staleness; refresh at the
  SYNC-2 note per the L7 schema.
- **F4 — 13-log §9 is partly superseded by the gates it postdates.** §9
  item 1 still lists G-C2-5 as pending work ("push; observe first run
  green…") although the STATUS gate row records PASS and §8 itself narrates
  run 5 green; §9 item 2's "if CI shows anything else, triage before M4" can
  be closed (five runs showed test_model fast on cache-less runners exactly
  as D-11 predicted). 13-log is living — prune §9 to the true M4 remainders
  (A1–A4) at the next edit; do not rewrite history entries.
- **F5 — 14 §1/§4 "result pending" items now have results; 14 §3.1 is
  resolved.** Dispatch 38138800778 completed **FAILURE** (the D-19
  false-red); 14 §4's own trigger row requires updating the "pending"
  sentence with outcome + the C10-successor dispatch path. Conversely,
  14 §3.1's stale-`BUILD_FROM_SOURCE.md` item was **fixed by C10**
  (verified: the file now documents the tests-ON full build; the ID-C2-14
  acceptance grep `grep -c 'env:CC'` → 0, the `$env:CXX` collision concern
  is moot). Record the resolution so the M4 doc pass doesn't redo it.
- **F6 — Supersession map for the next reader (no action; frozen artifacts
  stay frozen).** 11 §6's template env (`LSE_LARGE_IO_FLAG`) and
  `--label-include` policy → superseded by `windows.yml` at C10; 07 §5's
  Batch-1 PR draft → superseded by 14 §5 → PR-A contents further re-cut by
  E1 here; 02 SC5's KNOWN_FAILURES-based K definition → superseded by the
  K-LEDGER mechanism (ID-6/I-5); 02 §4.1 "weekly rebase" → superseded by
  merge doctrine (08 DP1/09 §3.3). Where this file and 14 disagree
  (PR-A slice), THIS file is the later word; both are living.

---

## 8. Iteration-3 verdict: **THIN M4-execution cycle** (not a full cycle 3)

**Verdict.** The next pipeline loop should be a **THIN M4-execution cycle**:
no new product work is expected or needed; the cycle executes A4's ordered
runbook (rule-writing → schedule arming → SYNC-2 → release-protocol record
→ RC declaration → 3× evidence window → M4 → doc pass → PR filing).

**Justification.** (1) Product scope is empty: X = 0, K = 2 ≤ 5, W = 98,
M3 ACHIEVED, G-C2-0…5 all PASS, six product defects found+fixed, and 13-log
§9's remainder list contains no product items — only evidence, protocol and
watch items. (2) The remaining work is dominated by *waiting* (CI runs) and
*mechanical rituals with proven doctrine* (SYNC-2 per 09 §3.2–3.6, exercised
once with 0 conflicts; doc refresh per 14 §4 triggers). (3) The two
genuine decision gaps (B2 counting rule, A3 release-protocol definition) are
one-paragraph specs, not work packages. (4) Cycle 2's own charter
anticipated this shape: 09 §1.2 scoped "WP13b/M4 3× green protocol" OUT of
cycle 2 as cycle-3 work, and 08 backlog #8 sized it **S–M**. A FULL
plan→test-strategy→implement chain would regenerate governance artifacts
with nothing to govern — over-engineering by this project's own 14 §5.2
standard.

**Compressed stage mapping for the thin cycle.** Stage 2 (PM): emit the M4
runbook = B2 rule text + A4 sequence + A3 definition + C4/C6 runbook lines
(one short artifact or a STATUS.md section). Stage 5 (implementation): small
commit authority — SYNC-2 merge, release-protocol/evidence commits, the
D2-ii/iv workflow tweaks (actionlint is local-only; env-probe is ~10 lines).
Stage 6 (tech writer): M4 doc pass (F1/F2/F4/F5, docs/WINDOWS.md header →
"supported (CPU backend)/M4" per 14 §4) + PR-A hand-craft (E2) + PR-B
refresh (E3). Reviewers: verify evidence-bundle completeness (L5: every
cited file in git) and rule compliance (B2 windows, C6 no-push rule).

**Re-escalate to a FULL cycle ONLY if:** SYNC-2 hits an 09 §3.6 abort
criterion or the PR1 trigger (> 2 conflicting files, 2 consecutive syncs);
a new upstream test breaches the X = 0 invariant and the K/X governance
question re-opens; any **bucket-(a) product failure** appears during the 3×
window (D-17-class recurrence = real product work); arming the schedule
exposes cron-run flakiness needing workflow redesign; or the user
un-defers post-M4 scope (WP15/HRX spike, cl.exe path — both already gated
post-M4 by 02 §2/13-log §9.6).

---

## 9. Decision points for the user (resolve before the thin cycle executes)

| # | Decision | Options | Recommendation |
|---|---|---|---|
| **DP-A** | Adopt the B2 counting rule (3 consecutive counted greens at one declared RC SHA; pre-recorded false-red exception) and write it down before the C10 run lands | (a) adopt B2; (b) distinct-SHA rule; (c) leave unwritten | **(a)** — (b) is a Zeno trap, (c) is rule-shopping-by-accident |
| **DP-B** | Arm the weekly schedule: flip the fork default branch to `feature/windows-enablement` (vs carrying windows.yml on fork master, vs weekly manual dispatch) | settings change / branch copy / runbook | **Settings change** — zero diff, one command, before Mon 06:17 UTC; confirm via an observed schedule run |
| **DP-C** | Ratify the A3 minimal release-protocol definition (no fork-side versioning scheme; RC record = versions + sc4 transcripts + toolchain identities in `logs/m4-release-protocol.md`) | (a) minimal; (b) invent a version tag scheme now | **(a)** — versioning is upstream's call at PR time |
| **DP-D** | Approve the E1 PR-A re-cut (220-line cross-platform core; LSERust + HRX guard move to PR-B) | (a) re-cut; (b) keep 14 §5.2's 280-line slice | **(a)** — avoids hand-splitting one LSERust hunk; keeps PR-A 100 % upstream-ci-testable |

---

## 10. Self-check

- [x] One new file written (`windows-enablement/15-iteration3-loopback-analysis.md`);
      nothing committed, pushed, or edited elsewhere; frozen 01–12 untouched.
- [x] Every number re-measured this session with the command stated
      (N = 8, PR-A 220/272/280, PR-B 71 files +5,465/−370, run states via
      `gh`, default branch via `gh`, weekday via date computation, K/W/X
      from the ledgers, `docs/WINDOWS.md:371` and `BUILD_FROM_SOURCE.md`
      states by direct read/grep).
- [x] In-flight runs NOT claimed: 38141203856 recorded as in-progress; the
      successor large-io dispatch is named as pending, per 14 §7's rule.
- [x] Item taxonomy: A1–A4, B1–B3, C1–C7, D1–D3, E1–E4, F1–F6 = **27 items**;
      each carries an owner and/or a disposition; §F flags without fixing.
- [x] No artifact edited to accommodate this analysis; supersessions are
      recorded (F6, E1) rather than applied.

*End of iteration-3 loopback analysis. Next loop = THIN M4-execution cycle
per §8; DP-A…DP-D resolve before A4 step 1 starts.*
