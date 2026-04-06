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


def create_element_files(category_name: str, element_name: str) -> None:
    include_category_dir = ventra_include_dir / category_name
    test_category_dir = test_dir / category_name

    files_to_create = {
        include_category_dir / f"{element_name}.hpp": render_header_file(element_name),
        include_category_dir / f"{element_name}.tpp": render_tpp_file(element_name),
        test_category_dir / f"{element_name}_test.cpp": render_test_file(category_name, element_name),
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
            f"  add_executable({target_name} {test_path})\n"
            f"  target_link_libraries({target_name} PRIVATE libVentra GTest::gtest_main)"
        )
        for target_name, test_path in targets
    ]

    return "\n" + "\n\n".join(blocks) + "\n\n"


def render_discover_tests_block() -> str:
    return "".join(
        f"  gtest_discover_tests({target_name})\n"
        for target_name, _ in sorted_test_targets()
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

    if not updated_cmakelist_data.endswith("\n"):
        updated_cmakelist_data += "\n"

    if updated_cmakelist_data != cmakelist_data:
        cmakelist_file.write_text(updated_cmakelist_data, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a new categorized libVentra element and synchronize sorted CMakeLists.txt blocks.",
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
        f"Created {element_name} in category {category_name} and synchronized CMakeLists.txt."
    )


if __name__ == "__main__":
    main()