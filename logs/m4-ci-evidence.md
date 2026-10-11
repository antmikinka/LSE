# M4 CI evidence ledger (living — created 2026-10-11 at C11)

**Counting rule (DP-A / 15-iteration3 B2, adopted BEFORE the conclusion of
run 38141203856 was observed):** a *counted green* = one `windows.yml` run,
any trigger (push / schedule / dispatch), BOTH jobs strictly green
(cardinality asserts included) at the run's SHA, duration < 45 min, recorded
here (URL + SHA + trigger + duration) within 24 h. **M4 = 3 counted greens
on 3 consecutive runs at ONE declared RC SHA.** A failed run pre-classified
as a workflow-only false-red (zero product/test failures, D-numbered,
diagnosis recorded before the next run starts) is non-countable — it neither
adds a green nor breaks the chain. The first run after any workflow-only
edit is rehearsal-class (non-counted regardless of outcome).

**Window rule (C6):** from RC declaration until the 3rd counted green — no
commits, and no push/dispatch while a run is in flight (the concurrency
group `win-${{ github.ref }}` cancels the evidence run).

**RC SHA: NOT YET DECLARED** (A4 ordering: SYNC-2 merge + release-protocol
commit(s) land first; counting opens at the RC push run).

## Pre-classified runs (on the board at adoption time)

| Run | SHA | Trigger | Conclusion | Counted toward 3×? | Classification |
|---|---|---|---|---|---|
| [38137423880](https://github.com/antmikinka/LSE/actions/runs/38137423880) | `5b16e94` (C9) | push | **success** (both jobs, 20 m 24 s) | **no** | Banked G-C2-5/M3 acceptance (same-SHA rule); different SHA than any RC candidate — B3 |
| [38138800778](https://github.com/antmikinka/LSE/actions/runs/38138800778) | `5b16e94` (C9) | dispatch (`run_large_io=true`) | failure (both jobs, "Main ctest (strict)") | **no** | Workflow-only false-red **D-19** (`--label-include` selects; diagnosed + recorded in 13-log §4 before the next run started) — non-countable, breaks no chain |
| [38141203856](https://github.com/antmikinka/LSE/actions/runs/38141203856) | `209d025` (C10) | push | **success** (both jobs; 14 m 28 s wall, windows job 12 m 44 s) | **no** | Rehearsal-class (D3-iii: first run after workflow-only edit C10) — non-counted regardless of outcome; conclusion filled in post-hoc, classification pre-dated |
| [38142179205](https://github.com/antmikinka/LSE/actions/runs/38142179205) | `209d025` (C10) | dispatch (`run_large_io=true`) | **success** (both jobs; 20 m 06 s wall; windows ran the FULL suite incl. the >2 GiB `test_large_file_io` — hosted-disk guard passed; linux-parity green at WL−1 incl. LARGE_IO) | **no** | Supplementary D-19 acceptance proof — **MET**; doubles as the LARGE_IO-on-hosted proof ahead of the first armed cron run |

## Counted greens (RC window)

| # | Run URL | RC SHA | Trigger | Both jobs | Duration | Recorded (≤24 h) |
|---|---|---|---|---|---|---|
| 1 | — | — | — | — | — | — |
| 2 | — | — | — | — | — | — |
| 3 | — | — | — | — | — | — |

*Maintenance: rows are added within 24 h of the run; the table above the
line is historical and only gains conclusion values for pending rows.*
