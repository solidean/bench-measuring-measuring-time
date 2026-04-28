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


# ── Test renderers ──────────────────────────────────────────────────────────


def render_granularity(csv_path: Path):
    LOG10_MIN_S = -9
    LOG10_MAX_S = -1
    BINS_PER_DECADE = 12

    out_dir = csv_path.parent

    df = pd.read_csv(csv_path, keep_default_na=False)
    df["latency_ns"] = df["latency_ns"].astype(float)
    df["latency_s"] = df["latency_ns"] * 1e-9
    df = df[df["latency_s"] > 0].copy()

    method_order = list(df["method"].drop_duplicates())
    method_color = {m: df[df["method"] == m]["color"].iloc[0] for m in method_order}

    bins = np.linspace(
        LOG10_MIN_S,
        LOG10_MAX_S,
        int(round((LOG10_MAX_S - LOG10_MIN_S) * BINS_PER_DECADE)) + 1,
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
            x="latency_s",
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
    bottom_ax.set_xlim(10.0**LOG10_MIN_S, 10.0**LOG10_MAX_S)
    bottom_ax.set_xlabel("Granularity")
    bottom_ax.xaxis.set_major_locator(ticker.LogLocator(base=10, numticks=12))
    bottom_ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: fmt_seconds(v)))
    bottom_ax.xaxis.set_minor_locator(
        ticker.LogLocator(base=10, subs=tuple(range(2, 10)), numticks=100)
    )
    bottom_ax.xaxis.set_minor_formatter(ticker.NullFormatter())

    g.set_titles("")
    g.figure.suptitle(
        "Empirical clock granularity",
        color=FG,
        fontsize=14,
        y=0.995,
    )

    out = out_dir / "chart_granularity.svg"
    g.figure.savefig(out, format="svg", facecolor=BG, bbox_inches="tight")
    plt.close(g.figure)
    print(f"Saved {out.name}")


# ── Dispatcher ──────────────────────────────────────────────────────────────

RENDERERS = {
    "granularity": render_granularity,
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
