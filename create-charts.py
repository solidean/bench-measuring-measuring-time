#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pandas", "matplotlib", "seaborn", "numpy"]
# ///

import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# Dark mode
BG = "#121622"
FG = "#e0e0e0"
GRID = "#30333e"
ROW_LABEL = "#f0f0f0"

sns.set_theme(
    style="white",
    rc={
        "figure.facecolor": BG,
        "axes.facecolor": BG,
        "savefig.facecolor": BG,
        "text.color": FG,
        "axes.labelcolor": FG,
        "xtick.color": FG,
        "ytick.color": FG,
        "axes.edgecolor": GRID,
    },
)


def fmt_seconds(v: float) -> str:
    if v <= 0 or not np.isfinite(v):
        return ""
    if v < 1e-6:
        return f"{v * 1e9:g} ns"
    if v < 1e-3:
        return f"{v * 1e6:g} μs"
    if v < 1:
        return f"{v * 1e3:g} ms"
    return f"{v:g} s"


def fmt_count(v: float) -> str:
    if v <= 0 or not np.isfinite(v):
        return ""
    if v < 1000:
        return f"{v:g}"
    if v < 1e6:
        return f"{v / 1e3:g}k"
    if v < 1e9:
        return f"{v / 1e6:g}M"
    return f"{v / 1e9:g}G"


# ── Test renderers ──────────────────────────────────────────────────────────


def _render_ridge(
    df: pd.DataFrame,
    out_dir: Path,
    *,
    title: str,
    out_name: str,
    value_col: str,
    log10_min: float,
    log10_max: float,
    xlabel: str,
    fmt_value,
    bins_per_decade: int = 12,
):
    df = df.copy()
    df[value_col] = df[value_col].astype(float)
    df = df[df[value_col] > 0].copy()

    method_order = list(df["method"].drop_duplicates())
    method_color = {m: df[df["method"] == m]["color"].iloc[0] for m in method_order}

    bins = np.linspace(
        log10_min,
        log10_max,
        int(round((log10_max - log10_min) * bins_per_decade)) + 1,
    )

    g = sns.FacetGrid(
        df,
        row="method",
        row_order=method_order,
        aspect=9,
        height=1.2,
        sharex=True,
        sharey=False,
    )

    def plot_hist(data, **_):
        m = data["method"].iloc[0]
        sns.histplot(
            data=data,
            x=value_col,
            bins=bins,
            log_scale=(True, False),
            stat="density",
            element="step",
            fill=True,
            alpha=0.75,
            linewidth=1.3,
            color=method_color[m],
        )

    g.map_dataframe(plot_hist)

    def add_row_label(values, **_):
        ax = plt.gca()
        ax.text(
            0.005,
            0.3,
            values.iloc[0],
            fontweight="bold",
            color=ROW_LABEL,
            ha="left",
            va="center",
            transform=ax.transAxes,
            fontsize=10,
            bbox=dict(facecolor=BG, edgecolor="none", alpha=0.7, pad=2),
        )

    g.map(add_row_label, "method")

    for ax in g.axes.flat:
        ax.set_facecolor((0, 0, 0, 0))
        ax.set_yticks([])
        ax.set_ylabel("")
        for spine in ("left", "right", "top"):
            ax.spines[spine].set_visible(False)
        ax.spines["bottom"].set_color(GRID)
        ax.grid(True, which="major", axis="x", color=GRID, linewidth=0.5)
        ax.set_axisbelow(True)

    g.figure.subplots_adjust(hspace=-0.15)

    bottom_ax = g.axes[-1][0]
    bottom_ax.set_xlim(10.0**log10_min, 10.0**log10_max)
    bottom_ax.set_xlabel(xlabel)
    bottom_ax.xaxis.set_major_locator(ticker.LogLocator(base=10, numticks=12))
    bottom_ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: fmt_value(v)))
    bottom_ax.xaxis.set_minor_locator(
        ticker.LogLocator(base=10, subs=tuple(range(2, 10)), numticks=100)
    )
    bottom_ax.xaxis.set_minor_formatter(ticker.NullFormatter())

    g.set_titles("")
    g.figure.suptitle(
        title,
        color=FG,
        fontsize=14,
        y=0.995,
    )

    out = out_dir / out_name
    g.figure.savefig(out, format="svg", facecolor=BG, bbox_inches="tight")
    plt.close(g.figure)
    print(f"Saved {out.name}")


def render_granularity(csv_path: Path):
    df = pd.read_csv(csv_path, keep_default_na=False)
    df["latency_s"] = df["latency_ns"].astype(float) * 1e-9
    _render_ridge(
        df,
        csv_path.parent,
        title="Empirical clock granularity",
        out_name="chart_granularity.svg",
        value_col="latency_s",
        log10_min=-9,
        log10_max=-1,
        xlabel="Granularity",
        fmt_value=fmt_seconds,
    )


def render_granularity_rdtsc(csv_path: Path):
    df = pd.read_csv(csv_path, keep_default_na=False)
    df["latency_s"] = df["latency_ns"].astype(float) * 1e-9
    _render_ridge(
        df,
        csv_path.parent,
        title="Clock granularity (measured by rdtsc)",
        out_name="chart_granularity_rdtsc.svg",
        value_col="latency_s",
        log10_min=-9,
        log10_max=-1,
        xlabel="Granularity",
        fmt_value=fmt_seconds,
    )


def render_calls_per_change(csv_path: Path):
    df = pd.read_csv(csv_path, keep_default_na=False)
    _render_ridge(
        df,
        csv_path.parent,
        title="Calls until clock value changes",
        out_name="chart_calls_per_change.svg",
        value_col="calls",
        log10_min=0,
        log10_max=8,
        xlabel="Calls per change",
        fmt_value=fmt_count,
    )


def _draw_step(csv_path: Path, *, instr_name: str, out_name: str, unroll: int):
    out_dir = csv_path.parent
    df = pd.read_csv(csv_path, keep_default_na=False)
    cycles = df["cycles"].astype(float).to_numpy()

    upper = 100.0
    bins = np.arange(0, upper + 1)

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.hist(
        cycles,
        bins=bins,
        density=True,
        color="#ec407a",
        edgecolor=BG,
        linewidth=0.5,
    )

    for spine in ("right", "top"):
        ax.spines[spine].set_visible(False)
    ax.spines["left"].set_color(GRID)
    ax.spines["bottom"].set_color(GRID)
    ax.grid(True, axis="y", color=GRID, linewidth=0.5)
    ax.set_axisbelow(True)
    ax.set_xlim(0, upper)
    ax.set_xlabel(f"Cycles between consecutive {instr_name} reads")
    ax.set_ylabel("Relative frequency")
    ax.set_title(f"{instr_name} step distribution ({unroll}-unrolled, {unroll - 1} deltas per iter)")

    fig.tight_layout()
    out = out_dir / out_name
    fig.savefig(out, format="svg", facecolor=BG, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved {out.name}")


def _draw_step_mt(csv_path: Path, *, instr_name: str, out_name: str, unroll: int):
    out_dir = csv_path.parent
    df = pd.read_csv(csv_path, keep_default_na=False)

    if df.empty:
        print(f"{csv_path.name} has no data — skipping (likely empty shared interval)")
        return

    upper = 100.0
    bins = np.arange(0, upper + 1)

    tight = df[df["tight"] == 1]["diff"].astype(float).to_numpy()
    wrap = df[df["tight"] == 0]["diff"].astype(float).to_numpy()

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.hist(
        tight,
        bins=bins,
        density=True,
        color="#ec407a",
        edgecolor=BG,
        linewidth=0.5,
        alpha=0.65,
        label=f"tight {instr_name} (within {unroll}-unroll)",
    )
    ax.hist(
        wrap,
        bins=bins,
        density=True,
        color="#42a5f5",
        edgecolor=BG,
        linewidth=0.5,
        alpha=0.65,
        label=f"loop wraparound (between {unroll}-unrolls)",
    )

    for spine in ("right", "top"):
        ax.spines[spine].set_visible(False)
    ax.spines["left"].set_color(GRID)
    ax.spines["bottom"].set_color(GRID)
    ax.grid(True, axis="y", color=GRID, linewidth=0.5)
    ax.set_axisbelow(True)
    ax.set_xlim(0, upper)
    ax.set_xlabel(f"Cycles between consecutive {instr_name} reads")
    ax.set_ylabel("Relative frequency")
    ax.set_title(f"{instr_name} step distribution (multithreaded, shared interval)")
    ax.legend(facecolor=BG, edgecolor=GRID, labelcolor=FG)

    fig.tight_layout()
    out = out_dir / out_name
    fig.savefig(out, format="svg", facecolor=BG, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved {out.name}")


def render_rdtsc_step(csv_path: Path):
    _draw_step(csv_path, instr_name="rdtsc", out_name="chart_rdtsc_step.svg", unroll=8)


def render_rdtscp_step(csv_path: Path):
    _draw_step(csv_path, instr_name="rdtscp", out_name="chart_rdtscp_step.svg", unroll=4)


def render_rdtsc_step_mt(csv_path: Path):
    _draw_step_mt(csv_path, instr_name="rdtsc", out_name="chart_rdtsc_step_mt.svg", unroll=8)


def render_rdtscp_step_mt(csv_path: Path):
    _draw_step_mt(csv_path, instr_name="rdtscp", out_name="chart_rdtscp_step_mt.svg", unroll=4)


# ── Dispatcher ──────────────────────────────────────────────────────────────

RENDERERS = {
    "granularity": render_granularity,
    "granularity_rdtsc": render_granularity_rdtsc,
    "calls_per_change": render_calls_per_change,
    "rdtsc_step": render_rdtsc_step,
    "rdtsc_step_mt": render_rdtsc_step_mt,
    "rdtscp_step": render_rdtscp_step,
    "rdtscp_step_mt": render_rdtscp_step_mt,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dir", nargs="?", default=".", help="Directory containing result_*.csv")
    args = parser.parse_args()

    target = Path(args.dir)
    out_dir = target.parent if target.is_file() else target

    found = False
    for csv in sorted(out_dir.glob("result_*.csv")):
        suffix = csv.stem.removeprefix("result_")
        renderer = RENDERERS.get(suffix)
        if renderer is None:
            print(f"no renderer for {csv.name} — skipping")
            continue
        renderer(csv)
        found = True

    if not found:
        print(f"no result_*.csv files found in {out_dir.resolve()}")


if __name__ == "__main__":
    main()
