#!/bin/bash -e
# Run the benchmarks and write a single, self-contained HTML report.
#
# Output: benchmarks/results/report.html
#   - head-to-head BlackThorn vs BehaviorTree.CPP tables (with speedups)
#   - green/red cell backgrounds for latency comparison
#   - the full raw output embedded in a collapsible appendix
#
# No timestamped or per-engine text files are produced: the report is
# self-contained and simply overwritten on each run.

P="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$P/build/BlackThorn-Benchmark"
OUT="$P/benchmarks/results"
REPORT="$OUT/report.html"

mkdir -p "$OUT"

if [ ! -x "$BIN" ]; then
    echo "Missing $BIN - run: make benchmarks"
    exit 1
fi

# Capture the run once, print it live, then render the HTML report.
RAW="$(mktemp)"
trap 'rm -f "$RAW"' EXIT

"$BIN" | tee "$RAW"

python3 "$P/benchmarks/report.py" --input "$RAW" --output "$REPORT"

echo
echo "Report saved: $REPORT"
