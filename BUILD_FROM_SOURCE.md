# Build from source

## Prerequisites

| macOS (Apple Silicon) | Linux x86_64 |
| --- | --- |
| LLVM 21.1.8 and LLD 21.1.8 (Homebrew `llvm@21`, `lld@21`) | GCC 16 (`g++-16`) |
| Xcode command-line tools | ROCm at `/opt/rocm`, or set `ROCM_PATH` |
| CMake, Ninja, Rust/Cargo, Python 3 | CMake 3.24+, Ninja, Rust/Cargo, Python 3, clang |
| A [mac_linuxgpu](https://github.com/lemonade-sdk/mac_linuxgpu) checkout, build 267 or later | |

## macOS

```bash
brew install llvm@21 lld@21 cmake ninja rust
make -C /path/to/mac_linuxgpu hsa
HSA_RUNTIME_DIR=/path/to/mac_linuxgpu/build/hsa bash .github/scripts/build-macos.sh
# binaries: build/macos/lse-build/{lse,lse-server}; archive: dist/
```

## Linux

```bash
bash .github/scripts/build-linux-loom.sh > loom.env
export $(grep '^LSE_' loom.env)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-16 \
  -DLSE_HRX_INCLUDE_DIR="$LSE_HRX_INCLUDE_DIR" -DLSE_HRX_LIBRARY="$LSE_HRX_LIBRARY" \
  -DLSE_LOOMC_INCLUDE_DIR="$LSE_LOOMC_INCLUDE_DIR" -DLSE_LOOMC_LIBRARY="$LSE_LOOMC_LIBRARY" \
  -DLSE_LOOMC_VERSION_FILE="$LSE_LOOMC_VERSION_FILE" \
  -DCMAKE_BUILD_RPATH="$(dirname "$LSE_LOOMC_LIBRARY")"
cmake --build build
# binaries: build/lse, build/lse-server
```

## Windows

Prerequisites:

| Windows x86_64 |
| --- |
| LLVM **21.1.8** `clang-cl`, with the MSVC STL/UCRT headers from Visual Studio 2022 or Build Tools. PATH `clang-cl` from the ROCm SDK is not this pin. |
| CMake 3.30+, Ninja |
| Rust/Cargo with the `x86_64-pc-windows-msvc` target |
| Python 3 |
| AMD ROCm **7.1** (`C:\Program Files\AMD\ROCm\7.1`) so `amdhip64_7.dll` loads at run time |

`LSE_ENABLE_HRX` stays ON. The tracked library is `libhrx/`. Configure links
`build-libhrx/hrx.lib` when that import library already exists, and the build
copies `hrx.dll` and the in-tree `hsa-runtime64.dll` next to `lse.exe`. The
summary line `HSA runtime : LSE_ROCM_RUNTIME_DIR-NOTFOUND` is the Linux
soname search; the Windows DLL is the one built beside hipwin. With
`reference/hrx-system/build/hrx-install` containing `include/loomc/loomc.h`
and `lib/loomc.lib`, the 2026-10-12 `build-win` configure reports
`loomc (JIT) : ON 0.1.0`. That library is built from pin `631c0b785` and is
not vendored. `lse.exe` and `lse-server.exe` import `loomc.dll`, and
`lse --devices` lists `hrx:0` toolchains as `loom, hip`. Kernel dispatch
through HRX loads an AMDGPU code object and launches it
on the one host submission path. `hrx.dll` does not import `amdhip64`. On the
Radeon 890M, `hrx-win-smoke` ran a gfx1150 `lse_vadd` and the 1024-float
result matched. `hrx-compile-smoke` compiled that kernel with comgr through
`HrxBackend` and the 1048576-float result matched. One warm Qwen3.8-27B
4-bit generate through HRX has been measured on this GPU: a 256-token
HumanEval/0 prompt padded with the token " the", 256 generated tokens, MTP
off, CPU fallback off, warm cache. The engine printed prefill 28.05 tok/s and
decode 2.71 tok/s (device groups non-zero, host=0, fallbacks=0, compile
0.000 s). A warm 512-token run printed prefill 19.72 tok/s and decode 2.54
tok/s under the same rules. A warm 1024-token run printed prefill 13.29 tok/s
and decode 2.20 tok/s. A warm 2048-token run printed prefill 9.16 tok/s and
decode 1.72 tok/s. A 4096-token run with `--kv-len 4352` printed prefill 5.51
tok/s and decode 1.18 tok/s (compile 1.879 s, host groups 0, fallbacks 0).
32K has not been measured. `lse --devices`
listed `hrx:0 gfx1150 (Node 1)`. hipwin enumerates that same GPU. Default
order is hrx, then hipwin, then cpu.

Build `hrx.lib` first, from an MSVC x64 shell (`vcvars64.bat`). Then configure
the engine. If an older cache stored `LSE_HRX_LIBRARY` as not found, delete
that cache entry and reconfigure.

```powershell
cmake -S libhrx -B build-libhrx -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe"
cmake --build build-libhrx --target hrx

cmake -S . -B build-win-notests -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" `
  -DLSE_BUILD_TESTS=OFF
cmake --build build-win-notests --target lse
# binary: build-win-notests\lse.exe
```

Run with the ROCm 7.1 `bin` directory on `PATH` (that is where
`amdhip64_7.dll` is). A device listing is the check that HRX is up. It is not
a kernel launch. The 2026-10-12 listing used the tests-ON tree
`build-win\lse.exe`. This recipe writes `build-win-notests` so it does not
reconfigure that tree.

```powershell
$env:PATH = "C:\Program Files\AMD\ROCm\7.1\bin;" + $env:PATH
.\build-win-notests\lse.exe --devices
```

Do not pass a C compiler (no CC env var, no `-DCMAKE_C_COMPILER`): the project
is `LANGUAGES CXX` only. The full test tree builds on Windows (WP12 complete,
zero compile exclusions). That tree is `build-win` with `-DLSE_BUILD_TESTS=ON`,
then `ctest --test-dir build-win`. The canonical Windows guide is
[`docs/WINDOWS.md`](docs/WINDOWS.md).
