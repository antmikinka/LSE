# LSE Windows Enablement — Stage 2: Program Plan

**Branch:** `feature/windows-enablement` (base: master @ `a5b82f6`, v0.6.0-28; fork `antmikinka/LSE`, upstream `Geramy/LSE`)
**Date:** 2026-10-10
**Author:** Stage 2 (program management)
**Inputs:** `windows-enablement/01-planning-analysis.md` (Stage 1, verified — all file:line references below are Stage 1 evidence and are NOT re-derived)
**Consumers:** quality reviewer → test strategist → quality reviewer → senior developer (implementer) → technical writer
**Status:** Plan only. No source was modified. No commits made.

---

## 1. Program Charter

### 1.1 Objective

Enable LSE (C++26 host engine + Rust tokenizer FFI) to **configure, build, link, and pass its test suite natively on Windows x64** using clang-cl (LLVM 21) + MSVC STL/UCRT + Ninja + Rust `x86_64-pc-windows-msvc`, with the HRX/ROCm backend explicitly gated OFF, while keeping the fork branch continuously mergeable with a high-churn upstream and introducing **zero behavioral change on Linux/macOS**.

### 1.2 Scope

**In scope (this program):**
- Phase 1: toolchain/CMake foundation, platform shim, 7 module ports, Rust FFI Windows branch, link hygiene, HRX gating → full build + registration smoke green locally on Windows (M2a libs, M2b executables).
- Phase 2: test-suite ports, Windows KNOWN_FAILURES policy, `.github/workflows/windows.yml` on `windows-latest`, Linux/macOS parity job → CI green (M3 local tests, M4 CI).
- Documentation: Windows build docs, cache-location and semantic-delta notes (M5).
- Governance: weekly rebase discipline + upstream PR batches back to `Geramy/LSE`.

**Out of scope:**
- HRX/ROCm runtime on Windows (Phase 3 is a **compile-only stretch spike**, WP15, started only after M4; dispatch permanently out of scope until hrx-system ships a Windows port — Stage 1 R5).
- DLL builds / `__declspec(dllexport)` ABI (Stage 1 Q5 — deferred unless requested).
- ARM64 Windows, MinGW-w64 (rejected, Stage 1 §6), Windows versions < 10 1803 (AF_UNIX floor).
- Any rewrite of portable code, any reformatting, any behavioral change on Linux/macOS.
- iOS/Android/macOS-specific work beyond parity protection.

### 1.3 Success criteria (explicit, measurable)

| # | Criterion | Measurement |
|---|---|---|
| SC1 | Configure succeeds | `cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DLSE_ENABLE_HRX=OFF -DCMAKE_BUILD_TYPE=Release` exits 0 on Windows; summary block (`CMakeLists.txt:510-524`) prints `HRX backend: OFF` |
| SC2 | Full build | `cmake --build build` produces all `lse_*` libraries, `lse.exe`, `lse-server.exe`, with `fastokens_ffi.lib` linked (Stage 1 P1 acceptance 1–2) |
| SC3 | Self-registration intact | Registration smoke ctest (WP2) passes: CPU backend enumerable by name; kernel-primitive census > 0 — proves `/WHOLEARCHIVE` replicated all 8 whole-archive sites (Stage 1 P1 acceptance 3, R2) |
| SC4 | Runtime sanity | `lse.exe --version` and a `--list-backends`-style query run with no missing-DLL errors (Stage 1 P1 acceptance 4) |
| SC5 | Tests pass | Let **M_win** = number of ctest entries registered by a successful Windows configure (baseline captured at M1 via `ctest -N`, committed to `windows-enablement/logs/`). Repo registers 102 `lse_add_test` calls total (verified count; Windows subset smaller due to `if(UNIX)`/`if(APPLE)`/HRX/tokenizer gating). At M3: **≥ (M_win − K) of M_win pass**, where K = KNOWN_FAILURES.md Windows entries, **K ≤ 5**, each K entry documented as a platform-capability gap (e.g., abstract AF_UNIX namespace), never a product defect |
| SC6 | No upstream-tracked behavioral change on Linux/macOS | Linux (WSL) parity run green at every gate M1–M4 on the same SHA; all fork edits outside new additive files are `#ifdef _WIN32` / `if(WIN32)` / compiler-ID-guarded (review checklist, §4.3); Stage 1 P1 acceptance 5 (zero modification to `src/backends/hrx/**` behavior) |
| SC7 | CI green | `windows.yml` green **3 consecutive runs** on `windows-latest` + parity jobs green; workflow duration < 45 min (PR3) |
| SC8 | Mergeability preserved | Branch rebases onto `upstream/master` weekly with ≤ 2 conflicting files per rebase (PR1 trigger threshold); pre-rebase tags pushed to fork |

---

## 2. Work Breakdown Structure (16 work packages)

Size: S ≤ ~half day, M ≈ 1–2 days, L ≈ 3–5 days of senior-developer-agent effort. All WPs produce one commit (or small stack) referencing the WP-ID; evidence logs go to `windows-enablement/logs/`.

### Phase 1 — Core + CPU backend build green

#### WP0 — Environment provisioning & toolchain pre-flight · **S** · deps: none · *parallel: starts day 0, gates everything*
- **Inputs:** Stage 1 §6 (toolchain decision), `BUILD_FROM_SOURCE.md` prerequisites table; D2 (Stage 1 §7.1.D2); R4 (`ring`/NASM concern, `third_party/fastokens-ffi/Cargo.toml:15`).
- **Changes:** No repo code. Install/verify: LLVM 21 (clang-cl), VS 2022 Build Tools (MSVC STL/UCRT), Ninja, Rust stable `x86_64-pc-windows-msvc`, git, NASM. Smoke: (a) trivial C++26 std CMake+Ninja+clang-cl project builds; (b) **D2 pre-flight** — clone `reference/fastokens` at pinned SHA and run `cargo build --locked --release --target x86_64-pc-windows-msvc` for `third_party/fastokens-ffi`; record exact versions.
- **Acceptance:** version log committed (`logs/wp0-toolchain.md`); smoke project builds; D2 produces `fastokens_ffi.lib` **or** a documented failure that triggers the P1a/P1b fallback (Appendix A, Q1 disposition). D2 success is the entry gate for WP8.

#### WP1 — CMake foundation & HRX gating · **M** · deps: WP0 · *delivers M1*
- **Inputs:** A1 (`CMakeLists.txt:1`), A2 (`CMakeLists.txt:5-10` g++-16 probe), A3 (`cmake/LSECompilerFlags.cmake:4-45`), F1 (`cmake/LSEOptions.cmake:6`, skip line at `src/backends/CMakeLists.txt:19-23`); Stage 1 §4.
- **Changes:** Bump `cmake_minimum_required` to 3.30; guard g++-16 auto-probe with `if(NOT WIN32)`; make `lse_apply_common_flags()` compiler-ID-branched (clang-cl keeps the accepted `-W` set, `-Wformat=2` → `-Wformat -Wformat-security`; cl.exe `/W4` subset prepared for fast-follow; `-fno-fast-math` kept on clang-cl, **never** mapped to `/fp:fast` — R7; ASan spelling per compiler); default `LSE_ENABLE_HRX=OFF` on WIN32 with status message "HRX backend: not supported on Windows in Phase 1".
- **Acceptance (=M1):** SC1 holds; Linux and macOS configure output unchanged (parity spot-check); `ctest -N` on Windows yields the **M_win baseline**, committed to `logs/m1-ctest-baseline.txt`; all edits are additive `if(WIN32)`/compiler-ID branches.

#### WP2 — Force-load helper + registration smoke ctest · **M** · deps: WP1 · *parallel: WP3 can start immediately after commit; WP2 must land before first integration link (WP10)*
- **Inputs:** A4 (8 whole-archive sites: `CMakeLists.txt:117-122,157-162,273-278,285-290,374-379,427-432`; `src/backends/cpu/CMakeLists.txt:12-17`; `src/backends/hrx/CMakeLists.txt:83-88`); R2 (critical risk); Stage 1 §4 (LINKER: prefix rejected by lld-link).
- **Changes:** New `lse_force_load(target)` CMake helper (new file `cmake/LSELink.cmake` preferred — additive): APPLE → `-force_load`, WIN32 → `"LINKER:/WHOLEARCHIVE:$<TARGET_FILE:tgt>"`, else GNU `--whole-archive`; replace all 8 sites. New smoke ctest asserting CPU backend enumerable by name and kernel-primitive census > 0.
- **Acceptance:** helper + smoke test committed and **verified on Linux/macOS first** (existing suites still pass — proves no regression); smoke test registered on Windows (will only execute once WP10 builds it); per-archive checklist (8/8 sites use helper) recorded in the WP2 commit message.

#### WP3 — Platform shim (B1) · **L** · deps: WP1 · *blocks WP4–WP7; the fork-delta concentration point (R1 mitigation)*
- **Inputs:** B1; full API surface enumerated in Stage 1 §3.1 (rows: unistd, mman, dlfcn, pread/open flags, flock, mkdtemp, setenv, sysconf, /proc/self/{statm,exe}, clocks, gethostname, getpid, HOME resolution); precedent patterns `src/communication/socket.cpp:129-130`, `loomc_compiler.cpp:22-28`.
- **Changes:** New `include/lse/core/platform.hpp` + `src/core/platform_posix.cpp` + `src/core/platform_win.cpp`, added to `lse_core` via additive `elseif(WIN32)` source branch (`CMakeLists.txt:40-50`). APIs: fd ops (`pread`, `open` flag mapping: `O_CLOEXEC`→`_O_NOINHERIT`, `O_NOFOLLOW`→`CreateFileW`+`FILE_FLAG_OPEN_REPARSE_POINT`); anon + file-backed map/unmap/discard (`VirtualAlloc`/`CreateFileMappingW`/`MapViewOfFile`/`DiscardVirtualMemory`); page size; file lock (`flock`↔`LockFileEx`, semantic delta documented in header — R8); `mkdtemp`; `setenv`→`_putenv_s`; pid; hostname; monotonic/boottime clocks (`QueryPerformanceCounter`/`QueryUnbiasedInterruptTime`); dl shim (open/sym/this-module/close/addr → `LoadLibraryW`/`GetProcAddress`/`GetModuleHandleW`/`GetModuleHandleExW(FROM_ADDRESS)`+`GetModuleFileNameW`); home-dir resolution (`%USERPROFILE%`/`%LOCALAPPDATA%`, `\\?\` long-path policy — R9). `platform_win.cpp` includes `<winsock2.h>` before `<windows.h>`, defines `WIN32_LEAN_AND_MEAN` (Stage 1 P1 risk note). POSIX side is a pure pass-through (zero Linux/macOS behavior change).
- **Acceptance:** shim compiles on all 3 platforms; Linux/macOS parity run green; a small shim self-test ctest (map/lock/clock/dl/env round-trips) passes everywhere; header doc-comments record every semantic delta. Disposition of Stage 1 Q2: **single shim** (Appendix A).

#### WP4 — Core & trace module ports (C1, C4) · **M** · deps: WP3 · *parallel: WP4→WP5 is a serial pair, running concurrently with WP6 and WP7*
- **Inputs:** C1 (`src/core/file_read.cpp:22,41,56,67,80-82` — Stage 1 marks it "Root dependency; do first"), C4 (`src/trace/record.cpp:14-35`).
- **Changes:** Route `pread`/`open`/mmap/munmap/madvise/page-size in `file_read.cpp` through the shim; add `_WIN32` clock arm to `record.cpp` (Apple `mach_continuous_time` branch untouched; `__uint128_t` at :32 stays Apple-guarded).
- **Acceptance:** `lse_core`, `lse_trace` compile under clang-cl; no public API change; Linux/macOS core/trace tests unchanged and passing.

#### WP5 — Model & graph module ports (C2, C3) · **M** · deps: WP3, WP4 · *hot-file watch: `jit_cache.cpp` on R1 watchlist — check upstream log before starting*
- **Inputs:** C2 (`src/model/weights.cpp:4,176,206,227-243,452,493-499`; `src/model/dflash2_convert.cpp:242,250,366,374,612,701`, `__APPLE__` branches 278-313 kept as-is), C3 (`src/graph/jit_cache.cpp:336-349,1017-1019,1033`).
- **Changes:** weights.cpp file-map/discard/HOME → shim; dflash2_convert pread/open/mkdtemp → shim; jit_cache lock → shim (`LockFileEx`), setenv → shim, cache root fallback to `%USERPROFILE%`/`%LOCALAPPDATA%` keeping `LSE_CACHE_DIR` authoritative; lock-file paths unchanged for hf-hub cross-tool compat (R8).
- **Acceptance:** `lse_model`, `lse_graph` compile; jit-cache round-trip exercisable on Windows (test port itself is WP12); Linux paths/behavior byte-identical (parity).

#### WP6 — Communication module port (C5 + E1) · **L** · deps: WP3 · *parallel with WP4/WP5/WP7*
- **Inputs:** C5 (`src/communication/endpoint.cpp:3-4,273-274`; `socket.cpp:7-11,122-149,160-165,189,373,444-455,616-622`; `stream_link.cpp:3-4,209,327,403`; `poller_epoll.cpp`; poller selection `CMakeLists.txt:97-101`), E1 (link `ws2_32`/`crypt32` at `CMakeLists.txt:102-123,442-446`); R6.
- **Changes:** New `src/communication/poller_winsock.cpp` (WSAPoll or select + loopback-socket wake pair replacing eventfd) selected via additive `elseif(WIN32)` at `CMakeLists.txt:97-101`; socket.cpp winsock includes/`closesocket`/`ioctlsocket(FIONBIO)`/abstract-namespace rejection extended to `_WIN32` (macOS precedent :129-130); stream_link.cpp `sendmsg`→`WSASend` (no MSG_NOSIGNAL — Windows never raises SIGPIPE); endpoint.cpp one-time `WSAStartup`, winsock getaddrinfo, `nanosleep`→`std::this_thread::sleep_for`; rdma.cpp registers transport "unavailable on Windows" (no verbs SDK); link `ws2_32` (+`crypt32` if httplib TLS paths compile in) to `lse_communication` and `lse_server`.
- **Acceptance:** `lse_communication` compiles and links on Windows; TCP-loopback round-trip smoke runs locally; abstract-socket path returns a clean capability error; kqueue/epoll selection and Linux behavior unchanged (Linux `test_communication` parity).

#### WP7 — Place/API/hub/probe/CLI/server ports (C6, C7) · **L** · deps: WP3; soft dep WP6 (ws2_32/httplib coordination) · *parallel with WP4–WP6*
- **Inputs:** C6 (`src/place/devices.cpp:568-604,620,627,633,643,656-661,677`), C7 (`src/api/lse.cpp:124-159,187-212,508-510`; `src/hub/pull.cpp:314,330-348,372,397,451,470`, `cache.cpp:209`, `cli.cpp`, `companions.cpp`; `src/probe/profile_store.cpp:157,178`, `link_probe.cpp:62`; `src/cli/server_main.cpp:498-499`; `src/server/http_server.cpp:13`); Stage 1 §3.1/§3.3 rows.
- **Changes:** devices.cpp dl-shim + `/proc/self/exe`→`GetModuleFileNameW` + `_WIN32` soname arm (HSA sonames stay unreachable in P1; `%HIP_PATH%` probe is a P3 stub only); api/lse.cpp LogCapture via UCRT `_pipe`/`_dup`/`_dup2`/`_read`/`_write`/`_close`, memory footprint via `GetProcessMemoryInfo` (heap stats report 0 with comment — **JSON keys preserved for API stability**), setenv/HOME via shim; hub flock/lseek/ftruncate/write via shim; probe `_getpid`/hostname/home via shim; server_main `SetConsoleCtrlHandler` (SIGTERM not deliverable on Windows); `CPPHTTPLIB_USE_POLL` guarded `#ifndef _WIN32` so httplib falls back to select().
- **Acceptance:** `lse_place`, `lse_api`, `lse_hub`, `lse_probe`, `lse_server` compile under clang-cl; devices.cpp with HRX off degrades gracefully (no devices, no crash) on Windows; API JSON schema unchanged (key-presence assertion); Linux/macOS parity green.

#### WP8 — Rust FFI Windows wiring (D1) · **L** · entry gate: D2 passed in WP0 · deps: WP1 · *parallel with WP3–WP7; near-critical path for M2b*
- **Inputs:** D1 (`cmake/LSERust.cmake:15-37,42-50,55-89,97,103-110`; consumption at `CMakeLists.txt:300-309`); iOS-branch precedent `LSERust.cmake:45-50`; R4.
- **Changes:** windows-msvc branch in `lse_provide_fastokens()`: archive name `fastokens_ffi.lib` (`:97`); `cargo --target x86_64-pc-windows-msvc` + target-dir `x86_64-pc-windows-msvc/release` (modeled on iOS branch); native-lib parsing for MSVC-style rustc output (`ws2_32.lib bcrypt.lib advapi32.lib userenv.lib`) replacing GNU `-l<name>` assumptions (`:64-72`); Windows fallback libs replacing `pthread dl` (`:75`); `PCRE2_SYS_STATIC=1` env; verify configure-time git clone works on Windows. Fail loudly (existing `FATAL_ERROR` paths) — never silently drop tokenizer targets.
- **Acceptance:** configure+build on Windows produces `lse_tokenizer`, `lse_hub`, `lse_server`, `lse_api`, `lse.exe`, `lse-server.exe` with `fastokens_ffi.lib` linked; one existing tokenizer test runs on Windows; Linux/macOS Rust chain untouched (parity).

#### WP9 — Portable builtins (E2) · **S** · deps: none (compile-check needs WP1) · *fully parallel float; upstream-PR batch 1 candidate*
- **Inputs:** E2 (`include/lse/core/hash.hpp:28`; `include/lse/core/dtype.hpp:37,46`; `src/graph/graph.cpp:187,257`).
- **Changes:** Convert the 4 host-side `__builtin_memcpy` sites to `std::memcpy` (or `std::bit_cast` where semantically apt; codebase already uses `<bit>` — `src/core/sha256.cpp:3`). Not required for clang-cl; lands now so both clang-cl and the cl.exe fast-follow are green and the diff is upstreamable. **Excludes** `hip_emitter.cpp:290` (HRX-scoped, Phase 3).
- **Acceptance:** 5-line-class diff; compiles clang-cl + Linux/macOS parity; no behavior change (pure spelling).

#### WP10 — P1a integration gate: all libraries + registration smoke · **M** · deps: WP2, WP4, WP5, WP6, WP7, WP9 · *delivers M2a*
- **Inputs:** Stage 1 P1 acceptance 1–3, 5; PR2 checklist.
- **Changes:** No new features expected — integration fallout fixes only (winsock include-order explosions, UCRT edge cases), each still bound by §4.3 diff rules. Full `cmake --build`; run every ctest that can build; per-archive `/WHOLEARCHIVE` review checklist (8/8); on doubt inspect `lld-link /MAP` output for registrar symbols.
- **Acceptance (=M2a):** all `lse_*` libs build under clang-cl; registration smoke ctest **passes on Windows** (SC3); warning inventory recorded in `logs/m2a-build.txt`; Linux/macOS parity green (SC6).

#### WP11 — P1b executables gate · **S** · deps: WP8, WP10 · *delivers M2b*
- **Changes:** None expected beyond link-line fixes.
- **Acceptance (=M2b):** `lse.exe`, `lse-server.exe` produced; SC2/SC4 demonstrated; all five Stage 1 P1 acceptance criteria evidenced in `logs/m2b-acceptance.md`.

### Phase 2 — Tests green + Windows CI

#### WP12 — Test-suite ports (T1–T5, T7) · **L** · deps: WP11 · *parallel with WP13* · **owner decisions: test-strategist stage (Q3)**
- **Inputs:** T1 (19 `unistd.h` test files, Stage 1 §3.1 row 2; prefer harness-level shim in `tests/harness.hpp`), T2 (`tests/test_server_shutdown.cpp:5-6,24-37` fork→`CreateProcess`+`GetExitCodeProcess` or `#ifndef _WIN32` split), T3 (`test_communication.cpp` abstract-socket cases → path-based AF_UNIX in `%TEMP%` or TCP loopback — test strategist decides per Q3; `CPPHTTPLIB_USE_POLL` guard from WP7 covers `test_server_shutdown.cpp:2`), T4 (env-var audit: `set_tests_properties(ENVIRONMENT ...)` portable, verify values only), T5 (KNOWN_FAILURES.md Windows entries with reason-string format defined by test strategist; mechanism `tests/CMakeLists.txt:9-11,597-606`), T7 (`test_server_cli.py` POSIX-assumption audit — subprocess/signal semantics).
- **Acceptance (=M3, SC5):** local Windows `ctest --output-on-failure`: ≥ (M_win − K) pass, K ≤ 5, every K entry a documented platform-capability gap; zero product-defect skips; Linux/macOS suites unchanged (T8 parity run in same evidence bundle).

#### WP13 — Windows CI workflow + parity job (T6, T8) · **M** · deps: WP11 · *parallel with WP12 (workflow may land while WP12 finishes; gate M4 requires both)*
- **Inputs:** T6 (model on `.github/workflows/ci.yml` minus self-hosted GPU steps `ci.yml:25-28`), T8, PR3 (CI minutes), R3 (C++26 drift detection wants scheduled runs).
- **Changes:** New `.github/workflows/windows.yml`: `windows-latest`; install LLVM 21 (winget), Rust stable-msvc, Ninja; cargo + `reference/fastokens` caching; configure per SC1; build; `ctest --output-on-failure`; triggers: PR + push to `feature/windows-enablement` + **weekly schedule** (surfaces upstream C++26 drift within a week — R3). Linux parity job on the same SHA (WSL or ubuntu-latest, mirroring existing ci structure).
- **Acceptance (=M4, SC7):** 3 consecutive green runs; duration < 45 min; parity green on identical SHA; workflow file is purely additive to `.github/workflows/`.

#### WP14 — Documentation · **S** · deps: WP13 · *delivers M5* · **owner: technical-writer stage (Q4)**
- **Inputs:** Q4; A1 stub (Windows prerequisites in `BUILD_FROM_SOURCE.md` created in WP1, finalized here); R8/R9 semantic deltas; KNOWN_FAILURES policy from WP12.
- **Changes:** `BUILD_FROM_SOURCE.md` Windows column (LLVM 21 + VS 2022 + Rust + NASM/Ninja); "no GPU required" note mirroring README claim; cache locations (`%LOCALAPPDATA%` replacing `~/.lse`/`~/.cache`, env overrides authoritative); LockFileEx-vs-flock and long-path notes; HRX-off status + Phase 3 outlook.
- **Acceptance (=M5):** tech-writer artifact reviewed; **fresh-clone dry run**: reviewer follows the doc alone on a clean Windows machine/VM and reaches a green configure+build.

### Phase 3 — Stretch (post-M4 only)

#### WP15 — HRX compile-only spike (S1–S5) · **L** · deps: M4 + Q8 SDK-layout verification (entry gate)
- **Inputs:** Stage 1 §7 Phase 3 (S1 SDK discovery in `LSEOptions.cmake`; S2 device-independent HRX TU ports; S3 comgr dl-shim + clang-resource-dir discovery; S4 loader paths behind `_WIN32`; S5 capture-target tests as acceptance vehicle, pattern `tests/CMakeLists.txt:580-593`); R5.
- **Acceptance:** one emitted kernel compiles to an AMDGPU code object via Windows comgr in a ctest; backend registers "compile-only, no dispatch"; dispatch explicitly out of scope.

### 2.1 Dependency graph (edges)

```
WP0 ─► WP1 ─► WP2 ────────────────────────┐
        │                                 │
        ├─► WP3 ─► WP4 ─► WP5 ────────────┤
        │     └────► WP6 ─────────────────┼─► WP10 (M2a) ─► WP11 (M2b) ─► WP12 ─┬─► M3
        │     └────► WP7 (soft-dep WP6) ──┘         ▲                    WP13 ─┴─► M4 ─► WP14 (M5) ─► [WP15]
        └─► WP8 (entry gate: D2 from WP0) ──────────┘
WP9: free float, must precede WP10
Parallelizable sets: {WP8 ∥ WP3..WP7}; {WP4→WP5 ∥ WP6 ∥ WP7}; {WP9 anytime}; {WP12 ∥ WP13}
```

---

## 3. Sequencing Rationale & Critical Path

**Critical path:** `WP0 → WP1 → WP3 → WP6/WP7 (L-size ports) → WP10 → WP11 → WP12 → WP13 → WP14`. WP8 (Rust) is a **parallel near-critical path** merging at WP11; WP2 merges at WP10.

Why this order:

1. **WP1 before everything:** no Windows artifact can be validated until configure survives (g++-16 probe, GNU flag wall, cmake_minimum 3.24 all block at configure time). F1 is folded in so **every configure from day 1 prints HRX OFF** — nobody accidentally explores HRX (scope-creep guard PR5).
2. **WP2 (force-load + smoke) immediately after WP1, before any port:** Stage 1 names silent self-registration loss (R2) the single most dangerous defect class — link succeeds, registrars stripped, everything fails confusingly downstream. The guard must exist **before the first successful Windows link** (WP10), and it is verified on Linux first so it can never itself be the source of a regression. *(Note: the tasking brief attributed this protection to E1/E2; Stage 1 correctly attributes it to A4 — this plan follows Stage 1. E1 rides with WP6 because ws2_32 is needed exactly when winsock code lands; E2 is toolchain portability, floated as WP9.)*
3. **WP3 (shim) before module ports C1–C7:** every port item calls shim APIs. Porting first would scatter `#ifdef _WIN32` through core files, inflating the weekly rebase surface (R1) and the upstream-PR size (Q2). The shim concentrates ~90% of the fork delta into 3 new files that **cannot conflict** with upstream.
4. **C1 (WP4) first among ports:** Stage 1 marks `file_read.cpp` the root dependency (`weights.cpp`/`jit_cache.cpp` consume it); WP5 therefore trails WP4.
5. **WP6 ∥ WP7 ∥ WP4/5:** communication (winsock/poller) and place/api/hub touch disjoint files; a single senior-dev agent executes them in numeric order, but nothing serializes them — enabling rescue by parallel agents if schedule slips.
6. **WP8 parallel from day 0 (D2 pre-flight):** the Rust chain is the longest-lead external uncertainty (ring/NASM/pcre2 — R4) and is independent of all C++ ports. Testing it standalone in WP0 costs minutes and de-risks the P1b milestone; per Q1 disposition, P1a (libs) survives even if WP8 fails.
7. **Integration gates (WP10/WP11) after all ports:** avoids repeatedly debugging half-ported trees; smoke evidence is collected once, cleanly.
8. **WP12 before WP13's M4 gate:** local tests must pass before burning CI minutes iterating (PR3); the workflow file itself may land in parallel so infrastructure shakeout (runner tool versions, caching) overlaps test porting.
9. **WP14 last:** docs describe a green, stable state; the fresh-clone dry run doubles as final acceptance.
10. **WP15 strictly post-M4:** Phase 3 must never compete with Phases 1–2 for capacity (PR5).

---

## 4. Governance: Long-Lived Fork Branch vs. Churning Upstream

Upstream velocity: **654 commits / 8 weeks (~80/week)**. The branch must stay mergeable for the program's full duration (est. 4–8 weeks) and beyond.

### 4.1 Sync cadence & checkpoints
- **Weekly rebase** of `feature/windows-enablement` onto `upstream/master` (fixed day; never let two weeks pass).
- **Mandatory sync checkpoint before every milestone gate M1–M4**: a gate certifies only against a base ≤ 7 days old.
- **Pre-rebase safety tag** (`pre-rebase/YYYY-MM-DD`) pushed to the fork before any rebase; never force-push without it.
- Post-rebase ritual: Linux parity configure+build, Windows configure smoke (SC1), then resume.

### 4.2 Hot-file watchlist (R1 conflict concentrators)
`src/graph/graph.cpp`, `src/graph/jit_cache.cpp`, `src/backends/hrx/hrx_backend.cpp`, root `CMakeLists.txt`. Before starting WP5/WP9 (and any work touching these), review `git log upstream/master -- <file>` since last sync; if upstream rewrote a hot file, re-derive the affected shim call-sites before editing.

### 4.3 Diff-minimization rules (binding on the implementer stage; enforced by quality reviewers)
1. **Additive files first:** new files (`platform.hpp`, `platform_posix.cpp`, `platform_win.cpp`, `poller_winsock.cpp`, `cmake/LSELink.cmake`, `windows.yml`, smoke test) are the preferred delta — zero-conflict surface.
2. **Guards, not rewrites:** in-place edits are minimal `#ifdef _WIN32` / `elseif(WIN32)` / compiler-ID branches following the codebase's existing `if(APPLE)` and `__APPLE__`/`__linux__` idiom (`CMakeLists.txt:97-101`, `devices.cpp:580-584`, `loomc_compiler.cpp:22-28`).
3. **No reformatting, no renames, no code motion, no whitespace churn** in any upstream-owned file — ever.
4. **One WP = one commit (or tight stack)** with `WP<n>:` prefix in the message; each commit independently revertable.
5. **`src/backends/hrx/**` is frozen in Phases 1–2** (except the already-guarded E2 exclusion) — F1 keeps it out of the build; touching it is a PR5 trigger.
6. **POSIX-side pass-through:** the shim's POSIX implementations must be behavior-identical wrappers; any Linux/macOS parity failure blocks the WP.

### 4.4 Upstream engagement (Q6 disposition: YES, upstream in batches)
Every merged upstream PR permanently removes rebase cost. Three batches, each small and idiom-matched (the `if(APPLE)` precedent suggests high acceptance odds):
- **Batch 1 (after WP2/WP9, pre-M2):** `lse_force_load` helper, compiler-ID-branched flags, `cmake_minimum_required` bump, `std::memcpy`/`bit_cast` conversions — build-system hygiene, valuable to upstream regardless of Windows.
- **Batch 2 (after M2b):** platform shim + guarded call-site one-liners (C1–C7 deltas).
- **Batch 3 (after M3):** test ports + KNOWN_FAILURES mechanism usage.
Rules: each PR ≤ ~300 diff lines; issue-first for semantic questions (abstract-AF_UNIX policy, LockFileEx delta); reference the fork branch with full context; never block a milestone on upstream review (fork remains source of truth until merged).

---

## 5. Risk Management Plan

### 5.1 Inherited register (Stage 1 §8) — owner mapping

| # | Risk (Stage 1) | Owner in this program | Primary WP hooks |
|---|---|---|---|
| R1 | Upstream churn vs fork | Maintainer (§4 cadence) | all; §4.3 rules, §4.4 batches |
| R2 | Silent whole-archive/registration loss | Implementer + quality reviewer | WP2 (smoke gate), WP10 (/MAP checklist) |
| R3 | C++26 feature drift upstream | Maintainer | WP13 weekly-scheduled CI |
| R4 | Rust chain on windows-msvc | Implementer | WP0 (D2 pre-flight), WP8 |
| R5 | HRX has no Windows runtime | Maintainer (scope) | WP15 compile-only spike, gated post-M4 |
| R6 | AF_UNIX abstract namespace absent | Test strategist (Q3) | WP6 (rejection), WP12/T3 |
| R7 | Numerics parity / fast-math | Quality reviewer | WP1 (flag rules), WP12 bit-exact tests |
| R8 | flock→LockFileEx semantics | Implementer + tech writer | WP3 (doc'd in shim), WP5, WP14 |
| R9 | Long paths (MAX_PATH) | Implementer | WP3 (`\\?\` home resolution), WP13 (runner policy) |
| R10 | No AMD GPU on CI runners | — (by design) | WP13 (existing guards skip device tests) |

### 5.2 Program-level risks (new)

| # | Risk | Owner | Trigger (early-warning) | Mitigation | Contingency |
|---|---|---|---|---|---|
| PR1 | **Upstream divergence outpaces rebase capacity** — hot-file rewrite makes weekly rebase > 1 day | Maintainer | > 2 conflicting files in 2 consecutive weekly rebases; upstream rewrite of a §4.2 hot file | §4.3 diff-minimization; §4.4 batch 1 upstreamed ASAP; sync before each gate | Pin fork base to a stable upstream tag; drop to monthly sync with explicit drift budget; raise upstream PR urgency |
| PR2 | **Registration loss escapes smoke test** (smoke covers CPU backend + kernels; other registrars — comm/dist/graph/model/place — could still strip) | Implementer | any "no backend / empty registry" test failure at WP10; census mismatch vs Linux counts | Extend WP2 smoke to census **every** self-registering archive (8/8) with expected-nonzero counts; `/MAP` inspection checklist at WP10 | Per-archive `/WHOLEARCHIVE` verification; convert to object-library linking if lld-link misbehaves |
| PR3 | **CI minutes/cost overrun** (windows-latest multiplier; cargo+ring build is heavy) | Maintainer | workflow duration > 45 min or weekly minute budget > 80% consumed | Cargo + `reference/fastokens` caching; sccache evaluation; path filters; no per-commit pushes to the branch | Convert to nightly `schedule` + `workflow_dispatch`; run Windows CI on the fork only until M4 |
| PR4 | **Single-maintainer bus factor** (solo developer + agent pipeline) | Maintainer | maintainer unavailable > 2 weeks mid-phase | This plan + Stage 1 evidence + decision log make every WP independently executable; branch and tags always pushed to fork; upstream PRs make work publicly recoverable | Handoff package = `windows-enablement/` directory (charter, WBS, gates, logs); any stage agent can resume from the last committed gate evidence |
| PR5 | **Scope creep into Phase 3 / HRX before M4** | Quality reviewer | any diff touching `src/backends/hrx/**` (beyond E2 exclusion) before M4 | F1 default-OFF; §4.3 rule 5; review checklist item | Revert offending commit; re-run gate |
| PR6 | **LLVM 21 / clang-cl instability** with `/std:c++latest` (ICE, header bugs) | Implementer | any compiler crash or miscompile reproducing on minimal case | Pin exact LLVM version in docs + CI (WP13); minimal-case reduction; report to LLVM | Patch-version rollback (21.x); temporary cl.exe path (WP9 already makes it viable); hold WP at last green toolchain |
| PR7 | **Test semantic drift** from platform deltas (mandatory LockFileEx, long paths, SIGTERM undeliverable) producing flaky Windows failures | Test strategist | intermittent jit_cache/hub/server-shutdown failures on Windows | Semantic deltas documented in shim header (WP3) and handed to test strategist; T2/T3 rewrites use platform-native patterns | KNOWN_FAILURES entry with reason (counts against K ≤ 5) + upstream issue describing the delta |

### 5.3 Review cadence
Risk register reviewed at every milestone gate; PR1/PR3 triggers checked at each weekly rebase; new risks appended with IDs PR8+ in the decision log.

---

## 6. Milestones & Gates

| Milestone | Gate criteria (all must hold) | Evidence required (committed under `windows-enablement/logs/` unless noted) |
|---|---|---|
| **M0 — Plan approved** | This document + Stage 1 reviewed by quality-reviewer stage; Appendix A dispositions (Q1/Q2/Q6/Q7) confirmed by maintainer | Review artifact (`03-quality-review-*.md`); decision-log entry `DECISIONS.md` |
| **M1 — Configure succeeds** (WP1) | SC1 (clang-cl+Ninja configure exit 0, `HRX backend: OFF` printed); upstream base ≤ 7 days old (§4.1); Linux/macOS configure parity spot-check | Configure log; `m1-ctest-baseline.txt` (M_win from `ctest -N`); rebase tag name |
| **M2 — Core+CPU build** (WP10+WP11; two sub-gates per Q1 disposition) | **M2a (libs):** all `lse_*` libraries build; registration smoke ctest passes on Windows (SC3); 8/8 force-load checklist signed off; Linux/macOS parity green. **M2b (executables):** `lse.exe` + `lse-server.exe` built with `fastokens_ffi.lib` linked; SC2 + SC4 demonstrated; all 5 Stage 1 P1 acceptance criteria met. *Fallback:* if WP8/D2 fails, M2a alone may be declared with M2b deferred (formal scope decision logged, not silent) | `m2a-build.txt` (full build log + smoke output + warning inventory); `m2b-acceptance.md` (version/list-backends transcripts); parity run logs |
| **M3 — Tests pass** (WP12) | SC5: ≥ (M_win − K) of M_win pass locally, K ≤ 5, every K entry a documented platform-capability gap; SC6: Linux/macOS suites unchanged on same SHA | Windows `ctest --output-on-failure` log; Linux parity ctest log; KNOWN_FAILURES.md diff with reasons |
| **M4 — CI green** (WP13) | SC7: `windows.yml` green 3 consecutive runs on `windows-latest` + parity job green on same SHA; duration < 45 min; weekly schedule active (R3) | 3 GitHub Actions run URLs + durations in `m4-ci-evidence.md` |
| **M5 — Docs** (WP14) | Windows column complete in `BUILD_FROM_SOURCE.md`; cache-location + semantic-delta + KNOWN_FAILURES-policy notes merged; fresh-clone dry run succeeds following docs alone | Dry-run log by reviewer; doc diffs; tech-writer artifact |

Gate discipline: a gate is declared only in `DECISIONS.md` with evidence links; any gate miss is logged with options + recommendation **before** proceeding (no silent scope changes). Stretch WP15 unlocks only after M4.

---

## 7. Stakeholder & Communication Plan

| Stakeholder | Interest | Communication vehicle & cadence |
|---|---|---|
| **Maintainer/decision-maker (user, solo)** | Scope, gates, upstream strategy | `DECISIONS.md` decision log appended at every WP completion and gate; milestone evidence bundles (§6); escalation = logged options + recommendation, decision before proceeding |
| **Pipeline agent stages** (quality reviewer ×2, test strategist, senior developer, technical writer) | Executable, unambiguous WP specs | This document + Stage 1 = the contract; each stage consumes the numbered artifacts in `windows-enablement/`; commits carry `WP<n>:` prefixes so implementer/reviewer can trace every change to a work package; handoffs at gates only |
| **Upstream project (Geramy/LSE)** | Small, idiomatic, valuable patches | §4.4 batch PRs (≤ ~300 lines each); issue-first for semantic questions (abstract AF_UNIX policy, LockFileEx delta, C++26 guard pattern per `reflect.hpp:5`); PR descriptions link fork-branch evidence; fork never waits on upstream review |
| **Future Windows users** | Working build instructions, known limitations | M5 docs (WP14): prerequisites, no-GPU note, cache locations, KNOWN_FAILURES policy, HRX-off status |

Rhythm: weekly rebase note (conflicts touched, hot-file status, PR1 trigger check) appended to `DECISIONS.md`; risk-register review at each gate; no other standing meetings (solo program — asynchronous artifacts are the only channel).

---

## Appendix A — Open-question dispositions (PM authority)

| Q (Stage 1 §9) | Disposition |
|---|---|
| Q1 reduced P1 milestone | **Adopted:** M2 splits into M2a (libs+smoke) / M2b (executables). If D2/WP8 fails, M2a may be declared with M2b formally deferred via decision-log entry — never silently. |
| Q2 shim granularity | **Single `platform.hpp` + two platform .cpp files** (Stage 1 recommendation): lowest rebase cost, smallest upstream PR, zero-conflict additive surface. |
| Q3 test socket strategy & KNOWN_FAILURES format | **Delegated to test-strategist stage** (WP12); constraint from this plan: K ≤ 5, reason strings mandatory, capability-gaps only. |
| Q4 docs | **Delegated to technical-writer stage** (WP14) with the M5 acceptance list. |
| Q5 DLL/`dllexport` | **Deferred** — static linking only; out of scope (§1.2). |
| Q6 upstream engagement | **Yes — 3-batch PR plan** (§4.4). |
| Q7 warning parity on clang-cl | **Land permissive, tighten later:** keep the `-W` set clang-cl accepts, `LSE_WERROR` stays OFF through M3; warning-parity backlog opened after M4. |
| Q8 Windows HIP SDK layout | **WP15 entry gate** — SDK layout verification (S1) is the spike's first task; WP15 does not start on assumption. |

## Appendix B — Downstream stage contracts

- **Quality reviewer (post-plan):** validate WBS completeness vs Stage 1's 17 items (all covered: A1-A3+F1→WP1, A4→WP2, B1→WP3, C1/C4→WP4, C2/C3→WP5, C5+E1→WP6, C6/C7→WP7, D1/D2→WP8/WP0, E2→WP9; T1-T8→WP12/WP13), gate measurability, §4.3 enforceability.
- **Test strategist:** consumes WP12 inputs + Q3 delegation; produces KNOWN_FAILURES policy, T2/T3 test designs, M3 evidence spec.
- **Senior developer:** executes WP0–WP14 in §3 order; bound by §4.3 diff rules; commits evidence per §6; logs decisions per §7.
- **Technical writer:** consumes M4 state + Q4; produces WP14 docs; runs fresh-clone dry run for M5.

*End of Stage 2 artifact. All technical references inherited from Stage 1 (verified at `a5b82f6`); no source modified, nothing committed.*
