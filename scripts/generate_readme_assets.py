"""Build README images under assets/readme/ (tracked in git).

Usage (from repo root):
  python scripts/generate_readme_assets.py

By default this will:
  1. Run summarize_benchmark.py on each benchmark_results*.csv found (if non-empty)
  2. Run wikirace option 2 for terminal screenshots (unless --skip-demo)
  3. Copy benchmark_*_{unweighted,weighted}.png into assets/readme/

  python scripts/generate_readme_assets.py --skip-demo --no-summarize
  python scripts/generate_readme_assets.py --demo-capture path/to/wikirace.txt
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "assets" / "readme"
OUTPUT_CHARTS = REPO / "output"
SUMMARIZE = REPO / "scripts" / "summarize_benchmark.py"

README_CHARTS = (
    "benchmark_time_ms_unweighted.png",
    "benchmark_nodes_explored_unweighted.png",
    "benchmark_time_ms_weighted.png",
    "benchmark_nodes_explored_weighted.png",
)

# Legacy summarize output names → README asset name
LEGACY_FALLBACK = {
    "benchmark_time_ms_unweighted.png": "benchmark_time_ms.png",
    "benchmark_nodes_explored_unweighted.png": "benchmark_nodes_explored.png",
}


def extract_demo_block(text: str) -> str:
    """Keep query + comparison table + shortest path from wikirace option 2 output."""
    lines = text.splitlines()
    start = None
    end = None
    for i, line in enumerate(lines):
        if line.strip() == "Query":
            start = i
        if start is not None and line.strip().startswith("===========") and i > start + 5:
            end = i
            break
    if start is None:
        return text.strip()
    chunk = lines[start:end] if end else lines[start:]
    while chunk and not chunk[-1].strip():
        chunk.pop()
    return "\n".join(chunk)


def render_terminal_png(text: str, dest: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib required: pip install matplotlib", file=sys.stderr)
        sys.exit(1)

    dest.parent.mkdir(parents=True, exist_ok=True)
    line_count = max(1, text.count("\n") + 1)
    height = min(14, 2.8 + 0.22 * line_count)
    fig, ax = plt.subplots(figsize=(11, height))
    ax.set_axis_off()
    fig.patch.set_facecolor("#0d1117")
    ax.text(
        0.02,
        0.98,
        text,
        transform=ax.transAxes,
        fontfamily="monospace",
        fontsize=10,
        color="#e6edf3",
        verticalalignment="top",
        horizontalalignment="left",
    )
    fig.savefig(dest, dpi=144, bbox_inches="tight", facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"Wrote {dest}")


def csv_has_rows(path: Path) -> bool:
    if not path.is_file():
        return False
    try:
        return path.stat().st_size > 20
    except OSError:
        return False


def discover_benchmark_csvs() -> list[Path]:
    """Prefer explicit unweighted/weighted files; avoid summarizing the same run twice."""
    found: list[Path] = []
    unweighted = REPO / "benchmark_results_unweighted.csv"
    weighted = REPO / "benchmark_results_weighted.csv"
    default = REPO / "benchmark_results.csv"

    if csv_has_rows(unweighted):
        found.append(unweighted)
    elif csv_has_rows(default):
        found.append(default)

    if csv_has_rows(weighted):
        found.append(weighted)

    return found


def run_summarize(csv_path: Path) -> None:
    print(f"Summarizing {csv_path.name}…")
    proc = subprocess.run(
        [sys.executable, str(SUMMARIZE), str(csv_path)],
        cwd=REPO,
    )
    if proc.returncode != 0:
        print(f"Warning: summarize failed for {csv_path} (exit {proc.returncode})", file=sys.stderr)


def copy_chart(name: str) -> None:
    src = OUTPUT_CHARTS / name
    if not src.is_file():
        legacy = LEGACY_FALLBACK.get(name)
        if legacy:
            alt = OUTPUT_CHARTS / legacy
            if alt.is_file():
                src = alt
                print(f"Note: using {legacy} for {name}")
    if not src.is_file():
        print(f"Skip missing chart {OUTPUT_CHARTS / name}")
        return
    dest = OUT / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    print(f"Copied {dest.name}")


def capture_wikirace_demo(exe: Path) -> str:
    if not exe.is_file():
        raise FileNotFoundError(f"Build wikirace first: {exe}")
    payload = "2\nUniversity of Florida\nPizza\n5\n"
    proc = subprocess.run(
        [str(exe)],
        input=payload,
        text=True,
        capture_output=True,
        cwd=REPO,
        timeout=180,
    )
    if proc.returncode != 0:
        print(proc.stderr, file=sys.stderr)
        raise RuntimeError(f"wikirace exited {proc.returncode}")
    return proc.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description="Refresh assets/readme for the GitHub README.")
    parser.add_argument(
        "--demo-capture",
        type=Path,
        help="Use saved wikirace stdout instead of running the binary",
    )
    parser.add_argument("--skip-demo", action="store_true", help="Skip wikirace option 2 screenshots")
    parser.add_argument(
        "--no-summarize",
        action="store_true",
        help="Do not run summarize_benchmark.py (only copy existing output/ PNGs)",
    )
    args = parser.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)

    if not args.no_summarize:
        csvs = discover_benchmark_csvs()
        if not csvs:
            print("No benchmark_results*.csv with data; skipping summarize (run wikirace option 3 first).")
        for csv_path in csvs:
            run_summarize(csv_path)

    if not args.skip_demo:
        if args.demo_capture and args.demo_capture.is_file():
            raw = args.demo_capture.read_text(encoding="utf-8", errors="replace")
        else:
            exe = REPO / "build" / "wikirace.exe"
            if sys.platform != "win32":
                exe = REPO / "build" / "wikirace"
            print("Running wikirace option 2 (University of Florida → Pizza)…")
            raw = capture_wikirace_demo(exe)
        block = extract_demo_block(raw)
        render_terminal_png(block, OUT / "compare-demo.png")

        menu_m = re.search(
            r"(={10,}.*?WikiRace Pathfinder.*?={10,}.*?Choice: )",
            raw,
            re.DOTALL,
        )
        if menu_m:
            menu = menu_m.group(1).strip() + "2"
            render_terminal_png(menu, OUT / "menu-demo.png")

    for name in README_CHARTS:
        copy_chart(name)


if __name__ == "__main__":
    main()
