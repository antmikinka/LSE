#!/usr/bin/env bash
# ID-3(c): generate the authoritative COMPILE_EXPOSURE set programmatically.
#
# Scan tests/CMakeLists.txt for every `target_sources(<t> ...)` block that
# references src/backends/hrx. On Windows HRX is forced OFF, so each such test
# compiles the loomc/hipc translation units directly and must be built by the
# targeted M2a gate. Two shapes occur:
#   * direct:  target_sources(test_foo PRIVATE .../hrx/...)
#   * foreach: foreach(target a b c) target_sources(${target} PRIVATE .../hrx/...)
# The foreach loop variable is expanded to its ITEMS list.
#
# Minus HRX-gated recipients: a target_sources that only exists under a
# POSITIVE `if(TARGET lse_backend_hrx)`/`if(LSE_ENABLE_HRX)` never builds on
# Windows. The sole such recipient is test_matrix_probe_lifecycle (it links the
# hrx backend and adds hrx/kernels/probe.cpp only when the backend exists), so
# it is subtracted here — 36 raw recipients - 1 = the corrected 35 of ID-3.
set -euo pipefail
cd "$(dirname "$0")/.."

awk '
  # Remember the item list of the most recent foreach(<var> ...). CMake has two
  # forms: `foreach(v IN ITEMS a b c)` and the older `foreach(v a b c)`; both
  # appear here, so strip an optional leading "IN ITEMS" before collecting.
  # The header may wrap across lines (the loomc foreach does), so accumulate
  # until the closing paren before parsing.
  function parse_foreach(buf,   n,tok,start,i) {
    sub(/\).*/, "", buf)
    n=split(buf, tok, /[ \t]+/); foreach_items=""; start=2
    if (tok[2]=="IN" && tok[3]=="ITEMS") start=4
    for(i=start;i<=n;i++) if(tok[i]!="") foreach_items=foreach_items tok[i] "\n"
  }
  match($0, /foreach\(/) {
    line=$0; sub(/.*foreach\(/, "", line)
    if (line ~ /\)/) { parse_foreach(line); next }
    inforeach=1; foreach_buf=line; next
  }
  inforeach {
    foreach_buf = foreach_buf " " $0
    if ($0 ~ /\)/) { inforeach=0; parse_foreach(foreach_buf) }
    next
  }
  # Start of a target_sources(<name> ...) block.
  match($0, /target_sources\(/) {
    line=$0; sub(/.*target_sources\(/, "", line)
    split(line, a, /[ \t]+/); tname=a[1]
    inblock=1; buf=$0
    if ($0 ~ /\)[ \t]*$/) { emit(tname, buf); inblock=0; buf="" }
    next
  }
  inblock {
    buf=buf "\n" $0
    if ($0 ~ /\)/) { emit(tname, buf); inblock=0; buf="" }
    next
  }
  function emit(name, body,   i,arr,m) {
    if (body !~ /src\/backends\/hrx/) return
    if (name ~ /^\$\{/) {            # foreach loop variable -> expand ITEMS
      m=split(foreach_items, arr, "\n")
      for(i=1;i<=m;i++) if(arr[i]!="") print arr[i]
    } else {
      print name
    }
  }
' tests/CMakeLists.txt \
  | sort -u \
  | grep -vx 'test_matrix_probe_lifecycle'   # the single HRX-gated recipient
