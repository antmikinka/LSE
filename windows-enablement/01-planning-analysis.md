# LSE Windows Enablement — Stage 1: Planning & Requirement Analysis

**Branch:** `feature/windows-enablement` (base: master @ `a5b82f6`, v0.6.0-28)
**Date:** 2026-10-10
**Author:** Stage 1 (planning & analysis) — consumed by: PM, quality reviewers, test strategist, implementer, tech writer
**Status:** Analysis only. No source was modified.

---

## 1. Executive Summary

LSE has **zero Windows support today**: `CMakeLists.txt` contains 7 `if(APPLE)` guards and **no** `WIN32`/`MSVC` handling anywhere in the build (verified: `grep -c "if(APPLE)" CMakeLists.txt` → 7; `grep -n "WIN32\|MSVC" CMakeLists.txt` → no hits). The code assumes a POSIX host (Linux primary, macOS/iOS secondary) across file I/O, sockets, dynamic loading, clocks, and locking.

Good news that bounds the effort:

- The **host C++ code is nearly free of GNU extensions**. No inline asm in host code (all `__builtin_amdgcn_*`/`__attribute__((ext_vector_type))` occurrences are inside *emitted device-source strings*, e.g. `src/backends/hrx/hipc/hip_sources.cpp:83-176`, compiled by comgr, never by the host compiler). Only 4 host-side `__builtin_memcpy` uses exist: `include/lse/core/hash.hpp:28`, `include/lse/core/dtype.hpp:37,46`, `src/graph/graph.cpp:187,257` (+ `src/backends/hrx/hipc/hip_emitter.cpp:290`, HRX-scoped).
- The **C++26 requirement degrades gracefully**: P2996 reflection is fully guarded by `__cpp_impl_reflection` (`include/lse/core/reflect.hpp:5`, `src/backends/hrx/hipc/hip_types.cpp:47`, `src/backends/hrx/loomc/loom_types.cpp:67`) with non-reflection fallbacks. CI's claim that "g++-16 is not optional" (`.github/workflows/ci.yml:80-84`) reflects policy, not a hard compile dependency for non-GNU compilers.
- The **CPU backend is portable std C++** (`src/backends/cpu/cpu_backend.cpp` — `std::aligned_alloc`:78, `std::thread::hardware_concurrency`:31, memcpy copies). No SIMD intrinsics, no affinity, no hugepages anywhere in `src/`.
- **HRX/ROCm gates itself off naturally**: `src/backends/CMakeLists.txt:19-23` only adds the HRX backend when `LSE_ENABLE_HRX AND LSE_HRX_INCLUDE_DIR`; on Windows the `find_path` probes (`cmake/LSEOptions.cmake:110-129`) find nothing, so the backend is skipped with a status message. `LSE_ROCM_PATH` defaulting to `/opt/rocm` (`cmake/LSEOptions.cmake:22`) is inert (all `find_*` calls use `NO_DEFAULT_PATH` with those hints).
- Vendored deps are Windows-capable: `third_party/vendor/httplib.h` v0.18.3 (line 11) has full Winsock support; `nlohmann/json` is portable. The public C API header **already has a `_WIN32` branch**: `include/lse/lse.h:26-30`.
- Test infrastructure is self-contained (`tests/harness.hpp` uses only `getenv`, lines 52/75/94) and registers via a plain `add_test` helper (`tests/CMakeLists.txt:4-16`). Several POSIX-specific test blocks already no-op on Windows (`if(UNIX)` at `tests/CMakeLists.txt:429-452`, `if(APPLE)` at `317-328`).

**Recommendation:** toolchain = **clang-cl (LLVM 21) + MSVC STL/UCRT + Ninja + Rust `x86_64-pc-windows-msvc`**, with pure `cl.exe` as a fast-follow (needs only a 3-site `__builtin_memcpy` shim). MinGW-w64 rejected (§6). Phase 1 = **17 work items** (§7.1). The single most dangerous defect class is *silent* runtime loss of self-registering components if whole-archive linking is not replicated exactly (§7.1 item A4, §8 R2).

---

## 2. Verified Current State

| Fact | Evidence |
|---|---|
| C++ standard: C++26 host / C++20 device | `CMakeLists.txt:20-23` (`CMAKE_CXX_STANDARD 26`, `LSE_DEVICE_CXX_STANDARD 20`) |
| `cmake_minimum_required(VERSION 3.24)` | `CMakeLists.txt:1` — **too old**: CMake learned `cxx_std_26` in 3.25; bump required |
| Compiler auto-pick: `g++-16` before `project()` | `CMakeLists.txt:5-10` (`find_program(LSE_GXX16 NAMES g++-16)`) — must be guarded on Windows |
| 7 `if(APPLE)` guards, 0 WIN32 | `CMakeLists.txt:97,117,157,273,285,374,427` |
| Self-registration via whole-archive linking (8 sites) | `CMakeLists.txt:117-122` (communication), `157-162` (dist), `273-278` (kernels), `285-290` (graph), `374-379` (model), `427-432` (place); `src/backends/cpu/CMakeLists.txt:12-17`; `src/backends/hrx/CMakeLists.txt:83-88` |
| GNU-only warning wall + `-fno-fast-math` applied to every target | `cmake/LSECompilerFlags.cmake:5-19` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion ... -Wformat=2`), `:38` (`-fno-fast-math`), `:21-23` (`-Werror`), `:31-34` (`-fsanitize=address,undefined`) — all applied via `lse_apply_common_flags()` called ~25× |
| Poller selection: kqueue on Apple, **epoll on everything else** | `CMakeLists.txt:97-101` |
| HRX wiring: gated on `LSE_ENABLE_HRX` + found `hrx_runtime.h`; comgr/hipRTC/loomc probed under `LSE_ROCM_PATH` | `cmake/LSEOptions.cmake:6,14,49-86,108-129,137-170`; `src/backends/CMakeLists.txt:19-23`; `src/backends/hrx/CMakeLists.txt:35-70` |
| HSA runtime found by soname `libhsa-runtime64.so.1` | `cmake/LSEOptions.cmake:103-106`; consumed as `LD_LIBRARY_PATH` test env at `tests/CMakeLists.txt:266-270` (no-ops on Windows since the var stays unset) |
| Tests: `lse_add_test()` → `add_test(... WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})`, dozens of binaries; KNOWN_FAILURES.md disables tests at configure time | `tests/CMakeLists.txt:4-16, 597-606` |
| Rust FFI: cargo-driven `staticlib`, path-dep `reference/fastokens` **cloned at configure time**; archive name hardcoded `libfastokens_ffi.a` | `cmake/LSERust.cmake:10-13,15-37,97`; consumed at `CMakeLists.txt:300-309`; tokenizer gates server/CLI/hub and ~15 tests (`CMakeLists.txt:439-497`, `tests/CMakeLists.txt:129-149,164-168,205-207`) |
| Native-lib extraction assumes GNU `-l<name>` flags; fallback `pthread dl` | `cmake/LSERust.cmake:55-89` (esp. `:64-72`, `:75`) |
| No ExternalProject/FetchContent in C++ build; only the git-clone of fastokens and cargo | `cmake/LSERust.cmake:23-36` |
| Build identity: hashes whole `src/`+`include/` tree into `lse_graph`; portable (file SHA256 via `cmake -P`) | `cmake/LSEBuildIdentity.cmake:20-37,39-74`; wired at `CMakeLists.txt:298` |
| CI today: self-hosted Linux GPU runner (`ci.yml`, workflow_dispatch only), macOS workflow, release scripts all bash | `.github/workflows/{ci.yml,macos.yml,build-release.yml,runner-toolchain.yml}`; `.github/scripts/*.sh`; `scripts/bootstrap-hrx.sh`, `scripts/ios/*.sh` |
| Documented toolchains: LLVM 21 (macOS), GCC 16 (Linux) | `BUILD_FROM_SOURCE.md` prerequisites table |

---

## 3. Platform-Coupling Inventory

Severity: **blocker** = Phase 1 cannot build/link/run without it · **moderate** = needed for full Phase 1/2 function, workaroundable short-term · **trivial** = mechanical or already inert on Windows.

### 3.1 Headers & libc surface

| Construct | Files:lines | Severity | Recommended fix |
|---|---|---|---|
| `<unistd.h>` (src) | `src/api/lse.cpp:17`; `src/core/file_read.cpp:5`; `src/hub/{cache.cpp:4,cli.cpp:3,companions.cpp:3,pull.cpp:13}`; `src/probe/{profile_store.cpp:3,link_probe.cpp:3}`; `src/communication/{poller_epoll.cpp:3,poller_kqueue.cpp:8,stream_link.cpp:5,socket.cpp:13}`; `src/graph/jit_cache.cpp:7`; `src/model/{dflash2_convert.cpp:25,weights.cpp:6}`; `src/backends/hrx/{hrx_backend.cpp:7,sq_profiler.cpp:43}` | blocker | Route through a new `include/lse/core/platform.hpp` shim (§7.1 B1); HRX files out of scope Phase 1 |
| `<unistd.h>` (tests, 19 files — mostly `usleep`/`access`) | `tests/test_api_parity.cpp:9`, `test_capture_prefill.cpp:18`, `test_communication.cpp:10`, `test_dflash2.cpp:30`, `test_dflash2_convert.cpp:26`, `test_graph.cpp:16`, `test_hub.cpp:8`, `test_hybrid_tokens.cpp:12`, `test_jit.cpp:10`, `test_loom_cache.cpp:19`, `test_memory_lifecycle.cpp:10`, `test_model_info.cpp:9`, `test_probe.cpp:18`, `test_sampling_defaults.cpp:8`, `test_server_shutdown.cpp:6`, `test_tree_verify.cpp:26`, `test_variant_book.cpp:7` (+ `test_jit.cpp:9` `<sys/stat.h>`) | moderate (Phase 2) | Shim include in `tests/harness.hpp` or per-file `#ifdef _WIN32` (`Sleep(ms)`, `_access`) |
| `<sys/mman.h>` mmap/munmap/madvise/mprotect | `src/core/file_read.cpp:4,56,67,82` (anon RW scratch); `src/model/weights.cpp:4,176,206,227-243` (file-backed RO map + `MADV_DONTNEED`); `src/backends/hrx/loomc/loomc_compiler.cpp:21,89-91` (HRX, out of scope P1) | blocker | `VirtualAlloc`/`VirtualFree` for anon; `CreateFileMappingW`/`MapViewOfFile` for files; `DiscardVirtualMemory`/`VirtualUnlock` for MADV_DONTNEED |
| `<dlfcn.h>` dlopen/dlsym/dlclose/dladdr/RTLD_NOLOAD/RTLD_DEFAULT | **Always-built:** `src/place/devices.cpp:3,568-569,594-604,627,643,677`; `src/communication/rdma.cpp:20,46,59-61,114`. **HRX-gated (P3):** `src/backends/hrx/{hrx_backend.cpp:6,267-288,450, hipc/comgr_compiler.cpp:6,325, sq_profiler.cpp:39,68,404-416}`; headers `include/lse/backends/hrx/loaded_library.hpp:2`, `include/lse/backends/hrx/compiler_image_identity.hpp:10` | blocker (devices.cpp, rdma.cpp) | `LoadLibraryW`/`GetProcAddress`/`FreeLibrary`; `RTLD_NOLOAD`→`GetModuleHandleW`; `dladdr`→`GetModuleHandleExW(FROM_ADDRESS)`+`GetModuleFileNameW`. `rdma.cpp`: register transport as "unavailable on Windows" (no verbs SDK) |
| POSIX sockets: `<sys/socket.h>`,`<netinet/*>`,`<arpa/inet.h>`,`<netdb.h>`,`<sys/uio.h>` | `src/communication/endpoint.cpp:3-4`; `socket.cpp:7-11`; `stream_link.cpp:3-4` | blocker | `<winsock2.h>`+`<ws2_32>` (include before `<windows.h>`); `closesocket`; `ioctlsocket(FIONBIO)` for `fcntl O_NONBLOCK` (`socket.cpp:160-165`); `WSAStartup` once per process; `writev`→`WSASend` iovec |
| `sendmsg`/`MSG_NOSIGNAL` | `src/communication/stream_link.cpp:209,327,403` | blocker | `WSASend` (no `MSG_NOSIGNAL` concept; SO_NOSIGPIPE n/a — Windows never raises SIGPIPE) |
| AF_UNIX + **abstract namespace** (`sun_path[0]='\0'`) | `src/communication/socket.cpp:122-149,189,373,444-455,616-622`; abstract already errors on macOS (`socket.cpp:129-130`) | blocker | Win10 1803+ has AF_UNIX but **no abstract namespace**; extend the macOS error branch to `_WIN32`; tests must use filesystem-path unix sockets or TCP loopback |
| epoll / eventfd poller | `src/communication/poller_epoll.cpp:1-2,26-27,39,113,138-149`; selected for all non-Apple at `CMakeLists.txt:97-101` | blocker | New `src/communication/poller_winsock.cpp` (WSAPoll or select + loopback-socket wake pair instead of eventfd); add `elseif(WIN32)` branch |
| kqueue poller | `src/communication/poller_kqueue.cpp` | trivial | Apple-only; untouched |
| `pipe`/`dup`/`dup2`/`read`/`write`/`close` on fds | `src/api/lse.cpp:124-159` (stderr log capture) | moderate | UCRT `_pipe`/`_dup`/`_dup2`/`_read`/`_write`/`_close` (`<io.h>`) — near drop-in |
| `pread`/`lseek`/`ftruncate`/`truncate`/`open(O_CLOEXEC\|O_NOFOLLOW)` | `src/core/file_read.cpp:22-23,41`; `src/hub/pull.cpp:314,372,397,451,470`; `src/model/dflash2_convert.cpp:242,250,366,374,612`; `src/graph/jit_cache.cpp:336` (`O_NOFOLLOW`) | blocker (file_read), moderate (rest) | `_pread`/`_lseeki64`/`_chsize_s`/`_open` (UCRT); `O_CLOEXEC`→`_O_NOINHERIT`; `O_NOFOLLOW`→`CreateFileW` + `FILE_FLAG_OPEN_REPARSE_POINT` |
| `flock(LOCK_EX)` file locking | `src/graph/jit_cache.cpp:345-349`; `src/hub/pull.cpp:330-348` (hf-hub-compatible `.locks/` protocol) | moderate | `LockFileEx(LOCKFILE_EXCLUSIVE_LOCK)` — mandatory locking, strictly stronger than advisory flock; document semantic delta |
| `mkdtemp` | `src/model/dflash2_convert.cpp:701`; `src/backends/hrx/sq_profiler.cpp:462` (HRX, P3) | moderate | `GetTempPathW` + retry loop on random name (`_wmktemp` semantics differ) |
| `setenv` (no MSVC equivalent) | `src/graph/jit_cache.cpp:1033`; `src/place/devices.cpp:633`; `src/hub/cache.cpp:209`; `src/api/lse.cpp:510` | moderate | `_putenv_s` |
| `sysconf(_SC_PAGESIZE)` / `getpagesize()` | `src/api/lse.cpp:193`; `src/core/file_read.cpp:80`; `src/backends/hrx/hrx_backend.cpp:1342` (P3) | moderate | `GetSystemInfo().dwPageSize` |
| `/proc/self/statm`, `mallinfo2()` (**glibc-only**), `<malloc.h>` | `src/api/lse.cpp:12,187-193,211-212` | moderate | `GetProcessMemoryInfo` (PROCESS_MEMORY_COUNTERS.WorkingSetSize); heap stats via `_heapwalk` or report 0 with a comment — keep JSON keys for API stability |
| `/proc/self/exe` | `src/place/devices.cpp:620` | moderate | `GetModuleFileNameW(NULL,...)` |
| `/proc/meminfo` | `src/backends/hrx/loomc/loomc_compiler.cpp:72` | trivial (P3) | `GlobalMemoryStatusEx` when HRX lands |
| `clock_gettime(CLOCK_MONOTONIC / CLOCK_BOOTTIME)` | `src/trace/record.cpp:14-35` (Apple branch already uses `mach_continuous_time`, line 26-32) | blocker for lse_trace | Add `_WIN32` branch: `QueryPerformanceCounter` (monotonic) + `QueryUnbiasedInterruptTime`/`QueryInterruptTime` (boottime). Note `__uint128_t` at `record.cpp:32` is Apple-guarded — no MSVC `__int128` problem |
| `nanosleep` | `src/communication/endpoint.cpp:273-274` | trivial | `std::this_thread::sleep_for` |
| `gethostname` | `src/probe/link_probe.cpp:62` | trivial | Winsock `gethostname` (needs WSAStartup) or `GetComputerNameExA` |
| `getpid` | `src/probe/profile_store.cpp:157`; `src/hub/pull.cpp:241` | trivial | `_getpid` / `GetCurrentProcessId` |
| `std::signal(SIGINT/SIGTERM)` | `src/cli/server_main.cpp:498-499` | moderate | SIGINT works; SIGTERM is not deliverable on Windows → `SetConsoleCtrlHandler` |
| `ssize_t`/`off_t` | passim (`file_read.cpp:22`, `socket.cpp:638`, `stream_link.cpp:208...`, `api/lse.cpp:124`, `pull.cpp:470`) | trivial | UCRT `<sys/types.h>` defines both; shim header can typedef for safety |

### 3.2 GNU/Clang language & ABI constructs (host code)

| Construct | Files:lines | Severity | Recommended fix |
|---|---|---|---|
| `__builtin_memcpy` in **public headers** | `include/lse/core/hash.hpp:28`; `include/lse/core/dtype.hpp:37,46` | blocker for cl.exe; trivial for clang-cl | Prefer upstreaming `std::memcpy`/`std::bit_cast` (code already includes `<bit>`: `src/core/sha256.cpp:3`); else `#ifdef _MSC_VER #define __builtin_memcpy memcpy` in platform shim |
| `__builtin_memcpy` in src | `src/graph/graph.cpp:187,257`; `src/backends/hrx/hipc/hip_emitter.cpp:290` (P3) | same | same |
| `__attribute__((visibility("default")))` | `include/lse/lse.h:29` — **already inside `#else` of `#if defined(_WIN32)`** (line 26-30) | trivial | none (LSE_API is empty on Windows; fine for static libs — see Open Question Q5) |
| `__asm__("label")` symbol aliasing | `src/backends/hrx/hrx_backend.cpp:56-80` — inside `#if defined(__APPLE__) && TARGET_OS_IOS` (line 50) | trivial | none (Apple-only, HRX-gated) |
| `__attribute__((ext_vector_type))`, `__builtin_amdgcn_*` | `src/backends/hrx/hipc/{hip_types.cpp:59, hip_sources.cpp:61-176}` — **string literals** emitted into device source | trivial | none — compiled by comgr's clang, never host |
| P2996 reflection | guarded: `include/lse/core/reflect.hpp:5`; `hip_types.cpp:47-50`; `loom_types.cpp:67` | trivial | none — fallback paths exist; do not enable `-freflection` (GNU≥16 only, `LSECompilerFlags.cmake:26-29`) |
| `__uint128_t` | `src/trace/record.cpp:32` (Apple-guarded) | trivial | none |

### 3.3 Hardcoded paths & environment

| Construct | Files:lines | Severity | Recommended fix |
|---|---|---|---|
| `/opt/rocm` default | `cmake/LSEOptions.cmake:22`; `src/place/devices.cpp:656,661`; baked as `LSE_CONFIGURED_ROCM_PATH` at `CMakeLists.txt:423-424` | trivial (P1) / moderate (P3) | inert in P1 (find fails → HRX off). P3: probe `%HIP_PATH%` / `C:\Program Files\AMD\ROCm\<ver>` |
| `libhsa-runtime64.so.1` / `.dylib` sonames | `src/place/devices.cpp:580-584`; `src/backends/hrx/hrx_backend.cpp:257-260`; `cmake/LSEOptions.cmake:103-106`; `LSE_HSA_RUNTIME_DIR` define `CMakeLists.txt:481-494` | trivial (P1, unreachable) / moderate (P3) | add `_WIN32` branch `hsa-runtime64.dll` (or `amdhdl64.dll` per HIP SDK) when P3 lands |
| `LD_LIBRARY_PATH` / `DYLD_*` handling | `tests/CMakeLists.txt:266-270,441-451`; `src/place/devices.cpp:542`; `src/cli/main.cpp:480-484` | trivial | env-var test blocks no-op when `LSE_ROCM_RUNTIME_DIR` unset; Windows would use `PATH` if ever needed |
| `$HOME/.lse/cache`, `$HOME/.cache/huggingface` | `src/graph/jit_cache.cpp:1017-1019`; `src/model/weights.cpp:452,493-499` (`hf_cache_root`); `src/probe/profile_store.cpp:178`; `src/api/lse.cpp:508-510` | moderate | Fall back to `%USERPROFILE%`/`%LOCALAPPDATA%`; keep `LSE_CACHE_DIR`/`HF_HOME`/`HF_HUB_CACHE` env overrides authoritative (they already are) |
| `0644`/`0600` mode bits on `open()` | `src/hub/pull.cpp:330,372`; `src/graph/jit_cache.cpp:336`; `src/model/dflash2_convert.cpp:366,612` | trivial | ignored by UCRT; keep for portability |
| Bash scripts (`scripts/bootstrap-hrx.sh`, `scripts/ios/*.sh`, `.github/scripts/*.sh`) | `scripts/`, `.github/scripts/` | trivial (P1) | not needed for P1/P2 CI; optional PowerShell dev-setup later |

### 3.4 Tests

| Construct | Files:lines | Severity | Recommended fix |
|---|---|---|---|
| `fork()`/`waitpid()`/`pipe()`/`WIFEXITED` | `tests/test_server_shutdown.cpp:5-6,24-37` | blocker (P2) | Rewrite case 1 with `CreateProcess` + `GetExitCodeProcess`, or split into `#ifndef _WIN32` + a Windows twin; remaining cases (lines 39+) are pure std + httplib and portable |
| `CPPHTTPLIB_USE_POLL` define | `tests/test_server_shutdown.cpp:2`; `src/server/http_server.cpp:13` | moderate | `poll()` absent on Windows — guard the define with `#ifndef _WIN32` so httplib falls back to select() |
| Abstract-namespace unix socket cases | `tests/test_communication.cpp` (+ design note `tests/CMakeLists.txt:225-229` "no network and no device… unguarded on purpose") | moderate (P2) | Switch to path-based AF_UNIX in `%TEMP%` or TCP loopback; mirror the macOS rejection at `socket.cpp:129-130` |
| `if(UNIX)` compiler-identity fixture block (dlopen-based shared-lib fixtures) | `tests/CMakeLists.txt:429-452` | trivial | already skipped on Windows; optionally re-enable later with `LoadLibrary` + `.dll` fixtures |
| `if(APPLE)` loaded-runtime fixture | `tests/CMakeLists.txt:317-328` | trivial | skipped naturally |
| ctest WORKING_DIRECTORY = repo root; fixtures relative | `tests/CMakeLists.txt:13-15` | trivial | portable as-is |
| KNOWN_FAILURES.md gating | `tests/CMakeLists.txt:9-11,597-606` | trivial | mechanism portable; add Windows-specific entries with reasons (test strategist, Stage later) |

### 3.5 Rust FFI (cargo) integration

| Construct | Files:lines | Severity | Recommended fix |
|---|---|---|---|
| Hardcoded Unix archive name `lib${lib_name}.a` | `cmake/LSERust.cmake:97` | blocker | `windows-msvc` produces `fastokens_ffi.lib` — branch on `CMAKE_SYSTEM_NAME`/cargo target |
| Native-libs parse of GNU `-l<name>` flags; filter `^(c|gcc_s|m)$` | `cmake/LSERust.cmake:64-72` | blocker | rustc on msvc prints names like `ws2_32.lib bcrypt.lib advapi32.lib userenv.lib`; parse and pass through (CMake accepts `.lib` names) — or map explicitly |
| Fallback libs `pthread dl` + pkg-config PCRE2 | `cmake/LSERust.cmake:74-87` | moderate | Windows fallback: `ws2_32 bcrypt advapi32 userenv`; PCRE2 via `PCRE2_SYS_STATIC=1` env (exact precedent: iOS branch `LSERust.cmake:45-50`) — no pkg-config on Windows |
| No cargo `--target` for Windows host builds | `cmake/LSERust.cmake:42-50` (iOS-only target args) | moderate | Add `x86_64-pc-windows-msvc` target args + `LSE_CARGO_TARGET_DIR` adjustment (`x86_64-pc-windows-msvc/release`) |
| Configure-time git clone of `reference/fastokens` | `cmake/LSERust.cmake:15-37`; `CMakeLists.txt:300` | trivial | portable (needs git on PATH — CI runners have it) |
| `ring` (rustls dep of ureq 2, `third_party/fastokens-ffi/Cargo.toml:15`) needs NASM+CMake on windows-msvc | — | moderate (risk, not code) | GitHub `windows-latest` ships NASM & CMake; verify early in a standalone `cargo build` smoke |

### 3.6 Not found (verified absent — reduces scope)

`pthread.h` direct use (only `find_package(Threads)`, `CMakeLists.txt:445` — portable), `sys/shm.h`/`shm_open`, `fork()` in src (only tests), `posix_memalign`, `sched_setaffinity`/`pthread_setaffinity`/numactl, hugepages/`MAP_HUGETLB`, `endian.h`/`byteswap.h` (uses `<bit>`/portable code: `src/core/sha256.cpp:3`), `popen`, `realpath`, `getauxval`, x86 SIMD intrinsics (`_mm_*`/`immintrin`), `PATH_MAX`, inline asm in host code.

---

## 4. Build-System Analysis

**Structure.** Root `CMakeLists.txt` (525 lines) + 4 modules (`cmake/LSEOptions.cmake`, `LSECompilerFlags.cmake`, `LSERust.cmake`, `LSEBuildIdentity.cmake`) + 3 nested `src/backends/**/CMakeLists.txt` + `tests/CMakeLists.txt` (607 lines). Targets: `lse_core → lse_ir → lse_backend_registry → {lse_opt, lse_graph, lse_kernels, lse_kv, lse_ops, lse_model, lse_runtime, lse_sampling, lse_communication, lse_dist, lse_probe, lse_trace, lse_place, lse_tokenizer, lse_hub, lse_server, lse_api}` + executables `lse`, `lse-server` (`CMakeLists.txt:466-497`).

**C++ standard.** C++26 required (`CMakeLists.txt:20-22`, `CMAKE_CXX_EXTENSIONS OFF`). Two consequences: (1) `cmake_minimum_required(VERSION 3.24)` at line 1 must rise to ≥3.25 (first version with `cxx_std_26`); recommend 3.30 for mature clang-cl/MSVC support. (2) MSVC/clang-cl receive `/std:c++latest`; the only actual C++26 feature in use (reflection) is feature-macro-guarded (§3.2), and `std::jthread` (C++20, `src/api/lse.cpp:645`, `src/cli/main.cpp:758`) is fully supported by both.

**Compiler flags.** All targets get `lse_apply_common_flags()` (`cmake/LSECompilerFlags.cmake:4-45`): GNU-spelled warnings (5-19), `-Werror` opt-in (21-23), `-freflection` for GCC≥16 only (26-29), ASan via `-fsanitize` (31-34), **`-fno-fast-math`** (36-38, numerics reproducibility policy — the Windows equivalent intent is MSVC's default `/fp:precise`; must NOT be translated to `/fp:fast`), plus `POSITION_INDEPENDENT_CODE`/hidden visibility properties (40-44 — harmless no-ops on PE/COFF).

**APPLE guards' purpose (7).** (1) `CMakeLists.txt:97-101`: kqueue vs epoll poller source selection. (2)-(6) `117-122, 157-162, 273-278, 285-290, 374-379, 427-432`: `-force_load` (Apple ld64) vs `--whole-archive` (GNU ld) for self-registering archives — the `LINKER:` prefix syntax is GNU-ld-specific and **lld-link/MSVC link reject it**; Windows needs `"LINKER:/WHOLEARCHIVE:$<TARGET_FILE:tgt>"`. (7) backend/test-level equivalents: `src/backends/cpu/CMakeLists.txt:12-17`, `src/backends/hrx/CMakeLists.txt:83-88`, `tests/CMakeLists.txt:317`.

**HRX/ROCm wiring.** Discovery in `cmake/LSEOptions.cmake`: HIP headers (49-56), amd_comgr (61-72), hipRTC (75-86), HRX root/lib (93-129), loomc (137-170), HSA runtime soname (103-106). Backend added only when `LSE_ENABLE_HRX AND LSE_HRX_INCLUDE_DIR` (`src/backends/CMakeLists.txt:19-23`). `lse_place` still *dlopens* HSA at runtime regardless (`src/place/devices.cpp:594-677`) — that path must Windows-compile even with HRX off. `LSE_CONFIGURED_ROCM_PATH` is always baked into lse_place (`CMakeLists.txt:423-424`) — harmless string on Windows.

**ctest registration.** `lse_add_test()` (`tests/CMakeLists.txt:4-16`): one executable per test, `add_test` with `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`, `LSE_TEST_BINARY`/`LSE_KNOWN_FAILURES_FILE` defines; env `LSE_BACKEND=cpu;LSE_POOL=cpu` on device-ish tests (e.g. 142, 170-172, 188-189); `LD_LIBRARY_PATH` appended when ROCm found (266-270, no-op on Windows); `if(UNIX)`/`if(APPLE)` fixture blocks skip naturally (429-452, 317-328); Python3-driven `test_server_cli` needs `lse-server` (273-278). KNOWN_FAILURES.md parsed at configure time to disable tests (597-606).

**External projects.** No FetchContent/ExternalProject for C++. The Rust shim is the only external build: `lse_provide_fastokens()` git-clones `crusoecloud/fastokens` @ pinned SHA into `reference/fastokens` at configure time (`cmake/LSERust.cmake:10-37`), then `add_custom_command` runs `cargo build --locked --release` (103-110) and `_lse_rust_native_libs` queries rustc for link deps (55-89). If cargo is absent, `lse_tokenizer`/`lse_hub`/`lse_server`/`lse_api`/`lse`/`lse-server` all vanish (`CMakeLists.txt:304-309, 391-404, 439-497`) — so **Windows Phase 1 "CLI builds" REQUIRES the Rust chain working** (or an explicit reduced milestone without tokenizer targets).

---

## 5. Existing Windows-Readiness Nuggets

- `include/lse/lse.h:26-30` — `_WIN32` branch already exists for `LSE_API` (currently empty macro; adequate for static linking).
- `src/communication/socket.cpp:129-130` — a clean precedent pattern for "capability missing on this platform" errors (abstract sockets on macOS); replicate for Windows.
- `src/backends/hrx/loomc/loomc_compiler.cpp:22-28` and `src/place/devices.cpp:580-584` — existing `__APPLE__`/`__linux__` two-way branches; adding a third `_WIN32` arm is idiomatic for this codebase and upstream-friendly.
- `cmake/LSERust.cmake:45-50` — iOS branch is the template for a Windows cargo-target branch (incl. `PCRE2_SYS_STATIC=1`).
- httplib 0.18.3 (`third_party/vendor/httplib.h:11`) — first-class Winsock support; just needs `ws2_32`/`crypt32` linked and the `CPPHTTPLIB_USE_POLL` defines guarded (`src/server/http_server.cpp:13`, `tests/test_server_shutdown.cpp:2`).

---

## 6. Toolchain Recommendation

**Primary: clang-cl (LLVM 21.x, matching the macOS pin in BUILD_FROM_SOURCE.md) + MSVC STL/UCRT + Ninja + Rust `x86_64-pc-windows-msvc`.**

Rationale tied to findings:

1. **GNU extensions:** clang-cl compiles `__builtin_memcpy` (`include/lse/core/hash.hpp:28`, `dtype.hpp:37,46`, `graph.cpp:187,257`) unchanged; cl.exe does not. clang-cl also accepts the existing `-W...` flag wall (`LSECompilerFlags.cmake:5-19`) with minimal gating (drop/replace only `-Wformat=2` → `-Wformat -Wformat-security`, and `-fno-fast-math` is accepted by clang-cl as-is).
2. **C++26 trajectory:** clang `/std:c++latest` tracks C++26 progressively; P2996 absent → guarded fallbacks engage automatically (`reflect.hpp:5`). MSVC's C++26 confluence is similar, so neither gains an edge here.
3. **ASan parity:** `-fsanitize=address` (`LSECompilerFlags.cmake:31-34`) works on clang-cl with MSVC STL — preserves `LSE_ASAN` option semantics.
4. **Phase-3 compatibility:** the Windows HIP SDK (comgr, hsa bits) is built with MSVC ABI; clang-cl is ABI-compatible; MinGW is not.
5. **Rust:** `windows-msvc` is the first-class tier-1 target for `ring`/`rustls`/`pcre2-sys` (bundled-source static build with `PCRE2_SYS_STATIC=1`), and GitHub `windows-latest` runners ship MSVC Build Tools + NASM + CMake.

**Secondary (fast-follow): pure MSVC `cl.exe`.** Cost: shim the 3+1 `__builtin_memcpy` sites (or upstream `std::bit_cast` — preferred), gate the `-W` flags by `CMAKE_CXX_COMPILER_ID`, use `/fsanitize=address` spelling. Worth doing because some Windows users only have VS installed; recommend keeping the flag function compiler-ID-branched so both work.

**Rejected: MinGW-w64.** It would compile the POSIX layer nearly as-is (it ships `unistd.h`, `sys/mman.h`, `fork`…), but: (a) Rust `windows-gnu` is tier-2 with a worse `ring`/`pcre2` build story; (b) MinGW ABI is incompatible with the MSVC-built Windows HIP SDK, dead-ending Phase 3; (c) not preinstalled on GitHub runners; (d) UCRT/MSVCRT runtime mismatches with httplib/Rust staticlib linking. Not worth the ecosystem friction for a "POSIX shim" saving that the platform layer (§7.1 B1) captures anyway.

---

## 7. Phased Roadmap

### Phase 1 — Core + CPU backend build green on Windows (17 work items)

**Scope:** everything except `src/backends/hrx/**` compiles and links with clang-cl; `lse.exe`/`lse-server.exe` produced (requires Rust chain); HRX explicitly OFF.

**A. Toolchain & CMake foundation (4)**
- **A1.** Bump `cmake_minimum_required` to ≥3.25 (`CMakeLists.txt:1`); document LLVM 21 + VS 2022 + Rust prerequisites in a new Windows section of `BUILD_FROM_SOURCE.md`.
- **A2.** Guard the `g++-16` auto-probe with `if(NOT WIN32)` (`CMakeLists.txt:5-10`).
- **A3.** Compiler-ID-gate `lse_apply_common_flags` (`cmake/LSECompilerFlags.cmake:4-45`): MSVC/Clang-CL branch (keep `-W` set on clang-cl; `/W4` subset on cl; `-fno-fast-math` → no-op/`/fp:precise`; ASan spelling per compiler).
- **A4.** Introduce `lse_force_load(target)` CMake helper replacing all 8 whole-archive sites (`CMakeLists.txt:117-122,157-162,273-278,285-290,374-379,427-432`; `src/backends/cpu/CMakeLists.txt:12-17`; `src/backends/hrx/CMakeLists.txt:83-88`): `WIN32` → `"LINKER:/WHOLEARCHIVE:$<TARGET_FILE:tgt>"`. **Add a startup smoke ctest asserting the CPU backend and kernel primitives actually registered** (silent-failure guard, see R2).

**B. Platform shim layer (1)**
- **B1.** New `include/lse/core/platform.hpp` + `src/core/platform_posix.cpp` / `src/core/platform_win.cpp` (added to `lse_core`, `CMakeLists.txt:40-50`): fd ops (`pread`,`open` flags), file-map + anon-map + unmap + discard, page size, file lock (flock↔LockFileEx), `mkdtemp`, `setenv`, pid, hostname, monotonic/boottime clocks, `dl` shim (open/sym/this-module/close/addr), home-dir resolution. Every later item calls the shim — this concentrates the fork delta and eases weekly rebases (R1).

**C. Module ports via shim (7)**
- **C1.** `src/core/file_read.cpp` (lines 22, 41, 56, 67, 80-82) → shim. *(Root dependency; do first.)*
- **C2.** `src/model/weights.cpp` (mmap 227-243, madvise 206, HOME 452/493-499) + `src/model/dflash2_convert.cpp` (pread 250, open 242/366/374/612, mkdtemp 701, `#if defined(__APPLE__)` branches 278-313 keep as-is).
- **C3.** `src/graph/jit_cache.cpp` (lock 336-349, setenv 1033, cache root 1017-1019 → `%USERPROFILE%`/`%LOCALAPPDATA%` fallback).
- **C4.** `src/trace/record.cpp` (clocks 14-35 → QPC/QueryUnbiasedInterruptTime arm).
- **C5.** `src/communication/`: new `poller_winsock.cpp` + `elseif(WIN32)` in `CMakeLists.txt:97-101`; `socket.cpp` (winsock includes, fcntl→ioctlsocket 160-165, abstract-namespace rejection extending 129-130, closesocket); `stream_link.cpp` (sendmsg→WSASend 209/327/403, `sys/uio.h`); `endpoint.cpp` (winsock getaddrinfo, WSAStartup, nanosleep 273-274); `rdma.cpp` (register transports unavailable: 46-116). Link `ws2_32`.
- **C6.** `src/place/devices.cpp` (dl-shim 568-677, `/proc/self/exe` 620, sonames 580-584 add `_WIN32` arm, `/opt/rocm` roots 656-661 add `%HIP_PATH%` for P3, setenv 633).
- **C7.** `src/api/lse.cpp` (LogCapture `_pipe`/`_dup2` 146-159, footprint/mallinfo2 187-212, setenv 510, HOME 508) + `src/hub/*` (flock `pull.cpp:330-348`, lseek/ftruncate/write 314/372/397/451/470, setenv `cache.cpp:209`, unistd `cli.cpp`/`companions.cpp`) + `src/probe/{profile_store.cpp:157,178, link_probe.cpp:62}` + `src/cli/server_main.cpp:498-499` (SetConsoleCtrlHandler) + guard `CPPHTTPLIB_USE_POLL` (`src/server/http_server.cpp:13`).

**D. Rust FFI (2)**
- **D1.** `cmake/LSERust.cmake`: windows-msvc branch — archive name (`:97`), cargo `--target x86_64-pc-windows-msvc` + target-dir (`:42-50`), native-lib parsing for MSVC-style output (`:64-72`), Windows fallback libs (`:75`), `PCRE2_SYS_STATIC=1` env (precedent `:45-50`).
- **D2.** Standalone pre-flight (no CMake): `cargo build --locked --release --target x86_64-pc-windows-msvc` for `third_party/fastokens-ffi` with `reference/fastokens` cloned — proves ring/NASM/pcre2 story before wiring (de-risks R4).

**E. Link hygiene (2)**
- **E1.** Link `ws2_32` (+`crypt32` if httplib TLS paths compile in) to `lse_communication` and `lse_server` (`CMakeLists.txt:102-123, 442-446`).
- **E2.** `__builtin_memcpy` shim or `std::bit_cast` conversion for `include/lse/core/hash.hpp:28`, `dtype.hpp:37,46`, `src/graph/graph.cpp:187,257` (not needed for clang-cl; required for the cl.exe fast-follow — land now to make both toolchains green).

**F. Explicit HRX gating (1)**
- **F1.** `cmake/LSEOptions.cmake:6`: default `LSE_ENABLE_HRX` to OFF on WIN32 (with status message "HRX backend: not supported on Windows in Phase 1"); verify configure prints the existing skip line (`src/backends/CMakeLists.txt:22`) and that `lse_place`/tests referencing `LSE_HRX_LIBRARY` degrade (they already guard: `tests/CMakeLists.txt:45-49,216-223,234-236,282-293,360-369,378-398,413-423,455-457,469-471,476-477,491-497`).

**Acceptance criteria (P1):**
1. `cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DLSE_ENABLE_HRX=OFF -DCMAKE_BUILD_TYPE=Release` configures with no errors; summary block (`CMakeLists.txt:510-524`) prints `HRX backend: OFF`.
2. `cmake --build build` produces `lse.exe`, `lse-server.exe`, all `lse_*` libs, `fastokens_ffi.lib` linked.
3. Registration smoke test passes: CPU backend enumerable by name; kernel-primitive census > 0 (proves `/WHOLEARCHIVE` worked).
4. `lse.exe --version` and a `--list-backends`-style query run without missing-DLL errors.
5. Zero modification to `src/backends/hrx/**` behavior on Linux/macOS (CI parity check).

**Risks (P1):** R1 churn, R2 silent registration loss, R4 Rust chain, plus: winsock include-order explosions in TUs that include `<windows.h>` transitively (mitigate: winsock2.h first in platform_win.cpp, `WIN32_LEAN_AND_MEAN`); UCRT `O_NOFOLLOW` absence (shim); MSVC STL rejecting `-Wconversion`-clean code paths differently (warnings only, `LSE_WERROR` stays OFF).

### Phase 2 — Test suite green + Windows CI (~8 work items)

- **T1.** Port the 19 `unistd.h` test files (§3.1 row 2) — mostly a harness-level shim (`tests/harness.hpp`).
- **T2.** Rewrite `tests/test_server_shutdown.cpp:24-37` fork-case (CreateProcess or platform-split).
- **T3.** `test_communication.cpp`: replace abstract-socket cases with path-socket/TCP (R6); confirm httplib poll guard (item C7) covers `test_server_shutdown.cpp:2`.
- **T4.** Audit env-var-driven tests (`LSE_BACKEND=cpu;LSE_POOL=cpu`, `tests/CMakeLists.txt:142,170-189,498-499,522-523,540-541,562-563,587-592`) — `set_tests_properties(ENVIRONMENT ...)` is portable; only verify values.
- **T5.** KNOWN_FAILURES.md: add Windows entries with reasons as discovered (mechanism at `tests/CMakeLists.txt:597-606` already supports per-binary case skips via `LSE_TEST_BINARY`).
- **T6.** New `.github/workflows/windows.yml`: `windows-latest`; install LLVM 21 (winget), Rust stable-msvc, Ninja; configure clang-cl + `-DLSE_ENABLE_HRX=OFF`; build; `ctest --output-on-failure`; cache cargo/git deps (`reference/fastokens`). Model on `ci.yml` structure minus GPU steps (which are self-hosted-only, `ci.yml:25-28`).
- **T7.** `test_server_cli` Python harness (`tests/CMakeLists.txt:273-278`): verify `test_server_cli.py` has no POSIX-only assumptions (subprocess semantics, signals) — port if needed.
- **T8.** Local WSL/Linux parity job in the same PR flow to prove no regression to the primary platform (guards R1/R3).

**Acceptance (P2):** on `windows-latest`: 100% of registered ctest entries pass or are listed in KNOWN_FAILURES.md with a Windows reason; Linux/macOS suites unchanged; workflow green 3 consecutive runs.

### Phase 3 — Stretch: HRX backend via Windows HIP SDK (spike, ~5 items)

**Reality check (verified from code):** the HRX path needs (a) `amd_comgr` — **available** in the Windows HIP SDK; (b) `libhrx`/`hrx_runtime.h` (hrx-system, `cmake/LSEOptions.cmake:93-129`) — **no Windows port exists**; (c) an HSA runtime exporting `hsa_amd_queue_create` (ROCr ≥1.21, `LSEOptions.cmake:95-97`; `devices.cpp:568-569`) — **ROCm's HSA runtime does not ship for Windows** (Windows HIP runs over the WDDM driver stack). Therefore Phase 3 is scoped as **compile-only**:
- **S1.** Discover Windows HIP SDK layout in `LSEOptions.cmake` (`%HIP_PATH%`, `C:\Program Files\AMD\ROCm\*`); `find_library(amd_comgr)` → `amd_comgr.dll`/`.lib`.
- **S2.** Port HRX-hosted-but-device-independent TUs used by tests (`hipc/hip_emitter.cpp`, `loomc/loom_*` — note tests already compile many of these *without* the backend, e.g. `tests/CMakeLists.txt:27-32,50-53`, proving they are backend-link-independent; `__builtin_bit_cast` in emitted strings is comgr-side).
- **S3.** `comgr_compiler.cpp` dl-shim (`:6,325` dladdr→GetModuleHandleEx) + Windows clang-resource-dir discovery (`src/backends/hrx/CMakeLists.txt:41-48` globs Unix layouts).
- **S4.** `hrx_backend.cpp` loader paths (`:257-288` sonames, `:1342` sysconf) behind `_WIN32` arms; registration reports "compile-only, no dispatch" when HSA is absent.
- **S5.** Capture-target tests (pattern at `tests/CMakeLists.txt:580-593`, `LSE_CAPTURE_TARGET=gfx1151`) run emission+comgr compile without a device — the natural Phase-3 acceptance vehicle.

**Acceptance (P3, spike):** one emitted kernel compiles to an AMDGPU code object via Windows comgr in a ctest; **dispatch explicitly out of scope** unless hrx-system upstream lands a Windows port.

---

## 8. Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Upstream churn** (weekly; 654 commits just landed) vs long-lived fork branch — conflicts concentrate in hot files (`graph.cpp`, `jit_cache.cpp`, `hrx_backend.cpp`, CMakeLists) | High | High | Concentrate deltas in the platform shim (B1) so call-site edits are one-liners; keep all CMake edits additive behind `WIN32`/compiler-ID branches; rebase weekly with Linux parity run (T8); upstream guards as small PRs — the codebase already idiomatically branches `__APPLE__`/`__linux__` (`devices.cpp:580-584`, `loomc_compiler.cpp:22-28`) |
| R2 | **Silent whole-archive failure**: `/WHOLEARCHIVE` misapplied → link succeeds, self-registrars stripped, registry empty; every test fails with confusing "no backend" errors (design note: `CMakeLists.txt:116,271-272,284`; `src/backends/cpu/CMakeLists.txt:10-11`) | High if missed | Critical | A4 smoke ctest asserting registration counts runs before anything else; code-review checklist item per linked archive |
| R3 | **C++26 feature drift**: upstream adopts unguarded C++26 (CI says sources "reflect over kernel argument structs", `ci.yml:80-84`) that clang-cl/MSVC lack | Medium | High | Scheduled (not dispatch-only) windows CI from P2 so breakage surfaces within a week; contribute feature-test-macro guards upstream (pattern exists: `reflect.hpp:5`) |
| R4 | **Rust chain on windows-msvc**: `ring` needs NASM+CMake; `pcre2-sys` needs static build (`PCRE2_SYS_STATIC=1`, precedent `LSERust.cmake:45-50`); configure-time clone needs git+network (`LSERust.cmake:15-37`). Failure silently drops tokenizer/server/CLI/hub and ~15 tests (`CMakeLists.txt:304-309,439-497`) | Medium | High | D2 standalone cargo pre-flight before CMake work; CI cargo cache; fail loudly (existing `FATAL_ERROR` paths) |
| R5 | **amd_comgr/HRX on Windows** (P3): comgr exists in HIP SDK, but hrx-system and ROCr (`hsa_amd_queue_create`, `LSEOptions.cmake:95-97`) have no Windows port | Certain (for dispatch) | Medium | Scope P3 as compile-only spike (S1-S5); capture-target tests as acceptance; dispatch gated on upstream hrx-system Windows work |
| R6 | **AF_UNIX abstract namespace absent** on Windows (`socket.cpp:122-149`); `test_communication` deliberately unguarded (`tests/CMakeLists.txt:225-229`) | Certain | Medium | Extend macOS rejection pattern (`socket.cpp:129-130`); migrate tests to path sockets/loopback TCP (T3) |
| R7 | **Numerics parity**: `-fno-fast-math` policy (`LSECompilerFlags.cmake:36-38`) must survive translation; bit-exact tests (`test_fp8_conversion*`, `test_quant*`) will catch any reassociation | Low | Medium | `/fp:precise` is MSVC/clang-cl default; never map to `/fp:fast`; keep the explicit flag on clang-cl |
| R8 | **Locking semantics**: flock (advisory, whole-file) → LockFileEx (mandatory, byte-range) at `jit_cache.cpp:345-349`, `hub/pull.cpp:330-348` (hf-hub lock-file compat) | Medium | Low-Med | LockFileEx is strictly stronger; document in shim; keep lock files at same paths for cross-tool compat |
| R9 | **Long paths**: HF cache trees exceed MAX_PATH; `HOME`-string concatenation (`jit_cache.cpp:1018-1019`) | Medium | Low | Use `std::filesystem` (already dominant), enable runner long-path policy, prefer `\\?\` in shim home resolution |
| R10 | **No AMD GPU on CI runners** | Certain | Low (by design) | All device tests already guard on `TARGET lse_backend_hrx` / `LSE_ROCM_RUNTIME_DIR` (`tests/CMakeLists.txt:216-223,266-270`) — they skip exactly as on GPU-less Linux |

---

## 9. Open Questions for Later Stages

- **Q1 (PM/implementer):** Is a reduced Phase-1 milestone acceptable if D2 (cargo) slips — i.e., "libs + smoke test without `lse.exe`/`lse-server.exe`" — given tokenizer targets vanish without fastokens_ffi (`CMakeLists.txt:304-309`)? Recommend: yes, define P1a (libs) / P1b (executables).
- **Q2 (implementer):** Shim granularity — single `platform.hpp` (recommended, B1) vs per-module `#ifdef _WIN32`. Affects upstream PR size and rebase cost (R1).
- **Q3 (test strategist):** Which `test_communication` cases move to TCP loopback vs path-based AF_UNIX vs Windows-skip (R6)? And the Windows KNOWN_FAILURES policy: reason string format, review cadence.
- **Q4 (tech writer):** `BUILD_FROM_SOURCE.md` has macOS/Linux columns only — add a Windows column + a "no GPU required" note mirroring the README claim; document `%LOCALAPPDATA%` cache locations replacing `~/.lse`/`~/.cache`.
- **Q5 (implementer):** `LSE_API` is an empty macro on Windows (`include/lse/lse.h:26-27`) — fine for static linking; is a DLL build (`__declspec(dllexport/import)`) ever wanted for Swift-equivalent (C#?) consumers? Defer unless asked.
- **Q6 (PM):** Upstream engagement — do we PR the guards to Geramy/LSE (reduces R1 forever) or keep them fork-only? The `if(APPLE)` precedent suggests upstream would accept `elseif(WIN32)` branches.
- **Q7 (quality):** Should `-Wconversion`-heavy warning parity be enforced on clang-cl from day 1 (noise risk) or land permissive and tighten later?
- **Q8 (implementer, P3):** Confirm Windows HIP SDK actually ships `amd_comgr` import lib + headers in the version we target (SDK layout check is S1's first task; assumption from user brief, not yet verified against an installed SDK).

---

*End of Stage 1 artifact. All file:line references verified against working tree at `a5b82f6` on `feature/windows-enablement`, 2026-10-10.*
