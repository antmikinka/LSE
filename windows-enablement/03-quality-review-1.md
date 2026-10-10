# LSE Windows Enablement — Stage 3: Quality Review (First Pass)

**Branch:** `feature/windows-enablement` @ `a5b82f6` (verified: `git rev-parse HEAD` = `a5b82f689e219c388a00817d13db64f57ca403d3`, matches both artifacts' claimed base)
**Date:** 2026-10-10
**Reviewer:** Stage 3 (quality review, first pass)
**Artifacts under review:** `windows-enablement/01-planning-analysis.md` (Stage 1), `windows-enablement/02-program-plan.md` (Stage 2)
**Constraints honored:** no source modified, neither artifact modified, nothing committed. Review notes structured via sequential-thinking before writing.

---

## 1. Review Method

1. **Full read** of both artifacts, then adversarial verification against the working tree at the claimed base SHA.
2. **Evidence spot-check:** ~40 discrete file:line claims re-derived with `grep -n`/`sed` (well beyond the 8-claim minimum), spanning CMakeLists.txt, cmake/*.cmake, src/core, src/model, src/graph, src/communication, src/place, src/api, src/trace, src/cli, src/probe, src/hub, include/lse, tests/, third_party/vendor, .github/workflows, BUILD_FROM_SOURCE.md.
3. **Supplementary gap hunting:** independent greps for `sys/ioctl.h`, `termios`, `sys/resource.h`, `sys/wait.h`, `execv/execl`, `pipe2`, `memfd`, `inotify/timerfd/signalfd`, `getrlimit/setrlimit`, `madvise`, `MAP_POPULATE`, `O_DIRECT`, `aligned_alloc`, `dirent.h`, `getenv/setenv/unsetenv`, `strcasecmp`, `gmtime_r/localtime_r`, `rand_r`, all `clock_gettime`/`CLOCK_*` sites, all `#include <sys/*.h>`, `statvfs`, `S_IS*`/`lstat`, GNU-only constructs (`__int128`, statement expressions, `__typeof`, `__builtin_cpu_supports`, ifunc, `#pragma GCC target/optimize`, `__attribute__` census, `#pragma clang`), and Stage 1 §3.6's "verified absent" list.
4. **third_party Windows scope:** `third_party/vendor` (httplib.h 0.18.3, nlohmann/json) is header-only via include dirs (`CMakeLists.txt:57`, `tests/CMakeLists.txt:25,132,136,155`) — no C++ third_party component is *compiled* as a library; `third_party/fastokens-ffi` is Rust/cargo. So third_party needs no Windows port beyond httplib (already Winsock-capable) — Stage 1's treatment is correct.
5. **Coherence audit:** 17-item → 16-WP mapping, DAG cycle/edge check, acceptance-criteria measurability, gate-command executability ("would the stated command actually succeed?").
6. **Feasibility challenge:** clang-cl viability from the construct census; governance-rule achievability; M3 K≤5 realism; UCRT API-semantics review of Stage 1's *recommended fixes* (not just its problem statements).
7. **Quantitative cross-checks:** `if(APPLE)` count (7 ✓), whole-archive sites (8 ✓), `lse_add_test(` count (102 ✓ — Stage 2 SC5), upstream churn (`git log --since=2026-08-15` → 669 commits ≈ Stage 2's "654/8wk ~80/wk" ✓).

---

## 2. Evidence Spot-Check Results

Claim → verdict → actual file:line. **Every material claim checked was VERIFIED; no invented evidence was found.** Nits are line-range off-by-≤2 only.

| # | Stage-1/2 claim | Verdict | Actual evidence |
|---|---|---|---|
| 1 | 7 `if(APPLE)` at `CMakeLists.txt:97,117,157,273,285,374,427`; 0 WIN32/MSVC | **VERIFIED (exact)** | `grep -n` returns exactly those 7 lines; `WIN32|MSVC` → 0 hits |
| 2 | Whole-archive ×8: `CMakeLists.txt:117-122,157-162,273-278,285-290,374-379,427-432`; `src/backends/cpu/CMakeLists.txt:12-17`; `src/backends/hrx/CMakeLists.txt:83-88`; `LINKER:` prefix GNU-spelled | **VERIFIED (exact)** | force_load at 118/158/274/286/375/428, cpu:13, hrx:84; `LINKER:--whole-archive,...` at 121/161/277/289/378/431, cpu:16, hrx:87 |
| 3 | `cmake_minimum_required(3.24)`:1, g++-16 probe :5-10, C++26 :20-23 | **VERIFIED (exact)** | line 1 `VERSION 3.24`; `find_program(LSE_GXX16 ...)` 5-10; `CMAKE_CXX_STANDARD 26` :20, device std :23 |
| 4 | Poller kqueue/epoll selection `CMakeLists.txt:97-101` | **VERIFIED (exact)** | `if(APPLE)` :97, `poller_epoll.cpp` in else :100 |
| 5 | `src/core/file_read.cpp` pread :22-23, `open(O_RDONLY|O_CLOEXEC)` :41, `getpagesize` :80, `mmap` :82, munmap :56/:67, `<unistd.h>` :5 | **VERIFIED (exact)** | `::pread` 22-23; `::open` :41; `::getpagesize()` :80; `::mmap` :82; `::munmap` :56, :67; include :5 |
| 6 | `src/place/devices.cpp` dlopen/dlsym :568-569,594,627,643,677; `/proc/self/exe` :620; setenv :633; soname two-way :580-584; `/opt/rocm` :656,661; LD_LIBRARY_PATH :542 | **VERIFIED** | dlsym 568-569; dlopen 594/602/627/643/677; `read_symlink("/proc/self/exe")` exactly :620; setenv :633; soname branch actually 581-585 (claim 580-584 — nit); `/opt/rocm` roots + glob :656-672 |
| 7 | `cmake/LSERust.cmake:97` hardcoded `lib${lib_name}.a`; iOS branch :45-50 (`PCRE2_SYS_STATIC=1`); GNU `-l` parse :64-72 with `^(c|gcc_s|m)$` filter; fallback `pthread dl` :75; clone :15-37 | **VERIFIED (exact)** | :97 `set(_lib "...lib${lib_name}.a")`; :45-50 iOS incl. PCRE2_SYS_STATIC; :65-72 regex `-l[^ ]+` + filter; :76 `set(_libs pthread dl)`; clone 15-37 with `FATAL_ERROR` |
| 8 | `src/trace/record.cpp:14-35` clock_gettime/CLOCK_BOOTTIME; Apple `mach_continuous_time` :26-32; `__uint128_t` :32 Apple-guarded | **VERIFIED (exact)** | `clock_gettime` :16, CLOCK_MONOTONIC :25, `__uint128_t` :32, CLOCK_BOOTTIME :34, `#ifdef __APPLE__` :26 |
| 9 | `src/api/lse.cpp` `<malloc.h>` :12, unistd :17, `/proc/self/statm` :187-193 (`sysconf(_SC_PAGESIZE)` :193), `mallinfo2` :211-212, LogCapture pipe/dup2 :124-159 (`::pipe` :148, `::dup2` :151/159), HOME :508, setenv :510, `std::jthread` :645 | **VERIFIED (exact)** | all lines confirmed by grep; jthread exactly :645 |
| 10 | `__builtin_memcpy`: `hash.hpp:28`, `dtype.hpp:37,46`, `graph.cpp:187,257`, `hip_emitter.cpp:290` | **VERIFIED (exact)** | grep returns exactly these 6 sites; no others in host code |
| 11 | `tests/test_server_shutdown.cpp` `CPPHTTPLIB_USE_POLL` :2, `sys/wait.h`+`unistd.h` :5-6, fork/waitpid case :24-37 | **VERIFIED (exact)** | pipe :24, fork :25, waitpid :35, WIFEXITED :36 |
| 12 | `src/communication/socket.cpp` macOS abstract-ns rejection :129-130; fcntl configure :160-165; AF_UNIX handling :373, :444-455, :616-622 | **VERIFIED (exact)** | :129 `#ifdef __APPLE__`, :130 kUnimplemented error; configure_socket fcntl 159-166 (APPLE-gated); :373/:444-446/:615-622 confirmed |
| 13 | `stream_link.cpp` sendmsg/MSG_NOSIGNAL :209,327,403; `<sys/uio.h>` :4 | **VERIFIED (exact)** | all four exact |
| 14 | `cmake/LSECompilerFlags.cmake` warnings :5-19, Werror :21-23, `-freflection` GNU≥16 :26-29, ASan :31-34, `-fno-fast-math` :38, PIC/visibility :40-44 | **VERIFIED (exact)** | `-Wformat=2` :18, `-fno-fast-math` :38 |
| 15 | `cmake/LSEOptions.cmake` `LSE_ENABLE_HRX` ON :6, `/opt/rocm` :22, `libhsa-runtime64.so.1` find_path :103-106, ROCr≥1.21/`hsa_amd_queue_create` note :95-97 | **VERIFIED (exact)** | NAMES at :104 |
| 16 | `src/backends/CMakeLists.txt:19-23` HRX gate + skip message :22 | **VERIFIED (exact)** | `if(LSE_ENABLE_HRX AND LSE_HRX_INCLUDE_DIR)` :19, status :22 |
| 17 | `tests/CMakeLists.txt` `lse_add_test` :4-16 (WORKING_DIRECTORY :13-15), KNOWN_FAILURES parse :597-606, `if(UNIX)` fixtures :429-452, `if(APPLE)` :317-328, design note :225-229, LD_LIBRARY_PATH env :266-270, test_server_cli :273-278 | **VERIFIED** | all confirmed; server_cli block actually 274-278 (nit); **102 `lse_add_test(` calls — Stage 2 SC5 count VERIFIED exact** |
| 18 | `include/lse/lse.h:26-30` `_WIN32` LSE_API branch | **VERIFIED (exact)** | :26-30 |
| 19 | `third_party/vendor/httplib.h:11` = 0.18.3 | **VERIFIED (exact)** | `#define CPPHTTPLIB_VERSION "0.18.3"` :11 |
| 20 | `include/lse/core/reflect.hpp:5` `__cpp_impl_reflection` guard | **VERIFIED (exact)** | :5 |
| 21 | `.github/workflows/ci.yml:80-84` "g++-16 is not optional" | **VERIFIED** | comment+check at ~79-85 |
| 22 | `BUILD_FROM_SOURCE.md` LLVM 21 (macOS) / GCC 16 (Linux) | **VERIFIED** | :7 "LLVM 21.1.8 … GCC 16 (`g++-16`)"; :9 "CMake 3.24+" corroborates A1 |
| 23 | `weights.cpp` :4,176,206,227-243,452,493-499; `jit_cache.cpp` :336-349 (O_NOFOLLOW :336, flock :345-349), :1017-1019 HOME, :1033 setenv; `dflash2_convert.cpp` :242,250,366,374,612,701; `pull.cpp` :314,330-348,372,397,451,470; `profile_store.cpp:157`; `link_probe.cpp:62`; `server_main.cpp:498-499`; `http_server.cpp:13`; `cli/main.cpp:480-484,758`; `harness.hpp` getenv :52/75/94; `cpu_backend.cpp:31,78` | **VERIFIED** | all confirmed; weights hf_cache_root env checks actually ~489-499 (claim 493-499 — nit); `CPPHTTPLIB_USE_POLL` exactly `http_server.cpp:13` |
| 24 | Stage 2 churn: "654 commits / 8 weeks" | **VERIFIED (≈)** | local count since 2026-08-15: **669** — same magnitude; claim plausible |
| 25 | §3.6 "not found" claims (pthread.h, shm_open, fork in src, posix_memalign, sched_setaffinity, hugepages, endian.h, popen, realpath, getauxval, `_mm_*`/immintrin, inline asm in host code) | **VERIFIED** | independent greps return zero hits in src/include; fork() only in `tests/test_server_shutdown.cpp` as claimed |

**Line-count nits:** root `CMakeLists.txt` is 524 lines (artifact says 525), `tests/CMakeLists.txt` 606 (says 607) — cosmetic, likely trailing-newline counting.

---

## 3. Supplementary Gap Findings (missed by Stage 1)

All verified at `a5b82f6`. Severity assessed against Phase-1 scope (core lib + CPU backend + `lse`/`lse-server`, HRX OFF).

### G1 — `getrlimit`/`setrlimit`/`<sys/resource.h>` in always-built `lse_place` — **P1 compile blocker**
`src/place/devices.cpp:8` (`#include <sys/resource.h>`), `raise_fd_limit()` at :550-561 (`::getrlimit(RLIMIT_NOFILE,…)` :552, `::setrlimit` :556, :560), called from `prepare_device_runtime()` :688. UCRT has no `sys/resource.h`/`getrlimit`/`RLIMIT_NOFILE`. Stage 1's C6/WP7 change list (dl-shim 568-677, /proc/self/exe, sonames, /opt/rocm, setenv :633) **does not cover it**. Windows fix: no-op (or `_setmaxstdio`) — trivial, but must be inventoried and assigned.

### G2 — `localtime_r` in always-built `lse_model` — **P1 compile blocker**
`src/models/chat_template.cpp:1546` (`localtime_r(&t, &tm)` inside `strftime_now()`). No UCRT equivalent; `localtime_s` has **reversed argument order** (`localtime_s(&tm,&t)`) — a classic silent-misuse trap that belongs in the shim (B1), not at the call site. `test_chat_template` is registered unconditionally (`tests/CMakeLists.txt:127`), so this is squarely in P1/P2 scope. Same function carries `#pragma GCC diagnostic push/ignored "-Wformat-nonliteral"/pop` (:1549-1552): fine on clang-cl (accepts GCC spelling), warning-only on cl.exe — but neither artifact's construct inventory (§3.2) mentions this file at all.

### G3 — `statvfs` in always-built `lse_hub` and `lse_model` — **P1 compile blocker**
`src/hub/pull.cpp:12,539-540` and `src/model/dflash2_convert.cpp:24,688-689` (free-space checks). No Windows `statvfs`; needs `GetDiskFreeSpaceExW`. C2/C7 change lists cover pread/open/flock/mkdtemp in these files but not statvfs.

### G4 — `S_ISREG` / `::lstat` — **P1 compile blocker**
`src/graph/jit_cache.cpp:339`, `src/hub/cache.cpp:144,147,415` (`::lstat` + `S_ISREG`), `src/model/weights.cpp:841`. UCRT `<sys/stat.h>` provides `S_IFREG`/`_S_IFMT` but **not** the `S_IS*` predicate macros, and has no `lstat`. Needs shim macros (`((m)&S_IFMT)==S_IFREG`) and an lstat substitute (`std::filesystem::symlink_status` or `GetFileInformationByHandle` on an `FILE_FLAG_OPEN_REPARSE_POINT` handle).

### G5 — `<dirent.h>` + `opendir/readdir/closedir` in always-built `lse_communication` — **P1 compile blocker (scope covered, inventory not)**
`src/communication/rdma.cpp:19` (include), :92-101 (`DIR*`, `::opendir`, `::readdir`, `::closedir` scanning `/sys/class/infiniband` and `/dev/odl_tb5_*`). C5's fix range "46-116" coincidentally brackets the call sites, but the inventory cites only dlfcn lines (:20,46,59-61,114) and the B1/WP3 shim API list has **no directory-enumeration API**. Implementer following the plan literally ships a broken rdma.cpp. Fix: `std::filesystem::directory_iterator` inside the Windows guard.

### G6 — Four `src/backends/hrx/loomc/*.cpp` TUs compile on Windows regardless of HRX gating — **scope/governance contradiction**
`tests/CMakeLists.txt` unconditionally adds `loom_emitter.cpp`, `loom_print.cpp`, `loom_types.cpp`, `loom_sources.cpp` via `target_sources` to registered tests: `test_pointwise_fusion` (:27-32), `test_loom_print` (:50-53), `test_loom_repeat` (:55-60), `test_loom_matrix` (:355-359). `LSE_BUILD_TESTS` defaults **ON** (`cmake/LSEOptions.cmake:3`). Consequences: (a) Stage 1's P1 scope sentence "everything except `src/backends/hrx/**` compiles and links" is **false as written** whenever tests are enabled; (b) Stage 2 §4.3 rule 5 ("`src/backends/hrx/**` frozen … touching it is a PR5 trigger") conflicts with the possibility that one of these TUs needs a Windows fix (they look portable — std headers + guarded `reflect.hpp` — but nobody verified); (c) WP10's "Full `cmake --build`" (with tests ON) compiles the 19 unistd.h test files, `fork`/`sys/wait` test, and loomc TUs **before WP12 exists** → the M2a gate command as written cannot succeed. See BL-3.

### G7 — KNOWN_FAILURES cannot absorb compile failures — **mechanism misconception affecting SC5/T5**
`lse_add_test` calls `add_executable` unconditionally (`tests/CMakeLists.txt:5`); KNOWN_FAILURES parsing (:597-606) only sets `DISABLED TRUE` on the *ctest entry*. A test TU that does not **compile** on Windows blocks `cmake --build` no matter what KNOWN_FAILURES.md says. SC5's K≤5 valve therefore covers runtime failures only; every registered-on-Windows test binary must compile. Neither artifact states this.

### G8 — UCRT 64-bit file I/O semantics contradict Stage 1's recommended fixes — **latent correctness defect**
Stage 1 C1/B1 recommend `_pread`/`_open`/`_chsize_s`/`_lseeki64`. On UCRT: `_pread`'s offset parameter is **32-bit `long`**; `off_t` is 32-bit; default `struct stat`/`fstat` is `_stat64i32` with **32-bit `st_size`**. LSE reads multi-GB checkpoints (`weights.cpp:217` fstat→size→mmap; `dflash2_convert.cpp:247-250` pread loops; `pull.cpp` resume writes at large offsets). Implemented as specified, the shim would **silently misreport sizes and truncate reads for files >2 GB** — small-fixture smoke tests would pass. Correct recipe: `_lseeki64`+`_read` or `ReadFile`+`OVERLAPPED`; `_stat64`/`_fstat64`; `MapViewOfFile` (64-bit-clean) for mapping. `_chsize_s` (as Stage 1 says) is correct. Needs an explicit >2 GB acceptance test on Windows.

### G9 — Type shims understated: `ssize_t`, `socklen_t`
Stage 1 rates `ssize_t`/`off_t` "trivial — UCRT `<sys/types.h>` defines both." UCRT defines `off_t` (32-bit, see G8) but **not `ssize_t`**; `socklen_t` (used at `socket.cpp:617`, `stream_link.cpp` passim) is **not** defined by `<winsock2.h>` either and appears nowhere in the inventory. Shim must typedef both.

### G10 — Small UCRT details absent from the inventory
- `STDERR_FILENO`/`STDOUT_FILENO` (used by LogCapture, `src/api/lse.cpp:~146-159`) do not exist in UCRT headers.
- POSIX-name deprecation: `open/read/write/close/lseek/fstat/getpid/fileno` etc. emit C4996-class deprecation warnings unless `_CRT_NONSTDC_NO_DEPRECATE` (+`_CRT_SECURE_NO_WARNINGS`) is defined — WP1/A3 should set these; neither artifact mentions it.
- Windows AF_UNIX `sockaddr_un` lives in `<afunix.h>`, not `<winsock2.h>` — C5/WP6 detail.

### G11 — Test-side `<sys/resource.h>` (P3-scope, but inventory imprecise)
`tests/test_capture_prefill.cpp:41,66-69` and `tests/measure_loomc_compile.cpp:9,39-46` use `sys/resource.h`/`getrusage`. Both binaries are `LSE_ENABLE_HRX`-gated (`tests/CMakeLists.txt:580-582`, :360-369) → **not registered on Windows P1/P2**; this is a WP15/S-item concern. Conversely, Stage 1's T1 "19 unistd files" list includes `test_capture_prefill.cpp:18` — wasted P2 effort since it never registers with HRX OFF. Inventory should be corrected.

### G12 — M_win baseline instability
SC5 fixes M_win from `ctest -N` at **M1**, but the registered-test set legitimately changes when WP8 lands (cargo-gated `TARGET fastokens_ffi` → ~15 tokenizer-dependent tests appear/disappear) and when WP12 adds configure-time exclusions (G7 fix). Baseline must be re-captured at M2a and M3 with committed diffs, else SC5 arithmetic compares different sets.

### Verified non-gaps (my greps found nothing beyond Stage 1)
`sys/ioctl.h`, `termios`, `execv/execl`, `pipe2`, `memfd`, `inotify/timerfd/signalfd`, `MAP_POPULATE`, `O_DIRECT`, `strcasecmp`, `gmtime_r`, `rand_r`, `popen`, `realpath`, `getauxval` — zero hits in src/include/tests. `clock_gettime` exists only in `record.cpp` (all other CLOCK_* hits are comments) — Stage 1's "QPC vs clock_gettime" surface is complete. `madvise` only `weights.cpp:206` (covered). `std::aligned_alloc` only `cpu_backend.cpp:78` (portable; MSVC STL provides it). `sys/time.h` only in `poller_kqueue.cpp` (not built on Windows). `sys/stat.h`×10 — UCRT provides the header (but see G4/G8).

---

## 4. Coherence Assessment

**Item→WP mapping (Stage 2 Appendix B):** all 17 Stage-1 items map — A1→WP1 (doc finalization declared in WP14; acceptable declared split), A2/A3/F1→WP1, A4→WP2, B1→WP3, C1/C4→WP4, C2/C3→WP5, C5+E1→WP6, C6/C7→WP7, D1→WP8, D2→WP0, E2→WP9; T1-T5/T7→WP12, T6/T8→WP13. **Complete and unique** (one declared exception). ✓

**DAG:** edges WP0→WP1→{WP2,WP3,WP8}; WP3→{WP4,WP6,WP7}; WP4→WP5; {WP2,WP4,WP5,WP6,WP7,WP9}→WP10; {WP8,WP10}→WP11; WP11→{WP12,WP13}; WP13→WP14; M4→WP15. **Acyclic** ✓; WP3→WP10 covered transitively ✓; WP9's "deps: none (compile-check needs WP1)" is a labeling wobble, not a cycle.

**Coherence defects found:**
1. **WP12/WP13 dep=WP11 contradicts the Q1 fallback (Appendix A).** If D2/WP8 fails, M2b is deferred — and with it, per the DAG, *all of Phase 2*. The fallback ("M2a alone may be declared") is unreachable as wired. Deps should be WP10, with WP11 conditional for executable-dependent tests only. → MJ-2.
2. **WP10 gate command vs default-on test tree (G6).** "Full `cmake --build`" at M2a cannot succeed before WP12; plan never says `-DLSE_BUILD_TESTS=OFF` for P1 gates. → BL-3.
3. **SC5 vs KNOWN_FAILURES mechanism (G7).** K≤5 pressure valve silently assumes runtime-only failures. → BL-4.
4. **SC5 baseline timing (G12).** → MJ-4.

**Measurability:** SC1-SC4, SC6-SC8 are measurable and evidence-backed (exact commands, logs, gate artifacts). SC5 measurable after fixes 2-4 above. WP-level acceptances are mostly binary-checkable; WP6's "TCP-loopback round-trip smoke" should name the concrete artifact/log (minor). Gate discipline (§6, DECISIONS.md, no silent scope changes) is strong. §4.3 diff rules are enforceable by review (additive-files-first, guards-not-rewrites, one-WP-one-commit).

**Governance rule ("zero behavioral change on Linux/macOS") — achievable, with one concentration of risk:** WP2's `lse_force_load` helper must reproduce the existing strings byte-equivalently (`LINKER:-force_load,$<TARGET_FILE:t>` / `LINKER:--whole-archive,…,--no-whole-archive`) across 8 sites; the plan's "verified on Linux/macOS first" acceptance is the right guard. WP9 (`__builtin_memcpy`→`std::memcpy`) is codegen-identical on GCC/Clang. Shim POSIX side is pass-through. The loomc carve-out (G6) requires one wording amendment to §4.3 rule 5 / PR5 so a Windows-compile fix in those four TUs isn't a governance violation.

**M3 gate (K≤5) realism:** registered-on-Windows set excludes HRX-gated and UNIX/APPLE-only entries; the two known certain-failure classes (abstract AF_UNIX — `test_communication` registered unconditionally at :229 with the "unguarded on purpose" note :225-228; fork-case `test_server_shutdown`) are both *ported*, not waived, per T2/T3. Residual K candidates (LockFileEx semantics, SIGTERM, long paths) are few. **K≤5 is realistic — conditional on BL-1…BL-4 fixes** (compile blockers don't count against K; they must be zero).

---

## 5. Feasibility Challenge Results

**clang-cl viability: CONFIRMED, with the fix-recipe caveat (G8).** Construct census: no statement expressions (grep hits were json brace-inits), no `__typeof`/`__extension__`/labels-as-values/`strdup`/`asprintf`/`__builtin_expect`/`alloca` in host code, no `__builtin_cpu_supports`, no ifunc, no `#pragma GCC target/optimize`, no VLAs found, no inline asm in host code. `__int128` occurs only at `record.cpp:32`, Apple-guarded ✓. `__attribute__` census: `visibility("default")` at `lse.h:29` (inside the `_WIN32` #else ✓) and `ext_vector_type` inside emitted device-source strings ✓. Two extra pragmas the artifacts missed: `#pragma clang fp contract(off)` (`dflash2_convert.cpp:38` — honored by clang-cl; **silently ignored by cl.exe**, losing the FMA-contract numerics policy — relevant to R7 and to the cl.exe fast-follow claim) and `#pragma GCC diagnostic` (`chat_template.cpp:1549-1552` — clang-cl OK; cl.exe C4068 warning). The `-W` flag wall: clang-cl accepts GNU-spelled `-W` options; Stage 1's own caveat on `-Wformat=2` and the WP0 smoke project are the right verification vehicle — the claim "`-fno-fast-math` accepted by clang-cl as-is" should be **proven in WP0's smoke**, not assumed.

**The real feasibility threat is not language extensions — it's UCRT API semantics** (G8: 32-bit `_pread` offset/`off_t`/default `stat`; G9: missing `ssize_t`/`socklen_t`; G4: missing `S_IS*`/`lstat`; G2: `localtime_s` reversed args). Stage 1's problem *identification* is excellent; a subset of its recommended *fixes* is wrong in ways that fail silently on real workloads (multi-GB checkpoints). This must be corrected in the shim spec (WP3) before implementation.

**Toolchain decision otherwise sound:** MinGW rejection rationale (Rust tier-2, MSVC-ABI HIP SDK, runners) verified consistent; Rust story (`ring`/NASM/`PCRE2_SYS_STATIC=1` precedent at `LSERust.cmake:45-50`) verified; `reference/` absent from the tree ✓ consistent with configure-time clone.

---

## 6. Issue Register

### BLOCKING (must fix before WP0/WP1 start)

| ID | Artifact | Issue | Corrective instruction |
|---|---|---|---|
| **BL-1** | 01 | Platform-coupling inventory misses 4 compile-blocker classes in always-built P1 libraries: **G1** rlimit (`devices.cpp:8,550-561,688`), **G2** `localtime_r` (`chat_template.cpp:1546`), **G3** `statvfs` (`pull.cpp:12,539-540`; `dflash2_convert.cpp:24,688-689`), **G4** `S_ISREG`/`lstat` (`jit_cache.cpp:339`; `cache.cpp:144,147,415`; `weights.cpp:841`). The 17-item P1 work list cannot produce a green build as scoped. | Add §3.1 inventory rows; extend B1 shim API (rlimit no-op on Windows; `localtime_r` shim wrapping `localtime_s` **with reversed-arg note**; `statvfs`→`GetDiskFreeSpaceExW`; `S_ISREG/S_ISDIR` macros; lstat→`symlink_status`); amend C2/C3/C6/C7 scope and WP5/WP7 change lists accordingly. |
| **BL-2** | 01 | **G8 fix-design defect:** recommended `_pread`/`off_t`/`stat` recipe is 32-bit on UCRT → silent corruption for >2 GB checkpoints (`weights.cpp:217`, `dflash2_convert.cpp:247-250`, `pull.cpp` resume). Most serious finding: passes smoke tests, fails on real models. | Rewrite B1/C1 fix spec: `_lseeki64`+`_read` or `ReadFile`+`OVERLAPPED`; `_stat64`/`_fstat64`; `MapViewOfFile` for maps; add a Windows acceptance test reading a range beyond 2 GB in a large/sparse fixture. |
| **BL-3** | 01+02 | **G6 scope/governance contradiction:** four `src/backends/hrx/loomc/*.cpp` TUs compile into unconditionally-registered tests (`tests/CMakeLists.txt:27-32,50-60,355-359`) with `LSE_BUILD_TESTS` default ON → "everything except `src/backends/hrx/**`" is false; §4.3 rule-5/PR5 freeze is violated by any needed loomc fix; **WP10 "Full `cmake --build`" cannot succeed before WP12** (19× `unistd.h` test files, fork test all still unported at M2a). | WP10/WP11 and M2a/M2b gate commands must configure `-DLSE_BUILD_TESTS=OFF` (state it in SC/WP text), or move a minimal test-compile-guard item into P1. Amend Stage 1 §7.1 scope sentence and Stage 2 §4.3 rule 5/PR5 to carve out the four loomc TUs; add "verify loomc TUs compile under clang-cl" to WP10 checklist. |
| **BL-4** | 02 | **G7 KNOWN_FAILURES misconception:** mechanism disables *execution* only (`tests/CMakeLists.txt:5` vs :597-606); compile failures of any registered test block the build regardless of K. SC5/T5 as written imply otherwise. | State in SC5/T5/WP12: K counts **runtime** failures only; WP12 must make every Windows-registered test TU compile (port, or new `if(NOT WIN32)` configure-time exclusion around the `lse_add_test` call — itself a §4.3-compliant additive guard). |

### MAJOR

| ID | Artifact | Issue | Corrective instruction |
|---|---|---|---|
| MJ-1 | 01 | **G5** `rdma.cpp:19 <dirent.h>` + `opendir/readdir/closedir` :92-101 absent from inventory and from B1/WP3 shim API list (C5's 46-116 range brackets the calls by luck). | Add inventory row; specify `std::filesystem::directory_iterator` rewrite inside the Windows guard of rdma.cpp (no shim dir API needed). |
| MJ-2 | 02 | WP12/WP13 deps=WP11 deadlocks Phase 2 under the declared Q1 fallback (WP8 failure ⇒ M2b deferred ⇒ P2 unreachable). | Rewire: WP12 deps = WP10 (+ WP11 for executable-dependent tests only); update §2.1 graph and Appendix A fallback wording. |
| MJ-3 | 01 | **G9** "UCRT defines both (ssize_t/off_t)" wrong for `ssize_t`; `off_t` 32-bit; `socklen_t` (`socket.cpp:617` etc.) uninventoried and absent from `<winsock2.h>`. | Correct §3.1 row; shim typedefs `ssize_t`, `socklen_t` (int); tie `off_t` handling to BL-2 recipe. |
| MJ-4 | 02 | **G12** M_win baseline captured at M1 shifts when WP8 lands (cargo-gated registration) and when WP12 adds exclusions → SC5 arithmetic compares different sets. | Re-capture `ctest -N` at M2a and M3; commit both; define SC5's M_win as of the M3 configure, with diff evidence vs M1. |
| MJ-5 | 01 | §6 secondary-toolchain claim "cl.exe needs only a 3-site `__builtin_memcpy` shim" undercounts: `#pragma clang fp contract(off)` (`dflash2_convert.cpp:38` — **silently dropped by cl.exe**, defeating the R7 numerics policy), `#pragma GCC diagnostic` (`chat_template.cpp:1549-52`), `localtime_r` (G2), GNU-spelled flags (A3 handles). | Correct §6 cost list; for any cl.exe path, add `#pragma fenv_contract(off)`/`/fp:precise`-equivalent guard or `#ifdef __clang__` around the fp pragma. |

### MINOR

| ID | Artifact | Issue | Corrective instruction |
|---|---|---|---|
| MN-1 | 01 | **G10** `STDERR_FILENO`/`STDOUT_FILENO` absent in UCRT (LogCapture `lse.cpp:~146-159`). | Add shim constants (0/1/2 or `_fileno(stderr)`). |
| MN-2 | 02 | WP1/A3 omits `_CRT_NONSTDC_NO_DEPRECATE`/`_CRT_SECURE_NO_WARNINGS` strategy → deprecation-warning noise across ~30 POSIX-name call sites under clang-cl/MSVC headers. | Add both defines (Windows-only, additive) to WP1 change list; record in Q7 warning-policy note. |
| MN-3 | 01 | Windows AF_UNIX `sockaddr_un` requires `<afunix.h>` — not mentioned in C5/WP6. | Add to C5 include plan (after `winsock2.h`). |
| MN-4 | 01 | Line-range nits: `weights.cpp` env checks ~489-499 (claimed 493-499); `devices.cpp` soname branch 581-585 (claimed 580-584); `test_server_cli` 274-278 (claimed 273-278); file lengths 524/606 vs "525/607". | Correct on next revision; no functional impact. |
| MN-5 | 01 | **G11** T1 lists `test_capture_prefill.cpp:18` though the binary is `LSE_ENABLE_HRX`-gated (:580-582) and never registers on Windows P2; its `sys/resource.h`/`getrusage` (:41,66-69) and `measure_loomc_compile.cpp:9,39-46` are WP15/S5 concerns. | Move both files from T1 to the P3 (S-items) inventory. |
| MN-6 | 02 | WP9 "deps: none" vs "compile-check needs WP1" labeling; WP6 acceptance "TCP-loopback smoke" names no evidence artifact. | Set WP9 dep=WP1 for the compile-check step; name the smoke log in `logs/` per §2 convention. |

---

## 7. Scores & Verdict

### Artifact 1 — `01-planning-analysis.md`: **7 / 10 — CONDITIONAL PASS**
Evidence discipline is outstanding: ~40 spot-checked file:line claims verified exact, zero invented evidence, "not-found" claims independently confirmed, quantitative claims (7 APPLE guards, 8 whole-archive sites, 102 tests, churn) all correct. But the inventory missed **four compile-blocker classes in always-built P1 libraries** (BL-1), one recommended fix is **silently data-corrupting for >2 GB files** (BL-2), the P1 scope sentence is false w.r.t. loomc TUs (BL-3), and two fix-recipe details are wrong (MJ-3, MJ-5). Remediate BL-1/BL-2/MJ-1/MJ-3/MJ-5 (inventory + shim-spec amendments only — the architecture and phasing survive intact) before implementation starts.

### Artifact 2 — `02-program-plan.md`: **6.5 / 10 — CONDITIONAL PASS**
Governance, milestones, risk ownership, diff-minimization rules, and the upstream-batch strategy are strong and the WBS mapping is complete and acyclic. But two gate definitions are **not executable as written**: M2a's "full `cmake --build`" collides with the default-on test tree (BL-3), and SC5's K≤5 valve rests on a KNOWN_FAILURES mechanism misconception (BL-4); the Q1 fallback is unreachable through the DAG (MJ-2); the SC5 baseline is unstable (MJ-4). Fix BL-3/BL-4/MJ-2/MJ-4 plus MN-2/MN-6, then this plan is implementation-ready.

### Most serious finding
**BL-2 (G8):** Stage 1's UCRT recipe (`_pread`, default `stat`, `off_t`) is 32-bit — the shim built to spec would **silently corrupt reads of multi-GB model checkpoints** while passing every small-fixture smoke test in M2a/M3. This is precisely the "silent defect" class the program claims to guard against (R2's philosophy), and neither artifact's acceptance criteria would catch it.

### Gate recommendation
**M0 (plan approved): NOT YET.** Both artifacts pass conditionally; the 4 BLOCKING issues are documentation/spec amendments (no re-architecture needed) and can be turned around in one revision pass. Re-review (Stage 3 second pass) should verify: amended B1/C-item shim spec (BL-1/BL-2/MJ-1/MJ-3), amended WP10/WP11 gate commands + loomc carve-out (BL-3), amended SC5/T5 (BL-4/MJ-4), rewired WP12/WP13 deps (MJ-2). WP0 (environment provisioning, no repo code) may proceed in parallel with the revision, since its D2 pre-flight is independent of all BLOCKING items.

---

*End of Stage 3 first-pass review. No source or artifact files were modified; nothing committed. All verification performed against working tree at `a5b82f6`, 2026-10-10.*
