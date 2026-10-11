# K-LEDGER — Windows runtime quarantine ledger (K)

Machine-readable (L3). **Frozen count pattern (A-6/ID-C2-9):** data rows begin
with pipe-space-backtick; count with `K=$(grep -cE '^\| `' windows-enablement/K-LEDGER.md || true)`
and assert `K <= 5` at G-C2-4. Header/separator rows never match.

Row format (one line per entry, exact — mirrors the in-test skip string):

```
 | `binary:case-group` | windows: <capability|flake> — <one-line platform delta> | #<issue> | <review-by YYYY-MM-DD, <= 90d> |
```

(The example row above is indented on purpose: the frozen count pattern
matches any line starting pipe-space-backtick, so an unindented example would
be counted as data. Found when the first real rows landed — C2-WP12a.)

Rules (ID-6/BL-4): every entry is a platform-capability gap or quarantined
flake — never a product defect; zero bucket-(a) entries ever; each entry has an
upstream issue and a review-by date <= 90 days. K counts case groups INSIDE
passing binaries (`LSE_SKIP` keeps exit 0) — K never reduces W. Binary-level
entries (exceptional) only via the KEX generator (11 §4). KNOWN_FAILURES.md
never gains a Windows entry (I-5).

| Binary:case-group | Reason | Issue # | Review-by |
|---|---|---|---|
| `test_communication:loopback-backpressure` | windows: capability — [test_communication:loopback-backpressure] Windows loopback TCP absorbs a single-call send whole (peer later reads back every byte of a 32 MiB WSASend), so post_send completes transfers that Linux loopback would stall in the socket buffer and the still-moving / still-queued / mid-transfer-death states never exist | #1 | 2027-01-08 |
| `test_reflect:p2996` | windows: capability — [test_reflect:p2996] no pinned Windows host compiler implements P2996 (clang-cl 21.1.8 has no -freflection) and the hard-fail case below asserts the reflection capability | #2 | 2027-01-08 |

**K = 2** (2026-10-10, rows added by C2-WP12a; ledger created 2026-10-10 by C2-WP3).
Case-group coverage: row 1 skips 3 cases (`a_queued_transfer_can_be_cancelled_and_a_moving_one_cannot`,
`a_peer_that_dies_mid_transfer_completes_every_ticket_then_closes`,
`a_ticket_on_a_dead_channel_reads_as_done`); row 2 skips 1 case
(`reflection_requires_gcc16_freflection`). Both binaries still exit 0, so W is
unreduced. Issues: https://github.com/antmikinka/LSE/issues/1,
https://github.com/antmikinka/LSE/issues/2.
