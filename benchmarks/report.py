#!/usr/bin/env python3
"""Turn raw benchmark output into a self-contained HTML report.

The benchmark executable prints one line per measurement, e.g.::

    [BlackThorn] tick.patrol                        50000     3.936        ms  (0.08 us/op)
    [BTCPP]      tick.patrol                        50000     59.731       ms  (1.19 us/op)

This script parses those lines, pairs comparable BlackThorn / BehaviorTree.CPP
metrics, and emits ``report.html`` featuring:

* a system/run header,
* head-to-head tables with green/red cell backgrounds (lower latency wins),
* per-engine sections for metrics that have no counterpart,
* a collapsible appendix with the raw output.

Usage::

    ./main-binary | python3 report.py --output results/report.html
    python3 report.py --input results/latest.txt --output results/report.html
"""

from __future__ import annotations

import argparse
import datetime as _dt
import html
import platform
import re
import sys
from dataclasses import dataclass

# ``[Engine] name    iters    total ms  (per_op us/op)``. The metric name may
# contain spaces (e.g. "reset.patrol x1000"), so it is matched non-greedily up
# to the first integer column.
_LINE_RE = re.compile(
    r"^\[(?P<engine>[^\]]+)\]\s+"
    r"(?P<name>.*?)\s+"
    r"(?P<iters>\d+)\s+"
    r"(?P<total_ms>[\d.]+)\s+ms\s+"
    r"\(\s*(?P<per_op>[\d.]+)\s+us/op\)\s*$"
)


@dataclass
class Row:
    """A single parsed benchmark measurement."""

    engine: str
    name: str
    iters: int
    total_ms: float

    @property
    def per_op_us(self) -> float:
        """Per-operation cost in microseconds, recomputed for full precision."""
        if self.iters <= 0:
            return 0.0
        return self.total_ms * 1000.0 / self.iters


@dataclass
class ReportData:
    """Parsed and grouped benchmark data ready for rendering."""

    blackthorn: dict[str, Row]
    btcpp: dict[str, Row]
    sections: dict[str, list[tuple[str, Row, Row, float]]]
    speedups: list[float]
    system: dict[str, str]
    raw_text: str


_COLOR_WIN = "win"
_COLOR_LOSE = "lose"

_HTML_STYLES = """\
:root {
  --bg: #f6f8fa;
  --card: #ffffff;
  --text: #1f2328;
  --muted: #656d76;
  --border: #d0d7de;
  --win: #d4edda;
  --lose: #f8d7da;
  --accent: #0969da;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  padding: 2rem 1.5rem 3rem;
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  font-size: 15px;
  line-height: 1.5;
  color: var(--text);
  background: var(--bg);
}
main { max-width: 1100px; margin: 0 auto; }
h1 { font-size: 1.75rem; margin: 0 0 1rem; }
h2 {
  font-size: 1.25rem;
  margin: 2rem 0 0.75rem;
  padding-bottom: 0.35rem;
  border-bottom: 1px solid var(--border);
}
.meta {
  display: grid;
  gap: 0.35rem;
  margin: 0 0 1.25rem;
  color: var(--muted);
}
.meta strong { color: var(--text); }
.summary {
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 1rem 1.15rem;
  margin-bottom: 1.5rem;
}
.summary strong { color: var(--accent); }
table {
  width: 100%;
  border-collapse: collapse;
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 0.5rem;
}
th, td {
  padding: 0.55rem 0.85rem;
  border-bottom: 1px solid var(--border);
}
th {
  background: #f3f4f6;
  font-weight: 600;
  text-align: left;
}
th.num, td.num { text-align: right; font-variant-numeric: tabular-nums; }
tr:last-child td { border-bottom: none; }
td.win { background: var(--win); }
td.lose { background: var(--lose); }
details {
  margin-top: 2rem;
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0.75rem 1rem;
}
summary {
  cursor: pointer;
  font-weight: 600;
  color: var(--accent);
}
pre {
  margin: 1rem 0 0;
  padding: 1rem;
  overflow-x: auto;
  background: #f3f4f6;
  border-radius: 6px;
  font-size: 0.82rem;
  line-height: 1.45;
}
"""


def parse(text: str) -> list[Row]:
    """Extract every measurement line from raw benchmark output."""
    rows: list[Row] = []
    for line in text.splitlines():
        match = _LINE_RE.match(line.strip())
        if match is None:
            continue
        rows.append(
            Row(
                engine=match["engine"].strip(),
                name=match["name"].strip(),
                iters=int(match["iters"]),
                total_ms=float(match["total_ms"]),
            )
        )
    return rows


def _comparison_key(name: str) -> str | None:
    """Normalise a metric name to its cross-engine key, or ``None`` to skip."""
    if name.startswith("load.full."):
        return "load." + name[len("load.full.") :]
    if name.startswith(("load.parse.", "load.build.")):
        return None
    if "visualizer" in name or "observer" in name:
        return None
    return name


def _category(key: str) -> str:
    """Group a comparison key into a report section."""
    if "blackboard" in key:
        return "Blackboard"
    if key.startswith("load."):
        return "Tree loading"
    if key.startswith("tick."):
        return "Tick + reset"
    if key.startswith("reset"):
        return "Reset"
    return "Other"


_CATEGORY_ORDER = ["Tree loading", "Tick + reset", "Blackboard", "Reset", "Other"]


def _fmt_us(value: float) -> str:
    return f"{value:.3f}"


def _fmt_speedup(ratio: float) -> str:
    return f"{ratio:.1f}x"


def _latency_classes(bt_us: float, cpp_us: float) -> tuple[str, str]:
    """Return CSS classes for BlackThorn / BT.CPP cells (lower is better)."""
    if bt_us < cpp_us:
        return _COLOR_WIN, _COLOR_LOSE
    if cpp_us < bt_us:
        return _COLOR_LOSE, _COLOR_WIN
    return "", ""


def _speedup_class(speedup: float) -> str:
    if speedup == float("inf"):
        return ""
    return _COLOR_WIN if speedup >= 1.0 else _COLOR_LOSE


def _system_info() -> dict[str, str]:
    """Machine and run-time facts."""
    now = _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    cpu = platform.processor() or platform.machine()
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("model name"):
                    cpu = line.split(":", 1)[1].strip()
                    break
    except OSError:
        pass

    return {
        "date": now,
        "cpu": cpu,
        "os": f"{platform.system()} {platform.release()}",
    }


def _collect(rows: list[Row], raw_text: str) -> ReportData:
    """Pair metrics and group them into report sections."""
    by_engine: dict[str, dict[str, Row]] = {}
    for row in rows:
        by_engine.setdefault(row.engine, {})[row.name] = row

    blackthorn = by_engine.get("BlackThorn", {})
    btcpp = by_engine.get("BTCPP", {})

    bt_by_key: dict[str, Row] = {}
    for name, row in blackthorn.items():
        key = _comparison_key(name)
        if key is not None:
            bt_by_key[key] = row

    cpp_by_key: dict[str, Row] = {}
    for name, row in btcpp.items():
        key = _comparison_key(name)
        if key is not None:
            cpp_by_key[key] = row

    sections: dict[str, list[tuple[str, Row, Row, float]]] = {}
    speedups: list[float] = []
    for key, bt_row in bt_by_key.items():
        cpp_row = cpp_by_key.get(key)
        if cpp_row is None:
            continue
        bt_us = bt_row.per_op_us
        speedup = cpp_row.per_op_us / bt_us if bt_us > 0.0 else float("inf")
        if speedup != float("inf"):
            speedups.append(speedup)
        sections.setdefault(_category(key), []).append(
            (key, bt_row, cpp_row, speedup)
        )

    return ReportData(
        blackthorn=blackthorn,
        btcpp=btcpp,
        sections=sections,
        speedups=speedups,
        system=_system_info(),
        raw_text=raw_text,
    )


def _td(text: str, css_class: str = "", numeric: bool = True) -> str:
    classes = ["num"] if numeric else []
    if css_class:
        classes.append(css_class)
    class_attr = f' class="{" ".join(classes)}"' if classes else ""
    return f"<td{class_attr}>{html.escape(text)}</td>"


def _comparison_table(entries: list[tuple[str, Row, Row, float]]) -> str:
    """Head-to-head table with green/red latency cells."""
    rows: list[str] = []
    for key, bt_row, cpp_row, speedup in entries:
        bt_us = bt_row.per_op_us
        cpp_us = cpp_row.per_op_us
        bt_cls, cpp_cls = _latency_classes(bt_us, cpp_us)
        speedup_txt = "-" if speedup == float("inf") else _fmt_speedup(speedup)
        sp_cls = _speedup_class(speedup)
        rows.append(
            "<tr>"
            f"<td>{html.escape(key)}</td>"
            f"{_td(_fmt_us(bt_us), bt_cls)}"
            f"{_td(_fmt_us(cpp_us), cpp_cls)}"
            f"{_td(speedup_txt, sp_cls)}"
            "</tr>"
        )

    return (
        "<table>"
        "<thead><tr>"
        "<th>Benchmark</th>"
        '<th class="num">BlackThorn (us/op)</th>'
        '<th class="num">BT.CPP (us/op)</th>'
        '<th class="num">Speedup</th>'
        "</tr></thead>"
        f"<tbody>{''.join(rows)}</tbody>"
        "</table>"
    )


def _engine_only_table(extras: list[Row]) -> str:
    rows = "".join(
        "<tr>"
        f"<td>{html.escape(row.name)}</td>"
        f"{_td(str(row.iters))}"
        f"{_td(f'{row.total_ms:.3f}')}"
        f"{_td(_fmt_us(row.per_op_us))}"
        "</tr>"
        for row in extras
    )
    return (
        "<table>"
        "<thead><tr>"
        "<th>Benchmark</th>"
        '<th class="num">Iterations</th>'
        '<th class="num">Total (ms)</th>'
        '<th class="num">us/op</th>'
        "</tr></thead>"
        f"<tbody>{rows}</tbody>"
        "</table>"
    )


def _engine_only_extras(
    engine_rows: dict[str, Row], compared: dict[str, Row]
) -> list[Row]:
    compared_rows = {id(row) for row in compared.values()}
    return [row for row in engine_rows.values() if id(row) not in compared_rows]


def build_html(data: ReportData) -> str:
    """Render the full HTML report."""
    body: list[str] = []

    body.append("<h1>BlackThorn vs BehaviorTree.CPP &mdash; Benchmark report</h1>")
    body.append('<div class="meta">')
    body.append(f'<div><strong>Date:</strong> {html.escape(data.system["date"])}</div>')
    body.append(f'<div><strong>CPU:</strong> {html.escape(data.system["cpu"])}</div>')
    body.append(f'<div><strong>OS:</strong> {html.escape(data.system["os"])}</div>')
    body.append("</div>")

    if data.speedups:
        avg = sum(data.speedups) / len(data.speedups)
        best = max(data.speedups)
        body.append(
            '<p class="summary">'
            f"Across <strong>{len(data.speedups)}</strong> comparable benchmarks, "
            f"BlackThorn is on average <strong>{avg:.1f}x</strong> faster than "
            f"BehaviorTree.CPP (best case <strong>{best:.1f}x</strong>). "
            "Speedup = BT.CPP time / BlackThorn time; higher is better. "
            "<span class=\"win\" style=\"padding:0 0.35rem\">Green</span> = faster, "
            "<span class=\"lose\" style=\"padding:0 0.35rem\">red</span> = slower."
            "</p>"
        )

    for category in _CATEGORY_ORDER:
        entries = data.sections.get(category)
        if not entries:
            continue
        body.append(f"<h2>{html.escape(category)}</h2>")
        body.append(_comparison_table(entries))

    bt_by_key = {
        key: row
        for name, row in data.blackthorn.items()
        if (key := _comparison_key(name)) is not None
    }
    cpp_by_key = {
        key: row
        for name, row in data.btcpp.items()
        if (key := _comparison_key(name)) is not None
    }

    bt_extras = _engine_only_extras(data.blackthorn, bt_by_key)
    if bt_extras:
        body.append("<h2>BlackThorn-only metrics</h2>")
        body.append(_engine_only_table(bt_extras))

    cpp_extras = _engine_only_extras(data.btcpp, cpp_by_key)
    if cpp_extras:
        body.append("<h2>BehaviorTree.CPP-only metrics</h2>")
        body.append(_engine_only_table(cpp_extras))

    body.append("<details>")
    body.append("<summary>Raw benchmark output</summary>")
    body.append(f"<pre>{html.escape(data.raw_text.rstrip())}</pre>")
    body.append("</details>")

    return (
        "<!DOCTYPE html>\n"
        '<html lang="en">\n'
        "<head>\n"
        '<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "<title>BlackThorn Benchmark Report</title>\n"
        f"<style>{_HTML_STYLES}</style>\n"
        "</head>\n"
        "<body>\n"
        "<main>\n"
        f"{''.join(body)}\n"
        "</main>\n"
        "</body>\n"
        "</html>\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        help="Raw benchmark output file (defaults to stdin).",
    )
    parser.add_argument(
        "--output",
        help="HTML report path (defaults to stdout).",
    )
    args = parser.parse_args()

    if args.input:
        with open(args.input, encoding="utf-8") as handle:
            raw_text = handle.read()
    else:
        raw_text = sys.stdin.read()

    report = build_html(_collect(parse(raw_text), raw_text))

    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(report)
    else:
        sys.stdout.write(report)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
