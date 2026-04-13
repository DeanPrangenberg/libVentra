#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime
import re
import sys
from pathlib import Path


script_dir = Path(__file__).resolve().parent
root_dir = script_dir.parent.resolve()
ventra_include_dir = (root_dir / "include" / "ventra").resolve()
test_dir = (root_dir / "tests").resolve()
benchmark_dir = (root_dir / "benchmarks").resolve()
docs_dir = (root_dir / "docs").resolve()
cmakelist_file = (root_dir / "CMakeLists.txt").resolve()

header_block_re = re.compile(
    r"target_sources\(libVentra INTERFACE[ \t]*\n"
    r"[ \t]+FILE_SET HEADERS[ \t]*\n"
    r"[ \t]+BASE_DIRS include[ \t]*\n"
    r"[ \t]+FILES[ \t]*\n"
    r".*?\n\)",
    re.DOTALL,
)

test_target_block_re = re.compile(
    r"(?P<prefix>[ \t]*FetchContent_MakeAvailable\(googletest\)[ \t]*\n)"
    r"(?P<body>.*?)"
    r"(?P<suffix>[ \t]*include\(GoogleTest\)[ \t]*\n)",
    re.DOTALL,
)

discover_tests_block_re = re.compile(
    r"(?P<prefix>[ \t]*include\(GoogleTest\)[ \t]*\n)"
    r"(?P<body>.*?)"
    r"(?P<suffix>[ \t]*endif\(\))",
    re.DOTALL,
)

benchmark_section_re = re.compile(
    r"(?P<prefix>[ \t]*find_package\(Threads REQUIRED\)[ \t]*\n)"
    r"(?P<body>.*?)"
    r"(?P<suffix>[ \t]*endif\(\))",
    re.DOTALL,
)

valid_identifier_re = re.compile(r"^[a-z][a-z0-9_]*$")


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def ensure_project_layout() -> None:
    if not cmakelist_file.exists() or not ventra_include_dir.exists() or not test_dir.exists():
        fail("Project layout is incomplete.")


def created_on() -> str:
    return datetime.datetime.now().strftime("%m/%d/%Y")


def normalize_identifier(raw_value: str, label: str) -> str:
    value = raw_value.strip()

    if not value:
        fail(f"{label} must not be empty.")

    if not valid_identifier_re.fullmatch(value):
        fail(f"{label} must match ^[a-z][a-z0-9_]*$.")

    return value


def render_header_file(element_name: str) -> str:
    return (
        "//\n"
        f"// Created by deanprangenberg on {created_on()}.\n"
        "//\n"
        "\n"
        "#pragma once\n"
        "\n"
        "namespace ventra {\n"
        "    template<typename V>\n"
        f"    class {element_name} {{\n"
        "    public:\n"
        f"        explicit {element_name}();\n"
        f"        ~{element_name}() = default;\n"
        "\n"
        "    private:\n"
        "\n"
        "    };\n"
        "\n"
        f'#include "{element_name}.tpp"\n'
        "}\n"
    )


def render_tpp_file(element_name: str) -> str:
    return (
        "//\n"
        f"// Created by deanprangenberg on {created_on()}.\n"
        "//\n"
        "\n"
        "#pragma once\n"
        "\n"
        "template<typename V>\n"
        f"{element_name}<V>::{element_name}() {{\n"
        "\n"
        "}\n"
    )


def render_test_file(category_name: str, element_name: str) -> str:
    test_name = f"{category_name}_{element_name}_test"

    return (
        "//\n"
        f"// Created by deanprangenberg on {created_on()}.\n"
        "//\n"
        "\n"
        "#include <gtest/gtest.h>\n"
        f"#include <ventra/{category_name}/{element_name}.hpp>\n"
        "\n"
        "/*\n"
        f"TEST({test_name}, caseTest) {{\n"
        f"    ventra::{element_name}<int> object;\n"
        "\n"
        "    ///TODO add test\n"
        "\n"
        "    ///TODO ASSERT test\n"
        "    //ASSERT_EQ(...);\n"
        "}\n"
        "*/\n"
    )


def render_benchmark_file(category_name: str, element_name: str) -> str:
    benchmark_name = f"BM_{category_name}_{element_name}_construct"

    return (
        "//\n"
        f"// Created by deanprangenberg on {created_on()}.\n"
        "//\n"
        "\n"
        "#include <benchmark/benchmark.h>\n"
        f"#include <ventra/{category_name}/{element_name}.hpp>\n"
        "\n"
        f"static void {benchmark_name}(benchmark::State& state) {{\n"
        "    for (auto _ : state) {\n"
        f"        ventra::{element_name}<int> object;\n"
        "\n"
        "        benchmark::DoNotOptimize(&object);\n"
        "        benchmark::ClobberMemory();\n"
        "    }\n"
        "}\n"
        f"BENCHMARK({benchmark_name});\n"
    )


def render_doc_file(category_name: str, element_name: str) -> str:
    return (
        f"# `ventra::{element_name}<T>`\n"
        "\n"
        "Short description.\n"
        "\n"
        "## Header\n"
        "\n"
        "```cpp\n"
        f"#include <ventra/{element_name}.hpp>\n"
        "```\n"
        "\n"
        "## Overview\n"
        "\n"
        "- TODO describe purpose\n"
        "- TODO describe important properties\n"
        "\n"
        "## Example\n"
        "\n"
        "```cpp\n"
        f"ventra::{element_name}<int> object;\n"
        "```\n"
    )


def create_element_files(category_name: str, element_name: str) -> None:
    include_category_dir = ventra_include_dir / category_name
    test_category_dir = test_dir / category_name
    benchmark_category_dir = benchmark_dir / category_name
    docs_category_dir = docs_dir / category_name

    files_to_create = {
        include_category_dir / f"{element_name}.hpp": render_header_file(element_name),
        include_category_dir / f"{element_name}.tpp": render_tpp_file(element_name),
        test_category_dir / f"{element_name}_test.cpp": render_test_file(category_name, element_name),
        benchmark_category_dir / f"{element_name}_benchmark.cpp": render_benchmark_file(category_name, element_name),
        docs_category_dir / f"{element_name}.md": render_doc_file(category_name, element_name),
    }

    existing_files = [
        path.relative_to(root_dir).as_posix()
        for path in files_to_create
        if path.exists()
    ]

    if existing_files:
        fail(f"Refusing to overwrite existing files: {', '.join(existing_files)}")

    include_category_dir.mkdir(parents=True, exist_ok=True)
    test_category_dir.mkdir(parents=True, exist_ok=True)
    benchmark_category_dir.mkdir(parents=True, exist_ok=True)
    docs_category_dir.mkdir(parents=True, exist_ok=True)

    for path, content in files_to_create.items():
        path.write_text(content, encoding="utf-8")


def sorted_header_entries() -> list[str]:
    return sorted(
        path.relative_to(root_dir).as_posix()
        for path in ventra_include_dir.rglob("*")
        if path.is_file() and path.suffix in {".hpp", ".tpp"}
    )


def build_test_target_name(test_path: Path) -> str:
    relative_without_suffix = test_path.relative_to(test_dir).with_suffix("")
    target_suffix = "_".join(relative_without_suffix.parts)
    return f"libVentra_{target_suffix}"


def sorted_test_targets() -> list[tuple[str, str]]:
    test_paths = sorted(
        (
            path
            for path in test_dir.rglob("*")
            if path.is_file() and path.name.endswith("_test.cpp")
        ),
        key=lambda path: path.relative_to(test_dir).as_posix(),
    )

    return [
        (
            build_test_target_name(path),
            path.relative_to(root_dir).as_posix(),
        )
        for path in test_paths
    ]


def build_benchmark_target_name(benchmark_path: Path) -> str:
    relative_without_suffix = benchmark_path.relative_to(benchmark_dir).with_suffix("")
    target_suffix = "_".join(relative_without_suffix.parts)
    return f"libVentra_{target_suffix}"


def sorted_benchmark_targets() -> list[tuple[str, str]]:
    if not benchmark_dir.exists():
        return []

    benchmark_paths = sorted(
        (
            path
            for path in benchmark_dir.rglob("*")
            if path.is_file() and path.suffix == ".cpp"
        ),
        key=lambda path: path.relative_to(benchmark_dir).as_posix(),
    )

    return [
        (
            build_benchmark_target_name(path),
            path.relative_to(root_dir).as_posix(),
        )
        for path in benchmark_paths
    ]


def render_header_block() -> str:
    entries = "\n".join(f"    {entry}" for entry in sorted_header_entries())

    return (
        "target_sources(libVentra INTERFACE\n"
        "    FILE_SET HEADERS\n"
        "    BASE_DIRS include\n"
        "    FILES\n"
        f"{entries}\n"
        ")"
    )


def render_test_target_block() -> str:
    targets = sorted_test_targets()

    if not targets:
        return "\n"

    blocks = [
        (
            f"    add_executable({target_name} {test_path})\n"
            f"    target_link_libraries({target_name} PRIVATE libVentra GTest::gtest_main)"
        )
        for target_name, test_path in targets
    ]

    return "\n" + "\n\n".join(blocks) + "\n\n"


def render_discover_tests_block() -> str:
    return "".join(
        f"    gtest_discover_tests({target_name})\n"
        for target_name, _ in sorted_test_targets()
    )


def render_benchmark_section_body() -> str:
    targets = sorted_benchmark_targets()

    if not targets:
        return "\n    enable_testing()\n\n"

    executable_blocks = [
        (
            f"    add_executable({target_name} {benchmark_path})\n"
            f"    target_link_libraries({target_name} PRIVATE\n"
            "        libVentra\n"
            "        benchmark::benchmark\n"
            "        benchmark::benchmark_main\n"
            "        Threads::Threads\n"
            "    )"
        )
        for target_name, benchmark_path in targets
    ]

    add_test_lines = [
        f"""
            add_test(
                NAME {target_name}
                COMMAND {target_name}
                --benchmark_out=$<SHELL_PATH:${{CMAKE_CURRENT_BINARY_DIR}}/{target_name}.json>
                --benchmark_out_format=json
                COMMAND_EXPAND_LISTS
            )
        """
        for target_name, _ in targets
    ]

    return (
        "\n"
        + "\n\n".join(executable_blocks)
        + "\n\n"
        + "    enable_testing()\n\n"
        + "\n".join(add_test_lines)
        + "\n\n"
    )


def replace_once(pattern: re.Pattern[str], text: str, replacement: str, section_name: str) -> str:
    if not pattern.search(text):
        fail(f"Could not find the {section_name} section in {cmakelist_file.name}.")

    return pattern.sub(replacement, text, count=1)


def replace_wrapped_section(
        pattern: re.Pattern[str],
        text: str,
        body: str,
        section_name: str,
) -> str:
    if not pattern.search(text):
        fail(f"Could not find the {section_name} section in {cmakelist_file.name}.")

    return pattern.sub(
        lambda match: f"{match.group('prefix')}{body}{match.group('suffix')}",
        text,
        count=1,
    )


def sync_cmakelists() -> None:
    cmakelist_data = cmakelist_file.read_text(encoding="utf-8")

    updated_cmakelist_data = replace_once(
        header_block_re,
        cmakelist_data,
        render_header_block(),
        "target_sources(libVentra INTERFACE)",
    )

    updated_cmakelist_data = replace_wrapped_section(
        test_target_block_re,
        updated_cmakelist_data,
        render_test_target_block(),
        "test target",
    )

    updated_cmakelist_data = replace_wrapped_section(
        discover_tests_block_re,
        updated_cmakelist_data,
        render_discover_tests_block(),
        "gtest_discover_tests",
    )

    updated_cmakelist_data = replace_wrapped_section(
        benchmark_section_re,
        updated_cmakelist_data,
        render_benchmark_section_body(),
        "benchmark targets",
    )

    if not updated_cmakelist_data.endswith("\n"):
        updated_cmakelist_data += "\n"

    if updated_cmakelist_data != cmakelist_data:
        cmakelist_file.write_text(updated_cmakelist_data, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a new categorized libVentra element with tests, benchmarks, docs, and synchronized CMake blocks.",
    )
    parser.add_argument(
        "category",
        nargs="?",
        help="Category directory below include/ventra and tests.",
    )
    parser.add_argument(
        "name",
        nargs="?",
        help="Name of the new element to create. Uses the interactive prompt when omitted.",
    )
    parser.add_argument(
        "--sync-only",
        action="store_true",
        help="Only regenerate the sorted CMakeLists.txt sections from the current project layout.",
    )

    return parser.parse_args()


def main() -> None:
    ensure_project_layout()
    args = parse_args()

    if args.sync_only:
        sync_cmakelists()
        print("Synchronized CMakeLists.txt.")
        return

    raw_category = args.category if args.category is not None else input("Enter category name: ")
    raw_name = args.name if args.name is not None else input("Enter new element name: ")

    category_name = normalize_identifier(raw_category, "Category name")
    element_name = normalize_identifier(raw_name, "Element name")

    create_element_files(category_name, element_name)
    sync_cmakelists()

    print(
        f"Created {element_name} in category {category_name} with test, benchmark, and doc files and synchronized CMakeLists.txt."
    )


if __name__ == "__main__":
    main()
