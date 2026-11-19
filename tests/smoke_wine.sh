#!/usr/bin/env bash
# Headless smoke test: runs every non-interactive feature through the built-in "--run=" hook under Wine + Xvfb
# and checks that each one produced output.  Requires: wine64, xvfb, a built multi-tool.exe in the repo root.
#   usage: tests/smoke_wine.sh [path/to/multi-tool.exe]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXE="${1:-$ROOT/multi-tool.exe}"
OUT="$(mktemp -d)"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-multitool}" WINEDEBUG=-all DISPLAY="${DISPLAY:-:99}"
WINE="$(command -v wine64 || echo /usr/lib/wine/wine64)"
pgrep Xvfb >/dev/null || { (Xvfb "$DISPLAY" -screen 0 1600x1000x24 >/dev/null 2>&1 &); sleep 2; }

pass=0; fail=0
run() {                                   # run <feature-page> <index> <option> <label> [text]
  local f="$OUT/f$1_$2.txt" winpath
  winpath="Z:$(echo "$f" | tr '/' '\\')"
  timeout 60 "$WINE" "$EXE" "--run=$1,$2,$3,$winpath${5:+,$5}" >/dev/null 2>&1
  if [ -s "$f" ]; then pass=$((pass+1)); printf '  ok    %s\n' "$4"; else fail=$((fail+1)); printf '  FAIL  %s\n' "$4"; fi
}
echo "System";   for i in 0 1 3 4 5 6 7 8; do run 0 $i 0 "system[$i]"; done
echo "Network";  run 1 3 2 "ping 127.0.0.1" 127.0.0.1; run 1 5 0 "dns lookup" localhost; run 1 2 0 "network info"; run 1 8 0 "wifi passwords"
echo "Cleanup";  run 2 3 0 "large files" "Z:$(echo "$ROOT/src" | tr '/' '\\')"
echo "Security"; for i in 0 1 2 3 4 5 6; do run 3 $i 0 "security[$i]"; done
echo "Tools";    run 4 4 0 "hash calculator" "Z:$(echo "$ROOT/Makefile" | tr '/' '\\')"
echo; echo "passed: $pass   failed: $fail   (outputs in $OUT)"
[ "$fail" -eq 0 ]
