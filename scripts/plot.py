import argparse
import json
import math
import os
import re
import sys

from pathlib import Path
from datetime import datetime

import matplotlib.pyplot as plt

AUTO_BENCHMARKS_BEGIN = "<!-- AUTO-BENCHMARKS:BEGIN -->"
AUTO_BENCHMARKS_END = "<!-- AUTO-BENCHMARKS:END -->"

BENCHMARK_OUTPUT_DIRNAME = "benchmarks"
DEFAULT_DOCS_DIR = Path(__file__).resolve().parents[1] / "docs"

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create plots from libVentra benchmark output and wire them into the docs.",
    )
    parser.add_argument(
        "-f",
        "--folder",
        default="build-bench",
        help="Path to the folder containing benchmark JSON files (default: build-bench).",
    )
    parser.add_argument(
        "--docs-dir",
        default=str(DEFAULT_DOCS_DIR),
        help=f"Path to the docs directory (default: {DEFAULT_DOCS_DIR}).",
    )
    return parser.parse_args()


def get_benchmark_data(folder_path: Path) -> list[dict]:
    # Find all .json files
    found_files: list[Path] = []
    for root, dirs, files in os.walk(folder_path):
        for file in files:
            if file.endswith(".json"):
                full_file_path = os.path.abspath(os.path.join(root, file))
                found_files.append(Path(full_file_path))
        break # Stop recursion

    benchmark_data: list[dict] = []
    for file_path in found_files:
        with open(file_path, "r") as file:
            data = json.loads(file.read())

            category = "unknown"
            if file_path.name.startswith("libVentra_"):
                match = re.match(r"libVentra_([^_.]+)", file_path.name)
                if match:
                    category = match.group(1)

            data["_category"] = category

            benchmark_data.append(data)

    return benchmark_data

def split_camel_case(text: str) -> str:
    return re.sub(r"(?<!^)([A-Z])", r" \1", text)

def process_benchmarks_data(benchmark_list: list[dict], multithread_aggregate_name: str):

    benchmarks_ready_for_plot: list[dict] = []

    for benchmark_file_data in benchmark_list:
        # Get benchmark run meta data
        benchmark_data: dict = {
            "category": benchmark_file_data.get("_category", "unknown"),
            "cpu_clock": benchmark_file_data["context"]["mhz_per_cpu"],
            "cpu_threads": benchmark_file_data["context"]["num_cpus"],
            "date": datetime.fromisoformat(benchmark_file_data["context"]["date"]).strftime("%d.%m.%Y"),
            "repetitions": benchmark_file_data["benchmarks"][0]["repetitions"]
        }

        # Sort and filter benchmarks
        sorted_benchmarks: dict[str, dict[int, dict[str, list[tuple[int, int]]]]] = {}

        name_re = re.compile(r"^[A-Za-z]+_([A-Za-z]+)_([A-Za-z_]+)/(\d+)/repeats:\d+/threads:\d+_*[a-z]*$")

        for benchmark in benchmark_file_data["benchmarks"]:
            benchmark_name: str = benchmark["name"]

            name_match = name_re.fullmatch(benchmark_name)
            if name_match is None:
                print(f"Error: Found Thread-Benchmark with invalid name: {benchmark_name}")
                continue

            data_structure = split_camel_case(name_match.group(1))
            test_name = name_match.group(2)
            operations = int(name_match.group(3))

            run_type = benchmark.get("run_type")
            aggregate_name = benchmark.get("aggregate_name")

            if run_type == "aggregate":
                if aggregate_name != multithread_aggregate_name:
                    continue
            elif run_type is not None:
                continue

            sorted_benchmarks.setdefault(test_name, {})
            sorted_benchmarks[test_name].setdefault(operations, {})
            sorted_benchmarks[test_name][operations].setdefault(data_structure, [])
            sorted_benchmarks[test_name][operations][data_structure].append((benchmark["threads"], benchmark["real_time"]))


        # merge info
        benchmark_data["benchmarks"] = sorted_benchmarks

        benchmarks_ready_for_plot.append(benchmark_data)

    return benchmarks_ready_for_plot


def choose_time_unit(values_ns: list[float]) -> tuple[float, str]:
    max_value = max(values_ns, default=0)

    if max_value < 1_000:
        return 1.0, "ns"
    if max_value < 1_000_000:
        return 1_000.0, "µs"
    if max_value < 1_000_000_000:
        return 1_000_000.0, "ms"
    return 1_000_000_000.0, "s"


def save_plots(benchmarks_data: list[dict[str, object]], output_dir: Path) -> dict[str, list[Path]]:
    saved_plots_by_category: dict[str, list[Path]] = {}

    for benchmark_file_data in benchmarks_data:
        benchmark_groups = benchmark_file_data["benchmarks"]
        category = benchmark_file_data.get("category", "unknown")

        if category not in saved_plots_by_category:
            saved_plots_by_category[category] = []

        if not isinstance(benchmark_groups, dict):
            raise TypeError(
                f"'benchmarks' must be a dict, got {type(benchmark_groups).__name__}"
            )

        cpu_clock = benchmark_file_data["cpu_clock"]
        cpu_threads = benchmark_file_data["cpu_threads"]
        date = benchmark_file_data["date"]
        repetitions = benchmark_file_data["repetitions"]

        for benchmark_name, benchmark_dict in benchmark_groups.items():
            fields_amount = len(benchmark_dict)
            cols = math.ceil(math.sqrt(fields_amount))
            rows = math.ceil(fields_amount / cols) if cols > 0 else 1

            fig_width = max(8, cols * 7)
            fig_height = max(6, rows * 5)

            fig, axes = plt.subplots(rows, cols, figsize=(fig_width, fig_height))

            fig.suptitle(
                f"Performance Benchmark: {benchmark_name}\n"
                f"Real time over {repetitions} repetitions | "
                f"{cpu_threads} CPU threads | {cpu_clock} MHz | {date}"
            )

            if hasattr(axes, "flatten"):
                axes = axes.flatten()
            else:
                axes = [axes]

            data_struct_names: str = ""

            for ax, (operations, implementations) in zip(axes, benchmark_dict.items()):
                all_y_values_ns = []
                for values in implementations.values():
                    for _, real_time in values:
                        all_y_values_ns.append(real_time)

                divisor, unit = choose_time_unit(all_y_values_ns)

                for implementation_name, values in implementations.items():
                    x_values = [threads for threads, _ in values]
                    y_values = [real_time / divisor for _, real_time in values]

                    ax.plot(x_values, y_values, marker="o", label=implementation_name)

                    data_struct_name = str(implementation_name).replace(" ", "_")

                    if data_struct_name not in data_struct_names:
                        data_struct_names += data_struct_name + "."

                ax.set_title(f"Operations: {operations}")
                ax.set_xlabel("Threads")
                ax.set_ylabel(f"Real time ({unit})")
                ax.legend()
                ax.grid(True)

            data_struct_names = data_struct_names.rstrip(".") + "-"

            for ax in axes[fields_amount:]:
                ax.remove()

            fig.tight_layout()

            output_path = output_dir / f"{data_struct_names}{benchmark_name}.png"
            fig.savefig(output_path, dpi=200, bbox_inches="tight")
            plt.close(fig)

            saved_plots_by_category[category].append(output_path)

    return saved_plots_by_category


def update_docs(saved_plots_by_category: dict[str, list[Path]], docs_dir: Path) -> None:

    std_re = re.compile(r"^std_.*$")

    for category, plot_paths in saved_plots_by_category.items():
        print(f"Update docs for category: {category}")
        for plot in plot_paths:
            plot_file_name: str = plot.stem

            plot_file_name_split: list[str] = plot_file_name.split("-")

            plot_data_structures: list[str] = plot_file_name_split[0].split(".")
            plot_test_name: str = plot_file_name_split[1]

            for data_structure_doc_file in plot_data_structures:
                file_name_raw = data_structure_doc_file.lower() + ".md"

                if std_re.match(file_name_raw) is not None:
                    continue

                file_name = re.sub(r"ventra_", "", file_name_raw)

                data_structure_doc_file_path: Path = Path(docs_dir / category / file_name)

                if not data_structure_doc_file_path.exists():
                    raise RuntimeError(f"Doc file: {data_structure_doc_file_path} not found")

                with open(data_structure_doc_file_path, "r", encoding="utf-8") as f:
                    content = f.read()

                start_idx = content.find(AUTO_BENCHMARKS_BEGIN)
                end_idx = content.find(AUTO_BENCHMARKS_END)

                if start_idx == -1 or end_idx == -1:
                    print(f"Warning: {AUTO_BENCHMARKS_BEGIN} or END missing in {file_name}. Skipping.")
                    continue

                block_content = content[start_idx:end_idx]

                formatted_test_name = split_camel_case(plot_test_name)

                rel_image_path = f"../benchmarks/{plot.name}"

                if rel_image_path not in block_content:
                    new_benchmark_md = (
                        f"### {formatted_test_name}\n\n"
                        f"![Benchmark {formatted_test_name}]({rel_image_path})\n\n"
                    )

                    new_content = (
                            content[:end_idx].rstrip() +
                            "\n\n\n" +
                            new_benchmark_md +
                            content[end_idx:]
                    )

                    with open(data_structure_doc_file_path, "w", encoding="utf-8") as f:
                        f.write(new_content)


def main() -> None:
    args = parse_args()
    folder_path: Path = Path(args.folder)
    docs_dir: Path = Path(args.docs_dir)

    if not docs_dir.exists() or not docs_dir.is_dir():
        print(f"Error: Docs directory '{docs_dir}' does not exist.")
        sys.exit(1)

    print(f"Reading JSON data from folder: {folder_path} ...")
    benchmarks: list[dict] = get_benchmark_data(folder_path)

    if len(benchmarks) == 0:
        print("No benchmarks found. Aborting.")
        sys.exit(1)

    print("Processing benchmark data ...")
    benchmarks_data = process_benchmarks_data(benchmarks, "median")

    print("Generating plots ...")
    output_dir = docs_dir / BENCHMARK_OUTPUT_DIRNAME
    saved_plots = save_plots(benchmarks_data, output_dir)

    print("Updating docs ...")
    update_docs(saved_plots, docs_dir)

    print("Done.")


if __name__ == "__main__":
    main()