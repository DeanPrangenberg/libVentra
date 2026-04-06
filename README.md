# libVentra

`libVentra` is a small header-only C++23 utility library focused on concurrency primitives and lightweight container utilities.

It currently ships with:

- `ventra::chan<T>`: a bounded blocking channel with FIFO and priority send support
- `ventra::thread_pool`: a future-based thread pool built on top of `chan`
- `ventra::hash_map<K, V>`: a simple hash map with separate chaining and rehashing
- `ventra::concurrent_hash_map<K, V>`: a thread-safe hash map for concurrent insert/read/remove workloads

## Requirements

- C++23
- CMake 4.1 or newer

The library itself is header-only. Tests use GoogleTest and are only enabled when the project is built as the top-level CMake project.

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

## Modules

### `ventra::chan<T>`

A bounded blocking channel for producer/consumer style communication.

Features:

- blocking `send()` and `recv()`
- `sendPriority()` for LIFO-style priority insertion
- `close()` for graceful shutdown
- `forceClose()` for immediate shutdown
- `isClosed()`, `isEmpty()`, and `isDone()` helpers

Example:

```cpp
#include <thread>
#include <vector>

#include <ventra/chan.hpp>

int main() {
    ventra::chan<int> ch(4);
    std::vector<int> out;

    std::jthread producer([&] {
        for (int i = 1; i <= 5; ++i) {
            ch.send(i);
        }
        ch.close();
    });

    std::jthread consumer([&] {
        while (!ch.isDone()) {
            try {
                out.push_back(ch.recv());
            } catch (const std::runtime_error&) {
                break;
            }
        }
    });
}
```

### `ventra::thread_pool`

A `std::future` based thread pool that supports resizing and priority tasks.

Features:

- configurable worker count and queue capacity
- `enqueue()` for regular jobs
- `enqueuePriority()` for higher-priority jobs
- graceful shutdown in the destructor
- `forceStop()` to abort queue processing

Example:

```cpp
#include <iostream>
#include <thread>

#include <ventra/thread_pool.hpp>

int main() {
    ventra::thread_pool pool(std::thread::hardware_concurrency(), 32);

    auto a = pool.enqueue([](int x, int y) { return x + y; }, 10, 32);
    auto b = pool.enqueuePriority([] { return 99; });

    std::cout << a.get() << '\n';
    std::cout << b.get() << '\n';
}
```

### `ventra::hash_map<K, V>`

A lightweight non-concurrent hash map with:

- insert/update support for lvalues and rvalues
- `get()` overloads with exception or fallback return
- `remove()`
- `getKeys()`, `getVals()`, `getPairs()`
- `isKey()`, `isVal()`
- `operator[]`

Example:

```cpp
#include <iostream>
#include <string>

#include <ventra/hash_map.hpp>

int main() {
    ventra::hash_map<std::string, int> map;

    map.insert("alpha", 1);
    map["beta"] = 2;

    std::cout << map.get("alpha") << '\n';
    std::cout << map.get("missing", -1) << '\n';
}
```

### `ventra::concurrent_hash_map<K, V>`

A thread-safe hash map using per-bucket locks plus a rehash lock to protect structural changes.

Supported operations:

- concurrent `insert()`
- concurrent `get()`
- concurrent `remove()`
- snapshot-style `getKeys()`, `getVals()`, `getPairs()`
- `isKey()`, `isVal()`
- `operator[]`

Example:

```cpp
#include <thread>
#include <vector>

#include <ventra/concurrent_hash_map.hpp>

int main() {
    ventra::concurrent_hash_map<int, int> map(4);
    std::vector<std::jthread> workers;

    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&, t] {
            for (int i = 0; i < 1000; ++i) {
                const int key = t * 1000 + i;
                map.insert(key, key * 2);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    for (const auto& [key, value] : map.getPairs()) {
        (void)key;
        (void)value;
    }
}
```

## Project Layout

```text
include/ventra/
  chan.hpp
  thread_pool.hpp
  hash_map.hpp
  concurrent_hash_map.hpp
tests/
scripts/
```

## Status

The repository includes unit tests for all four modules, including multithreaded stress tests for `concurrent_hash_map`.
