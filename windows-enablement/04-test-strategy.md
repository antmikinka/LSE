# LSE Windows Enablement — Stage 4: Test Strategy

**Branch:** `feature/windows-enablement` (base: master @ `a5b82f6`, v0.6.0-28; fork `antmikinka/LSE`, upstream `Geramy/LSE`)
**Date:** 2026-10-10
**Author:** Stage 4 (test strategy) — analysis structured via sequential-thinking before writing
**Inputs:** `01-planning-analysis.md` (Stage 1), `02-program-plan.md` (Stage 2), `03-quality-review-1.md` (Stage 3; BL-1…BL-4, G1–G12 all consumed)
**Constraints honored:** no source modified, nothing committed. All file:line evidence re-derived from the working tree at `a5b82f6`.
**Delegations discharged:** Stage 2 Q3 (socket strategy + KNOWN_FAILURES format), Appendix B test-strategist contract (T2/T3 designs, M3 evidence spec).

---

## 1. Test Inventory & Triage

### 1.1 Registration ground truth (re-derived)

`tests/CMakeLists.txt` contains **exactly 102 `lse_add_test()` calls** (confirms Stage 2 SC5 / Stage 3 row 17) plus **8 raw `add_test()` entries**:

| Raw entry | Line | Gate |
|---|---|---|
| `test_server_shutdown` | :158 | none (own `main`, **not** harness-based) |
| `test_server_cli` | :274-278 | `Python3_Interpreter_FOUND AND TARGET lse-server` |
| `test_loaded_runtime` | :325-327 | `if(APPLE)` :317 |
| `test_quant_operand_cache` | :417 | `if(TARGET lse_backend_hrx)` :413 |
| `compiler_identity_default` / `compiler_identity_override` | :446, :449 | `if(UNIX)` :429 |
| `test_capture_prefill_hip_gfx1151` / `_loom_gfx1151` | :585, :590 | `if(LSE_ENABLE_HRX)` :580 |

**HRX-gated `lse_add_test` entries (12 — never register on Windows with `LSE_ENABLE_HRX=OFF`, existing guards suffice):** `test_jit`, `test_sq_profiler`, `test_decode_occupancy` (:216-223), `test_loom_probe`, `test_matrix_probe_lifecycle` (:282-289), `test_loom_dot_native` (:378-380), `test_quant_defaults` (:391-393), `test_fp8_conversion_native` (:396-398), `test_q6_wmma` (:421-423), `test_tile_vector_load` (:469-471), `test_sparse_attention` (:577-579), `test_capture_prefill` (:580-582).

**Windows-registered set W = 92** ctest entries (90 `lse_add_test` + `test_server_shutdown` + `test_server_cli`), assuming WP8 (Rust) succeeds and Python3 is present (both true on GitHub runners). W must be re-captured via `ctest -N` at M2a and M3 with committed diffs (Stage 3 MJ-4/G12).

**Structural hazard (new finding):** `test_server` (:129), `test_chat_protocol` (:130), `test_api_parity` (:135), `test_memory_lifecycle` (:196) link `lse_server`/`lse_tokenizer` **unconditionally** — but those targets exist only `if(TARGET lse_tokenizer)` (root `CMakeLists.txt:439-465`). If WP8/D2 fails on Windows, these four produce **link errors, not clean exclusions**, breaking the whole test tree. WP12 fix (additive, 4 lines): wrap each registration in `if(TARGET lse_tokenizer)`. This also makes the Q1 fallback (M2a-without-M2b) survivable with tests ON.

### 1.2 The loomc/hipc compile-exposure set (fixes BL-3/G6 at COMPILE level)

Stage 3's G6 names four unconditionally-added loomc TUs; the true surface is larger. On Windows (no `lse_backend_hrx` target), **every `if(NOT TARGET lse_backend_hrx)` source block fires** (:114-124, :181-187, :197-202, :304-307, :334-344, :491-497, :524-536, :546-556, :564-574), on top of the unconditional `target_sources` blocks. Result: **35 Windows-registered tests compile TUs from `src/backends/hrx/loomc/*.cpp` and/or `src/backends/hrx/hipc/*.cpp`**:

- loomc-only, unconditional: `test_pointwise_fusion` (:28-32), `test_loom_print` (:50-53), `test_loom_repeat` (:56-60), `test_loom_extent` (:62-65), `test_loom_conv` (:67-72), `test_loom_gdn` (:75-79), `test_loom_words` (:81-84), `test_loom_concat` (:96-100), `test_loom_cache` (:102-106), `test_submission_constants` (:296-300), `test_attention_decode` (:311-315), `test_loom_matrix` (:356-359), `test_loom_dot` (:373-377), `test_q8_matrix_dispatch` (:401-405)
- loomc(+hipc) via `NOT TARGET lse_backend_hrx` else-paths: `test_quant_prefill`, `test_dflash2`, `test_runtime`, `test_perplexity`, `test_submission_decode`, `test_gdn_pair`, `test_cooperative_rms`, `test_wave32_l2`, `test_topk_parallel`, `test_quant_swiglu`, `test_typed_kernel`, `test_q4_activation_panel`, `test_q4_matrix_panel`, `test_q8_matrix_pack`, `test_dflash2_walk`, `test_kv_cache`
- loomc/hipc, CPU-gated: `test_scheduler_epilogue` (:347-353), `test_gdn_scheduler` (:383-389)
- hipc-bearing unconditional: `test_loom_flash` (:85-94), `test_quant_dispatch` (:407-411, `hip_sources.cpp`)

These TUs are *designed* to compile without libhrx (design comments :42-43, :54, :112; identical exposure on bare Linux builds), but **clang-cl compile is unverified**. Policy:

1. **WP10 checklist item (mandatory):** an experimental `-DLSE_BUILD_TESTS=ON` configure+build on Windows must compile all 35 loomc/hipc-bearing test binaries, or each failure is dispositioned *before* M2a.
2. **Compile-gating mechanism (not KNOWN_FAILURES):** for any TU that fails clang-cl, add an additive `if(NOT WIN32)` guard around the `target_sources` block **and** an in-test `#ifdef _WIN32 LSE_SKIP("windows: loomc TU not compiled — see COMPILE_EXCLUSIONS.md")` around the dependent cases; if the whole binary is unbuildable, guard the `lse_add_test` call itself. Per Stage 3 BL-4/G7: KNOWN_FAILURES.md disables **execution only** (`tests/CMakeLists.txt:5` vs :597-606) and can never absorb a compile failure.
3. **Ledger:** new `windows-enablement/COMPILE_EXCLUSIONS.md` (test, guarded CMake line, reason, issue link). Count = **X**, tracked separately from runtime K. Target X = 0; X ≤ 5 without escalation.
4. **Governance carve-out:** any *inside-TU* fix in the four loomc files requires the §4.3-rule-5 amendment Stage 3 demands (BL-3); the compile-gate path above avoids touching `src/backends/hrx/**` at all and is the preferred first resort.

### 1.3 Classification table (primary bucket = most restrictive Windows-enablement need)

Evidence greps: 15 Windows-registered test files include `<unistd.h>`/`<sys/wait.h>` (Stage 1 T1 list minus HRX-gated `test_jit.cpp`, `test_capture_prefill.cpp` per Stage 3 G11); `CPPHTTPLIB_USE_POLL` defined in tests at `test_api_parity.cpp:33`, `test_hub.cpp:24`, `test_server_shutdown.cpp:2`; hardcoded POSIX paths at `test_graph.cpp:3155` (`/tmp/lse-cold-spans-`), `test_tree_verify.cpp:703` (`mkdtemp` template `/tmp/lse-trials-XXXXXX`), `test_dflash2.cpp:1610` (`/private/tmp/...`), `test_model.cpp:312-328` (`HOME=/tmp/lse-fake-home` + `hf_cache_root()` string asserts); abstract sockets `"unix://@..."` at `test_communication.cpp:196,327,368,434,611,663,771,1025,1100,1392,1452`; fork/waitpid/pipe at `test_server_shutdown.cpp:24-37`.

| Bucket | Definition | Count | Members |
|---|---|---|---|
| **(a)** | Portable — expect green on Windows CPU backend after WP3–WP11 shims land, no test-code changes | **69** | All remaining registered suites: sampling/decoding (`test_speculative_sampler`, `test_draft_tree`, `test_decode_sample`, `test_generation_stats`, …), IR/graph/kernel-contract suites (`test_ir`, `test_graph`*, `test_primitive`, `test_shape`, `test_dtype`, `test_reflect`, …), loom emission suites (§1.2 set), quant/fp8 suites (`test_quant*`, `test_fp8_conversion`, `test_q4_*`, `test_q8_*`, `test_sha256`), CPU-backend numerics (`test_backend_cpu`, `test_reference`, `test_runtime`, `test_perplexity`, `test_attention_layout`), kv (`test_kv_cache`, `test_kv_memory`), trace/place/probe-policy (`test_trace`, `test_cpu_fallback`, `test_probe_policy`, `test_probe_measurement`, …), `test_compiler_identity` (binary itself portable; UNIX fixture variants :429-452 simply don't register), `test_hrx_copy_route` (pure logic over `copy_route.hpp`, no GPU — verified), etc. (*`test_graph` is bucket-(a) for logic but needs the :3155 `/tmp` port → counted in (b).) |
| **(b)** | Needs test-code port work first (POSIX constructs beyond sockets) | **10** | `test_server_shutdown` (fork/waitpid/pipe :24-37, POLL define :2), `test_dflash2` (:1610 `/private/tmp`, unistd :30), `test_dflash2_convert` (unistd :26), `test_graph` (:3155 `/tmp`, unistd :16), `test_hybrid_tokens` (unistd :12), `test_loom_cache` (unistd :19), `test_sampling_defaults` (unistd :8, `setenv/getpid` :15), `test_tree_verify` (:703 mkdtemp, unistd :26), `test_variant_book` (unistd :7, `setenv/unsetenv/getpid` :15-19), `test_model` (:312-328 HOME/`/tmp` asserts) |
| **(c)** | HRX/GPU-only — compile-gated OFF on Windows | **15 + 3 raw** | 12 HRX-gated `lse_add_test` (§1.1) + `kv_fragment_probe`/`compile_loom_matrix`/`measure_loomc_compile` executables (:45-49, :360-369, non-ctest) + raw `test_quant_operand_cache`, `test_capture_prefill_hip_gfx1151`, `test_capture_prefill_loom_gfx1151`. **Plus** the 35-test loomc/hipc compile-exposure set (§1.2) which stays registered but compile-verified/gated. Platform-gated raw (no Windows work): `compiler_identity_default/override` (UNIX), `test_loaded_runtime` (APPLE) — optional P2 stretch to re-enable via `LoadLibrary`/`.dll` fixtures. |
| **(d)** | Server/IPC (AF_UNIX / poller / httplib loopback) | **10** | `test_communication` (**abstract AF_UNIX** — the only true blocker: Windows AF_UNIX exists since 1803 but has **no abstract namespace**; `socket.cpp:128-140` builds `sun_path[0]='\0'`), `test_dist` (threads+loopback), `test_probe` (loopback link matrix), `test_server`, `test_chat_protocol`, `test_api_parity` (TCP loopback httplib, POLL define), `test_hub` (TCP loopback mock, POLL define), `test_http_timings` (in-process httplib), `test_memory_lifecycle` (server), `test_server_cli` (Python driving `lse-server.exe`) |
| **(e)** | Rust FFI (`fastokens_ffi`) dependent | **9** (3 exclusive, 6 overlap with (d)) | Exclusive: `test_model_info`, `test_power` (`TARGET lse_api` :139-141), `test_tokenizer` (:205-207). Overlap: `test_server`, `test_chat_protocol`, `test_api_parity`, `test_memory_lifecycle`, `test_hub` (`TARGET lse_hub` :164-168), `test_server_cli`. *(Stage 1's "~15 tests" counts `test_model_info_real` and case-level dependencies; the verified ctest-entry count is 9.)* |

**Totals: W = 92 = 69 (a) + 10 (b) + 10 (d) + 3 (e-exclusive); (c) = 15 ctest + 3 raw never register; (e) full set = 9.**

### 1.4 Socket-transport decisions (Q3 disposition)

- `socket.cpp:122-149`: path-based AF_UNIX works on Windows 1803+ (runner images are Server 2022+ = 1809+, floor met); **abstract namespace does not exist** — extend the macOS rejection at :129-130 to `_WIN32` (WP6, one-line `#if defined(__APPLE__) || defined(_WIN32)`).
- `test_communication.cpp`: **port, don't skip.** Add a file-static helper `pipe_url(name)` → `"unix://@" name` on POSIX, `"unix://" (std::filesystem::temp_directory_path() / (name + ".sock")).string()` on Windows; replace the 11 construction sites; add `std::filesystem::remove` at teardown on Windows (abstract leaves nothing; path sockets do). Keep one case asserting the `kUnimplemented` rejection for `@`-names on Windows (mirrors the macOS precedent). Runner `%TEMP%` paths (`C:\Users\RUNNER~1\AppData\Local\Temp\...`) fit the 108-byte `sun_path` limit — add a length guard in the helper.
- epoll: library-level, handled by `poller_winsock.cpp` (WP6); no test changes.
- httplib `poll()`: `#ifndef _WIN32` guard on the three test-side `CPPHTTPLIB_USE_POLL` defines (`test_server_shutdown.cpp:2`, `test_api_parity.cpp:33`, `test_hub.cpp:24`) matching WP7's `src/server/http_server.cpp:13` guard → httplib falls back to `select()`.
- TCP loopback suites (`test_api_parity`, `test_hub`, `test_server_shutdown` cases 2-4, `test_server_cli`): portable as-is after the POLL guard.

---

## 2. Correctness Verification Strategy (Windows ≡ Linux numerics)

### 2.1 FP flag risk verdict

Evidence: `cmake/LSECompilerFlags.cmake:36-38` applies `-fno-fast-math` to every target ("never let the compiler reassociate float math"); **no `-ffp-contract` setting exists anywhere**; `#pragma clang fp contract(off)` at `src/model/dflash2_convert.cpp:38` (Stage 3 §5) shows contraction is deliberately pinned in at least one numerics TU; Linux CI compiles with **g++-16** (`.github/workflows/ci.yml:95`) whose default is `-ffp-contract=fast` (FMA fusion across statements); macOS builds with **clang 21** (`macos.yml` `brew install llvm@21`); Windows will use **clang-cl 21** whose MSVC-compat default is `/fp:precise`.

- **Flush-to-zero/denormals-ar-zero: NOT a risk.** No toolchain in scope enables FTZ: `-fno-fast-math` is kept on clang-cl (Stage 2 WP1 rule; acceptance proven in WP0 smoke, not assumed), clang-cl `/fp:precise` never sets FTZ/DAZ, and the program must **never** map to `/fp:fast`. x87 is irrelevant on x64 (SSE2 baseline; no `/arch:` flags anywhere).
- **FP contraction: REAL risk — but pre-existing and already tolerated.** Linux-GCC-16 (contract=fast) and macOS-clang (no `/fp:fast`, expression-level contraction at most) **already** produce different last-ulp FP results today; the suite is green on both, which proves it is tolerance-based, not bit-exact cross-toolchain (`LSE_EXPECT_NEAR` `tests/harness.hpp:152-162`; `tests/reference.hpp:31-54` `Deviation{max_abs,max_rel}`). The Windows dangers are (i) relying on clang-cl defaults that can drift across LLVM releases, and (ii) diffing Windows against the GCC path and mistaking legitimate FMA deltas for porting defects.

### 2.2 Three-tier comparison policy

| Tier | Scope | Policy | Vehicles |
|---|---|---|---|
| **T1 — Bit-exact, cross-platform** | Contract-insensitive ops: hashing, quant packing, FP8/FP4 table conversions, dtype bit patterns | Output-byte SHA256 **must** match Linux byte-for-byte | `test_sha256`, `test_fp8_conversion`, `test_dtype`, `test_q8_matrix_pack`, `test_q8_matrix_dispatch`, `test_q4_matrix_panel`, `test_q4_activation_panel`, `test_quant`, `test_quant_dispatch`, `test_submission_constants` |
| **T2 — Tolerance, per-suite** | Float tensor math on the CPU backend | Existing suites unchanged; all must pass on Windows as-is (their tolerances already span GCC↔clang deltas) | `test_backend_cpu`, `test_graph`, `test_primitive`, `test_reference`, `test_perplexity`, `test_runtime`, `test_attention_*`, `test_gdn_*`, `test_kv_cache`, `test_flash_prefill`, `test_cooperative_rms` |
| **T3 — Controlled-pair fingerprint** | Deterministic cross-platform proof | New `tests/test_numerics_fingerprint.cpp` (additive): fixed-seed committed fixture drives GEMV/RMSNorm/softmax/rope/attention-step on the CPU backend; prints SHA256 of each output tensor + `max_rel` vs embedded goldens. Gate: both platforms pass tolerances **and** the Windows-clang-cl hash **equals** the Linux-parity clang++-21 hash (same LLVM major, both pinned `-ffp-contract=off`) | Runs in both CI jobs (§5); hashes uploaded as artifacts |

Supporting measures:
- **Explicit pin:** the Windows branch of `lse_apply_common_flags` adds `-ffp-contract=off` (clang-cl accepts GNU spelling; if not, `/clang:-ffp-contract=off`). The Linux parity job (§5) configures the same pin **via the fork's CI only** — upstream `ci.yml` and default Linux/macOS flags are untouched (SC6 zero-behavioral-change).
- **Canary:** new `tests/test_fp_model.cpp` — runtime detection that `a*b+c` was NOT FMA-contracted (values chosen so contraction differs in the last ulp; compares against `std::fma` and a precomputed non-contracted constant). Fails loudly if a weekly rebase silently drops the pin (R3 guard).
- **Endianness:** both platforms x64 little-endian; `src/core/sha256.cpp` uses `<bit>`/portable code (Stage 1 §3.6) — no action; `test_sha256` is the regression guard.

### 2.3 Golden reference data — make it non-vacuous

`tests/reference.hpp:1-5,18-26`: goldens come from `tests/data/reference.safetensors`, generated out-of-tree by `scripts/dump_reference.py`; **`tests/data/` does not exist in the repo**, so all `have_reference()`-gated cases in `test_reference.cpp` (:95,120,151,190,231,280,459,552,607,687) **silently no-op** on a fresh CI checkout. M3/M4 evidence therefore requires: generate once on Linux, record SHA256, cache as a CI artifact/action-cache key; both the Windows and parity jobs materialize it via `LSE_REFERENCE` env and run `test_reference` with `have_reference()==true`. An M3 run without materialized goldens is **incomplete evidence** and must be labeled as such in `DECISIONS.md`.

---

## 3. BL-2 Large-File Acceptance Test (>2 GB / >4 GB)

**Target defect class (Stage 3 BL-2/G8):** UCRT `_pread` offset is 32-bit `long`, `off_t` is 32-bit, default `struct stat` is `_stat64i32` → silent size misreport/truncated reads above 2 GB. Production consumers: `src/core/file_read.cpp:22-23` (`::pread(fd, …, static_cast<off_t>(offset+done))` — the cast itself truncates pre-shim), `src/model/weights.cpp:217` (`fstat`→`st_size`) → :227 (`mmap(file_size)`), `src/model/dflash2_convert.cpp:247-250` pread loops, `src/hub/pull.cpp` resume writes.

**New additive test: `tests/test_large_file_io.cpp`** — registered on **all** platforms (the POSIX pass-through gets parity-tested too), ctest label `LARGE_IO`, default-excluded from PR CI runs (`ctest --label-exclude LARGE_IO`), included in the weekly schedule and **mandatory locally for M3**.

Design:
1. **Fixture:** sparse file, logical size **4 GiB + 1 MiB**. Windows: `CreateFileW` + `FSCTL_SET_SPARSE` + `SetEndOfFile`; POSIX: `ftruncate` (sparse on ext4/APFS). Real committed bytes: four 1 MiB probe regions filled from a deterministic seeded PRNG at offsets **0**, **2³¹−512 KiB** (straddles the signed-32-bit boundary), **2³¹+1 MiB**, and **4 GiB−1 MiB+…** (beyond 2³² to catch 32-bit `st_size` wrap). ~4 MiB of real data; NTFS sparse cost is metadata-only.
2. **Assertions:**
   - shim `stat` size == 4 GiB + 1 MiB exactly (catches `_stat64i32`);
   - `lse::read_file_range` (`file_read.cpp:13-38` — the actual production path, chunked at `1<<30`) reads each probe back with SHA256 equal to the seeded expectation (reuses `src/core/sha256`);
   - one single call spanning `[2³¹−512KiB, 2³¹+512KiB]` verifies the `offset+done` arithmetic across the boundary (exactly where a 32-bit `off_t` cast truncates);
   - short-read at EOF returns `kIoError`, not garbage (`file_read.cpp:30-34` semantics).
3. **weights.cpp mmap path:** synthesize a minimal valid safetensors file (8-byte header length + JSON header describing one F32 tensor) whose data section begins beyond 2³¹ inside the sparse file; load via `model::SafeTensors::map_file` (`weights.cpp:212-250`) and `read_f32`; verify tensor bytes hash. Exercises `_fstat64` size, `CreateFileMappingW`/`MapViewOfFile` (64-bit-clean per the BL-2 recipe) beyond **both** 2 GB and 4 GB, and the streaming-fd pread path together.
4. **Guards:** `LSE_SKIP` if the volume isn't NTFS/sparse-capable or free space < 8 GiB — checked via `GetDiskFreeSpaceEx` (which itself exercises the statvfs shim, G3).
5. **Runtime budget:** sparse create ≈ instant; ~4 MiB writes + ~12 MiB reads + hashes ≈ **< 30 s** on NVMe. `windows-latest` `D:` scratch (~14 GB free) and local dev machines both qualify. CI: `workflow_dispatch` input `run_large_io` or weekly schedule only; local M3 pass recorded in `windows-enablement/logs/m3-large-file-io.txt`.

---

## 4. Platform-Shim Unit Tests (B1 direct coverage)

**New additive test: `tests/test_platform_shim.cpp`** — harness-based, registered on **all** platforms (POSIX side must prove pass-through parity, WP3 acceptance), ~14 case groups. Rule: **every shim API in the WP3/B1 list (as amended by Stage 3 BL-1/MJ-1/MJ-3/G10) gets ≥1 direct case; no reliance on integration coverage.**

| Shim | Cases |
|---|---|
| fd ops: `pread`, open-flag mapping (`O_CLOEXEC`→`_O_NOINHERIT`, `O_NOFOLLOW`→`FILE_FLAG_OPEN_REPARSE_POINT`) | positioned reads at unaligned offsets on a temp file; boundary/EOF behavior; flag round-trip; O_NOFOLLOW: opening a symlink fails/opens-link per documented semantics (skip if symlink privilege absent) |
| mmap shim: anon RW / file-backed RO / unmap / discard | write-read pattern on anon map; file-backed map equals `pread` content; `MADV_DONTNEED`→`DiscardVirtualMemory` **reads back zero** after discard (documented POSIX MAP_PRIVATE semantic — a real trap); double-unmap safety |
| clocks (`CLOCK_MONOTONIC`→QPC, `CLOCK_BOOTTIME`→`QueryUnbiasedInterruptTime`) | non-decreasing over 10k samples; resolution ≥ 100 ns distinguishable; boottime ≥ monotonic; 10 ms sleep measured within [8, 200] ms; drift vs `GetTickCount64` < 1% over 500 ms |
| memory probe (`/proc/self/statm`+`mallinfo2` → `GetProcessMemoryInfo`) | working-set > 0; API JSON keys preserved with heap stats = 0 (WP7 contract — assert key presence) |
| `flock` → `LockFileEx` | second exclusive try-lock on same file fails while held, succeeds after release; whole-file semantics as used by `jit_cache.cpp:345-349`; semantic-delta doc reference (R8) |
| `setenv`/`getenv`/`unsetenv` → `_putenv_s` | round-trip, overwrite, unset; visible via `std::getenv` |
| `localtime_r` → `localtime_s` (**reversed args**, G2) | known epoch → known `tm` fields; epoch chosen so swapped arguments yield visibly wrong values (trap detector); compare against `std::localtime` for same TZ |
| `statvfs` → `GetDiskFreeSpaceExW` (G3) | free ≤ total; matches a direct Win32 call on the temp volume |
| dirent → `std::filesystem` enumeration (rdma.cpp path, G5/MJ-1) | enumerate a created temp dir with 3 named files; exact name set |
| dl shim (`LoadLibraryW`/`GetProcAddress`/`GetModuleHandleExW(FROM_ADDRESS)`) | self-module lookup by function-pointer address returns this binary's path; `dlopen` of a nonexistent name returns null cleanly; `RTLD_NOLOAD` equivalent |
| `getrlimit` no-op (G1) | `raise_fd_limit()` succeeds/no-ops on Windows without crash |
| `S_ISREG`/`S_ISDIR`/`lstat` (G4) | regular file/dir classification; `lstat` non-following on a symlink (`LSE_SKIP` with reason if symlink creation lacks privilege — Developer Mode off) |
| `mkdtemp` | 100 iterations: exists, unique, removable |
| types (`ssize_t`, `socklen_t` — MJ-3; `STDERR_FILENO`/`STDOUT_FILENO` — G10) | `static_assert` size/existence checks; constants equal 1/2 |

Registration: plain `lse_add_test(test_platform_shim)` (unconditional, all platforms). Large-file cases live in §3's separate `test_large_file_io` (label-gated) so the shim unit test stays < 5 s.

---

## 5. CI Design

### 5.1 `.github/workflows/windows.yml` (new, purely additive — WP13)

**Runner assumptions (`windows-latest`):** Windows Server 2022+ (⇒ AF_UNIX path sockets available; ≥ 1803 floor met, abstract ns absent by design), MSVC v143+ Build Tools (STL/UCRT), CMake ≥ 3.30, Ninja, NASM (ring), Python 3, git preinstalled; 4 vCPU / 16 GB / ~14 GB free `D:`. Rust installed via `dtolnay/rust-toolchain@stable` (pin `x86_64-pc-windows-msvc`).

```yaml
on:
  pull_request:
  push: { branches: [feature/windows-enablement] }
  schedule: [ { cron: '17 6 * * 1' } ]        # weekly — R3 C++26 drift + flake classification
  workflow_dispatch:
    inputs: { run_large_io: { type: boolean, default: false } }
concurrency: { group: win-${{ github.ref }}, cancel-in-progress: true }
jobs:
  windows:
    runs-on: windows-latest
    timeout-minutes: 60                         # SC7 budget < 45 min
    steps:
      - uses: actions/checkout@v4
      - run: winget install --id LLVM.LLVM --version 21.1.8 --exact   # PR6: pin exact
      - run: clang-cl --version    # assert 21.x in log
      - uses: dtolnay/rust-toolchain@stable
      - uses: Swatinem/rust-cache@v2
      - run: |  # provision reference/fastokens at the same pin as ci.yml:36
          mkdir reference
          git clone --quiet https://github.com/crusoecloud/fastokens.git reference/fastokens
          git -C reference/fastokens checkout --quiet ${{ vars.LSE_FASTOKENS_REF || '7973014e4f3a6028ac48f305704eacd64d0b4ef6' }}
      - run: cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
                   -DCMAKE_BUILD_TYPE=Release -DLSE_ENABLE_HRX=OFF    # assert "HRX backend: OFF"
      - run: ctest --test-dir build -N | tee windows-enablement/logs/ctest-N.txt   # W baseline (MJ-4)
      - run: cmake --build build 2>&1 | tee build-log.txt
      - run: ctest --test-dir build --output-on-failure --timeout 300
                   --label-exclude LARGE_IO ${{ inputs.run_large_io && '' || '' }}
      - uses: actions/upload-artifact@v4        # build-log, LastTest.log, ctest-N, fingerprint hashes
```

**Flake policy:** PR/push runs are **strict — no `--repeat`** (a retry hides flakes; a flake must fail the run and enter quarantine). The **weekly schedule** adds `--repeat until-pass:3 --repeat until-fail:0` purely to *classify* intermittent tests; results feed the quarantine process (§5.3), never the gate.

**Budget/cost estimate:** cargo build (ring+pcre2, `PCRE2_SYS_STATIC=1`) cold ≈ 12 min / cached ≈ 3 min; ~250+ C++ TUs (incl. 35 loomc/hipc-bearing test binaries) ≈ 25-30 min; ctest ≈ 5 min ⇒ **~40 min cold, ~30 min warm** (SC7 < 45 min holds with margin). Repo/fork are public ⇒ GitHub-hosted minutes are free; concurrency-cancel caps waste.

### 5.2 Linux parity job (guards upstream behavior + T3 pair)

Upstream `ci.yml` stays **untouched** (self-hosted GPU, `workflow_dispatch`-only, g++-16+HRX mandatory — `ci.yml:6-7,28,82-91`). The fork adds a `linux-parity` job (same workflow or sibling file): `ubuntu-latest` + apt.llvm.org **clang++-21** (same LLVM major as Windows — the controlled T3 pair) + Ninja + Rust; configure `-DCMAKE_CXX_COMPILER=clang++ -DLSE_ENABLE_HRX=OFF -DLSE_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off` (CI-level only; no repo flag changes — SC6); build; strict ctest; fingerprint + T1 hash artifacts. **Same-SHA requirement:** M4 counts only runs where both jobs are green on the identical commit. Optional non-gating weekly job with distro/PPA g++ to smoke the GCC path (upstream `ci.yml` remains the authoritative GCC+GPU gate).

### 5.3 KNOWN_FAILURES format, Windows rules, and flake quarantine

**Mechanism facts driving policy** (`tests/CMakeLists.txt:4-16,597-606`; `tests/harness.hpp:49-66`):
- `- \`<binary>:<case>\` — reason` → skipped at **runtime on every platform** (harness reads the file).
- `- \`<binary>\` — reason` → ctest entry `DISABLED` **at configure time on every platform**; only effective mechanism for harness-less binaries.
- **The file is platform-agnostic — there is no Windows-only field.**

Therefore:
1. **Windows-only case skips must NOT use KNOWN_FAILURES.md** (they would silently disable Linux/macOS coverage too). Preferred: in-test `#ifdef _WIN32 LSE_SKIP("windows: <capability gap> — issue #N")` — additive, self-documenting in run output, zero Linux leakage.
2. KNOWN_FAILURES.md entries are reserved for: (a) harness-less binaries broken Windows-specifically (only `test_server_shutdown` qualifies — and it gets **ported** (§6), not disabled); (b) true cross-platform flakes discovered by the weekly `--repeat` runs.
3. **K (runtime) vs X (compile) accounting** (Stage 3 BL-4): K = Windows `LSE_SKIP`-groups + KNOWN_FAILURES runtime entries, **K ≤ 5**, each with reason format `windows: <capability|flake> — <issue #N> — review-by <date ≤ 90d>`; every entry a platform-capability gap or quarantined flake, **never a product defect**. X = COMPILE_EXCLUSIONS.md ledger entries (§1.2), tracked separately, target 0.
4. **Flake quarantine process:** weekly run flags a test needing retries → (1) file issue with `LastTest.log` excerpt; (2) add skip (mechanism per rules 1-2) with `flake` reason; (3) re-enable only after 3 consecutive clean weekly runs; (4) quarantine age reviewed at each milestone gate (PR7 ownership).
5. **M4 "green 3×" evidence protocol:** three **consecutive** green runs of `windows.yml` (both jobs) on **one declared gate SHA** (post-weekly-rebase, ≤ 7 days old per §4.1), each with zero test retries; recorded in `windows-enablement/logs/m4-ci-evidence.md` as {run URL, SHA, duration, ctest summary line} × 3. Any red run resets the counter to 0. A single re-run is permitted **only** for documented infrastructure faults (runner provisioning, winget/network timeout) with evidence quoted in the log — test-level flakes are not infra faults and reset the streak.

---

## 6. Test Ports Required (minimal-diff, §4.3-compliant)

| # | File | Change | Approach |
|---|---|---|---|
| P1 | **new** `tests/compat.hpp` | `usleep`→`Sleep`, `::access`→`_access`, `::getpid`, `setenv/unsetenv`→`_putenv_s`, `ssize_t` typedef | One additive header, included from `tests/harness.hpp` under `#ifdef _WIN32` (3-line guarded edit to a shared file) — fixes **14 of 15** unistd test files without touching them (T1) |
| P2 | `tests/test_server_shutdown.cpp` | case 1 (:24-37) `#ifndef _WIN32` / `#else` `CreateProcess` self-relaunch (`--child-stuck` argv mode) + `_pipe` + `GetExitCodeProcess` (expect 1) + marker-read-empty; `#define CPPHTTPLIB_USE_POLL` (:2) → `#ifndef _WIN32` | Guarded split inside the file; cases 2-4 (:39-97) are pure std+httplib and stay shared (T2) |
| P3 | `tests/test_communication.cpp` | `pipe_url(name)` helper (§1.4); replace 11 abstract-socket sites; Windows teardown `remove`; one new case asserting `kUnimplemented` for `@`-names on Windows | Single helper + one-line site edits; mirrors `socket.cpp:129-130` precedent (T3/Q3) |
| P4 | `tests/test_api_parity.cpp:33`, `tests/test_hub.cpp:24` | `#ifndef _WIN32` on `CPPHTTPLIB_USE_POLL` | One-line guards, matches WP7's `http_server.cpp:13` treatment |
| P5 | Hardcoded `/tmp` paths | `test_graph.cpp:3155`, `test_dflash2.cpp:1610` → `std::filesystem::temp_directory_path()`; `test_tree_verify.cpp:703` → shim `mkdtemp`; `test_model.cpp:312-328` → temp-dir HOME + separator-tolerant asserts | Portable rewrites (also upstream-PR material, batch 3); **decision needed from WP5:** keep `HOME` env authoritative on Windows (testability) vs `%USERPROFILE%` only — this test asserts `hf_cache_root()` strings |
| P6 | `tests/CMakeLists.txt` | wrap `test_server`/`test_chat_protocol`/`test_api_parity`/`test_memory_lifecycle` registrations in `if(TARGET lse_tokenizer)`; add `WIN32` compile-gates only per §1.2 ledger | Additive guards around existing lines |
| P7 | `tests/test_server_cli.py` | audit only — docstring asserts "no sockets, no signals" (:2); verify `subprocess` termination uses `terminate()`/`kill()` (both map to TerminateProcess on Windows) and no `os.setsid`/`pgrep` | T7; port only if audit fails |
| P8 | **new** `tests/test_platform_shim.cpp`, `tests/test_large_file_io.cpp`, `tests/test_numerics_fingerprint.cpp`, `tests/test_fp_model.cpp` | §4, §3, §2 additions | Purely additive files — zero rebase surface |

Env-var audit (T4): `set_tests_properties(ENVIRONMENT "LSE_BACKEND=cpu;LSE_POOL=cpu…")` blocks (:142,170-172,188-189,498-499,522-523,540-541,562-563) are portable CMake; `LD_LIBRARY_PATH` blocks (:266-270,500-503,542-545) no-op on Windows (var stays unset, `LSE_ROCM_RUNTIME_DIR` never set). No changes needed. Fixture resolution uses `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` (:13-15) — portable.

---

## 7. Exit Criteria (measurable, tied to §1.3 counts)

Let **W** = Windows-registered ctest count re-captured at the gate configure (expected 92 with Rust; 83 if WP8 fails and P6 lands; committed `ctest -N` diff vs M1 baseline per MJ-4). Buckets: (a)=69, (b)=10, (d)=10, (e)-excl=3. **K** = runtime skips (§5.3), **X** = compile-exclusion ledger entries.

| Gate | Criteria (all must hold) |
|---|---|
| **M2a (libs)** | Configure+build green with `-DLSE_BUILD_TESTS=OFF` (BL-3 fix stated in the gate command); registration smoke passes (SC3); **loomc/hipc compile check:** tests-ON experimental configure compiles all 35 §1.2 binaries under clang-cl **or** COMPILE_EXCLUSIONS.md accounts for every failure (X ≤ 5, each with issue link); Linux clang++-21 parity configure+build green on same SHA. Evidence: `logs/m2a-build.txt`, `logs/ctest-N.txt`, ledger. |
| **M2b (executables)** | `lse.exe` + `lse-server.exe` link with `fastokens_ffi.lib`; SC4 demonstrated; `test_tokenizer` (bucket-e sentinel) green on Windows. Fallback: if WP8 fails, M2b deferred per Q1 with P6 landed and the (e)-dependent 9 entries excluded from W by configure, logged in `DECISIONS.md` — never silently. |
| **M3 (tests pass locally)** | (1) **All 69 bucket-(a) suites pass** — zero (a)-bucket entries in K or X, ever (an (a) failure is a product defect and blocks the gate). (2) All 10 (b) + 10 (d) suites pass with §6 ports landed. (3) **K ≤ 5**, each entry `windows: <capability gap|flake> — issue #N`, no product defects. (4) X entries each ledgered with issue link; no new X since M2a without review. (5) `test_platform_shim` green on Windows **and** Linux (pass-through parity). (6) `test_large_file_io` passes locally (mandatory; §3) incl. the >4 GiB safetensors-mmap case. (7) Numerics: T1 hashes match Linux byte-for-byte; T2 suites green; T3 fingerprint hash Windows-clang-cl == Linux-parity-clang++-21 and `test_fp_model` canary passes. (8) `test_reference` executed **with materialized `reference.safetensors`** on both platforms (§2.3) — a run without goldens is labeled incomplete. (9) Linux clang++-21 full ctest green on same SHA (SC6). Evidence: Windows + Linux `ctest --output-on-failure` logs, KNOWN_FAILURES diff, COMPILE_EXCLUSIONS.md, fingerprint artifacts, `logs/m3-large-file-io.txt`. |
| **M4 (CI green)** | §5.3 protocol: 3 consecutive green `windows.yml` runs (windows + linux-parity jobs) on one gate SHA ≤ 7 days post-rebase; duration < 45 min (SC7); weekly schedule active (R3); LARGE_IO run at least once via dispatch/schedule during the 3× window; evidence file `logs/m4-ci-evidence.md` with 3 run URLs + SHAs + durations. |

**Gate arithmetic (SC5 restated with the BL-4 correction):** pass requirement is ≥ (W − K) of W, K ≤ 5 counting **runtime** skips only; compile-gated tests reduce W itself (via configure-time exclusion + ledger X) and are *not* K entries. W is fixed at the M3 configure and diffed against M1/M2a captures.

---

*End of Stage 4 artifact. No source files modified; nothing committed. All file:line references re-derived against the working tree at `a5b82f6` on 2026-10-10.*
