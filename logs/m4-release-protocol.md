# M4 release-protocol record (DP-C) — `logs/m4-release-protocol.md`

**Definition source:** STATUS.md "M4 evidence protocol" §DP-C (ratified
2026-10-11; 15-iteration3 A3). DP-C defines the minimal RC record: no
fork-side versioning scheme is invented (versioning is upstream's call at PR
time); the RC record = `--version` transcripts of `lse.exe` + `lse-server.exe`
built at the RC SHA + the sc4-script.md generation transcript + toolchain
identities, committed as this file.

**RC SHA: NOT DECLARED by this stage** (absolute prohibition; A4 ordering —
the orchestrator declares the RC SHA and owns the push). This record captures
the release-protocol evidence at the **SYNC-2 final tree**; the artifacts here
are product-identical across the records-only commit noted in §1, so they
correspond to whatever RC SHA the orchestrator declares at this tree.

---

## 1. Tree / provenance

- **Artifacts built at the SYNC-2 merge tree `bf542cf`** (C13 = merge of
  upstream `origin/master` `61f1cf0` into pre-sync HEAD `bd31995`; N = 8,
  0 conflicts). The `lse.exe` / `lse-server.exe` exercised below are the
  `build-win-notests` (tests-OFF) artifacts from the R1 regression build at
  that tree (built 2026-10-11T06:49, clang-cl 21.1.8).
- HEAD at DP-C execution = `6f28c35` (`git describe --tags` =
  `pre-sync/2026-10-12-10-g6f28c35`). Between the artifact tree `bf542cf` and
  HEAD sits only the orchestrator's **records-only** commit `6f28c35`
  (`logs/m4-ci-evidence.md`, 1 line; touches **no product/build file**) — so
  the binaries are product-identical to a build at HEAD.
- **Project version (CMake `project()`): Lemon Seed Engine `0.6.1`**
  (configure summary line `-- Lemon Seed Engine 0.6.1`). This is the sole
  in-tree version string; there is no separate Windows/fork version.

## 2. Toolchain identities (the pinned A-4 toolchain the build trees use)

> Note: bare `clang-cl` on `PATH` resolves to **Compute-Mirrors 21.0.0git**
> (the ROCm build flagged by ID-C2-14 for its constexpr bug), and
> `C:/Program Files/LLVM` is an older **18.1.8**. BOTH build trees cache the
> PINNED compiler by absolute path
> (`CMAKE_CXX_COMPILER = C:/Users/antmi/LLVM-21.1.8/bin/clang-cl.exe`,
> `CMAKE_LINKER = .../lld-link.exe`), so all compiles/links below use the
> official **21.1.8** pin (A-4). The identities recorded here are that pinned
> toolchain, not the PATH shadows.

```
$ "C:/Users/antmi/LLVM-21.1.8/bin/clang-cl.exe" --version
clang version 21.1.8
Target: x86_64-pc-windows-msvc
Thread model: posix
InstalledDir: C:\Users\antmi\LLVM-21.1.8\bin

$ "C:/Users/antmi/LLVM-21.1.8/bin/lld-link.exe" --version
LLD 21.1.8

$ rustc -vV
rustc 1.93.1 (01f6ddf75 2026-02-11)
binary: rustc
commit-hash: 01f6ddf7588f42ae2d7eb0a2f21d44e8e96674cf
commit-date: 2026-02-11
host: x86_64-pc-windows-msvc
release: 1.93.1
LLVM version: 21.1.8

$ cargo --version
cargo 1.93.1 (083ac5135 2025-12-15)

$ cmake --version
cmake version 3.31.9

$ ninja --version
1.13.0.git.kitware.jobserver-pipe-1

Windows SDK (installed): 10.0.26100.0  (also present: 10.0.22621.0, 10.0.19041.0)
MSVC STL/UCRT: Visual Studio 2022 Community (vcvars64); clang-cl auto-detects
  the MSVC toolchain env (INCLUDE/LIB) — no manual vcvars sourcing required.
```

## 3. Artifact version transcripts (HONEST — `--version` is NOT supported)

Per DP-C "FIRST check what the binaries actually support … if unsupported,
record that honestly … NEVER invent output". Empirically, **neither artifact
implements a `--version`/`-v` flag** in v0.6.1 — both print an
"unknown option" diagnostic and exit 0:

```
$ ./build-win-notests/lse.exe --version
lse: unknown option '--version'            (exit 0)
$ ./build-win-notests/lse.exe -v
lse: unknown option '-v'                   (exit 0)
$ ./build-win-notests/lse-server.exe --version
lse-server: unknown option '--version'     (exit 0)
```

`--help` prints the usage banner (no embedded version string). The honest
version evidence in lieu of a `--version` flag is therefore:

| Evidence | Value |
|---|---|
| Project version (configure summary) | **Lemon Seed Engine 0.6.1** |
| `git describe --tags` (at DP-C execution) | `pre-sync/2026-10-12-10-g6f28c35` |
| Artifact source tree (merge commit) | `bf542cf7c493078ac04249306732b9b005f5c696` (C13) |
| `git rev-parse HEAD` (at DP-C execution) | `6f28c359bd11f69f1b1fd467914e8270fa0abafc` |

*(A `--version` flag is upstream's CLI decision, not a fork-side addition —
consistent with DP-C's "no fork-side versioning scheme invented". Recorded as
a candidate upstream enhancement at PR time, out of scope here.)*

## 4. sc4-script.md generation transcript (final tree, `build-win-notests` binaries)

Model/port deviations inherited from the G-C2-2 precedent
(`logs/m2b-sc4-*.txt`): the primary fixture `Qwen/Qwen3-0.6B` is **rejected by
v0.6.1** ("unsupported architecture; tried: qwen3.5-moe, qwen3.5, lemonseed"),
so the documented **fallback** `mlx-community/Qwen3.5-0.8B-4bit` (cached) is
used; port **8080 is held** by an unrelated Windows service (PID 6692), so the
server binds **8931**. Script intent (loopback HTTP generation transcript)
unchanged. Generation runs on the **host interpreter** (no HRX/JIT on Windows
in Phase 1) — `host_groups=944`, `host_fallbacks=0` — hence the slow prompt
processing (~147 s for 19 tokens), which is expected and is not a failure.

### 4.1 CLI generation (sc4-script.md §2)

```
$ ./build-win-notests/lse.exe --model mlx-community/Qwen3.5-0.8B-4bit \
    --offline -n 16 -t 0 -s 0 "The capital of France is"
lse: kernel cache C:\Users\antmi/.lse/cache
KV cache: bf16; attention accumulation fp32 (target/MTP paged cache)
model: qwen3.5 (24 layers, hidden 1024)
lse: refused 153 tensor(s) under 'vision_tower.' (191.9 MiB): this build decodes text only; the Qwen3.5 vision tower is not implemented, so image and video tokens cannot be encoded
lse: this checkpoint declares an MTP module but none was found beside it; pass --mtp to name one
 Paris, and the capital of the United States is Washington, D.C.
(exit 0)
```

**PASS** (sc4 §2): exit code 0 AND non-empty generated text
(" Paris, and the capital of the United States is Washington, D.C.").

### 4.2 Server end-to-end (sc4-script.md §3)

```
$ ./build-win-notests/lse-server.exe --model mlx-community/Qwen3.5-0.8B-4bit \
    --offline --host 127.0.0.1 --port 8931 &
# server log: "engine ready"; "mlx-community/Qwen3.5-0.8B-4bit on http://127.0.0.1:8931"
#             "FlashPrefillV2 inactive for this configuration; prefill=dense"  (merged upstream path)

$ curl.exe -s http://127.0.0.1:8931/v1/models
{"data":[{"context_length":4096, ... ,"id":"mlx-community/Qwen3.5-0.8B-4bit",
 "kv_cache_dtype":"bf16","kv_len":4096,"object":"model","owned_by":"lse", ...}],
 "object":"list"}

$ curl.exe -s http://127.0.0.1:8931/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{"model":"mlx-community/Qwen3.5-0.8B-4bit","messages":[{"role":"user","content":"Reply with the single word: Paris"}],"max_tokens":16,"temperature":0}'
{"choices":[{"finish_reason":"stop","index":0,"logprobs":null,
  "message":{"content":"Paris","role":"assistant"},"stop_reason":"stop_token"}],
 "created":1791727688,"id":"chatcmpl-17917276880",
 "lse_context":{"context_length":4096,"tokens_remaining":4076,"tokens_used":20},
 "model":"mlx-community/Qwen3.5-0.8B-4bit","object":"chat.completion",
 "timings":{"decode_ms":7119.4557,"generated_n":1,"host_fallbacks":0,
   "host_groups":944,"prompt_ms":146877.3926,"prompt_n":19,
   "prompt_per_second":0.129,"spec_method":"none"},
 "usage":{"completion_tokens":1,"prompt_tokens":19,"total_tokens":20}}

# server log tail:
lse-server: device memory after request: 0.53 GiB live (peak 0.71 GiB) ...
lse-server: prompt 19 in 146.88s (0.1 tok/s) | decode 0 in 7.12s (0.0 tok/s)
```

**PASS** (sc4 §3): `/v1/models` returned JSON listing the model
(`"id":"mlx-community/Qwen3.5-0.8B-4bit"`, `"object":"list"`);
`/v1/chat/completions` returned HTTP 200 JSON with non-empty
`choices[0].message.content` = **"Paris"**, `finish_reason="stop"`.

---

## 5. DP-C verdict

The release-protocol record is **complete and honest**: toolchain identities
captured (pinned clang-cl/lld-link 21.1.8, rustc/cargo 1.93.1, cmake 3.31.9,
ninja 1.13.0, WinSDK 10.0.26100.0); artifact `--version` transcripts recorded
truthfully (flag unsupported in v0.6.1 — version evidence = project 0.6.1 +
git describe + source-tree SHA, never invented); sc4-script.md generation
transcript reproduced at the final tree with both CLI (§2) and server (§3)
paths PASS. Per DP-C, "3× RC runs" = the 3 counted CI runs at the RC SHA
(they build and exercise these artifacts) + this local artifact-run record.
The RC SHA declaration and the 3× counted-green window (DP-A) remain the
orchestrator's to open after this record lands (A4 ordering).
