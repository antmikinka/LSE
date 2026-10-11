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

**RC SHA: DECLARED 2026-10-11 = the C16 declaration commit itself**
(records-only atop the reviewed `77aa128`; A4 ordering satisfied — SYNC-2
merge C13 `bf542cf`, regression evidence C14 `b32995b`, release-protocol
record C15 `77aa128` all landed first; reviewer stage PASS-WITH-NOTES,
confirming `77aa128` as the reviewed tree). Resolve the exact SHA with
`git rev-parse` at the C16 commit; the fork ref after the RC push is
authoritative. Counting opens at the RC push run.

**Window discipline (C6, spelled out at declaration):** from the RC push
until the 3rd counted green — no pushes, no fork-side ref moves, no
dispatches while any run is in flight (dispatch only into an idle queue);
the RC tree on the fork stays byte-identical. Ledger rows for runs
concluding inside the window accumulate as LOCAL records-only commits
(they cannot cancel runs or move the fork ref) and are pushed with the M4
declaration commit.

**Window plan:** #1 = RC push run (Sun 2026-10-11); #2 = the Monday
2026-10-12 06:17 UTC `schedule` run at the RC SHA (armed via DP-B; trap C1:
if the tick is skipped because a run is in flight — or GitHub drops it — a
manual dispatch substitutes and counts identically); #3 = a dispatch into
an idle queue after #2 concludes.

## Pre-classified runs (on the board at adoption time)

| Run | SHA | Trigger | Conclusion | Counted toward 3×? | Classification |
|---|---|---|---|---|---|
| [38137423880](https://github.com/antmikinka/LSE/actions/runs/38137423880) | `5b16e94` (C9) | push | **success** (both jobs, 20 m 24 s) | **no** | Banked G-C2-5/M3 acceptance (same-SHA rule); different SHA than any RC candidate — B3 |
| [38138800778](https://github.com/antmikinka/LSE/actions/runs/38138800778) | `5b16e94` (C9) | dispatch (`run_large_io=true`) | failure (both jobs, "Main ctest (strict)") | **no** | Workflow-only false-red **D-19** (`--label-include` selects; diagnosed + recorded in 13-log §4 before the next run started) — non-countable, breaks no chain |
| [38141203856](https://github.com/antmikinka/LSE/actions/runs/38141203856) | `209d025` (C10) | push | **success** (both jobs; 14 m 28 s wall, windows job 12 m 44 s) | **no** | Rehearsal-class (D3-iii: first run after workflow-only edit C10) — non-counted regardless of outcome; conclusion filled in post-hoc, classification pre-dated |
| [38142179205](https://github.com/antmikinka/LSE/actions/runs/38142179205) | `209d025` (C10) | dispatch (`run_large_io=true`) | **success** (both jobs; 20 m 06 s wall; windows ran the FULL suite incl. the >2 GiB `test_large_file_io` — hosted-disk guard passed; linux-parity green at WL−1 incl. LARGE_IO) | **no** | Supplementary D-19 acceptance proof — **MET**; doubles as the LARGE_IO-on-hosted proof ahead of the first armed cron run |
| [38143804384](https://github.com/antmikinka/LSE/actions/runs/38143804384) | `bd31995` (C12) | push | **success** (both jobs) | **no** | Pre-RC evidence at a records-only SHA (RC not declared — A4 ordering: SYNC-2 lands first). Third consecutive green on the board (38141203856 → 38142179205 → 38143804384); recorded within 24 h per DP-A |

## Counted greens (RC window)

| # | Run URL | RC SHA | Trigger | Both jobs | Duration | Recorded (≤24 h) |
|---|---|---|---|---|---|---|
| 1 | (pending — triggered by the RC push) | C16 | push | — | — | — |
| 2 | (pending — Monday 06:17 UTC cron, or substitute dispatch per C1) | C16 | schedule | — | — | — |
| 3 | (pending — dispatch into idle queue) | C16 | dispatch | — | — | — |

*Maintenance: rows are added within 24 h of the run; the table above the
line is historical and only gains conclusion values for pending rows.*
