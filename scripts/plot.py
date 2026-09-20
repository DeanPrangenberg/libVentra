#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Iterable

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


# -----------------------------------------------------------------------------
# Labels / ordering
# -----------------------------------------------------------------------------

MEMORY_STRATEGIES = [
    "new/delete",
    "std::allocator",
    "mmap",
]

DEQUE_IMPLEMENTATIONS = [
    "new/delete",
    "std::allocator",
    "mmap",
    "std::deque + mutex",
]

IMPLEMENTATION_LABELS = {
    "New": "new/delete",
    "Allocator": "std::allocator",
    "Mmap": "mmap",
    "LockBasedStdDeque": "std::deque + mutex",
}

WORKLOAD_TITLES = {
    "ConstructDestroy": "Construction / destruction",
    "AllocateReleaseUnpublished": "Allocate / release unpublished",
    "AllocateRetireReclaim": "Allocate / retire / reclaim",
    "PushBackPopFront": "Single-thread push_back / pop_front",
    "PushFrontPopBack": "Single-thread push_front / pop_back",
    "ConcurrentPhasedPushBackPopFront": "Phased concurrent push_back / pop_front",
    "ConcurrentSteadyPushBackPopFront": "Steady-state concurrent push_back / pop_front",
    "ConcurrentSteadyPushFrontPopBack": "Steady-state concurrent push_front / pop_back",
    "BarrierOverhead": "Barrier overhead",
}

TIME_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}

OP_RE = re.compile(
    r"/(?P<kind>operations|total_operations|operations_per_thread):(?P<count>\d+)"
)
THREAD_RE = re.compile(r"/threads:(?P<threads>\d+)")

ARENA_RE = re.compile(
    r"^Arena_ConcurrentDeque(?P<impl>New|Allocator|Mmap)_"
    r"(?P<workload>[A-Za-z0-9]+)"
)

CONCURRENT_DEQUE_RE = re.compile(
    r"^Deque_ConcurrentDeque(?P<impl>New|Allocator|Mmap)_"
    r"(?P<workload>[A-Za-z0-9]+)"
)

LOCK_BASED_RE = re.compile(
    r"^Deque_LockBasedStdDeque_(?P<workload>[A-Za-z0-9]+)"
)


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Plot libVentra Google Benchmark JSON output. "
            "Produces PNG plots and CSV tables only."
        )
    )
    parser.add_argument(
        "json",
        nargs="+",
        type=Path,
        help="Google Benchmark JSON file(s)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("plots"),
        help="Output directory (default: plots)",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=220,
        help="PNG resolution (default: 220)",
    )
    parser.add_argument(
        "--bootstrap-samples",
        type=int,
        default=10_000,
        help="Bootstrap samples for median 95%% confidence intervals (default: 10000)",
    )
    return parser.parse_args()


# -----------------------------------------------------------------------------
# Parsing
# -----------------------------------------------------------------------------

def benchmark_name(entry: dict) -> str:
    return str(entry.get("run_name", entry.get("name", "")))


def parse_threads(entry: dict, name: str) -> int:
    if entry.get("threads") is not None:
        return int(entry["threads"])
    match = THREAD_RE.search(name)
    return int(match.group("threads")) if match else 1


def identify_benchmark(name: str) -> tuple[str, str, str] | None:
    """Return (family, implementation, workload)."""
    match = ARENA_RE.search(name)
    if match:
        return (
            "arena",
            IMPLEMENTATION_LABELS[match.group("impl")],
            match.group("workload"),
        )

    match = CONCURRENT_DEQUE_RE.search(name)
    if match:
        return (
            "deque",
            IMPLEMENTATION_LABELS[match.group("impl")],
            match.group("workload"),
        )

    match = LOCK_BASED_RE.search(name)
    if match:
        return (
            "deque",
            IMPLEMENTATION_LABELS["LockBasedStdDeque"],
            match.group("workload"),
        )

    if name.startswith("barrier_overhead"):
        return ("barrier", "barrier", "BarrierOverhead")

    return None


def parse_operations(name: str) -> tuple[str | None, int | None]:
    match = OP_RE.search(name)
    if not match:
        return None, None
    return match.group("kind"), int(match.group("count"))


def logical_operations(
        family: str,
        workload: str,
        operations_kind: str | None,
        operations: int | None,
        threads: int,
) -> tuple[int | None, int | None]:
    """
    Return (effective_batch_items, total_logical_operations).

    The benchmark harness uses one push + one pop (or allocate + release/retire)
    as two logical operations. For operations_per_thread, the total system work
    grows with thread count. For total_operations, the value is already global.
    """
    if family == "barrier":
        return None, None

    if workload == "ConstructDestroy":
        return 1, 1

    if operations is None:
        raise ValueError(f"Missing operation count for workload {workload}")

    if operations_kind == "operations_per_thread":
        effective = operations * threads
    else:
        effective = operations

    return effective, effective * 2


def parse_entry(entry: dict, source: Path) -> dict | None:
    # Google Benchmark aggregate rows are deliberately ignored. We recompute
    # statistics from the raw repetitions ourselves.
    if entry.get("run_type") == "aggregate" or "aggregate_name" in entry:
        return None

    name = benchmark_name(entry)
    identity = identify_benchmark(name)
    if identity is None:
        return None

    family, implementation, workload = identity
    operations_kind, operations = parse_operations(name)
    threads = parse_threads(entry, name)

    unit = str(entry.get("time_unit", "ns"))
    if unit not in TIME_TO_NS:
        raise ValueError(f"Unsupported time unit {unit!r}: {name}")

    factor = TIME_TO_NS[unit]
    error = bool(entry.get("error_occurred", False))

    base = {
        "source": source.name,
        "name": name,
        "family": family,
        "implementation": implementation,
        "workload": workload,
        "operations_kind": operations_kind,
        "operations": operations,
        "threads": threads,
        "repetition": entry.get("repetition_index"),
        "expected_repetitions": entry.get("repetitions"),
        "error_occurred": error,
        "error_message": entry.get("error_message"),
    }

    if error:
        return base

    effective, total_ops = logical_operations(
        family,
        workload,
        operations_kind,
        operations,
        threads,
    )

    # Google Benchmark reports multithreaded timing normalized across the
    # participating threads. For a synchronized benchmark iteration, the
    # corresponding elapsed wall time is therefore reported_time * threads.
    # SetItemsProcessed()/items_per_second already follows Google Benchmark's
    # multithread counter semantics, so prefer it for system throughput.
    reported_real_time_ns = float(entry["real_time"]) * factor
    wall_time_ns = reported_real_time_ns * threads
    cpu_time_ns = float(entry.get("cpu_time", math.nan)) * factor

    google_items_per_second = entry.get("items_per_second")

    if total_ops is None:
        ns_per_operation = math.nan
        throughput_total = math.nan
        throughput_per_thread = math.nan
    else:
        if google_items_per_second is not None:
            throughput_total = float(google_items_per_second)
        else:
            throughput_total = total_ops / (wall_time_ns * 1e-9)

        ns_per_operation = 1e9 / throughput_total if throughput_total > 0 else math.nan
        throughput_per_thread = throughput_total / threads

    return {
        **base,
        "iterations": entry.get("iterations"),
        "reported_real_time_ns": reported_real_time_ns,
        "wall_time_ns": wall_time_ns,
        "cpu_time_ns": cpu_time_ns,
        "effective_batch_items": effective,
        "logical_operations": total_ops,
        "ns_per_operation": ns_per_operation,
        "throughput_total_ops_per_second": throughput_total,
        "throughput_total_mops": throughput_total / 1e6,
        "throughput_per_thread_mops": throughput_per_thread / 1e6,
        "google_items_per_second": google_items_per_second,
        "bytes_per_second": entry.get("bytes_per_second"),
    }


def load_results(paths: list[Path]) -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    success_rows: list[dict] = []
    failure_rows: list[dict] = []
    context_rows: list[dict] = []

    for path in paths:
        with path.open("r", encoding="utf-8") as f:
            payload = json.load(f)

        context_rows.append({"source": path.name, **payload.get("context", {})})

        for entry in payload.get("benchmarks", []):
            parsed = parse_entry(entry, path)
            if parsed is None:
                continue
            if parsed["error_occurred"]:
                failure_rows.append(parsed)
            else:
                success_rows.append(parsed)

    if not success_rows:
        raise RuntimeError("No raw benchmark repetitions could be parsed.")

    return (
        pd.DataFrame(success_rows),
        pd.DataFrame(failure_rows),
        pd.DataFrame(context_rows),
    )


# -----------------------------------------------------------------------------
# Statistics / CSVs
# -----------------------------------------------------------------------------

def bootstrap_median_ci(
        values: np.ndarray,
        samples: int,
        confidence: float = 0.95,
        seed: int = 42,
) -> tuple[float, float]:
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]

    if len(values) == 0:
        return math.nan, math.nan
    if len(values) == 1 or samples <= 0:
        median = float(np.median(values))
        return median, median

    rng = np.random.default_rng(seed)
    sampled = rng.choice(values, size=(samples, len(values)), replace=True)
    medians = np.median(sampled, axis=1)
    alpha = (1.0 - confidence) / 2.0
    return (
        float(np.quantile(medians, alpha)),
        float(np.quantile(medians, 1.0 - alpha)),
    )


def summarize_metric(
        values: np.ndarray,
        *,
        bootstrap_samples: int,
) -> dict[str, float]:
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]

    if len(values) == 0:
        return {
            "mean": math.nan,
            "median": math.nan,
            "stddev": math.nan,
            "cv": math.nan,
            "q25": math.nan,
            "q75": math.nan,
            "ci95_low": math.nan,
            "ci95_high": math.nan,
        }

    mean = float(np.mean(values))
    stddev = float(np.std(values, ddof=1)) if len(values) > 1 else 0.0
    ci_low, ci_high = bootstrap_median_ci(values, bootstrap_samples)

    return {
        "mean": mean,
        "median": float(np.median(values)),
        "stddev": stddev,
        "cv": stddev / mean if mean != 0 else math.nan,
        "q25": float(np.quantile(values, 0.25)),
        "q75": float(np.quantile(values, 0.75)),
        "ci95_low": ci_low,
        "ci95_high": ci_high,
    }


def create_summary(df: pd.DataFrame, bootstrap_samples: int) -> pd.DataFrame:
    group_columns = [
        "source",
        "family",
        "implementation",
        "workload",
        "operations_kind",
        "operations",
        "threads",
    ]

    rows: list[dict] = []

    for keys, group in df.groupby(group_columns, dropna=False, sort=True):
        row = dict(zip(group_columns, keys))
        row["n"] = len(group)
        row["expected_repetitions"] = int(group["expected_repetitions"].dropna().iloc[0]) if group["expected_repetitions"].notna().any() else math.nan

        for column, prefix in [
            ("reported_real_time_ns", "reported_real_time_ns"),
            ("wall_time_ns", "wall_time_ns"),
            ("cpu_time_ns", "cpu_time_ns"),
            ("ns_per_operation", "ns_per_operation"),
            ("throughput_total_mops", "throughput_total_mops"),
            ("throughput_per_thread_mops", "throughput_per_thread_mops"),
        ]:
            stats = summarize_metric(
                group[column].to_numpy(dtype=float),
                bootstrap_samples=bootstrap_samples,
            )
            for stat_name, value in stats.items():
                row[f"{prefix}_{stat_name}"] = value

        effective = group["effective_batch_items"].dropna()
        logical = group["logical_operations"].dropna()
        row["effective_batch_items"] = int(effective.iloc[0]) if not effective.empty else math.nan
        row["logical_operations"] = int(logical.iloc[0]) if not logical.empty else math.nan
        rows.append(row)

    return pd.DataFrame(rows)


def config_key_columns() -> list[str]:
    return [
        "source",
        "family",
        "implementation",
        "workload",
        "operations_kind",
        "operations",
        "threads",
    ]


def create_coverage(df: pd.DataFrame, failures: pd.DataFrame) -> pd.DataFrame:
    columns = config_key_columns()

    success = (
        df.groupby(columns, dropna=False)
        .size()
        .rename("successful_repetitions")
        .reset_index()
    )

    metadata_frames = [df[columns + ["expected_repetitions"]]]
    if not failures.empty:
        metadata_frames.append(failures[columns + ["expected_repetitions"]])
    metadata = pd.concat(metadata_frames, ignore_index=True)

    expected = (
        metadata.groupby(columns, dropna=False)["expected_repetitions"]
        .first()
        .rename("expected_repetitions")
        .reset_index()
    )

    coverage = success.merge(expected, on=columns, how="outer")

    if not failures.empty:
        failed = (
            failures.groupby(columns, dropna=False)
            .size()
            .rename("failed_repetitions")
            .reset_index()
        )
        coverage = coverage.merge(failed, on=columns, how="outer")
    else:
        coverage["failed_repetitions"] = 0

    coverage["successful_repetitions"] = coverage["successful_repetitions"].fillna(0).astype(int)
    coverage["failed_repetitions"] = coverage["failed_repetitions"].fillna(0).astype(int)
    coverage["observed_repetitions"] = coverage["successful_repetitions"] + coverage["failed_repetitions"]
    coverage["complete"] = coverage["observed_repetitions"] == coverage["expected_repetitions"]

    return coverage.sort_values(columns).reset_index(drop=True)


def create_relative_performance(summary: pd.DataFrame) -> pd.DataFrame:
    data = summary.copy()
    key_columns = [
        "source",
        "family",
        "workload",
        "operations_kind",
        "operations",
        "threads",
    ]

    rows: list[dict] = []

    for keys, group in data.groupby(key_columns, dropna=False, sort=True):
        key_values = dict(zip(key_columns, keys))

        new_rows = group[group["implementation"] == "new/delete"]
        lock_rows = group[group["implementation"] == "std::deque + mutex"]

        new_thr = (
            float(new_rows.iloc[0]["throughput_total_mops_median"])
            if not new_rows.empty
            else math.nan
        )
        lock_thr = (
            float(lock_rows.iloc[0]["throughput_total_mops_median"])
            if not lock_rows.empty
            else math.nan
        )
        new_lat = (
            float(new_rows.iloc[0]["wall_time_ns_median"])
            if not new_rows.empty
            else math.nan
        )
        lock_lat = (
            float(lock_rows.iloc[0]["wall_time_ns_median"])
            if not lock_rows.empty
            else math.nan
        )

        for _, row in group.iterrows():
            throughput = float(row["throughput_total_mops_median"])
            latency = float(row["wall_time_ns_median"])

            rows.append({
                **key_values,
                "implementation": row["implementation"],
                "throughput_total_mops_median": throughput,
                "wall_time_ns_median": latency,
                "throughput_vs_new_delete": (
                    throughput / new_thr
                    if math.isfinite(new_thr) and new_thr > 0 and math.isfinite(throughput)
                    else math.nan
                ),
                "throughput_vs_lock_based": (
                    throughput / lock_thr
                    if math.isfinite(lock_thr) and lock_thr > 0 and math.isfinite(throughput)
                    else math.nan
                ),
                # > 1 means faster/lower latency than the baseline.
                "latency_speedup_vs_new_delete": (
                    new_lat / latency
                    if math.isfinite(new_lat) and latency > 0
                    else math.nan
                ),
                "latency_speedup_vs_lock_based": (
                    lock_lat / latency
                    if math.isfinite(lock_lat) and latency > 0
                    else math.nan
                ),
            })

    return pd.DataFrame(rows)


# -----------------------------------------------------------------------------
# Plot helpers
# -----------------------------------------------------------------------------

def save_png(fig: plt.Figure, output: Path, name: str, dpi: int) -> None:
    fig.tight_layout()
    fig.savefig(output / f"{name}.png", dpi=dpi, bbox_inches="tight")
    plt.close(fig)


def sanitize(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value)


def set_operation_axis(ax: plt.Axes, values: Iterable[int]) -> None:
    ticks = sorted({int(v) for v in values})
    if not ticks:
        return
    ax.set_xscale("log", base=2)
    ax.set_xticks(ticks)
    ax.set_xticklabels([str(v) for v in ticks], rotation=25, ha="right")


def set_thread_axis(ax: plt.Axes, values: Iterable[int]) -> None:
    ticks = sorted({int(v) for v in values})
    if not ticks:
        return
    ax.set_xscale("log", base=2)
    ax.set_xticks(ticks)
    ax.set_xticklabels([str(v) for v in ticks])


def hardware_threads(contexts: pd.DataFrame) -> int | None:
    if contexts.empty or "num_cpus" not in contexts:
        return None
    values = {
        int(v)
        for v in contexts["num_cpus"].dropna().tolist()
        if str(v).strip()
    }
    return next(iter(values)) if len(values) == 1 else None


def add_hardware_marker(
        ax: plt.Axes,
        hardware_thread_count: int | None,
        thread_values: Iterable[int],
) -> None:
    if hardware_thread_count is None:
        return
    values = sorted({int(v) for v in thread_values})
    if values and min(values) <= hardware_thread_count <= max(values):
        ax.axvline(
            hardware_thread_count,
            linestyle=":",
            linewidth=1.2,
            label=f"logical CPUs ({hardware_thread_count})",
        )


def ci_arrays(data: pd.DataFrame, prefix: str) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    median = data[f"{prefix}_median"].to_numpy(dtype=float)
    low = data[f"{prefix}_ci95_low"].to_numpy(dtype=float)
    high = data[f"{prefix}_ci95_high"].to_numpy(dtype=float)
    return median, low, high


def plot_latency_bars(
        data: pd.DataFrame,
        order: list[str],
        title: str,
        output: Path,
        filename: str,
        dpi: int,
) -> None:
    if data.empty:
        return

    rows = []
    for implementation in order:
        part = data[data["implementation"] == implementation]
        if not part.empty:
            rows.append(part.iloc[0])

    if not rows:
        return

    labels = [row["implementation"] for row in rows]
    medians = np.array([float(row["wall_time_ns_median"]) / 1000.0 for row in rows])
    low = np.array([float(row["wall_time_ns_ci95_low"]) / 1000.0 for row in rows])
    high = np.array([float(row["wall_time_ns_ci95_high"]) / 1000.0 for row in rows])

    lower_err = np.maximum(0.0, medians - low)
    upper_err = np.maximum(0.0, high - medians)

    fig, ax = plt.subplots(figsize=(8.4, 5.1))
    for i, (label, median, lo, hi) in enumerate(zip(labels, medians, lower_err, upper_err)):
        ax.bar(
            i,
            median,
            yerr=np.array([[lo], [hi]]),
            capsize=5,
            width=0.68,
            label=label,
        )
        ax.annotate(
            f"{median:.2f} µs",
            xy=(i, median),
            xytext=(0, 7),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    ax.set_xticks(range(len(labels)))
    ax.set_xticklabels(labels)
    ax.set_ylabel("Median latency [µs]")
    ax.set_title(title)
    ax.set_ylim(bottom=0)
    ax.grid(axis="y", alpha=0.25)
    save_png(fig, output, filename, dpi)


def plot_throughput_by_operations(
        data: pd.DataFrame,
        order: list[str],
        title: str,
        output: Path,
        filename: str,
        dpi: int,
) -> None:
    if data.empty:
        return

    fig, ax = plt.subplots(figsize=(8.6, 5.2))
    all_operations: set[int] = set()
    plotted = False

    for implementation in order:
        part = data[data["implementation"] == implementation].sort_values("operations")
        if part.empty:
            continue
        x = part["operations"].to_numpy(dtype=int)
        median, low, high = ci_arrays(part, "throughput_total_mops")
        all_operations.update(int(v) for v in x)
        ax.plot(x, median, marker="o", label=implementation)
        ax.fill_between(x, low, high, alpha=0.15)
        plotted = True

    if not plotted:
        plt.close(fig)
        return

    set_operation_axis(ax, all_operations)
    ax.set_xlabel("Operations per batch")
    ax.set_ylabel("Total throughput [million logical operations/s]")
    ax.set_title(title)
    ax.grid(alpha=0.25)
    ax.legend()
    save_png(fig, output, filename, dpi)


def concurrent_groups(summary: pd.DataFrame, workload: str):
    data = summary[
        (summary["family"] == "deque")
        & (summary["workload"] == workload)
        ].copy()

    if data.empty:
        return

    for kind in data["operations_kind"].dropna().astype(str).unique():
        kind_data = data[data["operations_kind"] == kind]
        for operation_count in sorted(int(v) for v in kind_data["operations"].dropna().unique()):
            yield kind, operation_count, kind_data[kind_data["operations"] == operation_count].copy()


def operation_description(kind: str, count: int) -> str:
    if kind == "total_operations":
        return f"{count} pushes + {count} pops total"
    if kind == "operations_per_thread":
        return f"{count} pushes + {count} pops per thread"
    return f"{count} pushes + {count} pops"


def plot_concurrent_throughput(
        summary: pd.DataFrame,
        workload: str,
        output: Path,
        dpi: int,
        hardware_thread_count: int | None,
) -> None:
    for kind, count, data in concurrent_groups(summary, workload) or []:
        fig, ax = plt.subplots(figsize=(8.7, 5.3))
        thread_values: set[int] = set()
        plotted = False

        for implementation in DEQUE_IMPLEMENTATIONS:
            part = data[data["implementation"] == implementation].sort_values("threads")
            if part.empty:
                continue
            x = part["threads"].to_numpy(dtype=int)
            median, low, high = ci_arrays(part, "throughput_total_mops")
            thread_values.update(int(v) for v in x)
            ax.plot(x, median, marker="o", label=implementation)
            ax.fill_between(x, low, high, alpha=0.12)
            plotted = True

        if not plotted:
            plt.close(fig)
            continue

        set_thread_axis(ax, thread_values)
        add_hardware_marker(ax, hardware_thread_count, thread_values)
        ax.set_xlabel("Threads")
        ax.set_ylabel("Total throughput [million logical operations/s]")
        ax.set_title(
            f"{WORKLOAD_TITLES[workload]}\n{operation_description(kind, count)}"
        )
        ax.grid(alpha=0.25)
        ax.legend()
        save_png(
            fig,
            output,
            f"{sanitize(workload)}_throughput_{kind}_{count}",
            dpi,
        )


def plot_scaling_and_efficiency(
        summary: pd.DataFrame,
        workload: str,
        output: Path,
        dpi: int,
        hardware_thread_count: int | None,
) -> None:
    for kind, count, data in concurrent_groups(summary, workload) or []:
        # Scaling / speedup
        fig, ax = plt.subplots(figsize=(8.7, 5.3))
        thread_values: set[int] = set()
        plotted = False

        for implementation in DEQUE_IMPLEMENTATIONS:
            part = data[data["implementation"] == implementation].sort_values("threads")
            if part.empty:
                continue
            baseline_rows = part[part["threads"] == 1]
            if baseline_rows.empty:
                continue

            baseline = float(baseline_rows.iloc[0]["throughput_total_mops_median"])
            if baseline <= 0:
                continue

            x = part["threads"].to_numpy(dtype=int)
            y = part["throughput_total_mops_median"].to_numpy(dtype=float) / baseline
            thread_values.update(int(v) for v in x)
            ax.plot(x, y, marker="o", label=implementation)
            plotted = True

        if plotted:
            ideal_x = np.array(sorted(thread_values), dtype=int)
            ax.plot(ideal_x, ideal_x, linestyle="--", label="ideal linear scaling")
            set_thread_axis(ax, thread_values)
            add_hardware_marker(ax, hardware_thread_count, thread_values)
            ax.set_xlabel("Threads")
            if kind == "total_operations":
                ylabel = "Speedup relative to 1 thread"
                name = "speedup"
            else:
                ylabel = "Total-throughput scaling relative to 1 thread"
                name = "throughput_scaling"
            ax.set_ylabel(ylabel)
            ax.set_title(
                f"{WORKLOAD_TITLES[workload]}\n{operation_description(kind, count)}"
            )
            ax.grid(alpha=0.25)
            ax.legend()
            save_png(fig, output, f"{sanitize(workload)}_{name}_{kind}_{count}", dpi)
        else:
            plt.close(fig)

        # Parallel efficiency
        fig, ax = plt.subplots(figsize=(8.7, 5.3))
        thread_values = set()
        plotted = False

        for implementation in DEQUE_IMPLEMENTATIONS:
            part = data[data["implementation"] == implementation].sort_values("threads")
            if part.empty:
                continue
            baseline_rows = part[part["threads"] == 1]
            if baseline_rows.empty:
                continue

            baseline = float(baseline_rows.iloc[0]["throughput_total_mops_median"])
            if baseline <= 0:
                continue

            x = part["threads"].to_numpy(dtype=int)
            throughput = part["throughput_total_mops_median"].to_numpy(dtype=float)
            efficiency = (throughput / baseline) / x * 100.0
            thread_values.update(int(v) for v in x)
            ax.plot(x, efficiency, marker="o", label=implementation)
            plotted = True

        if plotted:
            set_thread_axis(ax, thread_values)
            add_hardware_marker(ax, hardware_thread_count, thread_values)
            ax.axhline(100.0, linestyle="--", linewidth=1.2, label="ideal efficiency")
            ax.set_xlabel("Threads")
            ax.set_ylabel("Parallel efficiency [%]")
            ax.set_title(
                f"{WORKLOAD_TITLES[workload]}\n{operation_description(kind, count)}"
            )
            ax.grid(alpha=0.25)
            ax.legend()
            save_png(
                fig,
                output,
                f"{sanitize(workload)}_parallel_efficiency_{kind}_{count}",
                dpi,
            )
        else:
            plt.close(fig)


def plot_barrier_overhead(
        summary: pd.DataFrame,
        output: Path,
        dpi: int,
        hardware_thread_count: int | None,
) -> None:
    data = summary[summary["family"] == "barrier"].sort_values("threads")
    if data.empty:
        return

    x = data["threads"].to_numpy(dtype=int)
    median = data["wall_time_ns_median"].to_numpy(dtype=float) / 1000.0
    low = data["wall_time_ns_ci95_low"].to_numpy(dtype=float) / 1000.0
    high = data["wall_time_ns_ci95_high"].to_numpy(dtype=float) / 1000.0

    fig, ax = plt.subplots(figsize=(8.5, 5.1))
    ax.plot(x, median, marker="o", label="two barriers per benchmark iteration")
    ax.fill_between(x, low, high, alpha=0.15)
    set_thread_axis(ax, x)
    add_hardware_marker(ax, hardware_thread_count, x)
    ax.set_xlabel("Threads")
    ax.set_ylabel("Median wall time [µs]")
    ax.set_title("Barrier harness overhead")
    ax.grid(alpha=0.25)
    ax.legend()
    save_png(fig, output, "barrier_overhead", dpi)


# -----------------------------------------------------------------------------
# Plot orchestration
# -----------------------------------------------------------------------------

def create_plots(
        summary: pd.DataFrame,
        contexts: pd.DataFrame,
        output: Path,
        dpi: int,
) -> None:
    hw_threads = hardware_threads(contexts)

    # Arena construction/destruction.
    arena_construct = summary[
        (summary["family"] == "arena")
        & (summary["workload"] == "ConstructDestroy")
        ]
    plot_latency_bars(
        arena_construct,
        MEMORY_STRATEGIES,
        "Memory arena construction / destruction",
        output,
        "arena_construct_destroy_latency",
        dpi,
    )

    # Arena operation throughput.
    for workload in ["AllocateReleaseUnpublished", "AllocateRetireReclaim"]:
        data = summary[
            (summary["family"] == "arena")
            & (summary["workload"] == workload)
            ]
        plot_throughput_by_operations(
            data,
            MEMORY_STRATEGIES,
            f"Memory arena: {WORKLOAD_TITLES[workload]}",
            output,
            f"arena_{sanitize(workload)}_throughput",
            dpi,
        )

    # Complete deque construction/destruction, including mutex baseline.
    deque_construct = summary[
        (summary["family"] == "deque")
        & (summary["workload"] == "ConstructDestroy")
        ]
    plot_latency_bars(
        deque_construct,
        DEQUE_IMPLEMENTATIONS,
        "Deque construction / destruction",
        output,
        "deque_construct_destroy_latency",
        dpi,
    )

    # Single-thread full-deque operations.
    for workload in ["PushBackPopFront", "PushFrontPopBack"]:
        data = summary[
            (summary["family"] == "deque")
            & (summary["workload"] == workload)
            & (summary["threads"] == 1)
            ]
        plot_throughput_by_operations(
            data,
            DEQUE_IMPLEMENTATIONS,
            WORKLOAD_TITLES[workload],
            output,
            f"deque_{sanitize(workload)}_throughput",
            dpi,
        )

    # Concurrent tests.
    concurrent = [
        "ConcurrentPhasedPushBackPopFront",
        "ConcurrentSteadyPushBackPopFront",
        "ConcurrentSteadyPushFrontPopBack",
    ]
    for workload in concurrent:
        plot_concurrent_throughput(
            summary,
            workload,
            output,
            dpi,
            hw_threads,
        )
        plot_scaling_and_efficiency(
            summary,
            workload,
            output,
            dpi,
            hw_threads,
        )

    plot_barrier_overhead(summary, output, dpi, hw_threads)


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def main() -> None:
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    raw, failures, contexts = load_results(args.json)
    summary = create_summary(raw, args.bootstrap_samples)
    coverage = create_coverage(raw, failures)
    relative = create_relative_performance(summary)

    # CSV only; no JSON/PDF/SVG output.
    raw.to_csv(args.output / "raw_measurements.csv", index=False)
    summary.to_csv(args.output / "summary.csv", index=False)
    relative.to_csv(args.output / "relative_performance.csv", index=False)
    coverage.to_csv(args.output / "coverage.csv", index=False)
    contexts.to_csv(args.output / "benchmark_context.csv", index=False)

    # Always create failures.csv so CI/scripts can depend on its existence.
    if failures.empty:
        pd.DataFrame(columns=[
            "source", "name", "family", "implementation", "workload",
            "operations_kind", "operations", "threads", "repetition",
            "expected_repetitions", "error_occurred", "error_message",
        ]).to_csv(args.output / "failures.csv", index=False)
    else:
        failures.to_csv(args.output / "failures.csv", index=False)

    create_plots(summary, contexts, args.output, args.dpi)

    png_count = len(list(args.output.glob("*.png")))
    csv_count = len(list(args.output.glob("*.csv")))
    complete_count = int(coverage["complete"].sum())

    print(f"Parsed successful raw repetitions: {len(raw)}")
    print(f"Parsed failed raw repetitions:     {len(failures)}")
    print(f"Benchmark configurations:          {len(coverage)}")
    print(f"Complete configurations:           {complete_count}/{len(coverage)}")
    print(f"Generated PNG files:               {png_count}")
    print(f"Generated CSV files:               {csv_count}")
    print(f"Output directory:                  {args.output.resolve()}")

    incomplete = coverage[~coverage["complete"]]
    if not incomplete.empty:
        print("WARNING: incomplete benchmark configurations detected. See coverage.csv")


if __name__ == "__main__":
    main()