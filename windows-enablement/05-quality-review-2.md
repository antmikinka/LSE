# LSE Windows Enablement — Stage 5: Quality Review (Second Pass)

**Branch:** `feature/windows-enablement` @ `a5b82f6` (verified: `git rev-parse HEAD` = `a5b82f689e219c388a00817d13db64f57ca403d3` — matches the base claimed by all four artifacts)
**Date:** 2026-10-10
**Reviewer:** Stage 5 (quality review, second pass) — structured via sequential-thinking before writing
**Artifacts under review:** `01-planning-analysis.md` (S1), `02-program-plan.md` (S2), `03-quality-review-1.md` (S3), `04-test-strategy.md` (S4) — all four read fully, in order
**Constraints honored:** no source modified, no prior artifact modified, nothing committed. Where an artifact needs correction, the correction is issued as an **Implementation Directive (ID-n)** below — artifacts stay immutable; Stage 6 treats §7 of this review + S4 §7 as the authoritative correction layer over S1/S2 wherever they conflict.

---

## 1. Review Method

1. **Full sequential read** of all four artifacts; built a closure matrix for S3's 4 BLOCKING (BL-1..BL-4) and 5 MAJOR (MJ-1..MJ-5) issues, classifying each as RESOLVED / PARTIALLY RESOLVED / OPEN by S4 (or this review).
2. **Independent repo re-verification (~25 discrete claims, well past the 6-claim minimum)** — results in §6. Covered: hardcoded `/tmp` sites, abstract AF_UNIX in `socket.cpp`, the 4 unconditional `lse_tokenizer` linkers, `-fno-fast-math` / absence of `-ffp-contract`, ctest totals (102 + 8 = 110), `.github/workflows/ci.yml` structure and pins, `macos.yml` LLVM 21, KNOWN_FAILURES dual mechanism (harness runtime + configure-time DISABLED), `tests/data` absence, `file_read.cpp`/`weights.cpp` large-file paths, and a full re-derivation of the loomc/hipc compile-exposure set from `tests/CMakeLists.txt`.
3. **S3 gap re-verification:** G1 (`getrlimit`), G2 (`localtime_r`), G3 (`statvfs`), G4 (`S_ISREG`/`lstat`), G5 (`dirent` in rdma.cpp) all re-confirmed at the exact cited lines — S3's gap hunt was accurate.
4. **Cross-artifact arithmetic:** bucket totals (69+10+10+3=92; 92−9=83 fallback), W derivation (102−12+2), K/X accounting vs SC5, 35-binary compile-exposure count.
5. **Implementability walk-through** as the solo Stage-6 developer on Windows: for every gate command, asked "would this command, as written, actually succeed?" — this found two infeasibilities nobody had caught (§3.3, §6 row 18) and one unsound numeric-equality gate (§3.4).
6. **UCRT/Win32 semantics audit** of the *recommended fixes* (not just problem statements), continuing S3 §5's approach — produced the ID-1 fix spec the mandate requires.

---

## 2. BLOCKING Closure Table

| S3 issue | Status after S4 | Evidence / residual gap | Residual action |
|---|---|---|---|
| **BL-1** — inventory misses G1 rlimit / G2 `localtime_r` / G3 `statvfs` / G4 `S_ISREG`+`lstat` (all re-verified exact: `devices.cpp:8,552-560`; `chat_template.cpp:1546`; `pull.cpp:12,539-540` + `dflash2_convert.cpp:24,688-689`; `jit_cache.cpp:339`, `cache.cpp:144,147,415`, `weights.cpp:841`) | **PARTIALLY RESOLVED** | S4 §4 gives every gap a direct shim unit test (incl. the `localtime_s` reversed-arg trap detector) → *verification* closed. But no artifact amends the WP3 shim API list or the WP5/WP7 change lists; `chat_template.cpp` (lse_model) is in **no WP's scope at all**; `raise_fd_limit`, `statvfs`, `lstat` call-sites are assigned to no named WP line. | **ID-2** (exact shim API surface) + **ID-9** (WP scope amendments). |
| **BL-2** — UCRT 32-bit `_pread`/`off_t`/`stat` recipe silently corrupts >2 GB reads | **PARTIALLY RESOLVED — test side CLOSED, fix-spec side OPEN** | S4 §3 covers **both** required paths: (a) pread path — `read_file_range` probes at 0, 2³¹−512KiB straddle, 2³¹+1MiB, >2³² with SHA256 + a single boundary-spanning call (verified the production code matches: chunk `1<<30` at `file_read.cpp:17`, `static_cast<off_t>(offset+done)` at :22-23 — the cast itself truncates, so the *signature* must change, not just the implementation); (b) mmap path — synthetic safetensors with data beyond 2³¹ loaded through `SafeTensors::map_file` (`weights.cpp:212-250` — verified `fstat`:217 → `st_size`:221 → `mmap(file_size)`:227). The corrective APIs (`_lseeki64`/`_stat64`/`ReadFile`) exist only as S3 G8 prose — **no signatures anywhere an implementer will find**. Two additional traps no artifact covers: `_lseeki64`+`_read` races when the streaming fd is shared (weights.cpp keeps `read_fd` at :232-237 for positioned reads) → `ReadFile`+`OVERLAPPED` must be primary; `CreateFileMappingW` fails unless the file was opened `FILE_SHARE_READ` → `_wsopen_s(_SH_DENYNO)` mandatory. | **ID-1** — the full normative fix spec is written into this review (§7 ID-1). |
| **BL-3** — 35 loomc/hipc-bearing test TUs + WP10 "full build" collision + §4.3-rule-5 freeze conflict | **SUBSTANTIALLY RESOLVED, with 3 residual mechanics** | S4 §1.2/§7 adopt `-DLSE_BUILD_TESTS=OFF` in the M2a gate command, the X ledger (`windows-enablement/COMPILE_EXCLUSIONS.md`), and the governance carve-out. Residuals: (1) **enumeration defect** — the count "35" is correct (I re-derived: 36 `target_sources` recipients minus HRX-gated `test_matrix_probe_lifecycle` = 35) but the listed names total **34 — `test_attention_short` (`tests/CMakeLists.txt:505-510`, unconditional loomc block) is omitted**; the M2a checklist says "all 35 §1.2 binaries", so the implementer following the list literally misses one. (2) **New contradiction introduced by the fix itself:** with `BUILD_TESTS=OFF` the registration-smoke ctest (SC3, an M2a criterion) isn't built; S4's M2a then demands a "tests-ON experimental configure compiles all 35" — but a tests-ON build of `all` also builds the 15 `<unistd.h>` test files and `test_server_shutdown` (`fork`/`sys/wait.h`), which **cannot compile before WP12** → the gate command as written is infeasible. Fix: **targeted** build (`cmake --build build --target <35 names + smoke>`; Ninja supports per-target), never `all`, until WP12. (3) The smoke test has no name/location in any artifact. | **ID-3** (gate mechanics + corrected list + programmatic `COMPILE_EXPOSURE.txt`), **ID-11** (smoke identity). |
| **BL-4** — KNOWN_FAILURES disables execution only; SC5/T5 misconception | **RESOLVED at strategy level; wording amendment outstanding** | S4 §5.3 mechanism facts **verified exact** against `tests/harness.hpp:47-66` (runtime `binary:case` skips, `LSE_RUN_KNOWN_FAILURES` override) and `tests/CMakeLists.txt:597-606` (whole-binary regex → `DISABLED TRUE`; the regex correctly excludes `:case` entries). S4 §7 restates SC5 with K (runtime) vs X (compile) separation. Residuals: (a) S2's SC5/M3 still reads "K = KNOWN_FAILURES.md Windows entries" — which S4 rule 1 **forbids** for Windows-only skips (the file is platform-agnostic — verified: harness reads it on every platform, CMake parses it on every platform); an implementer reading only S2 does it wrong. (b) **Unit mismatch:** `LSE_SKIP` keeps the binary's exit code 0 (verified `run_all`: skip path increments `skipped`, not `failures`) → skipped-case binaries still PASS ctest, so "≥ (W−K) of W pass" with K counting *case groups* mixes binaries and cases. | **ID-6** (normative restatement: all W entries pass-or-disabled; K ≤ 5 Windows-disabled *case groups*; X ≤ 5 compile-excluded *binaries*, reducing W). |

**MAJOR closure:** MJ-1 (rdma `dirent`) — verification closed by S4 §4; spec via ID-9. MJ-2 (WP12/WP13 deps deadlock vs Q1 fallback) — **still OPEN in every artifact**, and S4's own P6 fix is assigned to WP12 which sits *behind* the deadlocked dep (see §3.2) → ID-4 + ID-7. MJ-3 (`ssize_t`/`socklen_t`) — closed by S4 §4 static-asserts + ID-2 typedefs. MJ-4 (M_win baseline instability) — **RESOLVED** (S4 §1.1/§5.1/§7: re-capture W at M2a/M3, committed `ctest -N` diffs). MJ-5 (cl.exe undercount) — numerics half resolved by S4 §2.2 pin + canary; one S3 mischaracterization to record: the pragma at `dflash2_convert.cpp:38` is wrapped in `#if defined(__clang__)` (:37-39), so cl.exe never sees it — "silently ignored" is the wrong failure mode (the effect is similar; MSVC `/fp:precise` doesn't fuse across statements anyway) → ID-14.

---

## 3. Cross-Artifact Consistency Findings

### 3.1 Bucket counts vs milestone gates — consistent after restatement
W=92 derivation re-verified against the repo: 102 `lse_add_test` − 12 HRX-gated = 90, + `test_server_shutdown` (:158, ungated) + `test_server_cli` (:274-278, Python3+`lse-server`, both present on runners) = 92 ✓; 92 = 69(a)+10(b)+10(d)+3(e-excl) ✓; fallback 83 = 92 − 9 ✓. All conditionals hold (`LSE_ENABLE_CPU` default ON — `LSEOptions.cmake:7`; `LSE_BUILD_TESTS` default ON — `:3`). S2's M3 "K≤5" and S4's "K≤5 runtime + X compile ledger" are **not contradictory** — S4 §7 is an explicit, sound restatement ("SC5 restated with the BL-4 correction") — but S2's text is now wrong in two ways (K source, units) and must be read as superseded (ID-6).

### 3.2 The 4-test tokenizer link hazard vs WP8/WP11 — reflected in S4 only, and its fix is mis-sequenced
S4 §1.1's finding is **verified exact**: `tests/CMakeLists.txt:129,130,135` (`test_server`, `test_chat_protocol`, `test_api_parity`) and `:196` (`test_memory_lifecycle`, inside `if(LSE_ENABLE_CPU)` but unconditional w.r.t. tokenizer) link `lse_server`/`lse_tokenizer`, targets that exist only under the cargo gate (`CMakeLists.txt:439-465`). If WP8 fails, these are **link errors, not clean exclusions** (CMake treats missing target names as `-l` flags → link-time failure). S4 assigns the 4-line P6 guard to **WP12** — but WP12 deps=WP11 deps=WP8, so on the exact failure path P6 exists to protect, P6 can never land. **The fix must be hoisted to WP1** (pure additive `if(TARGET lse_tokenizer)` guards; no-op on Linux where the target exists; §4.3-compliant). Neither WP8's nor WP11's risk text mentions the hazard → ID-4, ID-7.

### 3.3 The 35-set vs S2 WP10 vs S1 F1 — reconciled in substance, defective in enumeration
S1 F1 gates the HRX *backend library* off but is silent on the fact that tests compile 8 loomc/hipc TUs directly; S1's P1 scope sentence ("everything except `src/backends/hrx/**` compiles") remains false-as-written; S4 §1.2 supplies the true mechanism and supersedes. My re-derivation confirms **count 35 is right** but the named list is **34** (`test_attention_short` omitted), and §1.2's "every `if(NOT TARGET lse_backend_hrx)` block" line list omits the two `if(TARGET…) else()` blocks at `:455-467` (`test_cooperative_rms`) and `:476-488` (`test_wave32_l2`) — both tests *are* named in S4's group 2, so substance holds; refs need correcting. The corrected, authoritative set = every target in `target_sources(<t> … src/backends/hrx/…)` minus `test_matrix_probe_lifecycle`; ID-3 mandates generating it programmatically at WP10 (`COMPILE_EXPOSURE.txt`) rather than trusting any hand list.

### 3.4 NEW — T3 cross-platform hash equality is unsound as written (would make M4 unreachably red)
S4 §2.2 T3 requires the Windows-clang-cl fingerprint hash to **equal** the Linux-parity clang++-21 hash for GEMV/RMSNorm/**softmax/rope/attention-step** outputs. Softmax/rope/attention call transcendentals (`std::exp`, `std::tanh`, `std::sin/cos`), which come from the **STL** (MSVC-STL vs libstdc++), not from LLVM codegen; the two implementations differ in last ulps by design. Pinning `-ffp-contract=off` on both sides does **not** make `exp()` bit-identical across STLs. The T3 equality gate would fail on day one for reasons that are not porting defects — exactly the false-red loop PR3/PR7 fear. **Fix (ID-5):** partition the fingerprint — SHA256 hash-equality only for transcendental-free ops (IEEE-exact `+ − * /`, `std::sqrt`; T1's sha256/quant/fp8/dtype tiers are integer ops and already safe); kernels calling any transcendental compare `max_rel` vs committed goldens per-platform, no cross-platform hash equality. S4's own §2.1 logic ("the suite is tolerance-based, not bit-exact cross-toolchain") already proves the point — T3 contradicts §2.1.

### 3.5 Other consistency notes
- S4 §7 M2a requires **Linux clang++-21 parity from M2a**, while S2 M2a says "Linux/macOS parity green" (compiler unspecified) and WP0 provisions only Windows tooling → ID-13 (WP0 provisions WSL clang++-21, or g++-16 parity accepted at M2a with clang++-21 mandatory from M3).
- S4 §5.2 parity ctest doesn't state `--label-exclude LARGE_IO` while §5.1's Windows ctest does → an 8-GiB-sparse surprise on ubuntu PR runs; ID-10.
- S4 §5.1 YAML: `${{ inputs.run_large_io && '' || '' }}` is a **literal no-op** (both branches empty) — `run_large_io` can never enable LARGE_IO; ID-10.
- S1 T1 says "19 `unistd.h` test files"; actual grep = **17** total, **15** Windows-registered (S4's number is correct; S3 missed the S1 error) → ID-14.
- `lse_add_test` links test exes **without** whole-archive (verified `tests/CMakeLists.txt:4-16`) — Windows COFF member extraction is symbol-driven like ELF, so test-side registration parity holds; WP2's 8 sites are library-level only. Recorded so Stage 6 doesn't "fix" test link lines (ID-14 note).
- `windows.yml` passes `-DLSE_ENABLE_HRX=OFF` explicitly even though WP1 defaults it OFF on WIN32 — consistent belt-and-braces; matches SC1 command exactly ✓.
- fastokens pin in S4's YAML (`7973014e…`) matches `ci.yml:36` exactly ✓.

---

## 4. WP Amendments Required (exact, WP-by-WP)

Binding on Stage 6; full rationale in §7 directives.

| WP | Amendment |
|---|---|
| **WP0** | Smoke project must *prove* clang-cl accepts `-fno-fast-math`, `-ffp-contract=off`, the full `-W` set, `/std:c++latest` (S3 §5 requirement; S4 §2.1 "proven in WP0, not assumed"). Provision Linux/WSL **clang++-21** (apt.llvm.org) or record the M2a-parity compiler decision. Run the `test_fp_model` canary source under **g++-16 with `-std=c++26` strict** to empirically settle the GCC fp-contract default (S4 §2.1 assumes `fast`; repo builds with `CMAKE_CXX_EXTENSIONS OFF` — record the answer in DECISIONS.md). |
| **WP1** | Add Windows-only `_CRT_NONSTDC_NO_DEPRECATE` + `_CRT_SECURE_NO_WARNINGS` (MN-2). Add `-ffp-contract=off` to the MSVC/clang-cl branch of `lse_apply_common_flags` (S4 §2.2). **Hoist P6 here**: wrap `tests/CMakeLists.txt:129,130,135,196` registrations in `if(TARGET lse_tokenizer)` (ID-4). Introduce **no new CMake options**; Windows knobs = existing options + `-DCMAKE_CXX_COMPILER=clang-cl -G Ninja`; `LSE_ENABLE_HRX` defaults OFF under `if(WIN32)`; `LSE_BUILD_TESTS` stays default ON (gates pass `-D` explicitly). |
| **WP2** | Name the smoke test **`tests/test_registration_smoke.cpp`**, registered via `lse_add_test` (harness-based). Census **all 8** self-registering archives (PR2), each count > 0 and equal to Linux counts captured in `logs/` (ID-11). |
| **WP3** | The normative API surface is **ID-2 + ID-1** of this review (supersedes the prose lists in S1 B1 / S2 WP3). Includes: int64 file ops (never `off_t` in a Windows-compiled signature), `_wsopen_s(_SH_DENYNO)`, `ReadFile`+`OVERLAPPED` pread (primary; `_lseeki64`+`_read` forbidden on shared fds), `_stat64`/`_fstat64`, opaque `FileMap` record holding both mapping and file handles, `ssize_t`/`socklen_t` typedefs, `STD{IN,OUT,ERR}_FILENO`, `localtime_r` wrapper (reversed-arg note), `statvfs`→`GetDiskFreeSpaceExW`, `S_ISREG/S_ISDIR` + lstat substitute, `raise_fd_limit` no-op, home = `HOME`→`USERPROFILE` (ID-8), `DiscardVirtualMemory` zero-fill doc note. |
| **WP4** | `file_read.cpp` call sites per ID-1: replace `static_cast<off_t>(offset+done)` (:22-23) with the int64 shim call; `open_for_streaming` (:40-53) via `_wsopen_s`. |
| **WP5** | **Add `src/models/chat_template.cpp:1546` (`localtime_r`) — currently in no WP** (G2). `weights.cpp`: `:216-221` fstat→shim `file_size_fd`, `:227-243` mmap→shim `file_map_ro`, `:841` `S_ISREG`→shim, `hf_cache_root` HOME policy per ID-8. `dflash2_convert.cpp`: `:247-250` `pread_exact`→`pread64`, `:688-689` statvfs→shim. |
| **WP6** | `rdma.cpp:19,92-101`: `dirent`→`std::filesystem::directory_iterator` inside the Windows guard (G5/MJ-1). `<afunix.h>` included **after** `<winsock2.h>` for `sockaddr_un` (MN-3). |
| **WP7** | `devices.cpp:8,550-561` `raise_fd_limit`→shim no-op (G1); `pull.cpp:539-540` statvfs→shim, resume writes→`pwrite64` (WriteFile+OVERLAPPED twin); `cache.cpp:144,147,415` `lstat`/`S_ISREG`→shim (G4); LogCapture `STDERR_FILENO` constants (MN-1). |
| **WP8** | Note in acceptance: the 4-test link hazard is pre-neutralized by WP1's P6 hoist; R4's impact line ("silently drops … ~15 tests") is superseded by S4 §1.1's precise "9 entries vanish + 4 hard link errors (guarded at WP1)". |
| **WP10** | Gate = **two configures**: (i) `-DLSE_BUILD_TESTS=OFF` full build (libs + exes); (ii) tests-ON experimental configure + **targeted** build `cmake --build build --target <corrected 35 + test_registration_smoke>` — **never `all`** before WP12 (ID-3). Generate `windows-enablement/COMPILE_EXPOSURE.txt` programmatically (grep `target_sources` for `src/backends/hrx`), including `test_attention_short`. Failures → `COMPILE_EXCLUSIONS.md` ledger (X ≤ 5, issue links). |
| **WP11** | Add S4 §7 M2b sentinel: `test_tokenizer` green on Windows. |
| **WP12** | K/X accounting per ID-6; P6 verification only (landed in WP1); T1 unistd count = **15 files** via `tests/compat.hpp` (not 19); M3 evidence requires materialized `tests/data/reference.safetensors` (S4 §2.3) on both platforms. |
| **WP13** | Fix the dispatch-input no-op; add `--label-exclude LARGE_IO` to the parity job; weekly schedule includes one LARGE_IO run; M4 3× protocol per S4 §5.3.5 verbatim. |
| **WP14** | Document: Windows HOME env behavior (ID-8), K-vs-X policy, `COMPILE_EXCLUSIONS.md` location/format, `HOME`-first cache resolution for testability. |
| **§4.3 rule 5 / PR5** | Carve-out (S4 §1.2.4 made binding): a Windows-compile fix *inside* a loomc/hipc TU is permitted only after the compile-gate path is exhausted, with a DECISIONS.md entry + upstream issue. |

---

## 5. Ambiguity Register with Mandated Resolutions

| # | Ambiguity (where Stage 6 would otherwise guess) | Mandated resolution |
|---|---|---|
| AM-1 | Exact shim API surface for B1 — prose-only in S1/S2 | **ID-1 + ID-2 signature tables are normative.** Namespace `lse::platform`; POSIX side = inline pass-through wrappers; no `off_t`/`struct stat` in any Windows-compiled signature. |
| AM-2 | HOME vs USERPROFILE on Windows (S4 P5 explicitly leaves it open: "decision needed from WP5") | **`home_dir()` order: `getenv("HOME")` if non-empty → `USERPROFILE` → error.** `HF_HOME`/`HF_HUB_CACHE`/`LSE_CACHE_DIR` remain authoritative above both (already are). Rationale: `test_model.cpp:312-328` ScopedEnv-HOME asserts then pass in logic unmodified; real Windows shells rarely set HOME so production = USERPROFILE (matches Python hf-hub `expanduser`); `~`/`$HOME` expansion in `expand_path` must consult the same resolver. Cache-root strings built with `/` separators; P5's test port makes asserts separator-tolerant. |
| AM-3 | Where COMPILE_EXCLUSIONS.md lives; KNOWN_FAILURES format | `windows-enablement/COMPILE_EXCLUSIONS.md`, columns: test \| guarded CMake line \| reason \| issue link. KNOWN_FAILURES.md stays `tests/KNOWN_FAILURES.md`, existing format; **Windows-only case skips never enter it** (verified platform-agnostic) — use `#ifdef _WIN32 LSE_SKIP("windows: <capability|flake> — <issue #N> — review-by <date ≤ 90d>")`. |
| AM-4 | K vs X vs W units in SC5 (binaries vs case-groups) | ID-6: gate = **all W ctest entries pass or are configure-disabled with reason**; K = Windows-disabled *case groups* ≤ 5; X = compile-excluded *registered binaries* ≤ 5, each reducing W; W fixed at the M3 configure, diffed vs M1/M2a. |
| AM-5 | New CMake option names for Windows | **None invented.** Existing options only; `LSE_ENABLE_HRX` defaults OFF under `if(WIN32)`; explicit `-DLSE_ENABLE_HRX=OFF` still passed in SC1/gate/CI commands (matches S2 SC1 verbatim). |
| AM-6 | Smoke test identity & pass criteria | `tests/test_registration_smoke.cpp`, `lse_add_test`-registered, census all 8 archives vs committed Linux counts (ID-11). Built/run at M2a via the WP10 targeted tests-ON build. |
| AM-7 | T3 fingerprint equality across STLs | ID-5 partition: HASH-EQ set = transcendental-free kernels only; TOLERANCE set (max_rel vs goldens, per-platform) = anything calling exp/tanh/sin/cos/log/pow. The fingerprint test prints which set each kernel is in. |
| AM-8 | Canary constant-folding | `test_fp_model.cpp` operands must be runtime-opaque (`volatile` sink or argc/getenv-derived) so `a*b+c` cannot be folded at compile time — otherwise the canary passes even with the pin dropped (ID-12). |
| AM-9 | M2a Linux-parity compiler | clang++-21 preferred from M2a (WP0 provisions it in WSL); if unavailable, g++-16 parity accepted at M2a with clang++-21 **mandatory from M3** (the T3 controlled pair needs same-LLVM-major). Logged in DECISIONS.md either way (ID-13). |
| AM-10 | GCC-16 `-ffp-contract` default under strict `-std=c++26` (`CMAKE_CXX_EXTENSIONS OFF`) | Empirically probed in WP0 via the canary under g++-16; result recorded; mitigation (explicit pins on both CI paths) is invariant to the answer (ID-13). |
| AM-11 | LARGE_IO in parity/PR CI | Excluded by label in **both** jobs' PR/push runs; included in the weekly schedule (≥1 run inside the M4 3× window per S4 §7) and mandatory locally at M3 (ID-10). |
| AM-12 | Test-exe link lines & whole-archive | Do **not** add `/WHOLEARCHIVE` to `lse_add_test` link lines — POSIX side doesn't use it either (verified :4-16); symbol-driven extraction parity holds (ID-14 note). |

---

## 6. Stage-4 Spot-Check Results (independent re-verification at `a5b82f6`)

| # | S4 claim | Verdict | Actual |
|---|---|---|---|
| 1 | `test_graph.cpp:3155` `/tmp/lse-cold-spans-` | **EXACT** | `:3155` verbatim (uses `::getpid()` too) |
| 2 | `test_dflash2.cpp:1610` `/private/tmp/…` | **EXACT** | `:1610` `/private/tmp/lse-dflash2-split-ring-` |
| 3 | `test_tree_verify.cpp:703` mkdtemp `/tmp/lse-trials-XXXXXX` | **EXACT** | `:703-704` |
| 4 | `test_model.cpp:312-328` `HOME=/tmp/lse-fake-home` + `hf_cache_root()` string asserts | **EXACT** | `:312,316,320,324,327-328`; `hf_cache_root` at `weights.cpp:499`, `HOME` reads `:452,:493` |
| 5 | Abstract AF_UNIX `sun_path[0]='\0'` in `socket.cpp`, macOS rejection `:129-130` | **EXACT** | `:128` abstract flag, `:129-130` `#ifdef __APPLE__` kUnimplemented, `:139` `sun_path[0]='\0'` |
| 6 | 4 tests link `lse_tokenizer` unconditionally (`:129,130,135,196`) | **EXACT** | all four confirmed; `:196` is inside `if(LSE_ENABLE_CPU)` but ungated w.r.t. tokenizer; targets exist only under the cargo gate |
| 7 | `-fno-fast-math` in `cmake/LSECompilerFlags.cmake`; no `-ffp-contract` anywhere | **EXACT** | `:38`; repo-wide grep of all build files → zero `-ffp-contract` hits |
| 8 | 102 `lse_add_test` + 8 raw `add_test` = 110 total | **EXACT** | counts 102/8 confirmed; raw entries at `:158,275,325,417,446,449,585,590` (S4's `:274-278` block ref ✓) |
| 9 | W = 92 arithmetic (90 + 2; 12 HRX-gated; fallback 83) | **CONSISTENT** | 12 HRX-gated names/lines all verified; `LSE_ENABLE_CPU` ON default (`LSEOptions.cmake:7`); conditionals as S4 states |
| 10 | `ci.yml`: workflow_dispatch-only, self-hosted GPU, g++-16 at `:95`, fastokens pin `:36` = `7973014e…` | **EXACT** | `:6-7`, `:28`, `:95`, `:36` — S4's YAML mirrors the pin correctly |
| 11 | macOS clang 21 (`macos.yml` `brew install llvm@21`) | **EXACT** | `macos.yml:55` |
| 12 | 11 abstract-socket sites in `test_communication.cpp` | **EXACT** | all 11 line numbers (`196,327,368,434,611,663,771,1025,1100,1392,1452`) match |
| 13 | `CPPHTTPLIB_USE_POLL` at `test_api_parity.cpp:33`, `test_hub.cpp:24`, `test_server_shutdown.cpp:2`, `http_server.cpp:13` | **EXACT** | all four |
| 14 | KNOWN_FAILURES mechanism facts (§5.3): runtime case-skips via harness; configure-time whole-binary DISABLED; platform-agnostic | **EXACT** | `harness.hpp:47-66` (incl. `LSE_RUN_KNOWN_FAILURES` escape), `tests/CMakeLists.txt:597-606` (regex excludes `:case` entries from disabling) |
| 15 | `tests/data/` absent → goldens silently no-op; `scripts/dump_reference.py` exists; `LSE_REFERENCE` env | **EXACT** | `ls tests/data` → missing; `reference.hpp:1-5,18-26` as described; `LSE_EXPECT_NEAR` at `harness.hpp:~152-162` ✓ |
| 16 | `file_read.cpp` chunk `1<<30`, `static_cast<off_t>` truncation point, `kIoError` short-read; `weights.cpp:212-250` map_file/fstat/mmap/streaming-fd | **EXACT** | `:17,:22-23,:30-34`; `:212,:217,:221,:227,:232` |
| 17 | `#pragma clang fp contract(off)` at `dflash2_convert.cpp:38` | **EXACT (+1 detail)** | `:38`, wrapped in `#if defined(__clang__)` `:37-39` — see ID-14 (corrects S3 MJ-5's "silently ignored by cl.exe") |
| 18 | "35 Windows-registered tests compile loomc/hipc TUs" | **COUNT CORRECT, LIST INCOMPLETE** | re-derived 35 (36 `target_sources` recipients − HRX-gated `test_matrix_probe_lifecycle`); S4 names only 34 — **`test_attention_short` (`:505-510`) omitted**; §1.2 block-ref list also omits the `else()` blocks `:455-467`, `:476-488` (tests themselves are named) → ID-3/ID-14 |
| 19 | S3 gaps G1-G5 that S4 builds on | **ALL EXACT** | `devices.cpp:8,552-560`; `chat_template.cpp:1546`; `pull.cpp:12,539-540`; `dflash2_convert.cpp:24,688-689`; `jit_cache.cpp:339`; `cache.cpp:144,147,415`; `weights.cpp:841`; `rdma.cpp:19,92,95` |
| 20 | GCC-16 default `-ffp-contract=fast` (§2.1) | **PLAUSIBLE, NOT REPO-VERIFIABLE** | repo builds strict `-std=c++26` (`CMAKE_CXX_EXTENSIONS OFF`); GCC's fast default is documented for GNU dialect — empirical probe mandated (ID-13). Mitigation is invariant either way. |
| 21 | "15 Windows-registered unistd test files" (§1.3) | **EXACT** | grep: 17 files total include `<unistd.h>`/`<sys/wait.h>`; minus HRX-gated `test_jit.cpp`, `test_capture_prefill.cpp` = 15 ✓ (S1's "19" was wrong — ID-14) |

**Error count in S4: zero material factual errors.** One enumeration omission (row 18), one inherited-infeasible gate command (§3.3), one unsound equality gate (§3.4), one dead YAML expression (§3.5).

---

## 7. Implementation Directives (authoritative correction list for Stage 6)

Artifacts 01-04 remain immutable. **Where an ID conflicts with S1/S2/S4 text, the ID wins.** S4 §7 + this section = the operative spec.

### ID-1 · BLOCKING — 64-bit file-I/O fix spec (BL-2 corrective; supersedes S1 C1/B1 recipe)
**What.** Normative Windows implementation for `lse::platform` file I/O:
- `std::int64_t file_size_fd(int fd)` → `_fstat64` (`struct _stat64`); `std::int64_t file_size_path(const char*)` → `_wstat64`. **Never** `fstat`/`_stat64i32`/plain `stat` (32-bit `st_size`).
- `std::int64_t pread64(int fd, void* dst, std::size_t n, std::int64_t off)` → **primary recipe: `ReadFile((HANDLE)_get_osfhandle(fd), buf, (DWORD)chunk, nullptr, &OVERLAPPED{ .Offset=(DWORD)((std::uint64_t)off & 0xFFFFFFFFu), .OffsetHigh=(DWORD)((std::uint64_t)off >> 32) })`** looped with `chunk ≤ 0x7FFFFFFF`; `ERROR_HANDLE_EOF`→return 0 (caller maps to `kIoError` per `file_read.cpp:30-34`); map `GetLastError`→errno. **Forbidden:** `_pread` (32-bit `long` offset); `_lseeki64`+`_read` on any fd that can be shared across threads (the `weights.cpp:232-237` streaming fd is) — seek/read is not atomic. Twin `pwrite64` via `WriteFile`+`OVERLAPPED` for `pull.cpp` resume writes.
- Open: `_wsopen_s(&fd, path, _O_RDONLY|_O_BINARY, _SH_DENYNO, 0)` — **`_SH_DENYNO` mandatory** or `CreateFileMappingW` and concurrent readers fail (POSIX has no mandatory sharing). `O_CLOEXEC`→`_O_NOINHERIT`; `O_NOFOLLOW`→`CreateFileW`+`FILE_FLAG_OPEN_REPARSE_POINT`+`_open_osfhandle`.
- Truncate: `_chsize_s(fd, __int64)` (64-bit-clean; S1 was right here).
- File map: opaque `struct FileMap { void* ptr; std::uint64_t size; /*win:*/ void* hMap; void* hFile; }` — `CreateFileMappingW(h, NULL, PAGE_READONLY, (DWORD)(size>>32), (DWORD)(size & 0xFFFFFFFFu), NULL)` + `MapViewOfFile(hMap, FILE_MAP_READ, hi, lo, size)`; release = `UnmapViewOfFile` then `CloseHandle(hMap)`. Opaque record keeps `weights.cpp`'s POSIX `Mapping` layout untouched (§4.3-compliant).
- Anon map: `VirtualAlloc(MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)` / `VirtualFree(MEM_RELEASE)`; discard: `DiscardVirtualMemory` (page-aligned; zero-fill-on-next-access documented — matches S4 §4's read-back-zero expectation).
- Call-site edits: `file_read.cpp:22-23` (drop `static_cast<off_t>`), `weights.cpp:216-227`, `dflash2_convert.cpp:247-250`, `pull.cpp` large-offset writes. **No `off_t` in any Windows-compiled signature.**
**Where.** `include/lse/core/platform.hpp`, `src/core/platform_win.cpp` (WP3); call sites in WP4/WP5/WP7.
**Why.** S1's `_pread`/default-`stat` recipe silently corrupts >2 GB checkpoint loads (S3 BL-2); the thread-race and share-mode traps are new findings of this review.
**Acceptance.** `tests/test_large_file_io` (S4 §3 design, unmodified) green on Windows and Linux; shim unit cases green both platforms; `grep -n "off_t" src/ include/` shows no unguarded use in Windows-compiled paths.

### ID-2 · BLOCKING — Exact platform-shim API surface (BL-1/MJ-1/MJ-3/G10 closure)
**What.** `namespace lse::platform` must expose (POSIX = pass-through): ID-1 file set; `anon_alloc/anon_free/file_map_ro/file_unmap/map_discard`; `page_size()` (`GetSystemInfo`); `try_lock_exclusive(int fd)/unlock` (`LockFileEx`, whole-file range `0,0,0,0`, `LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY`, R8 delta doc'd); `mkdtemp(prefix)` (`GetTempPathW`+retry); `set_env/unset_env` (`_putenv_s`); `pid()`; `hostname()`; `home_dir()` (ID-8 order); `localtime_r(tm&, time_t)` wrapping `localtime_s` **with the reversed-arg trap comment** (G2); `free_space_bytes(path)` (`GetDiskFreeSpaceExW`, G3); `is_reg/is_dir/lstat_info` (`S_IF*` masks + `std::filesystem::symlink_status`, G4); `raise_fd_limit()` **no-op on Windows** (G1); `dir_entries(path)` (`std::filesystem::directory_iterator`, G5/MJ-1, used by rdma.cpp); clocks `now_monotonic_ns` (`QueryPerformanceCounter`) / `now_boottime_ns` (`QueryUnbiasedInterruptTime`); dl-set `dl_open/dl_sym/dl_close/dl_this_module/dl_addr` (`LoadLibraryW`/`GetProcAddress`/`GetModuleHandleExW(FROM_ADDRESS)`+`GetModuleFileNameW`/`RTLD_NOLOAD`→`GetModuleHandleW`); `using ssize_t = SSIZE_T; using socklen_t = int;` (MJ-3/G9); `STDIN/STDOUT/STDERR_FILENO = 0/1/2` (G10).
**Where.** WP3 (header + two .cpp files); every consumer WP calls only this surface.
**Why.** No artifact currently gives signatures; four compile-blocker classes (G1-G4) have no WP owner without this.
**Acceptance.** `tests/test_platform_shim` (S4 §4 table, plus one case per ID-2 addition) green on Windows **and** Linux (pass-through parity).

### ID-3 · BLOCKING — WP10/M2a gate mechanics + corrected compile-exposure set
**What.** (a) M2a build gate runs `-DLSE_BUILD_TESTS=OFF`. (b) The tests-ON experimental check builds **only targeted binaries**: `cmake --build build --target <COMPILE_EXPOSURE list + test_registration_smoke>` — **never `all`** before WP12 (the tree contains 15 `<unistd.h>` files + the fork test that cannot compile pre-WP12). (c) The authoritative exposure list is **generated programmatically** at WP10 (scan `tests/CMakeLists.txt` for `target_sources(<t>` blocks referencing `src/backends/hrx`, minus HRX-gated targets) and committed to `windows-enablement/COMPILE_EXPOSURE.txt`; the corrected hand-baseline is **35 including `test_attention_short` (`:505-510`)**. (d) Failures dispositioned into `windows-enablement/COMPILE_EXCLUSIONS.md` (X ≤ 5, issue links) via additive `if(NOT WIN32)` guards per S4 §1.2.2. (e) §4.3-rule-5/PR5 carve-out per S4 §1.2.4 is binding.
**Where.** WP10 checklist; S4 §7 M2a row read with this amendment.
**Why.** S4's M2a as written is infeasible (whole-tree tests-ON build fails pre-WP12; smoke test not built with tests OFF); S4's 35-list omits one binary.
**Acceptance.** `logs/m2a-build.txt` shows: tests-OFF full build green; targeted build of every COMPILE_EXPOSURE entry + smoke; smoke passes (SC3); ledger complete or empty.

### ID-4 · BLOCKING (fallback path) — Hoist the tokenizer guards to WP1
**What.** In WP1, wrap `tests/CMakeLists.txt:129,130,135,196` in `if(TARGET lse_tokenizer)` (4 additive guards; no-op on Linux/macOS where the target exists — §4.3-compliant, upstream-PR batch-3 material).
**Where.** WP1 change list (removed from WP12/P6 scope; WP12 keeps verification only).
**Why.** As sequenced (P6 in WP12, WP12 deps=WP11 deps=WP8), the guard that makes the Q1 fallback survivable can never land on the exact path that needs it (§3.2).
**Acceptance.** Windows configure with cargo artificially unavailable produces zero link errors and W=83; Linux ctest set unchanged.

### ID-5 · BLOCKING — Re-partition the T3 fingerprint gate
**What.** `tests/test_numerics_fingerprint.cpp` splits kernels: **HASH-EQ set** (cross-platform SHA256 equality required) = outputs computed with only IEEE-exact ops (`+ − * /`, `std::sqrt`) — e.g. GEMV, RMSNorm(if rsqrt is replaced by `1.0/std::sqrt` or moved to TOLERANCE — implementer verifies), quant/dtype packing; **TOLERANCE set** (per-platform `max_rel` vs committed goldens, no cross-platform hash equality) = any kernel calling `exp/tanh/sin/cos/log/pow` — softmax, rope, attention-step. The test prints each kernel's set membership. S4 §2.2 T3's blanket "hash equals" requirement is superseded for the TOLERANCE set.
**Where.** WP12 (test authoring), S4 §2.2 as amended, M3 criterion (7).
**Why.** MSVC-STL vs libstdc++ transcendentals differ in last ulps by design; the unpartitioned gate makes M4 unreachably red (§3.4).
**Acceptance.** Fingerprint job green on both CI jobs with HASH-EQ hashes equal and TOLERANCE kernels within golden `max_rel`.

### ID-6 · MAJOR — Normative SC5/M3 restatement (units)
**What.** M3 gate: **all W ctest entries pass** (or are configure-disabled with ledgered reason); **K** = number of distinct Windows-disabled *case groups* (in-test `LSE_SKIP` + any KNOWN_FAILURES runtime entry), **K ≤ 5**, each `windows: <capability|flake> — <issue #N> — review-by <date ≤ 90d>`; **X** = compile-excluded *registered binaries*, **X ≤ 5** (target 0), each reducing W and ledgered. S2 SC5's "K = KNOWN_FAILURES.md Windows entries" is superseded: Windows-only skips must not enter KNOWN_FAILURES.md (platform-agnostic file — verified).
**Where.** Supersedes S2 SC5/M3/WP12 acceptance text; S4 §5.3/§7 remain valid with this unit clarification.
**Why.** `LSE_SKIP` keeps exit code 0 (verified), so "≥ (W−K) of W pass" mixed binaries with case-groups.
**Acceptance.** M3 evidence bundle reports W, K, X as three separate numbers with per-entry reasons.

### ID-7 · MAJOR — Rewire WP12/WP13 dependencies (MJ-2 closure)
**What.** WP12 deps = **WP10** (+ WP11 only for executable-dependent suites: `test_server_cli`, `test_tokenizer` sentinel); WP13 infra deps = WP10, its M4 gate still requires WP12 complete. Update the §2.1 graph accordingly; Q1 fallback (M2a-without-M2b) now reaches Phase 2 with W=83.
**Where.** Supersedes S2 §2/§2.1 dep lines for WP12/WP13.
**Why.** As wired, WP8 failure defers M2b and with it all of Phase 2 — the declared fallback is unreachable (S3 MJ-2, still open).
**Acceptance.** Dry-run schedule check at M2a: if WP8 red, WP12/WP13 still startable per DECISIONS.md fallback entry.

### ID-8 · MAJOR — HOME resolution policy (closes S4 P5's open decision)
**What.** Windows `home_dir()`: `HOME` env if non-empty → `USERPROFILE` → error. `expand_path`'s `~` and `$HOME` expansion use the same resolver. Cache-root string building uses `/` separators. P5's `test_model` port: temp-dir HOME + separator-tolerant asserts (S4 §6 P5 as written, with this decision supplied).
**Where.** WP3 (shim), WP5 (`hf_cache_root` path), WP12 (P5 test port), WP14 (docs).
**Why.** Left explicitly undecided in S4; `test_model.cpp:312-328` and hf-hub compatibility both constrain it (§5 AM-2).
**Acceptance.** `test_model` passes on Windows unmodified in logic; production Windows behavior = `%USERPROFILE%` when HOME unset.

### ID-9 · MAJOR — WP scope amendments for orphaned call sites
**What.** Bind the §4 table rows WP5 (`chat_template.cpp:1546`; `weights.cpp:841`; `dflash2_convert.cpp:688-689`), WP6 (`rdma.cpp:19,92-101` dirent; `<afunix.h>` after `<winsock2.h>`), WP7 (`devices.cpp:550-561` raise_fd_limit; `pull.cpp:539-540` statvfs; `cache.cpp:144,147,415` lstat/S_ISREG), WP1 (`_CRT_NONSTDC_NO_DEPRECATE`, `_CRT_SECURE_NO_WARNINGS`, `-ffp-contract=off` in the Windows flag branch).
**Where.** WP1/WP5/WP6/WP7 change lists.
**Why.** These sites are in no WP's change list today (BL-1/MJ-1/MN-2/MN-3 residuals); an implementer following S2 literally ships a broken build.
**Acceptance.** Full clang-cl build of `lse_model`, `lse_communication`, `lse_hub`, `lse_place` green; `grep` audit shows each site routed through `lse::platform` or a guarded branch.

### ID-10 · MINOR — CI YAML corrections
**What.** (a) Replace the dead expression `${{ inputs.run_large_io && '' || '' }}` with a real conditional (e.g. an `if:`-split step or `$(( inputs.run_large_io && '--label-include LARGE_IO' || '--label-exclude LARGE_IO' ))`-style env indirection) so `run_large_io` actually toggles the label. (b) Add `--label-exclude LARGE_IO` to the linux-parity ctest; weekly schedule runs LARGE_IO once per platform. (c) Keep S4 §5.3.5's 3× protocol verbatim.
**Where.** WP13 `windows.yml`.
**Why.** As written the dispatch input is a no-op and the parity job could hit the 8-GiB sparse test unexpectedly.
**Acceptance.** `workflow_dispatch` with `run_large_io=true` shows LARGE_IO tests running; PR runs exclude them on both jobs.

### ID-11 · MAJOR — Registration smoke test identity & census
**What.** `tests/test_registration_smoke.cpp` via `lse_add_test`; asserts: CPU backend enumerable by name; kernel-primitive census > 0; **plus** non-zero registrar census for every self-registering archive among the 8 force-load sites (comm/dist/kernels/graph/model/place/cpu-backend/backends-registry), each compared against the Linux counts captured in `logs/m2a-smoke-linux.txt` (PR2's "census every archive" made concrete).
**Where.** WP2 (authoring), WP10/M2a (execution via ID-3 targeted build).
**Why.** No artifact names the test; PR2 demands full census; R2 is the program's own "most dangerous defect class".
**Acceptance.** Smoke green at M2a on Windows with all censuses matching Linux.

### ID-12 · MINOR — Canary anti-constant-folding
**What.** `test_fp_model.cpp` must take operands from runtime-opaque sources (`volatile` locals seeded from `argc`/`getenv`) so `a*b+c` cannot be folded; assert `a*b+c != std::fma(a,b,c)` for the chosen values and `==` the precomputed non-contracted constant.
**Where.** WP12 (test), listed in S4 §2.2 canary bullet as amended.
**Why.** A compile-time-folded canary passes even when `-ffp-contract=off` is silently dropped by a rebase (R3 guard would be vacuous).
**Acceptance.** Removing the pin from the flag branch locally makes the canary fail.

### ID-13 · MINOR — WP0 provisioning additions
**What.** (a) Smoke project proves clang-cl accepts `-fno-fast-math`, `-ffp-contract=off`, the `-W` wall, `/std:c++latest` (log outputs). (b) Provision WSL/Linux **clang++-21** (apt.llvm.org) for parity, or record "g++-16 at M2a, clang++-21 mandatory from M3" in DECISIONS.md. (c) Compile+run the ID-12 canary under g++-16 `-std=c++26` to empirically settle the GCC strict-ISO fp-contract default; record in DECISIONS.md.
**Where.** WP0 acceptance (`logs/wp0-toolchain.md`).
**Why.** S3 demanded proof-not-assumption for flag acceptance; S4 §7 M2a assumes a Linux clang++-21 that WP0 never installs; S4 §2.1's GCC-default claim is unverified under `CMAKE_CXX_EXTENSIONS OFF`.
**Acceptance.** Version/flag log committed; parity-compiler decision logged.

### ID-14 · MINOR — Recorded corrections (no re-architecture)
**What.** (a) S1 T1 "19 unistd test files" → actual 17 total / **15 Windows-registered** (use S4's list). (b) S3 MJ-5 "pragma silently ignored by cl.exe" → the pragma is `#if defined(__clang__)`-guarded (`dflash2_convert.cpp:37-39`); effect (no explicit pin under cl.exe) similar, characterization corrected; cl.exe path additionally needs an `/fp:precise`-is-default note, not a pragma fix. (c) S4 §1.2 list + `test_attention_short`; block refs += `:455-467`, `:476-488` (`else()` form). (d) Note for Stage 6: `lse_add_test` test exes are linked **without** whole-archive on POSIX too (`tests/CMakeLists.txt:4-16`) — do not add `/WHOLEARCHIVE` to test link lines; symbol-driven extraction parity is intended. (e) S1 P1 scope sentence is superseded by: "everything except the HRX backend *library*; the 35 test binaries still compile loomc/hipc TUs directly and are compile-verified at M2a per ID-3."
**Where.** DECISIONS.md errata entry at WP0; no artifact edits.
**Why.** Keeps the record straight without violating immutability.
**Acceptance.** Errata entry references this ID.

---

## 8. Scores & Final Verdict

| Artifact | Score | Basis |
|---|---|---|
| **01 — Planning & Analysis** | **7 / 10** (S3's score confirmed) | Evidence discipline outstanding (re-verified again this pass); four compile-blocker classes missed (BL-1), one silently-corrupting fix recipe (BL-2), false-as-written P1 scope sentence (BL-3), T1 count error (17 not 19). All now covered by IDs 1/2/3/9/14 — architecture and phasing survive intact. |
| **02 — Program Plan** | **7 / 10** (up from 6.5, conditional on directives) | Governance, milestones, risk ownership, upstream batching remain strong. Four texts need directive-supersession: WP10/M2a gate command (ID-3), SC5/M3 wording (ID-6), WP12/WP13 deps (ID-7), P6 timing (ID-4). With the IDs applied, the plan is executable as written. |
| **03 — Quality Review 1** | **8.5 / 10** | Excellent adversarial verification — every G1-G12 gap I re-checked was exact. Missed: the smoke-test-vs-`BUILD_TESTS=OFF` interaction its own BL-3 fix creates; the MJ-5 pragma-guard mischaracterization; S1's T1 count error. None invalidate its findings. |
| **04 — Test Strategy** | **8.5 / 10** | The strongest artifact of the chain: BL-2 test design covers both paths completely; BL-4 K/X accounting sound and mechanism-verified; Q3 discharged concretely; ~zero factual errors across 20+ spot-checks. Defects: 34-of-35 enumeration, T3 cross-STL hash-equality unsoundness (would have made M4 unreachably red), dead YAML dispatch expression, K/W unit mixing, inherited-infeasible M2a tests-ON command. |
| **CHAIN** | **8 / 10** | Coherent end-to-end after the directive layer; every BLOCKING from S3 is resolved or has a normative fix spec in this review; no re-architecture required; one new BLOCKING found and fixed here (ID-5). |

### Final verdict: **READY-WITH-DIRECTIVES**

Stage 6 may begin immediately with **WP0** (unaffected by every directive except ID-13's additions, which are WP0-native). **WP1 must not start before reading §7 of this review.** The following BLOCKING items remain open *until their directive is implemented* — they are all spec/gate-text corrections, not design flaws:

1. **ID-1** — 64-bit file-I/O shim spec (BL-2 fix side; the only *silent* data-corruption class in the program).
2. **ID-2** — exact shim API surface (BL-1/MJ-1/MJ-3/G10 spec side).
3. **ID-3** — WP10/M2a gate mechanics + corrected 35-binary list (BL-3 residual; the S4 M2a command is infeasible as written).
4. **ID-4** — tokenizer guards hoisted to WP1 (fallback-path deadlock).
5. **ID-5** — T3 fingerprint re-partition (new BLOCKING from this pass; cross-STL bit-equality is unattainable).

MAJOR: ID-6, ID-7, ID-8, ID-9, ID-11. MINOR: ID-10, ID-12, ID-13, ID-14. Total: **14 directives (5 BLOCKING, 5 MAJOR, 4 MINOR)**.

**Gate recommendation:** M0 (plan approved) — **GRANTED, conditional on adoption of §7 as the binding correction layer** (record in DECISIONS.md: "Stage 5 directives ID-1..ID-14 adopted; S4 §7 + 05 §7 supersede S1/S2 text on conflict"). M1 may proceed once WP0 completes with ID-13 additions.

---

*End of Stage 5 second-pass review. No source or prior-artifact files were modified; nothing committed. All verification performed against the working tree at `a5b82f6`, 2026-10-10.*
