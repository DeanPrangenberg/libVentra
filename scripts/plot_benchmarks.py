#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import sys
import re
import math
from pathlib import Path

import matplotlib.pyplot as plt

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create plots from libVentra benchmark output.",
    )
    parser.add_argument(
        "-f",
        "--folder",
        default="build-bench",
        help="Path to benchmark build folder containing the JSON files (default: build-bench).",
    )
    return parser.parse_args()


def read_benchmark_data(folder_path: Path) -> list:
    """Liest alle JSON-Dateien im angegebenen Ordner ein und sammelt die Benchmarks."""
    benchmarks = []

    if not folder_path.exists() or not folder_path.is_dir():
        print(f"Error: Ordner '{folder_path}' existiert nicht.")
        sys.exit(1)

    json_files = list(folder_path.glob("*.json"))
    if not json_files:
        print(f"Error: Keine JSON-Dateien in '{folder_path}' gefunden.")
        sys.exit(1)

    for json_file in json_files:
        with open(json_file, "r") as f:
            try:
                data = json.load(f)
                if "benchmarks" in data:
                    benchmarks.extend(data["benchmarks"])
            except json.JSONDecodeError:
                print(f"Warnung: {json_file} konnte nicht gelesen werden (ungültiges JSON).")

    return benchmarks


def process_data(benchmarks: list) -> tuple[dict, int]:
    """Extrahiert die Daten generisch anhand des Musters BM_DataType_TestName."""
    # Struktur: parsed_data[TestName][Elements][DataType] = {"threads": [], "times": []}
    parsed_data = {}
    repetitions_count = 1

    for bench in benchmarks:
        if bench.get("aggregate_name") != "median":
            continue

        # Speichere die Anzahl der Repetitions dynamisch
        repetitions_count = max(repetitions_count, bench.get("repetitions", 1))

        run_name = bench["run_name"]
        parts = run_name.split("/")
        bench_name = parts[0]

        # Element-Argument ermitteln (falls vorhanden, ansonsten "Default")
        elements = "Default"
        if len(parts) > 1:
            try:
                elements = int(parts[1])
            except ValueError:
                elements = parts[1]

        threads = int(bench.get("threads", 1))
        time_ms = bench["real_time"] / 1_000_000.0

        # Namens-Parsing: BM_DataType_TestName
        if bench_name.startswith("BM_"):
            bench_name = bench_name[3:]

        # Am ersten Unterstrich teilen
        if "_" in bench_name:
            data_type_raw, test_name = bench_name.split("_", 1)
        else:
            data_type_raw = bench_name
            test_name = "General_Test"

        # Mache aus "StdVector" -> "Std Vector" und "VentraConcurrentVector" -> "Ventra Concurrent Vector"
        data_type = re.sub(r'(?<!^)(?=[A-Z])', ' ', data_type_raw)

        # In Datenstruktur einsortieren
        if test_name not in parsed_data:
            parsed_data[test_name] = {}
        if elements not in parsed_data[test_name]:
            parsed_data[test_name][elements] = {}
        if data_type not in parsed_data[test_name][elements]:
            parsed_data[test_name][elements][data_type] = {"threads": [], "times": []}

        parsed_data[test_name][elements][data_type]["threads"].append(threads)
        parsed_data[test_name][elements][data_type]["times"].append(time_ms)

    return parsed_data, repetitions_count


def save_plots(parsed_data: dict, repetitions: int, output_dir: Path) -> None:
    """Erstellt dynamisch skalierende Diagramme basierend auf den extrahierten Test-Kategorien."""
    output_dir.mkdir(parents=True, exist_ok=True)

    for test_name, elements_dict in parsed_data.items():
        if not elements_dict:
            continue

        # Dynamisches Grid berechnen (z.B. 1x1, 1x2, 2x2, 2x3...)
        num_subplots = len(elements_dict)
        cols = math.ceil(math.sqrt(num_subplots))
        rows = math.ceil(num_subplots / cols) if cols > 0 else 1

        # Figur-Größe dynamisch anpassen
        fig_width = max(8, cols * 7)
        fig_height = max(6, rows * 5)
        fig, axes = plt.subplots(rows, cols, figsize=(fig_width, fig_height))

        # Falls axes nur ein einzelnes Objekt ist, mach es iterierbar
        if num_subplots == 1:
            axes_flat = [axes]
        else:
            axes_flat = axes.flatten()

        fig.suptitle(f"Performance Benchmark: {test_name}\nMedian Time values over {repetitions} repetitions",
                     fontsize=16, fontweight="bold")

        # fig.supylabel(f"Median Real Time (ms) - {repetitions} Repetitions", fontsize=14, fontweight="bold")

        # Versuche numerisch zu sortieren (für 100, 1000...), ansonsten alphabetisch
        try:
            sorted_elements = sorted(elements_dict.keys(), key=lambda x: int(x))
        except ValueError:
            sorted_elements = sorted(elements_dict.keys(), key=str)

        for i, elements in enumerate(sorted_elements):
            ax = axes_flat[i]
            vector_data = elements_dict[elements]
            all_threads = set()

            for data_type, metrics in vector_data.items():
                sorted_pairs = sorted(zip(metrics["threads"], metrics["times"]))
                x_threads = [p[0] for p in sorted_pairs]
                y_times = [p[1] for p in sorted_pairs]

                all_threads.update(x_threads)
                ax.plot(x_threads, y_times, marker='o', linewidth=2, label=data_type)

            title_suffix = "Elements per Thread" if isinstance(elements, int) else "Argument"
            ax.set_title(f"{elements} {title_suffix}", fontsize=12)
            ax.set_xlabel("Threads")
            ax.set_ylabel("Time (ms)", fontsize=10)

            # X-Ticks fixiert auf echte Thread-Zahlen
            ax.set_xticks(sorted(list(all_threads)))
            ax.grid(True, linestyle="--", alpha=0.7)
            ax.legend()

        # Verstecke überflüssige (leere) Subplots, falls das Raster nicht perfekt gefüllt ist
        for i in range(num_subplots, len(axes_flat)):
            fig.delaxes(axes_flat[i])

        # Sicheres Layout-Management
        fig.tight_layout(rect=[0.02, 0.02, 1, 0.92])

        filename = output_dir / f"benchmark_plot_{test_name}.png"
        plt.savefig(filename, dpi=300)
        print(f"Gespeichert: {filename.resolve()}")


def main() -> None:
    args = parse_args()
    folder_path = Path(args.folder)

    print(f"Lese JSON-Daten aus Ordner: {folder_path} ...")
    benchmarks = read_benchmark_data(folder_path)

    if not benchmarks:
        print("Keine Benchmark-Messungen gefunden. Abbruch.")
        sys.exit(1)

    print("Verarbeite Daten...")
    parsed_data, repetitions = process_data(benchmarks)

    print("Generiere Diagramme...")
    save_plots(parsed_data, repetitions, folder_path)

    print("Erfolgreich abgeschlossen!")


if __name__ == "__main__":
    main()