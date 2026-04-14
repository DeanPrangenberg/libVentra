# libVentra

`libVentra` is a small header-only C++23 utility library focused on concurrency primitives and lightweight container utilities.

It currently ships with:
<!-- AUTO-DATA-STRUCTURES:BEGIN -->
- [vector/vector]({Path(f"./docs/vector/vector.md")})
- [vector/concurrent_smart_vector]({Path(f"./docs/vector/concurrent_smart_vector.md")})
- [vector/concurrent_atomic_vector]({Path(f"./docs/vector/concurrent_atomic_vector.md")})
- [stack/stack](docs/stack/stack.md)<!-- AUTO-DATA-STRUCTURES:END -->

## Requirements

### To use the library
- C++23
- CMake 4.1 or newer

The library itself is header-only. Tests use GoogleTest and are only enabled when the project is built as the top-level CMake project.

### Benchmarks
- C++23
- CMake 4.1 or newer
- cpupower
- python 3.14 + matplotlib

## Build And Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

There is also a helper script that builds, tests, and installs the library:

```bash
./scripts/build_and_install_lib.sh
```

For a fresh machine, the repository also includes:

```bash
./scripts/install_online_script.sh
```

## CMake Integration

### Use via `add_subdirectory`

```cmake
add_subdirectory(path/to/libVentra-old)
target_link_libraries(your_target PRIVATE libVentra)
```

### Use after installation

```cmake
find_package(libVentra-old CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE Ventra::libVentra)
```

## Status

The repository includes unit tests and benchmarks for all modules, including multithreaded stress tests.
