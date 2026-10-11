# K-LEDGER — Windows runtime quarantine ledger (K)

Machine-readable (L3). **Frozen count pattern (A-6/ID-C2-9):** data rows begin
with pipe-space-backtick; count with `K=$(grep -cE '^\| `' windows-enablement/K-LEDGER.md || true)`
and assert `K <= 5` at G-C2-4. Header/separator rows never match.

Row format (one line per entry, exact — mirrors the in-test skip string):

```
| `binary:case-group` | windows: <capability|flake> — <one-line platform delta> | #<issue> | <review-by YYYY-MM-DD, <= 90d> |
```

Rules (ID-6/BL-4): every entry is a platform-capability gap or quarantined
flake — never a product defect; zero bucket-(a) entries ever; each entry has an
upstream issue and a review-by date <= 90 days. K counts case groups INSIDE
passing binaries (`LSE_SKIP` keeps exit 0) — K never reduces W. Binary-level
entries (exceptional) only via the KEX generator (11 §4). KNOWN_FAILURES.md
never gains a Windows entry (I-5).

| Binary:case-group | Reason | Issue # | Review-by |
|---|---|---|---|

**K = 0** (2026-10-10, created by C2-WP3).
