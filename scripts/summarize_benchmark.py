#!/usr/bin/env python3
"""Summarize benchmark_results.csv and write charts for the report.

Usage (from repo root):
  python scripts/summarize_benchmark.py
  python scripts/summarize_benchmark.py benchmark_results.csv

Requires: pip install matplotlib
Outputs (tag derived from the CSV name or Weighted column):
  benchmark_summary.csv or benchmark_summary_weighted.csv, etc.
  output/benchmark_time_ms_unweighted.png (and _weighted, etc.)
  output/benchmark_nodes_explored_<tag>.png
  output/benchmark_path_length_<tag>.png
"""

from __future__ import annotations

import csv
import math
import sys
from collections import defaultdict
from pathlib import Path


def parse_float(value: str) -> float | None:
    value = value.strip()
    if not value or value.lower() in {"inf", "nan", "n/a"}:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def summarize(rows: list[dict[str, str]]) -> dict[str, dict[str, float]]:
    totals: dict[str, dict[str, float]] = defaultdict(
        lambda: {
            "count": 0.0,
            "reachable": 0.0,
            "sum_path_length": 0.0,
            "sum_nodes": 0.0,
            "sum_time_ms": 0.0,
            "sum_memory_mb": 0.0,
            "memory_samples": 0.0,
        }
    )
    for row in rows:
        algo = row.get("Algorithm", "").strip()
        if not algo:
            continue
        bucket = totals[algo]
        bucket["count"] += 1.0
        length = parse_float(row.get("PathLength", ""))
        if length is not None:
            bucket["reachable"] += 1.0
            bucket["sum_path_length"] += length
        bucket["sum_nodes"] += float(row.get("NodesExplored", 0) or 0)
        bucket["sum_time_ms"] += float(row.get("TimeMS", 0) or 0)
        memory = parse_float(row.get("MemoryMB", ""))
        if memory is not None and memory > 0:
            bucket["sum_memory_mb"] += memory
            bucket["memory_samples"] += 1.0
    return totals


def write_summary_csv(totals: dict[str, dict[str, float]], out_path: Path) -> None:
    with out_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "Algorithm",
                "Runs",
                "Reachable",
                "AvgPathLength",
                "AvgNodesExplored",
                "AvgTimeMS",
                "AvgMemoryMB",
            ]
        )
        for algo in sorted(totals):
            bucket = totals[algo]
            runs = bucket["count"]
            reachable = bucket["reachable"]
            writer.writerow(
                [
                    algo,
                    int(runs),
                    int(reachable),
                    f"{bucket['sum_path_length'] / reachable:.4f}" if reachable else "",
                    f"{bucket['sum_nodes'] / runs:.2f}",
                    f"{bucket['sum_time_ms'] / runs:.2f}",
                    f"{bucket['sum_memory_mb'] / bucket['memory_samples']:.2f}"
                    if bucket["memory_samples"]
                    else "",
                ]
            )


def print_summary_table(totals: dict[str, dict[str, float]]) -> None:
    print("\nAverage over benchmark runs\n")
    print(f"{'Algorithm':<16} {'Avg path':>10} {'Avg nodes':>12} {'Avg ms':>10} {'Avg MB':>8}")
    print("-" * 60)
    order = ["BFS", "Bidirectional", "Dijkstra", "Bellman-Ford", "DFS"]
    names = order + [name for name in sorted(totals) if name not in order]
    for algo in names:
        if algo not in totals:
            continue
        bucket = totals[algo]
        runs = bucket["count"]
        reachable = bucket["reachable"]
        avg_len = bucket["sum_path_length"] / reachable if reachable else float("nan")
        avg_nodes = bucket["sum_nodes"] / runs
        avg_ms = bucket["sum_time_ms"] / runs
        avg_mb = (
            bucket["sum_memory_mb"] / bucket["memory_samples"] if bucket["memory_samples"] else float("nan")
        )
        print(
            f"{algo:<16} "
            f"{(f'{avg_len:.2f}' if not math.isnan(avg_len) else 'N/A'):>10} "
            f"{avg_nodes:>12,.0f} "
            f"{avg_ms:>10,.0f} "
            f"{(f'{avg_mb:.0f}' if not math.isnan(avg_mb) else 'n/a'):>8}"
        )


def chart_tag_for(csv_path: Path, rows: list[dict[str, str]]) -> str:
    """Suffix for PNG names: _unweighted or _weighted."""
    name = csv_path.name.lower()
    if "unweighted" in name:
        return "_unweighted"
    if "weighted" in name:
        return "_weighted"
    if rows:
        flag = rows[0].get("Weighted", "").strip().lower()
        if flag == "true":
            return "_weighted"
        if flag == "false":
            return "_unweighted"
    return "_unweighted"


def summary_csv_path(csv_path: Path) -> Path:
    stem = csv_path.stem
    if stem.startswith("benchmark_results"):
        suffix = stem.removeprefix("benchmark_results")
        return csv_path.with_name(f"benchmark_summary{suffix}.csv")
    return csv_path.with_name("benchmark_summary.csv")


def make_charts(
    totals: dict[str, dict[str, float]], out_dir: Path, chart_tag: str
) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed; skipping charts (pip install matplotlib)")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    order = ["BFS", "Bidirectional", "Dijkstra", "Bellman-Ford", "DFS"]
    labels = [name for name in order if name in totals]
    labels += [name for name in sorted(totals) if name not in labels]

    def series(key: str) -> list[float]:
        values = []
        for algo in labels:
            bucket = totals[algo]
            runs = bucket["count"]
            if key == "path":
                reachable = bucket["reachable"]
                values.append(bucket["sum_path_length"] / reachable if reachable else 0.0)
            elif key == "nodes":
                values.append(bucket["sum_nodes"] / runs)
            elif key == "time":
                values.append(bucket["sum_time_ms"] / runs)
            else:
                values.append(
                    bucket["sum_memory_mb"] / bucket["memory_samples"]
                    if bucket["memory_samples"]
                    else 0.0
                )
        return values

    specs = [
        ("time", "Average time (ms)", f"benchmark_time_ms{chart_tag}.png"),
        ("nodes", "Average nodes explored", f"benchmark_nodes_explored{chart_tag}.png"),
        ("path", "Average path length", f"benchmark_path_length{chart_tag}.png"),
    ]
    for key, ylabel, filename in specs:
        plt.figure(figsize=(8, 4.5))
        plt.bar(labels, series(key), color="#2b6cb0")
        plt.ylabel(ylabel)
        plt.title(f"WikiRace benchmark — {ylabel}")
        plt.xticks(rotation=20, ha="right")
        plt.tight_layout()
        plt.savefig(out_dir / filename, dpi=150)
        plt.close()
        print(f"Wrote {out_dir / filename}")


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else repo_root / "benchmark_results.csv"
    if not csv_path.is_file():
        print(f"Missing {csv_path}. Run wikirace option 3 first.")
        return 1
    rows = load_rows(csv_path)
    if not rows:
        print(f"{csv_path} is empty.")
        return 1
    totals = summarize(rows)
    tag = chart_tag_for(csv_path, rows)
    summary_path = summary_csv_path(csv_path)
    write_summary_csv(totals, summary_path)
    print(f"Wrote {summary_path}")
    print_summary_table(totals)
    make_charts(totals, repo_root / "output", tag)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
