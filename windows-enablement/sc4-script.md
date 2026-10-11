# SC4 Script — real end-to-end generation on Windows (A-5 / ID-C2-13)

G-C2-2 runs the commands below **verbatim** (Git Bash, repo root,
`build-win-notests` from the tests-OFF configure). Transcripts land in
`logs/m2b-acceptance.md`. Committed BEFORE the sentinel run per ID-C2-13.

## Model choice (recorded per ID-C2-13)

- **Primary fixture: `Qwen/Qwen3-0.6B`** — already in the local HF cache
  (`~/.cache/huggingface/hub/models--Qwen--Qwen3-0.6B`, ~1.2 GB fp16
  safetensors). Zero network, smallest supported-arch candidate; deviation
  from docs/WINDOWS.md §5's `mlx-community/Qwen3.5-0.8B-4bit` suggestion is
  recorded here and in 13-cycle2-implementation-log.md (budget-driven:
  avoids a ~600 MB download; choice delegated to the implementer by R-11).
- **Fallback (only if the primary cannot load/generate on the CPU backend):
  `mlx-community/Qwen3.5-0.8B-4bit`** via the pull below. Budget: ~600 MB
  network, ~1 GB disk (91 GB free at authoring), ≤10 min on a normal link.

## 1. Pull (fallback path only; primary is pre-cached)

```bash
./build-win-notests/lse.exe pull mlx-community/Qwen3.5-0.8B-4bit
```

## 2. CLI generation — fixed prompt (pass criteria below)

```bash
./build-win-notests/lse.exe --model Qwen/Qwen3-0.6B --offline \
  -n 16 -t 0 -s 0 "The capital of France is"
```

**Pass:** exit code 0 AND non-empty generated text in the transcript.
`--help`/`--version` output does NOT satisfy SC4 (ID-C2-13).

## 3. Server end-to-end — launch + HTTP transcript

```bash
./build-win-notests/lse-server.exe --model Qwen/Qwen3-0.6B --offline \
  --host 127.0.0.1 --port 8080 &
SERVER_PID=$!
sleep 5
curl.exe -s http://127.0.0.1:8080/v1/models
curl.exe -s http://127.0.0.1:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen/Qwen3-0.6B","messages":[{"role":"user","content":"Reply with the single word: Paris"}],"max_tokens":16,"temperature":0}'
kill $SERVER_PID
```

**Pass:** `/v1/models` returns JSON listing the model; `/v1/chat/completions`
returns HTTP 200 JSON with a non-empty `choices[0].message.content`.

## Budget line (ID-C2-13)

Network: 0 bytes (primary, cached) / ~600 MB (fallback pull). Disk: ~1 GB
worst case. Wall clock: < 15 min total including 16-token CPU generation.
