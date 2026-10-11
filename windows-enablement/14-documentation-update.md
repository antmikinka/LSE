# 14 — Documentation update (cycle-2 refresh) + upstream-PR plan (M4-deferred)

*Cycle-2 Stage-6 artifact, 2026-10-11. Living doc (07 §3 rules apply). Scope:
this file + `docs/WINDOWS.md` only; STATUS.md and 13-log are the
orchestrator's to finalize. Every claim below carries a file/run reference;
all repo facts were re-verified against the working tree at `5b16e94` (C9)
before writing.*

---

## 1. Why this artifact exists

Cycle 2 ended with **M3 achieved**: local protocol green at the C8/C9 tree
(13-log §6 re-run: 97/97 in 73.89 s, LARGE_IO 1/1, fingerprint pair 2/2,
K = 2, X = 0, W = 98) and CI run 5 (**38137423880**) green on BOTH jobs at
`5b16e94` — G-C2-5 PASS under the same-SHA rule (11 §6). A supplementary
`workflow_dispatch` proof run (**38138800778**, `run_large_io=true`) is in
flight at the same SHA: **large-io hosted proof dispatched at the acceptance
SHA; result pending** (this artifact does not claim its outcome).

**USER DECISION (recorded, §6):** the upstream PR to `Geramy/LSE` is
**DEFERRED until M4** (CI green 3× + Linux parity + release protocol per
13-log §9). The Batch-1 PR draft of 07 §5 is therefore **refreshed here, not
filed**.

## 2. Delta table — what changed in `docs/WINDOWS.md` and why

| § | Change | Why (ground truth) |
|---|---|---|
| Title + status header | "cycle 2 / M3-partial" → "cycle 2 complete / M3 achieved"; adds the local M3 numbers, CI run 5 green both jobs at `5b16e94`, and the pending large-io dispatch | M3 conversion happened at run 5 (D-12/A-3.3 same-SHA rule); "partial" was stale the moment both jobs went green on one SHA |
| Preamble | `BUILD_FROM_SOURCE.md` note rewritten: the snippet is now correct for binaries-only, but its "`-DLSE_BUILD_TESTS=OFF` is required for now / targeted tests only until WP12" line is flagged **stale**; `README.md` remains untouched | WP12 landed X = 0 (`COMPILE_EXCLUSIONS.md`; `grep -c 'if(NOT WIN32)' tests/CMakeLists.txt` == 0); the DOC-1-era warning no longer matches the tree |
| "Verified on" | adds CI acceptance line (run 38137423880 at `5b16e94`) | gate evidence belongs in the header claim |
| §1 toolchain | unchanged — the LLVM **21.1.8** official pin + rationale (ROCm clang-cl 21.0.0git retired over a constexpr codegen bug; A-4/ID-C2-14; CI `winget --exact`) was already landed at C5 | verified against `windows.yml:98-104` and STATUS.md "Toolchain pin" |
| §3 | **Restructured**: tests-ON full build is now the NORMAL path (mirrors the CI configure line); tests-OFF demoted to a "binaries-only variant"; stale claims removed: "only supported configuration is tests=OFF", the ROCm 7.1 clang-cl path example (retired toolchain), the hardcoded `[192/192]` expectation (edge counts drift — assert exit 0); configure summary swapped to the tests-ON transcript (`c2-sync-configure-tests.txt`, incl. the `ntdll.lib` native-deps line with a pointer to the D-16 troubleshooting entry) | X = 0 since WP12; CI configures `-DLSE_BUILD_TESTS=ON` (`windows.yml:144-145`); `LSE_BUILD_TESTS` default ON at `cmake/LSEOptions.cmake:3`; 192 was the M2a tests-OFF capture (`logs/m2a-build.txt:1189`); ROCm retirement per §1/13-log §7 |
| §4 | "Current state" → cycle-2-complete; duplicated configure block replaced by a pointer to §3 + explicit note that the targeted-34 workaround is obsolete/removed; run commands now carry expected counts (census 98 == `ci-expected-w.txt`; main run "out of 97" = W−1; LARGE_IO "out of 1"; fingerprint pair "out of 2"); **new P8 table** for the four cycle-2 tests (`test_fp_model` ID-12 layer-C canary + mutation transcript; `test_platform_shim` seam units; `test_numerics_fingerprint` ID-5 partition + `LSE_BACKEND=cpu;LSE_POOL=cpu` env; `test_large_file_io` sparse 4 GiB+1 MiB fixture, probes at 0/2^31−512K/2^31+1M/4G−1M, `LABELS "LARGE_IO"` + `TIMEOUT 120`, ≥ 8 GiB-free self-skip); K = 2 note expanded (quarantine semantics: in-test `LSE_SKIP`, binary exits 0, W never reduced, ledger location + ceiling 5 + review-by 2027-01-08 + the two entries' one-line reasons, KNOWN_FAILURES.md never gains a Windows entry); new `.gitattributes tests/fixtures/** -text` note | registration facts verified verbatim at `tests/CMakeLists.txt:203-221`; guards at `tests/test_large_file_io.cpp:93-123,191`; ledger rows + skip-case counts at `K-LEDGER.md`; counts from `logs/m3-*.txt` and `c8-local-m3-rerun.txt`; `.gitattributes:7` |
| §6 limitations | Tests row: adds "main run green locally and on CI (97/97, LARGE_IO excluded; >2 GiB fixture 1/1 locally)"; Numerics row: cross-OS hash equality now "green at the M3 acceptance SHA `5b16e94`" (was pending); deviations pointer split cycle-1 (06 §4) / cycle-2 (13 §4, D-1…D-18); product-fixes intro: now says **six** defects and calls out #5–#6 as cross-drive correctness fixes that matter to any Windows deployment with cwd and temp/model paths on different volumes — not CI-only | run 5 both-jobs green; D-17 class description (13-log §4) |
| **§7 (NEW)** | Continuous-integration section: triggers (PR / push-to-branch / cron `17 6 * * 1` weekly toolchain-drift alarm + the `gh workflow view … | grep cron` verification line / `workflow_dispatch` with `run_large_io`), concurrency group `win-${{ github.ref }}` cancel-in-progress (push during an acceptance run cancels the evidence), `LSE_LARGE_IO_FLAG` policy (push/PR exclude, schedule include, dispatch asks), windows-job step sequence (21.1.8 pin, msvc-dev-cmd, ntdll guards, canary A/B, census == 98, strict ctest + count assertion, fingerprint pair, K/X/I-5 guards, schedule-only non-gating flake classification, PATH coreutils sink D-15/D-16), linux-parity scope (clang++-21 apt.llvm.org = T3 pair, CPU-only honest label, W == WE+2 structural offset, labeled `^test_reflect$` exclusion, HASH-EQ cross-job equality, ID-11 smoke), same-SHA rule + M4 3× requirement + pending large-io dispatch | every line verified against `.github/workflows/windows.yml` (triggers :57-68, concurrency :70-72, env :74-85, guards :148-159/:220-237, parity :253-330) and 13-log D-10/D-13/D-15…D-18 |
| §8 (was §7) Troubleshooting | "could not query rustc for native libs" upgraded from benign to **fatal-for-link** (fallback set lacks `ntdll.lib` → `undefined symbol NtReadFile/NtWriteFile`), with the `link(1)`-shadow cause, the `/usr/bin/link: extra operand` symptom and the PATH-order fix; "Unable to find executable" reworded to stale-tree advice (no more targeted mode) | D-16 (run 38130378891) proved the fallback path fails the link on current Rust std; the old "not fatal" text was written before that evidence |
| §9 (was §8) Roadmap | cycle-2 state → complete/M3-achieved with run numbers; M4 definition (3× green RC runs + release protocol + version pinning per 13-log §9); K reviews due 2027-01-08; LLVM-22 revisit with the cron as drift alarm; cl.exe still planned-not-landed; **upstream PR deferred to M4 by user decision** with a pointer to this file; artifact table gains STATUS.md, this file, 13-log, 11-plan, windows.yml, K-LEDGER, m3 evidence | 13-log §9; STATUS.md incidents; user decision §6 below |

Numbering side-effect: Troubleshooting/Roadmap renumbered 7→8, 8→9. External
references to `docs/WINDOWS.md §1/§2` (in `windows.yml` comments) are
unaffected; references to "§8 roadmap" inside the frozen 13-log are
historical and stay.

## 3. Inconsistencies found outside this stage's file scope (recorded, not fixed)

1. **`BUILD_FROM_SOURCE.md:57-58` is stale**: "-DLSE_BUILD_TESTS=OFF is
   required for now — targeted test builds only until WP12". WP12 landed; the
   tests-ON full build is the normal path. `docs/WINDOWS.md`'s preamble now
   flags this explicitly, but the file itself needs a one-line fix (drop the
   "required for now" clause, keep the binaries-only snippet + pointer).
   Suggested owner: the M4 doc pass or the orchestrator's STATUS/13-log
   finalization commit. Also note ID-C2-14's acceptance grep
   (`grep -c 'env:CC' BUILD_FROM_SOURCE.md == 0`) would today match the
   retained `$env:CXX` line at `:50` — substring collision in the acceptance
   check itself, worth re-speccing when the file is touched.
2. **07-documentation.md is frozen but its §5 PR draft and §1 ledger counts
   (X = 2, 35-name exposure) are cycle-1 vintage.** Per 07 §3's own rule
   (corrections land in a new numbered artifact), this file supersedes 07 §5
   for PR planning; 07 §1 rows 9/10 are superseded by the live ledgers
   (`COMPILE_EXPOSURE.txt` = 40 names, `COMPILE_EXCLUSIONS.md` X = 0).
3. **No committed transcript for the ROCm clang-cl constexpr bug** — the pin
   rationale lives only in STATUS.md/13-log prose (A-4/ID-C2-14). Acceptable
   (the decision record is the artifact), but an M4 toolchain-watch note
   should keep citing it as prose, not evidence.
4. `logs/m2a-configure.txt` still shows `host compiler: Clang 21.0.0`
   (pre-pin capture). `docs/WINDOWS.md` §3 now annotates this instead of
   silently showing a version that contradicts the §1 pin.

## 4. Doc-maintenance triggers for M4 (extends 07 §3's map)

| Trigger | Required doc updates |
|---|---|
| **Each green release-candidate run (×3)** | Append run URL + duration + SHA to `logs/m4-ci-evidence.md` (L5: create at first run). No `docs/WINDOWS.md` change until the 3rd. |
| **M4 achieved (3× green, same-SHA rule, < 45-min budget)** | `docs/WINDOWS.md` header: "experimental — cycle 2 complete / M3" → "supported (CPU backend) / M4"; retire "Honest scope note" language in §5 that M4 evidence supersedes; §7 same-SHA paragraph → cite the three run URLs; §9 roadmap → next milestone (cl.exe path / HRX spike / upstream merge state). 07 §3's M4 row applies verbatim. |
| **Release protocol + version pinning executed** (02-program-plan; 13-log §9.3) | New short section or `windows-enablement/15-*` artifact: artifact names (`lse.exe`, `lse-server.exe`), versioning scheme, where the RC evidence lives. `docs/WINDOWS.md` §5 gains the released-artifact acquisition line if releases ship binaries. |
| **Large-io hosted proof result (run 38138800778)** | When it lands: §7 "result pending" sentence → outcome + run URL (if green) or triage note (if the runner disk guard fails, the weekly schedule run is the LARGE_IO proof per 11 §6 — record which path was used). |
| **K-ledger reviews (due 2027-01-08)** | Re-test #1 (loopback backpressure) against the then-current Windows networking stack; recheck #2 when a pinned LLVM ships `-freflection`. Either outcome edits `K-LEDGER.md` (append-only rows) + `docs/WINDOWS.md` §4/§6 K sentences + this trigger table. |
| **Toolchain drift (LLVM 22, Rust std changes)** | §1 table re-verified live (07 §3 invariant: re-run commands, don't trust line refs); the weekly cron is the alarm; a pin change touches §1, §7 and `windows.yml` in ONE commit (A-4 truth rule). |
| **Upstream sync (SYNC-n)** | Re-verify every `docs/WINDOWS.md` command + line ref against the new base (07 §3); regenerate `ci-expected-w.txt` from `ctest -N` in any registration-changing commit (ID-C2-10); update STATUS.md sync note (L6/L7). |
| **Upstream PR filed (post-M4)** | §9 roadmap: "deferred to M4" → filed, with PR URLs; this file's §6 gains the filed-against SHA and base age. |

## 5. Refreshed upstream-PR plan (supersedes 07 §5)

### 5.1 Full delta the PR(s) must cover (cycle 1 + cycle 2)

The 07 §5 draft covered cycle 1 only (seam + CMake foundation + "34/34
targeted tests", X = 2, no CI). The refreshed scope is the whole branch delta
vs upstream v0.6.1 (`90d66ab`):

1. **CMake Windows foundation**: clang-cl compiler-ID branch + C++26 dialect
   override (`CMakeLists.txt:18-31`), `/WHOLEARCHIVE` spelling in
   `lse_force_load()`, MSVC warning-wall + `/clang:-fno-fast-math
   /clang:-ffp-contract=off` numerics pins (`cmake/LSECompilerFlags.cmake`),
   raw-dylib stub filter + MSVC-triple lib path (`cmake/LSERust.cmake`), HRX
   force-OFF (`cmake/LSEOptions.cmake:16-22`), `std::memcpy` portable-builtin
   conversions.
2. **Platform seam** `lse::platform` (`platform.hpp`, `platform_{posix,win}.cpp`):
   64-bit-safe positioned I/O, mapping + handle registry, clocks, env, pipes,
   locks — POSIX side inline pass-throughs. Includes cycle-2 defect fixes #2
   (`winsock_compat.hpp` WSASend) and #3 (`map_discard` hybrid) which live in
   seam files.
3. **Communication seam**: `sock_*` wrappers, WSAPoll poller, abstract-
   namespace `kUnimplemented` guard — plus cycle-2 defect fix #5
   (`socket.cpp` drive-letter re-attach for `unix://C:/…` binds).
4. **Six product-defect fixes** (13-log §2/§4; all minimal, zero POSIX
   behavior change): #1 `hub/pull.cpp` close-before-remove/rename (both
   paths), #2 `winsock_compat.hpp`, #3 `platform_win.cpp` map_discard,
   #4 `probe/profile_store.cpp` `write_text_atomic` per-CALL temp name (a
   concurrency race on every platform that only Windows surfaced — repro
   3/50 → 0/100), **#5–#6 cross-drive correctness** (`socket.cpp` drive
   re-attach; `hub/cache.cpp` drive-aware `LSE_MODEL_DIRS` split): these two
   matter to **Windows users generally** — any deployment whose cwd and
   temp/model dirs sit on different volumes (multi-drive workstations, hosted
   runners) hit silent path degradation without them. #4 and #6 keep the
   POSIX code path byte-identical; #5's `_WIN32` branch fixes a
   Windows-only misparse.
5. **Compat test layer + full test port**: `tests/compat.hpp` + `tests/compat/`
   shim include-dir (WIN32-only), ~16 ported test files, X 2→0,
   `.gitattributes tests/fixtures/** -text`, K-LEDGER mechanism (K = 2 ≤ 5).
6. **Four new all-platform tests** (P8): `test_fp_model`, `test_platform_shim`,
   `test_numerics_fingerprint`, `test_large_file_io` — registered on POSIX
   too; they add coverage upstream does not have on any platform.
7. **CI workflow** `.github/workflows/windows.yml` (new file, purely
   additive; upstream `ci.yml` untouched) + the T3 cross-OS fingerprint pair
   mechanism.
8. **Docs**: `docs/WINDOWS.md` (user guide) + the `windows-enablement/`
   decision record (offer upstream a curated subset — the full 14-artifact
   program record may be more governance than an upstream repo wants; ask
   the maintainer).

### 5.2 PR structure: single Batch-1 PR vs split — **recommendation: SPLIT (two stacked PRs)**

**Recommended:** at M4, file **PR-A (build hygiene)** first, then **PR-B
(full Windows enablement)** stacked on it:

- **PR-A — build-system hygiene** (the original Batch-1 of 02 §4.4 / 08 §4
  item #9, ≤ ~300 diff lines): `lse_force_load`/`/WHOLEARCHIVE`, compiler-ID
  flag branch incl. the FP pins, C++26 dialect override, `std::memcpy`
  conversions, LSERust raw-dylib filter.
- **PR-B — everything else** (§5.1 items 2–8): seam, comm, the defect fixes
  that touch upstream-owned files (#1, #4, #5, #6 + the `test_hub.cpp`
  regression extension), compat layer + ports, P8 tests, `windows.yml`,
  docs.

**Rationale:**
1. *Conflict economics*: `CMakeLists.txt` (94 upstream commits since
   2026-08-01) and `tests/CMakeLists.txt` (81) are the #1/#2 conflict
   concentrators (08 §3.2). PR-A is exactly that surface; landing it early
   permanently shrinks every future sync — and it stays mergeable even if
   PR-B stalls in review.
2. *Reviewability*: upstream churns ~125 commits/week; a single ~7k-line PR
   ages badly and is hard to accept atomically. A ≤ 300-line idiom-matched
   PR-A (precedent: existing `if(APPLE)` branches) has high acceptance odds.
3. *Independent value*: defect fixes #4 (all-platform race) and #5–#6
   (cross-drive correctness) are correct-on-every-platform fixes upstream
   may want regardless of whether it adopts Windows support; inside PR-B
   they should be a separable commit (or a PR-B1 mini-PR if the maintainer
   prefers) so they can merge alone.
4. *Evidence partitioning*: PR-A rides upstream's own `ci.yml` (zero Windows
   dependency); PR-B attaches the M4 evidence bundle (§5.3). One monolith
   forces all evidence into one review conversation.

**Honest caveats:** (a) the user's M4-deferral means PR-A is NOT filed early
(08 DP2 recommended filing it immediately — that recommendation is superseded
by the user decision, §6); both PRs file at M4, and PR-A's conflict-reduction
value decays the longer upstream churns. (b) Stacking adds rebase choreography
across two open PRs. (c) Fallback: if the maintainer prefers one PR, combine —
but keep build-hygiene as the first commit so the review can proceed in the
same order.

### 5.3 M4 evidence that MUST be attached when filed

- **3× consecutive green `windows.yml` runs** (both jobs, same-SHA rule per
  11 §6) on the release-candidate SHA(s): run URLs + durations from
  `logs/m4-ci-evidence.md` (L5), each < 45 min (PR3 budget).
- **Linux-parity green on each of those SHAs** (T3 pair): fingerprint HASH-EQ
  hashes equal across the two jobs' artifacts; Wₗ == WE + 2 census offset.
- **Large-io hosted proof**: the dispatched `run_large_io=true` run (38138800778
  or its successor) or, if the runner disk guard fails, the weekly-schedule
  run showing LARGE_IO included — record which path.
- **Local M3 protocol transcripts**: `logs/m3-*.txt` +
  `windows-enablement/logs/c8-local-m3-rerun.txt` (97/97, LARGE_IO 1/1,
  fingerprint 2/2, K = 2, X = 0, W = 98).
- **Ledger states at the filed SHA**: `COMPILE_EXCLUSIONS.md` (X = 0),
  `K-LEDGER.md` (K = 2 ≤ 5, issues #1/#2 filed on the fork, review-by dates),
  zero Windows entries in `tests/KNOWN_FAILURES.md`.
- **Defect repro evidence** for the six fixes: `d17-crossdrive-repro.txt`
  (#5–#6, `subst X:` byte-identical runner signature), 13-log D-14 repro
  counts (#4, 3/50 → 0/100), `id12-fp-canary-mutation.txt` (canary
  effectiveness).
- **Release-protocol record**: version pinning + RC artifact runs on
  `lse.exe`/`lse-server.exe` (02-program-plan; 13-log §9.3).
- **Base freshness**: sync note showing the PR base is ≤ 7 days old at
  filing (08 §3.3 cadence rule) + N (rev-list count).

### 5.4 PR title/body skeleton for PR-B (refreshes 07 §5's draft)

Title: `Windows x64 support (clang-cl + MSVC STL, CPU backend): full test suite green in CI, six cross-platform defect fixes`

Body sections: What (seam + CMake foundation + compat layer + CI, one
paragraph each) · Verification (M4 evidence list, §5.3, with run URLs) ·
Product fixes (the six, #4/#5/#6 highlighted as valuable independent of
Windows adoption) · What this PR does NOT do (no GPU/HRX on Windows; K = 2
ledgered capability gaps with review-by dates; WSAPoll ms granularity;
heap_stats zeros; abstract UNIX sockets unimplemented as on macOS) ·
Governance (additive-first: every delta `#ifdef _WIN32`/`if(WIN32)` or a
seam call; POSIX behavior byte-preserved; decision record in
`windows-enablement/`) · Suggested review path (1. `windows.yml` + run URLs;
2. `platform_win.cpp` header; 3. defect-fix commits; 4. 13-log §4 deviations).

## 6. Deferred-decision trail

- **Decision:** upstream PR to `Geramy/LSE` deferred until M4.
- **Decided by:** the user (orchestrator relayed), cycle-2 Stage 6, 2026-10-11.
- **M4 bar (13-log §9):** CI green 3× (both jobs, same-SHA), Linux parity on
  those SHAs, release protocol executed on Windows artifacts, version pinning.
- **Supersedes:** 08 §6 DP2's recommendation (a) "file Batch-1 now, parallel
  with WP11/WP12" — the deferral is deliberate: the PR files once, with the
  full M4 evidence bundle, instead of twice with partial evidence.
- **Consequence accepted:** continued merge-conflict exposure on the two hot
  CMakeLists at every sync until PR-A lands (08 §3.2 quantifies it; the
  weekly-sync cadence bounds it).
- **Contributing constraint at decision time:** `windows.yml` concurrency
  group (`win-${{ github.ref }}`, cancel-in-progress) — pushes/dispatches
  during the in-flight large-io proof run would cancel it; this stage
  therefore committed WITHOUT pushing, and the orchestrator pushes after the
  run completes.
- **Status of the 07 §5 draft:** REFRESHED (this file §5), NOT FILED.

## 7. Self-check

- [x] Only `docs/WINDOWS.md` + this file modified; nothing pushed
      (orchestrator pushes after run 38138800778 completes).
- [x] No invented flags/counts: every command, label, timeout, env var, cron
      line and count re-verified against `CMakeLists.txt`, `cmake/*.cmake`,
      `tests/CMakeLists.txt`, `tests/test_large_file_io.cpp`,
      `.github/workflows/windows.yml`, `K-LEDGER.md`,
      `COMPILE_EXCLUSIONS.md`, `ci-expected-w.txt` (98), `.gitattributes`
      at `5b16e94`.
- [x] Expected-output blocks in `docs/WINDOWS.md` copied from real
      transcripts (`c2-sync-configure-tests.txt`), not typed.
- [x] 07 §3 standing invariants honored: no claim without a command/file
      ref; limitations section only gained precision (nothing removed while
      its in-code deviation note still exists); large-io dispatch outcome not
      claimed.
