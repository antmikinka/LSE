# Building LSE on Windows (experimental — cycle 2 complete / milestone M3)

Native Windows x64 build of the Lemon Seed Engine with **clang-cl + MSVC
STL/UCRT + Ninja + Rust (MSVC toolchain)**. Status: **M3 achieved
(2026-10-11)** — the engine, the CLI, the server **and the full test tree
build with `-DLSE_BUILD_TESTS=ON`** (X = 0 compile exclusions), all 98
registered ctest entries run natively (K = 2 ledgered capability quarantines),
the local M3 full-suite protocol is green (main run 97/97 with LARGE_IO
excluded, >2 GiB fixture 1/1, fingerprint pair 2/2), and CI run 5 of
`.github/workflows/windows.yml` is green on **both** jobs (`windows` +
`linux-parity`) at the same SHA (`5b16e94`). A supplementary large-io hosted
proof was dispatched at the acceptance SHA; result pending. The CPU reference
backend is the only backend: **the HRX/HIP GPU backend is gated OFF on
Windows** (no ROCm runtime port exists yet).

This guide is self-contained. Program history, plans, reviews and gate evidence
live in [`windows-enablement/`](../windows-enablement/) (see
[Roadmap](#roadmap-and-further-reading) at the end). `README.md` is
deliberately not modified on this branch. The `BUILD_FROM_SOURCE.md` §Windows
snippet builds the binaries-only configuration and is correct for that, but
its "`-DLSE_BUILD_TESTS=OFF` is required for now" note is **stale** — the
tests-ON full build is the normal path since cycle 2 (§3–§4 below). Where
either disagrees with this file, **this file is correct**.

Verified on: Windows x64 (10.0.26100), branch `feature/windows-enablement`
(fork `antmikinka/LSE`, base upstream `Geramy/LSE` **v0.6.1** `90d66ab`,
merged at SYNC-1), 2026-10-10/11; CI acceptance at `5b16e94` (windows.yml
run 38137423880, both jobs green).

---

## 1. Prerequisites

Exact versions verified for this branch (newer patch versions are expected to
work; the LLVM major and the MSVC STL/UCRT presence are what matter):

| Component | Verified version | Where to get it |
|---|---|---|
| clang-cl / lld-link | LLVM **21.1.8** — the pinned version (CI installs `winget install --id LLVM.LLVM --version 21.1.8 --exact`; A-4/ID-C2-14). Verification host originally ran AMD ROCm 7.1's clang-cl 21.0.0git, which was **retired after a constexpr codegen bug**; use official LLVM builds. Same LLVM major (21) as the CI `linux-parity` job's clang++-21 — that pair is the T3 numerics control. | [llvm.org releases](https://github.com/llvm/llvm-project/releases) or `winget install LLVM.LLVM --version 21.1.8 --exact` |
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

## 3. Configure and build

The **normal path is the full tests-ON build** — the same configuration CI
runs (§7). Every test source compiles (X = 0), so plain `all` links the
engine, the CLI, the server and the whole test tree. From the repo root, in an
MSVC shell (§2):

```bash
cmake -S . -B build-win -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=ON

cmake --build build-win        # no --target: the full 'all' build must exit 0
```

Binaries-only variant (engine + CLI + server, no tests — faster; this is what
`BUILD_FROM_SOURCE.md` §Windows documents):

```bash
cmake -S . -B build-win-notests -G Ninja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLSE_BUILD_TESTS=OFF

cmake --build build-win-notests
```

`LSE_BUILD_TESTS` defaults to **ON** (`cmake/LSEOptions.cmake:3`); it is
spelled explicitly above so the two trees can coexist. Identical in PowerShell
(line continuations are backticks there).

Notes:

* `-DCMAKE_CXX_COMPILER=clang-cl` relies on clang-cl being on `PATH`; pass the
  full path if it is not — for the pinned official LLVM (§1),
  `"C:/Program Files/LLVM/bin/clang-cl.exe"`. CMake finds the
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

Expected configure summary, tests-ON (full transcript:
[`windows-enablement/logs/c2-sync-configure-tests.txt`](../windows-enablement/logs/c2-sync-configure-tests.txt);
the tests-OFF transcript is
[`logs/m2a-configure.txt`](../logs/m2a-configure.txt)):

```
-- HRX backend: not supported on Windows in Phase 1 (forced OFF)
-- fastokens_ffi native deps: legacy_stdio_definitions.lib;advapi32.lib;cfgmgr32.lib;kernel32.lib;user32.lib;ws2_32.lib;bcrypt.lib;advapi32.lib;kernel32.lib;ntdll.lib;userenv.lib;ws2_32.lib;dbghelp.lib
-- Lemon Seed Engine 0.6.1
--   build type      : RelWithDebInfo
--   host compiler   : Clang 21.0.0
--   host std        : C++26
--   device std      : C++20
--   HRX backend     : OFF
--   HIP available   : OFF
--   comgr (JIT)     : OFF
--   loomc (JIT)     : OFF
--   tests           : ON
```

(The `host compiler` line shows whichever clang-cl configured the tree — the
transcript predates the 21.1.8 pin; CI configures with 21.1.8. The
`native deps` line must contain `ntdll.lib` — see
[Troubleshooting](#8-troubleshooting) "could not query rustc".)

Benign configure output you will see (not errors):

* `Looking for pthread_create in pthread - not found` — FindThreads probing;
  it ends with `Found Threads: TRUE`.
* `windows.0.53.0.lib` / `windows.0.52.0.lib` may appear in cargo's raw
  `native-static-libs` output — raw-dylib stubs that `LSERust.cmake:80-85`
  filters out before linking (the summary line above shows the filtered set).

Expected build result: exit 0, 0 errors; the final edges link `lse.exe` and
`lse-server.exe` (tests-ON: plus the test binaries under `build-win/tests/`).
The `[N/N]` edge count drifts with the upstream target set (192 in the M2a
tests-OFF capture) — assert exit 0, not a count. Warnings exist (upstream
sign-conversion/shadow noise kept per minimal-diff governance — inventory in
[`logs/m2a-build.txt`](../logs/m2a-build.txt)). Artifacts:
`build-win/lse.exe`, `build-win/lse-server.exe` (and the `build-win-notests/`
pair from the binaries-only tree). A rerun prints `ninja: no work to do`.

## 4. Testing

**Current state (cycle 2 complete, M3):** every test source compiles under
clang-cl. The UCRT-missing POSIX spellings (`<unistd.h>`, `<sys/wait.h>`,
`<dlfcn.h>`, `setenv`/`getpid`/`fork`/`dladdr`) are supplied by a
Windows-only compat layer:

* [`tests/compat.hpp`](../tests/compat.hpp) — `setenv`/`unsetenv`, `usleep`,
  `sleep`, `mkdtemp`, `utimensat`, `pid_t`/`ssize_t`, `S_ISREG`/`S_ISDIR`,
  `STDIN_FILENO`… plus the `winsock2`-before-`windows.h` order and the
  `near`/`far` legacy-macro neutralisation. Every semantic delta from POSIX is
  documented at its definition.
* [`tests/compat/`](../tests/compat/) — a shim include-directory
  (`unistd.h`, `sys/wait.h`, `dlfcn.h`) added to the test include path only when
  `WIN32`, so literal `#include <unistd.h>` in otherwise-untouched test files
  resolves. On POSIX it is never added and the real headers win.
* `harness.hpp` includes `compat.hpp` under `#ifdef _WIN32`, covering tests that
  call `::setenv` without including `<unistd.h>`.

The compile-exposure ledger is now empty (**X = 0**,
[`windows-enablement/COMPILE_EXCLUSIONS.md`](../windows-enablement/COMPILE_EXCLUSIONS.md));
`grep -c "if(NOT WIN32)" tests/CMakeLists.txt` == 0. There are therefore no
configure-time test exclusions — the whole `all` target builds and every test is
registered with ctest.

### Build

The §3 `build-win` tree *is* the test build — `cmake --build build-win` (no
`--target`) must exit 0; that is the X = 0 acceptance. The old targeted-build
workaround (34 binaries, pre-WP12) is obsolete and removed from this guide.

### Run the suite

```bash
# W (registered test count) is censused, never hand-carried:
ctest --test-dir build-win -N | grep -oP 'Total Tests: \K[0-9]+'
#   -> 98, and must equal windows-enablement/logs/ci-expected-w.txt

# Routine full run (the LARGE_IO fixture is a separate, deliberate step):
ctest --test-dir build-win --output-on-failure --timeout 300 --label-exclude LARGE_IO
#   -> "100% tests passed ... out of 97"   (W - 1; LARGE_IO excluded)

# >2 GiB I/O fixture (sparse NTFS file, probes across the 2^31 and 4 GiB
# boundaries; needs >= 8 GiB usable on the volume, else it self-skips):
ctest --test-dir build-win -R '^test_large_file_io$' --output-on-failure
#   -> "out of 1"

# FP canary + numerics fingerprint pair (the T3 evidence both CI jobs run):
ctest --test-dir build-win -R '^(test_fp_model|test_numerics_fingerprint)$' --output-on-failure
#   -> "out of 2"
```

### The four cycle-2 tests (P8 block, `tests/CMakeLists.txt`)

| Test | What it proves | Registration facts |
|---|---|---|
| `test_fp_model` | FP-flag canary layer C (ID-12): `a*b+c` is NOT contracted into an FMA in a running binary, so the `/clang:-ffp-contract=off` pin (§6) is *effective*, not just present. Mutation-accepted: removing the pin makes it fail (transcript `windows-enablement/logs/id12-fp-canary-mutation.txt`) | all platforms, no libs beyond default |
| `test_platform_shim` | Direct unit coverage of the `lse::platform` seam (every API ≥ 1 case: I/O, stat, temps, mmap, locks, pipes, env, clocks, dl) | all platforms, < 5 s |
| `test_numerics_fingerprint` | ID-5 partition: gemv/rms_norm/rope_apply = HASH-EQ against embedded cross-platform SHA-256 constants; softmax/attention_step = TOLERANCE against f32-bit goldens (`max_rel` printed). Prints the partition + hashes; the cross-OS equality is the `linux-parity` job's T3 check (§7) | `ENVIRONMENT LSE_BACKEND=cpu;LSE_POOL=cpu` — never host-GPU-dependent |
| `test_large_file_io` | ID-1 >2 GiB acceptance: sparse fixture (logical 4 GiB + 1 MiB, ~4 MiB real), probes at 0 / 2^31−512 KiB / 2^31+1 MiB / 4 GiB−1 MiB, a straddle read across 2^31, EOF short-read → `kIoError`, safetensors with data past 2^31 | `LABELS "LARGE_IO"` + `TIMEOUT 120`; self-skips unless the volume is sparse-capable with ≥ 8 GiB free; excluded from routine runs and push/PR CI (§7) |

Three runtime notes:

* **`test_model`'s heavy real-model cases take hours on a machine whose HF
  cache holds `mlx-community/Qwen3.5-0.8B-4bit`** — the CPU fp64 interpreter
  drives ~2048 token-forwards plus the split checks (>2 h measured; no
  `TIMEOUT` property was added, deliberately). CI runners have no cache, so
  every heavy case self-skips in seconds via its `if (!paths.ok()) return;`
  guard, and the `--timeout 300` above assumes exactly that. To reproduce the
  CI condition on a cached machine, hide the cache for the run:
  `export HF_HUB_CACHE=<path-to-an-EMPTY-dir-ending-in-/hub>` (the `/hub`
  suffix satisfies `hf_cache_root_honours_env`; light cases pin their own
  `HF_HUB_CACHE` in-scope and are unaffected). test_model then passes 55/55
  in ~1 s. Full honest record:
  `windows-enablement/logs/test-model-heavy-path-note.txt`.
* **Two binaries print `skip ... (windows: ...)` lines and still exit 0** —
  the **K = 2 runtime capability quarantines**, ledgered in
  [`windows-enablement/K-LEDGER.md`](../windows-enablement/K-LEDGER.md)
  (ceiling 5; each row carries the platform delta, an issue link and a
  review-by date — both currently 2027-01-08):
  `test_communication:loopback-backpressure` (3 cases — Windows loopback TCP
  absorbs a single 32 MiB send whole, so the still-queued / mid-transfer-death
  states the cases assert on never exist) and `test_reflect:p2996` (1 case —
  no pinned Windows compiler ships `-freflection`; upstream `ci.yml` on g++-16
  remains the P2996 gate). A quarantine is an in-test `LSE_SKIP`: the binary
  passes, W is never reduced, and `tests/KNOWN_FAILURES.md` never gains a
  Windows entry.
* **Fixture byte-exactness**: `.gitattributes` pins `tests/fixtures/** -text`
  — CRLF checkout conversion would corrupt byte-exact fixtures on Windows.
  Do not normalize that directory.

The 40 loomc/hipc-exposure binaries are machine-generated into
[`windows-enablement/COMPILE_EXPOSURE.txt`](../windows-enablement/COMPILE_EXPOSURE.txt)
by `gen_compile_exposure.sh` (retained for reference and for the CI count
assertion); with X = 0 they are a subset of the full build, not a separate
gate.

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
help/models paths**; **M2b (accepted, cycle 2) verified end-to-end
generation** on the CPU reference backend via the SC4 sentinel (`lse.exe`
prompt + `lse-server.exe` HTTP round-trip; evidence
`windows-enablement/logs/`, `logs/m2b-*`). Inference runs on the **CPU
reference backend only** — there is no GPU acceleration on Windows yet.

## 6. Current limitations

Each of these is deliberate and documented in code; the full deviations lists
are `windows-enablement/06-implementation-log.md` §4 (cycle 1) and
`windows-enablement/13-cycle2-implementation-log.md` §4 (cycle 2, D-1…D-18).

| Area | Limitation | Reference |
|---|---|---|
| GPU | HRX/HIP/amd_comgr/loomc backends **OFF**; CPU backend only; no kernel JIT | `cmake/LSEOptions.cmake:16-22` |
| Poller | WSAPoll has **millisecond** timeout granularity (epoll: nanoseconds); timeouts are rounded up. `Poller::fd()` returns the wake socket (WSAPoll has no waitable fd) | `src/communication/poller_winsock.cpp` header |
| Heap stats | `heap_stats()` returns **zeros** (no `mallinfo2` in the UCRT); `malloc_trim` is a no-op. API JSON keys are unchanged | `src/api/lse.cpp`, 06-log §4.4 |
| >2 GB checkpoint I/O | Implemented per directive ID-1: positioned reads via `ReadFile` + `OVERLAPPED` with 64-bit offsets (never the 32-bit `_pread`), memory mapping via `_wsopen_s(_SH_DENYNO)` + `CreateFileMappingW` + `MapViewOfFile` with a process-wide handle registry | `src/core/platform_win.cpp` |
| Clocks | Biased `QueryInterruptTime` is resolved via `GetProcAddress` at runtime (SDKs declare it but `kernel32.lib` ships only the `Unbiased` thunk), with unbiased fallback | `src/core/platform_win.cpp`, 06-log §4.6 |
| Sockets | Abstract UNIX-socket namespace returns `kUnimplemented` (Linux-only, same as the existing macOS guard). httplib keeps its `select` default; `FD_SETSIZE`=64 counts sockets-in-set | 06-log §5.18-20 |
| Tests | Full suite builds and is registered (W = 98, X = 0); main run green locally and on CI (97/97 with LARGE_IO excluded; >2 GiB fixture 1/1 locally); K = 2 ledgered capability quarantines (loopback backpressure; P2996 — both review-by 2027-01-08); the >2 GiB fixture runs only under its LARGE_IO label | §4/§7 above, `windows-enablement/K-LEDGER.md` |
| Numerics | `-fno-fast-math` + `-ffp-contract=off` pinned via `/clang:-` spelling (`cmake/LSECompilerFlags.cmake:74-75`), guarded by canary layers A/B/B′ and the runtime `test_fp_model` (ID-12, mutation-accepted); `test_numerics_fingerprint` records the ID-5 partition (gemv/rms_norm/rope_apply = HASH-EQ against embedded cross-platform SHA-256 constants; softmax/attention_step = TOLERANCE against f32 goldens, max_rel ≤ 1e-5). The cross-OS hash equality is asserted by the CI `linux-parity` job (same LLVM major, same SHA) — green at the M3 acceptance SHA `5b16e94` | `windows-enablement/04-test-strategy.md` §T3, `11-cycle2-test-plan.md` §7.3 |

**Cycle-2 product fixes** — six defects found *by* the Windows port, fixed for
all platforms or `#ifdef _WIN32`-additive (details in
`windows-enablement/13-cycle2-implementation-log.md`). #5–#6 are
**cross-drive correctness fixes that matter to any Windows deployment** whose
working directory and temp/model paths sit on different volumes
(multi-drive workstations, hosted CI runners) — not CI-only issues:

1. `src/hub/pull.cpp` — close the file handle before delete/rename (Windows
   forbids removing open files; POSIX unaffected).
2. `src/communication/winsock_compat.hpp` — `sock_sendmsg` uses `WSASend`
   (`WSASendMsg` returns WSAEINVAL on stream sockets).
3. `src/core/platform_win.cpp` — `map_discard` hybrid: `DiscardVirtualMemory`
   (deterministic zero-fill) for page-aligned anon ranges, `MEM_RESET`
   fallback for file-backed views (which `DiscardVirtualMemory` refuses and
   which must re-fault file content, not zeros).
4. `src/probe/profile_store.cpp` — `write_text_atomic` temp names are unique
   per CALL (PID + atomic sequence), not merely per process: concurrent ranks
   of one pool qualification all save the same fingerprint, and on Windows the
   shared `.tmp<pid>` let the loser's error-path remove delete the winner's
   temp so the profile never landed (POSIX hid the collision). Repro 3/50 →
   0/100 post-fix (13-log D-14).
5. `src/communication/socket.cpp` — a `unix://C:/path/x.sock` endpoint parses
   with the drive in the authority slot; the bind used the bare `/path/...`,
   a drive-relative name that Windows resolves against the process's CURRENT
   drive. Invisible when code and %TEMP% share a letter, fatal when they do
   not (CI: checkout on D:, sockets on C:). The socket name now re-attaches
   the drive letter (`_WIN32` only; POSIX untouched) — 13-log D-17.
6. `src/hub/cache.cpp` — `LSE_MODEL_DIRS` split unconditionally on `':'`,
   which cuts every absolute Windows path in half (`C` + `\models\...`, the
   half again resolving against the current drive). On Windows the list now
   splits on `';'` and on `':'` only where it is not a drive colon; POSIX
   keeps the `':'` behavior verbatim — 13-log D-17.

## 7. Continuous integration (`.github/workflows/windows.yml`)

Landed at C2-WP6 (purely additive — upstream's `ci.yml`, self-hosted g++-16 +
HRX, remains the authoritative GPU and P2996 gate). Acceptance: run **5
(38137423880) green on BOTH jobs** at `5b16e94`.

**Triggers:** `pull_request`; `push` to `feature/windows-enablement`;
`schedule` — cron `17 6 * * 1`, the weekly **toolchain-drift alarm** (a
silent C++26/compiler break shows up within a week; verify the line with
`gh workflow view windows.yml --repo antmikinka/LSE --ref feature/windows-enablement --yaml | grep cron`).
GitHub fires `on.schedule` only from the workflow file on the repo's
**default branch** — the fork's default branch is
`feature/windows-enablement` (flipped 2026-10-11, 15-iteration3 A2), which
arms the cron; arming is confirmed by the first observed `schedule`-event
run;
`workflow_dispatch` with one input, **`run_large_io`** (boolean, default
false).

**Concurrency:** group `win-${{ github.ref }}`, `cancel-in-progress: true` —
any push or dispatch to the branch while a run is in flight **cancels the
evidence run**. Do not push during an acceptance run.

**LARGE_IO policy** (workflow env `LARGE_IO_ON`, renamed at C10 — D-19:
ctest's `--label-include` *selects* the labeled test(s) instead of adding
them, so ON is expressed as NO label filter): push/PR runs use
`--label-exclude LARGE_IO` (`LARGE_IO_ON=false`; hosted disks are small);
the weekly schedule is always ON (full suite including the LARGE_IO test);
a dispatch is ON only with `run_large_io=true`. Bash steps derive the ctest
flag from the boolean (GitHub expressions cannot yield an empty string —
`''` is falsy). The authoritative local run is §4's separate
`ctest -R '^test_large_file_io$'`.

**`windows` job** (windows-latest, 60-min budget) mirrors the local gate
commands: pin LLVM **21.1.8** (`winget install --id LLVM.LLVM --version
21.1.8 --exact`, §1) → MSVC environment (`ilammy/msvc-dev-cmd`, x64) → stable
Rust (MSVC target) → configure with the same flags as §3 plus fail-fast
guards (HRX-OFF line; `native deps` must contain `ntdll`) → FP-canary layers
A/B asserted on `build.ninja` → tests-ON `all` build → W census must equal
`windows-enablement/logs/ci-expected-w.txt` (98) → strict main ctest with
count assertion (W − 1 when LARGE_IO is excluded) → the fingerprint pair
(`test_fp_model` + `test_numerics_fingerprint`, 2/2) → K ≤ 5, X = 0 and
no-Windows-entry-in-`KNOWN_FAILURES.md` guards → `windows-evidence` artifact.
On scheduled runs only, a second **non-gating** ctest
(`--repeat until-pass:3`) classifies flakes; the gating run stays strict on
every event. Both the Configure and Build steps sink Git Bash's coreutils
dirs to the END of `PATH` — Git's `link(1)` otherwise shadows MSVC's
`link.exe` for rustc (run 1/2 defects D-15/D-16, 13-log §4).

**`linux-parity` job** (ubuntu-latest): clang++-**21** from apt.llvm.org —
the same LLVM major as the windows job, which makes the two the **T3
numerics control pair**. Scope is honest and limited: CPU-only, no ROCm/GPU.
It configures `-DLSE_ENABLE_HRX=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
-DCMAKE_CXX_FLAGS=-ffp-contract=off`, asserts W == WE + 2 (the structural
UNIX-only `compiler_identity_default`/`compiler_identity_override` offset —
any other delta fails the job), runs the main ctest with `^test_reflect$`
excluded (labeled in the step summary: LLVM 21 has no `-freflection`;
upstream `ci.yml` on g++-16 owns P2996), runs the fingerprint pair — **its
HASH-EQ hashes must equal the windows job's** — plus the ID-11
`test_registration_smoke` baseline, and uploads `linux-parity-evidence`.

**Same-SHA rule:** a gate counts only when BOTH jobs are green on the
identical commit (`windows-enablement/11-cycle2-test-plan.md` §6) — satisfied
at `5b16e94`. M4 additionally requires **3 consecutive green
release-candidate runs** and the release protocol (13-log §9). A
supplementary large-io hosted proof (`workflow_dispatch`,
`run_large_io=true`) was dispatched at the acceptance SHA; result pending.

## 8. Troubleshooting

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
On current Rust this is **fatal for the final link**: the fallback set
(`ws2_32.lib bcrypt.lib userenv.lib`, `LSERust.cmake:97-100`) lacks
`ntdll.lib`, which Rust std requires (`NtReadFile`/`NtWriteFile`) — `lse.exe`
dies with `lld-link: undefined symbol __declspec(dllimport) NtWriteFile`.
Usual cause: the query's cargo build spawned Git coreutils' `link(1)` instead
of MSVC `link.exe` (symptom in the log: `/usr/bin/link: extra operand`)
because a Git-Bash-style `PATH` puts `/usr/bin` first. Fix: configure from a
shell where the MSVC tools precede Git's coreutils (the vcvars shells of §2
do; CI sinks the coreutils dirs explicitly — §7). The configure summary's
`fastokens_ffi native deps:` line must contain `ntdll.lib`.

**ctest: "Unable to find executable" for many tests**
The tree is a stale pre-cycle-2 targeted-mode build. Delete the build dir and
rebuild the full tests-ON `all` target (§3) — every registered test then has
a binary.

**Build fails in a test source with `'unistd.h': No such file or directory`**
Fixed by the WP12a compat layer (`tests/compat.hpp` + the WIN32-only
`tests/compat/` shim include-dir, §4). If you still see it, your tree predates
WP12a — update the branch, and check the failing target is registered through
`lse_add_test` (which adds the shim dir on WIN32).

**Everything behaved, then the branch/toolchain changed**
Build trees cache the compiler. After switching branches or LLVM versions,
`rm -rf build-win-notests build-win` and reconfigure.

## 9. Roadmap and further reading

Cycle-2 state: **complete — M3 achieved 2026-10-11.** WP11/M2b done
(end-to-end generation verified); WP12a/b done (all test sources ported via
`tests/compat.hpp`, X → 0, full tests-ON `all` build green, the four P8 tests
landed); WP6/CI done — `.github/workflows/windows.yml` green on BOTH jobs at
`5b16e94` (run 38137423880 = the G-C2-5 acceptance and the linux-parity half
of M3; the large-io hosted proof is dispatched at the same SHA, result
pending). Six Windows product defects were found and fixed en route (§6).
Next: **M4** — 3 consecutive green release-candidate runs, the release
protocol and version pinning on the Windows artifacts (`lse.exe`,
`lse-server.exe`); K-ledger reviews due 2027-01-08; LLVM pin revisited at
22 (the weekly cron is the drift alarm until then); the cl.exe path remains
planned-not-landed. The upstream PR to `Geramy/LSE` is **deferred to M4** by
user decision; the refreshed PR plan lives in
[`windows-enablement/14-documentation-update.md`](../windows-enablement/14-documentation-update.md).

The living state file is
[`windows-enablement/STATUS.md`](../windows-enablement/STATUS.md); the
cycle-2 build log (session split, every deviation D-1…D-18) is
[`windows-enablement/13-cycle2-implementation-log.md`](../windows-enablement/13-cycle2-implementation-log.md).

| Artifact | What it is |
|---|---|
| [`windows-enablement/STATUS.md`](../windows-enablement/STATUS.md) | Living state: gate table, W/X/K, incidents |
| [`windows-enablement/14-documentation-update.md`](../windows-enablement/14-documentation-update.md) | This refresh's delta record + M4 doc triggers + upstream-PR plan |
| [`windows-enablement/13-cycle2-implementation-log.md`](../windows-enablement/13-cycle2-implementation-log.md) | Cycle-2 implementation record + deviations D-1…D-18 |
| [`windows-enablement/11-cycle2-test-plan.md`](../windows-enablement/11-cycle2-test-plan.md) | M3 protocol + CI spec (§5–§6) |
| [`.github/workflows/windows.yml`](../.github/workflows/windows.yml) | The CI gate itself (header comments record every deviation) |
| [`windows-enablement/K-LEDGER.md`](../windows-enablement/K-LEDGER.md) | Runtime quarantine ledger (K) |
| [`windows-enablement/07-documentation.md`](../windows-enablement/07-documentation.md) | Documentation-package index + maintenance map |
| [`windows-enablement/06-implementation-log.md`](../windows-enablement/06-implementation-log.md) | Cycle-1 record: environment, work packages, deviations, discrepancy log |
| [`windows-enablement/05-quality-review-2.md`](../windows-enablement/05-quality-review-2.md) | Directive layer ID-1…ID-14 (normative fix specs) |
| [`windows-enablement/04-test-strategy.md`](../windows-enablement/04-test-strategy.md) | Test/gate strategy, numerics policy |
| [`windows-enablement/02-program-plan.md`](../windows-enablement/02-program-plan.md) | WP0–WP15 program plan, milestones, risks |
| [`windows-enablement/01-planning-analysis.md`](../windows-enablement/01-planning-analysis.md) | Toolchain choice analysis |
| [`windows-enablement/COMPILE_EXCLUSIONS.md`](../windows-enablement/COMPILE_EXCLUSIONS.md) | Living exclusions ledger (X = 0) |
| [`logs/m2a-build.txt`](../logs/m2a-build.txt) | M2a gate evidence: full build log + ctest transcript |
| [`logs/m3-ctest-windows.txt`](../logs/m3-ctest-windows.txt) | M3 main-run evidence (97/97; see also `logs/m3-*.txt`) |
