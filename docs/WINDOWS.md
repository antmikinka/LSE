# Building LSE on Windows (experimental — Phase 1 / milestone M2a)

Native Windows x64 build of the Lemon Seed Engine with **clang-cl + MSVC
STL/UCRT + Ninja + Rust (MSVC toolchain)**. Status: Phase 1 / M2a is complete
and verified — the engine, the CLI and the server **build, link and pass the
34-binary targeted test gate (34/34)**. The CPU reference backend is the only
backend: **the HRX/HIP GPU backend is gated OFF on Windows** (no ROCm runtime
port exists yet).

This guide is self-contained. Program history, plans, reviews and gate evidence
live in [`windows-enablement/`](../windows-enablement/) (see
[Roadmap](#roadmap-and-further-reading) at the end). `README.md` and
`BUILD_FROM_SOURCE.md` are deliberately not modified on this branch; where the
older `BUILD_FROM_SOURCE.md` §Windows snippet disagrees with this file, **this
file is correct** (that snippet predates the test-porting constraint and its
plain `cmake --build build` fails with tests ON — see [Testing](#testing)).

Verified on: Windows x64, branch `feature/windows-enablement`
(fork `antmikinka/LSE`, base upstream `Geramy/LSE` v0.6.0-28), 2026-10-10.

---

## 1. Prerequisites

Exact versions verified for this branch (newer patch versions are expected to
work; the LLVM major and the MSVC STL/UCRT presence are what matter):

| Component | Verified version | Where to get it |
|---|---|---|
| clang-cl / lld-link | LLVM **21** (verification host: 21.0.0git from AMD ROCm 7.1; any LLVM 21.x works) | [llvm.org releases](https://github.com/llvm/llvm-project/releases) or `winget install LLVM.LLVM` |
| MSVC STL + UCRT + Windows SDK | MSVC **14.44.35207**, Windows SDK **10.0.26100.0**, VS 2022 **17.14** | [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) or Build Tools ("Desktop development with C++") |
| CMake | **3.31.9** (≥ 3.30 required — `CMakeLists.txt:1`) | [cmake.org](https://cmake.org/download/) or `winget install Kitware.CMake` |
| Ninja | **1.13.0** | [ninja-build.org](https://github.com/ninja-build/ninja/releases) or `winget install Ninja-build.Ninja` |
| Rust (MSVC ABI) | rustc/cargo **1.93.1**, target `x86_64-pc-windows-msvc` | [rustup.rs](https://rustup.rs/), then `rustup target add x86_64-pc-windows-msvc` |
| Git | any recent | needed at configure time: `cmake/LSERust.cmake` clones the pinned `fastokens` tokenizer into `reference/fastokens` |
| Python 3 | optional | only for helper scripts (`scripts/*.py`), not for the build |

Verify your toolchain (Git Bash or PowerShell):

```bash
clang-cl --version    # expect: clang version 21.x, Target: x86_64-pc-windows-msvc
lld-link --version    # expect: LLD 21.x
cmake --version       # expect: >= 3.30
ninja --version
rustc --version && cargo --version
rustup target list --installed | grep x86_64-pc-windows-msvc
```

Why clang-cl and not cl.exe: the codebase uses `__builtin_memcpy` in public
headers and a GNU-spelled warning wall; clang-cl compiles both unchanged. The
C++26 dialect override in `CMakeLists.txt:18-31` keys on compiler ID `Clang`.
A cl.exe path is planned but not landed (see `windows-enablement/01-planning-analysis.md` §5).

## 2. Enter the MSVC environment (required)

clang-cl does not find the MSVC STL/UCRT headers and libs by itself; `INCLUDE`
and `LIB` must be set. **Every configure/build/ctest shell below assumes this
has been done.** Pick one:

**cmd.exe / x64 Native Tools Command Prompt** (Start menu), or manually:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

**Developer PowerShell for VS 2022** (Start menu), or manually:

```powershell
Import-Module "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\2022\Community" `
  -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -no_logo"
```

**Git Bash**: launch `bash` from an x64 Native Tools prompt (it inherits the
environment), or:

```bash
cmd //c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && bash'
```

Build Tools installs: substitute `BuildTools` for `Community` in the paths.

## 3. Configure and build (full build, tests OFF)

The **only supported full-build configuration on Windows right now is
`-DLSE_BUILD_TESTS=OFF`** (why: [Testing](#testing)). From the repo root:

```bash
cmake -S . -B build-win-notests -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=OFF

cmake --build build-win-notests
```

Identical in PowerShell (line continuations are backticks there):

```powershell
cmake -S . -B build-win-notests -G Ninja `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DLSE_BUILD_TESTS=OFF
cmake --build build-win-notests
```

Notes:

* `-DCMAKE_CXX_COMPILER=clang-cl` relies on clang-cl being on `PATH`; pass the
  full path if it is not (verification host used
  `"C:/Program Files/AMD/ROCm/7.1/bin/clang-cl.exe"`). CMake finds the
  `lld-link` sitting next to clang-cl automatically (cache: `CMAKE_LINKER`).
* Do **not** pass `-DCMAKE_C_COMPILER`; the project is `LANGUAGES CXX` only and
  CMake will warn "Manually-specified variables were not used".
* `LSE_ENABLE_HRX` is **forced OFF on Windows automatically**
  (`cmake/LSEOptions.cmake:16-22`); you do not need to pass
  `-DLSE_ENABLE_HRX=OFF`. The cache may still show `ON` — the summary block is
  authoritative.
* First configure clones `reference/fastokens` at the pinned SHA
  (`cmake/LSERust.cmake:10-13`) and builds `third_party/fastokens-ffi` with
  cargo `--target x86_64-pc-windows-msvc`.

Expected configure summary (full transcript:
[`logs/m2a-configure.txt`](../logs/m2a-configure.txt), regenerated post-SYNC-1
at v0.6.1; the original from-scratch log was gitignored `*.log` and pre-merge):

```
-- HRX backend: not supported on Windows in Phase 1 (forced OFF)
-- fastokens_ffi native deps: legacy_stdio_definitions.lib;advapi32.lib;...
-- Lemon Seed Engine 0.6.1
--   build type      : RelWithDebInfo
--   host compiler   : Clang 21.0.0
--   host std        : C++26
--   device std      : C++20
--   HRX backend     : OFF
--   HIP available   : OFF
--   comgr (JIT)     : OFF
--   loomc (JIT)     : OFF
--   tests           : OFF
```

Benign configure output you will see (not errors):

* `Looking for pthread_create in pthread - not found` — FindThreads probing;
  it ends with `Found Threads: TRUE`.
* `windows.0.53.0.lib` / `windows.0.52.0.lib` may appear in cargo's raw
  `native-static-libs` output — raw-dylib stubs that `LSERust.cmake:80-85`
  filters out before linking (the summary line above shows the filtered set).

Expected build result:

```
[192/192] Linking CXX executable lse-server.exe
```

0 errors; warnings exist (upstream sign-conversion/shadow noise kept per
minimal-diff governance — inventory in [`logs/m2a-build.txt`](../logs/m2a-build.txt)).
Artifacts: `build-win-notests/lse.exe` and `build-win-notests/lse-server.exe`.
A rerun prints `ninja: no work to do`.

## 4. Testing

**Current state (M2a):** 15 test source files still `#include <unistd.h>`
(getpid/setenv/fork), which the UCRT does not provide. They are *not* ported
yet (Phase 2 / WP12). Therefore:

* a tests-ON build of **`all` will fail** — do not run
  `cmake --build build-win` without `--target` before WP12;
* the verified gate is a **targeted build** of the 34 binaries in
  [`windows-enablement/COMPILE_EXPOSURE.txt`](../windows-enablement/COMPILE_EXPOSURE.txt)
  (36 loomc/hipc-exposure tests, generated by `gen_compile_exposure.sh`) minus
  the 3 ledgered exclusions, plus `test_registration_smoke`;
* exclusions (**X = 3**, ledger:
  [`windows-enablement/COMPILE_EXCLUSIONS.md`](../windows-enablement/COMPILE_EXCLUSIONS.md)):
  `test_dflash2` and `test_loom_cache` — each test's own `.cpp` includes
  `<unistd.h>` — and `test_launch_template` (upstream v0.6.1; calls `setenv()`
  without including `<unistd.h>`); all three are WP12 ports via
  `tests/compat.hpp`. They are excluded at configure time (`if(NOT WIN32)` in
  `tests/CMakeLists.txt`), so they are not registered with ctest on Windows
  at all.

### Configure the test tree

```bash
cmake -S . -B build-win -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=ON
```

### Build the 34 targeted binaries and run ctest — Git Bash

```bash
# ledger-derived version (canonical, with cardinality assertions):
# windows-enablement/logs/r3-gate.sh
EXCL=$(awk -F'|' '/^\| `test_/ {gsub(/[` ]/,"",$2); print $2}' windows-enablement/COMPILE_EXCLUSIONS.md)
RE=$( (grep -vxF "$EXCL" windows-enablement/COMPILE_EXPOSURE.txt; \
       echo test_registration_smoke) | sort | paste -sd'|' -)

cmake --build build-win --target $(echo "$RE" | tr '|' ' ')

ctest --test-dir build-win -R "^($RE)$" --output-on-failure
```

### Same — PowerShell

```powershell
$names = @(Get-Content windows-enablement\COMPILE_EXPOSURE.txt |
    Where-Object { $_ -notin @('test_dflash2','test_loom_cache') })
$names += 'test_registration_smoke'

cmake --build build-win --target $names

ctest --test-dir build-win -R ('^(' + ($names -join '|') + ')$') --output-on-failure
```

Expected result (verified 2026-10-10, `logs/m2a-build.txt` PART 3):

```
100% tests passed, 0 tests failed out of 34
Total Test time (real) =  13.37 sec
```

The 34 include the full loom dialect suite (`test_loom_*`), the HIP-emitter
exposure tests, quantization/kernel tests (`test_q4_*`, `test_q8_*`,
`test_quant_*`), `test_runtime`, `test_perplexity`, `test_kv_cache` and
`test_registration_smoke` (the self-registration/force-load check — the
`/WHOLEARCHIVE` mechanism from `cmake/LSELink.cmake`).

Plain `ctest --test-dir build-win` without `-R` fails with "Unable to find
executable" for the ~57 registered-but-unbuilt tests. That is expected pre-WP12.

## 5. Running

Verified on Windows at M2a (exit code 0):

```bash
build-win-notests/lse.exe --help          # full CLI surface, same as Linux
build-win-notests/lse.exe models list     # HF cache scan (%USERPROFILE%\.cache\huggingface\hub)
build-win-notests/lse-server.exe --help
```

Typical usage (same syntax as Linux; see `README.md`):

```bash
lse.exe pull mlx-community/Qwen3.5-0.8B-4bit     # download into the HF cache
lse.exe -m mlx-community/Qwen3.5-0.8B-4bit "Hello"
lse-server.exe -m mlx-community/Qwen3.5-0.8B-4bit --host 127.0.0.1 --port 8080
```

Honest scope note: M2a verified the executables **build, link and run their
help/models paths**. End-to-end generation on the CPU backend is the next
milestone (M2b, Phase 2) and is not yet part of the verified gate. Inference
runs on the **CPU reference backend only** — there is no GPU acceleration on
Windows yet.

## 6. Current limitations

Each of these is deliberate and documented in code; the full deviations list is
`windows-enablement/06-implementation-log.md` §4.

| Area | Limitation | Reference |
|---|---|---|
| GPU | HRX/HIP/amd_comgr/loomc backends **OFF**; CPU backend only; no kernel JIT | `cmake/LSEOptions.cmake:16-22` |
| Poller | WSAPoll has **millisecond** timeout granularity (epoll: nanoseconds); timeouts are rounded up. `Poller::fd()` returns the wake socket (WSAPoll has no waitable fd) | `src/communication/poller_winsock.cpp` header |
| Heap stats | `heap_stats()` returns **zeros** (no `mallinfo2` in the UCRT); `malloc_trim` is a no-op. API JSON keys are unchanged | `src/api/lse.cpp`, 06-log §4.4 |
| >2 GB checkpoint I/O | Implemented per directive ID-1: positioned reads via `ReadFile` + `OVERLAPPED` with 64-bit offsets (never the 32-bit `_pread`), memory mapping via `_wsopen_s(_SH_DENYNO)` + `CreateFileMappingW` + `MapViewOfFile` with a process-wide handle registry | `src/core/platform_win.cpp` |
| Clocks | Biased `QueryInterruptTime` is resolved via `GetProcAddress` at runtime (SDKs declare it but `kernel32.lib` ships only the `Unbiased` thunk), with unbiased fallback | `src/core/platform_win.cpp`, 06-log §4.6 |
| Sockets | Abstract UNIX-socket namespace returns `kUnimplemented` (Linux-only, same as the existing macOS guard). httplib keeps its `select` default; `FD_SETSIZE`=64 counts sockets-in-set | 06-log §5.18-20 |
| Tests | 34-binary targeted gate, not the full suite; 2 ledgered exclusions (X=2); 15 `<unistd.h>` test files await WP12 | §4 above |
| Numerics | `-fno-fast-math` + `-ffp-contract=off` pinned via `/clang:-` spelling (`cmake/LSECompilerFlags.cmake:74-75`); cross-platform hash-equality fingerprinting (T3) is an M3 deliverable, not yet run | `windows-enablement/04-test-strategy.md` §T3 |

## 7. Troubleshooting

**Configure: "The CXX compiler ... is not able to compile a simple test program"
or `fatal error: 'stdio.h' file not found`**
The MSVC environment is not loaded. Run `vcvars64.bat` / Developer shell
(§2) in *this* shell, then delete the build dir and reconfigure.

**`clang-cl: command not found` / CMake picks cl.exe or g++**
clang-cl is not on `PATH` — install LLVM 21 or pass the absolute path:
`-DCMAKE_CXX_COMPILER="C:/path/to/clang-cl.exe"`. If CMake reports compiler ID
`MSVC`, you configured with cl.exe: the C++26 override won't apply; delete the
build dir and reconfigure with clang-cl.

**Error mentioning `CXX26 ... not supported`**
The dialect override (`CMakeLists.txt:18-31`) only fires for
`WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"`. You configured a non-Clang
compiler or a stale tree — wipe the build dir, set `-DCMAKE_CXX_COMPILER=clang-cl`.

**`CMake Error: CMake was unable to find a generator` / "Ninja: no output
specified" / it tries MSBuild or nmake**
`-G Ninja` is missing or `ninja` is not on `PATH`. Verify with
`ninja --version`; generator cannot be changed in an existing tree — delete it.

**cargo: `target 'x86_64-pc-windows-msvc' may not be installed`**
`rustup target add x86_64-pc-windows-msvc` (rustup from rustup.rs, MSVC ABI —
the GNU ABI toolchain produces `.a`, not the `.lib` `LSERust.cmake:126` expects).

**Configure fails cloning fastokens**
`cmake/LSERust.cmake` clones `https://github.com/crusoecloud/fastokens.git` into
`reference/fastokens` at a pinned SHA. Behind a firewall: clone it by hand at
that SHA, or point `-DLSE_FASTOKENS_REPO=` at a mirror.

**Warning "could not query rustc for native libs"**
Not fatal: the build falls back to `ws2_32.lib bcrypt.lib userenv.lib`
(`LSERust.cmake:97-100`). If the final link then misses a crate dependency,
report it with the configure log.

**ctest: "Unable to find executable" for many tests**
You ran plain `ctest` in a tree where only the 34 targeted binaries were built.
Use the `-R` invocation in §4 (expected until WP12 ports the remaining tests).

**Build fails in a test source with `'unistd.h': No such file or directory`**
You built `all` with `-DLSE_BUILD_TESTS=ON`. Use the targeted build (§4) or a
tests-OFF tree (§3). The UCRT has no `<unistd.h>`; WP12 ports these files.

**Everything behaved, then the branch/toolchain changed**
Build trees cache the compiler. After switching branches or LLVM versions,
`rm -rf build-win-notests build-win` and reconfigure.

## 8. Roadmap and further reading

Phase 2 (planned, in order): **WP11/M2b** — executables + tokenizer sentinel
(end-to-end runs); **WP12** — port the 15 `<unistd.h>` test files via
`tests/compat.hpp`, driving X → 0 and unlocking the full tests-ON `all` build;
**WP13** — CI (`.github/workflows/windows.yml` + Linux-parity job); **M3/M4** —
full-suite green, numerics fingerprint, 3× release protocol.

| Artifact | What it is |
|---|---|
| [`windows-enablement/07-documentation.md`](../windows-enablement/07-documentation.md) | Documentation-package index + maintenance map |
| [`windows-enablement/06-implementation-log.md`](../windows-enablement/06-implementation-log.md) | What was actually done: environment, work packages, deviations, discrepancy log |
| [`windows-enablement/05-quality-review-2.md`](../windows-enablement/05-quality-review-2.md) | Directive layer ID-1…ID-14 (normative fix specs) |
| [`windows-enablement/04-test-strategy.md`](../windows-enablement/04-test-strategy.md) | Test/gate strategy, numerics policy |
| [`windows-enablement/02-program-plan.md`](../windows-enablement/02-program-plan.md) | WP0–WP15 program plan, milestones, risks |
| [`windows-enablement/01-planning-analysis.md`](../windows-enablement/01-planning-analysis.md) | Toolchain choice analysis |
| [`windows-enablement/COMPILE_EXCLUSIONS.md`](../windows-enablement/COMPILE_EXCLUSIONS.md) | Living exclusions ledger (X) |
| [`logs/m2a-build.txt`](../logs/m2a-build.txt) | M2a gate evidence: full build log + ctest transcript |
