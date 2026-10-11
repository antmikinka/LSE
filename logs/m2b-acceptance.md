# M2b acceptance — Windows runtime milestone (G-C2-2)

Date: 2026-10-10 · Branch: `feature/windows-enablement` · Build:
`build-win-notests` (tests-OFF, clang-cl RelWithDebInfo, SYNC-1 tree).
Script of record: `windows-enablement/sc4-script.md` (committed at c73c6bb
BEFORE the runs, per ID-C2-13).

## 1. Tokenizer sentinel (bucket-e)

`ctest -R "^test_tokenizer$" -V` → **1/1 passed** (7.86 s). Transcript:
`windows-enablement/logs/` (G-C2-0 evidence commit 4e584aa).

## 2. SC4 — CLI real generation

Model: primary fixture `Qwen/Qwen3-0.6B` **rejected by upstream v0.6.1**
(`unsupported architecture; tried: qwen3.5-moe, qwen3.5, lemonseed` — plain
qwen3 is no longer a registered architecture). The script's recorded
**fallback** was invoked: `lse.exe pull mlx-community/Qwen3.5-0.8B-4bit`
→ 12 files, 621.8 MiB, `PULL_EXIT=0`.

```
./build-win-notests/lse.exe --model mlx-community/Qwen3.5-0.8B-4bit \
  --offline -n 16 -t 0 -s 0 "The capital of France is"
```

Transcript: `logs/m2b-sc4-cli.txt`. Exit **0**; generated text:
`" Paris, and the capital of the United States is Washington, D.C."`
(coherent, deterministic at t=0/s=0). Vision-tower tensors refused with the
documented text-only notice; MTP-companion notice informational.
**PASS** (exit 0 + non-empty coherent generation; not `--help`).

## 3. SC4 — server end-to-end

Transcript: `logs/m2b-sc4-server.txt`. Port deviation: 8080 → **8931**
(8080 is held by an unrelated Windows service, `AgentService.exe`, on this
machine; the first bind attempt failed cleanly and the run was redone on a
free loopback port). `/v1/models` → JSON listing the model;
`/v1/chat/completions` → HTTP 200, `choices[0].message.content == "Paris"`,
`finish_reason: stop`. Host-interpreter path (no code-generating backend on
Windows): prompt 19 tok in 137 s. **PASS**.

## 4. Product defects found and fixed in-cycle (S4 §7 rule 1 — fixed, never
ledgered; all Windows-guarded or Windows-only files, POSIX untouched)

1. **`src/core/platform_win.cpp` `path_size`** — `_wstat64`/`GetFileAttributesExW`
   reported size 0 for HuggingFace-cache symlinks (snapshots→blobs) inside
   `lse.exe`; rewritten to `CreateFileW`+`GetFileSizeEx`, which always
   resolves the target. Symptom fixed: "too small to be safetensors".
2. **`src/hub/pull.cpp` `download_blob`** — Windows refuses `rename` over an
   open handle; guarded close-before-rename after `fsync` (`#ifdef _WIN32`).
   Symptom fixed: "cannot move …incomplete into place: being used by another
   process".
3. **`CMakeLists.txt` executables link** — CPU-only builds had no edge to
   `lse_kernels` (only `lse_backend_hrx` links it), so `lse.exe`/`lse-server.exe`
   registered zero dispatch primitives and every generation died with
   `no primitive named 'gdn.silu_l2norm.v1'`. Guarded fix:
   `if(WIN32 AND NOT TARGET lse_backend_hrx) target_link_libraries(... lse_kernels)`.
   This is the exact trap upstream fixed for `test_runtime`
   (tests/CMakeLists.txt comment) but never for the executables; **it is
   platform-independent — CPU-only Linux/macOS builds have the same gap —
   flagged for M4 upstream feedback.**

## 5. G-C2-2 declaration

Tokenizer sentinel PASS · SC4 CLI PASS · SC4 server PASS · K-ledger
unchanged (K=0: all three defects were fixed, not quarantined) ·
KNOWN_FAILURES.md untouched (I-5). **G-C2-2: PASS.**
