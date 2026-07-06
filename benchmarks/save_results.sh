#!/bin/bash -e
# Run benchmarks and save BlackThorn + BT.CPP results separately.

P="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$P/build/BlackThorn-Benchmark"
OUT="$P/benchmarks/results"
STAMP="$(date +%Y%m%d_%H%M%S)"

mkdir -p "$OUT"

if [ ! -x "$BIN" ]; then
    echo "Missing $BIN — run: make benchmarks"
    exit 1
fi

FULL="$OUT/run_${STAMP}.txt"
BT="$OUT/blackthorn_latest.txt"
BTCPP="$OUT/btcpp_latest.txt"

: > "$BT"
: > "$BTCPP"

"$BIN" | tee "$FULL" | awk -v bt="$BT" -v btcpp="$BTCPP" '
/^=== BlackThorn ===/ { s=1; next }
/^=== BehaviorTree.CPP ===/ { s=2; next }
s==1 { print >> bt }
s==2 { print >> btcpp }
'

cp "$FULL" "$OUT/latest.txt"
echo "Saved:"
echo "  $BT"
echo "  $BTCPP"
echo "  $FULL"
