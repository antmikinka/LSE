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

## Windows (experimental — Phase 1)

Prerequisites:

| Windows x86_64 |
| --- |
| LLVM 21+ providing `clang-cl`, with the MSVC STL/UCRT headers from Visual Studio 2022 or Build Tools |
| CMake 3.30+, Ninja |
| Rust/Cargo with the `x86_64-pc-windows-msvc` target |
| Python 3 |

The HRX backend has no Windows runtime; configure forces `LSE_ENABLE_HRX=OFF`
and the engine builds with the CPU reference backend only.

```powershell
$env:CXX = "clang-cl"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLSE_BUILD_TESTS=OFF
cmake --build build
# binaries: build\lse.exe, build\lse-server.exe
```

Do not pass a C compiler (no CC env var, no `-DCMAKE_C_COMPILER`): the project
is `LANGUAGES CXX` only. `-DLSE_BUILD_TESTS=OFF` is required for now —
targeted test builds only until WP12; the canonical Windows guide is
[`docs/WINDOWS.md`](docs/WINDOWS.md).
