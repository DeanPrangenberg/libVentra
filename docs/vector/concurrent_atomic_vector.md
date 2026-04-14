# `ventra::concurrent_atomic_vector<T>`

`ventra::concurrent_atomic_vector<T>` is a header-only append-only vector for
types that can be stored in `std::atomic<T>`.
It is designed for concurrent writers and atomic in-place updates without
global locking.

## Header

```cpp
#include <ventra/vector/concurrent_atomic_vector.hpp>
```

## Overview

- Append-only container with atomic per-element storage
- Multiple threads can call `push_back()` concurrently
- Existing elements can be updated with `store()`, `fetch_*()`, and
  `compare_exchange_*()`
- Read helpers such as `try_load()`, `front()`, and `back()` return
  `std::optional<T>`
- No reallocation or element moves after construction

The container lives in namespace `ventra` and is implemented entirely in
`include/ventra/vector/concurrent_atomic_vector.hpp` and
`include/ventra/vector/concurrent_atomic_vector.tpp`.

## Basic Properties

- The implementation stores each element as `std::atomic<T>`.
- `push_back()` reserves an index with an atomic counter and then publishes the
  value by setting an internal `ready` flag.
- `load()`, `store()`, `fetch_*()`, and `compare_exchange_*()` operate directly
  on the atomic element at a given index.
- The non-`try_` functions do not perform bounds checks.
- The `try_` functions fail gracefully for indexes `>= size()`.
- `capacity()` is a fixed theoretical maximum, not a growable runtime capacity.
- Copy construction and copy assignment are deleted.

## Platform And Type Requirements

- C++23
- Linux-style `mmap()` / `munmap()` support, because the implementation uses
  `<sys/mman.h>`
- `T` must be usable with `std::atomic<T>`; in practice this usually means a
  trivially copyable type
- `fetch_add()` / `fetch_sub()` / `fetch_and()` / `fetch_or()` / `fetch_xor()`
  are only available when the corresponding atomic operation exists for `T`

## Construction

### Default construction

```cpp
ventra::concurrent_atomic_vector<int> values;
```

Creates an empty vector with `size() == 0`.

### Capacity construction

```cpp
ventra::concurrent_atomic_vector<int> values(1024);
```

Creates an empty vector.
In the current implementation, this constructor does not eagerly reserve or
initialize 1024 elements; it only uses the same backing allocation strategy as
the default constructor.

### Fill construction

```cpp
ventra::concurrent_atomic_vector<int> values(4, 7);
```

Creates `count` elements and initializes each one with the provided value.

### Initializer-list construction

```cpp
ventra::concurrent_atomic_vector<int> values{1, 2, 3};
```

Creates a vector from the given values.

## Element Access

### `load(size_t idx, std::memory_order order = std::memory_order_acquire)`

Loads the atomic value at `idx`.
The index must already refer to a constructed element.

### `try_load(size_t idx, std::memory_order order = std::memory_order_acquire)`

Returns the value at `idx` when the index is valid and the element has been
published.
Returns `std::nullopt` otherwise.

### `front()` / `back()`

Return the first or last published value as `std::optional<T>`.
Return `std::nullopt` when the vector is empty.

## Capacity

### `empty()`

Returns `true` when `size() == 0`.

### `size()`

Returns the number of reserved element slots that have been appended so far.

### `capacity()`

Returns the fixed maximum element count:

```cpp
64ULL * 1024 * 1024 * 1024 / sizeof(T)
```

This is not the same as `std::vector` capacity growth.
The implementation reserves a large virtual memory region up front and does not
reallocate as elements are appended.

### `reserve(size_t new_capacity)`

Validates that `new_capacity` does not exceed the fixed maximum.
In the current implementation it does not commit memory or change the return
value of `capacity()`.

## Modifiers

### `push_back(const T&)` / `push_back(T&&)`

Appends a new element using an atomic index increment.
This is the main concurrent write path.

### `store(idx, value)` and `try_store(idx, value)`

Overwrite an existing element with the provided memory order.
`try_store()` returns `false` for an invalid index.

### `fetch_add()` / `fetch_sub()` / `fetch_and()` / `fetch_or()` / `fetch_xor()`

Perform the corresponding atomic read-modify-write operation on an existing
element.
Each operation also has a `try_` variant that returns `false` for an invalid
index.

### `compare_exchange_weak()` / `compare_exchange_strong()`

Perform atomic compare-exchange on an existing element and return whether the
exchange succeeded.
The `try_` variants return `std::nullopt` for an invalid index.

### `clear_NOT_THREAD_SAFE()`

Destroys all currently stored elements and resets the vector to empty.
This function must not be called concurrently with any other operation.

## Iteration

The container supports range-based `for` loops:

```cpp
ventra::concurrent_atomic_vector<int> values{10, 20, 30};

for (int value : values) {
    // value is a copy loaded atomically
}
```

Important iterator properties:

- Dereferencing an iterator returns `T` by value, not by reference.
- Iteration uses `load()` internally.
- `end()` is based on the size observed when it is called.
- Iterators are read-only helpers, not raw pointers into contiguous storage.

## Concurrency Notes

- Concurrent `push_back()` from multiple threads is supported.
- Concurrent atomic updates on already inserted elements are supported through
  the atomic member functions.
- Reading with `try_load()` is safer than `load()` when another thread may still
  be publishing an element.
- The non-`try_` APIs assume the index is valid and the element exists.
- `clear_NOT_THREAD_SAFE()` is explicitly not safe under concurrent access.

## Complexity

- `empty()`, `size()`, `capacity()`, `load()`, `try_load()`, `front()`,
  `back()`: `O(1)`
- `push_back()`: `O(1)`
- `store()`, `fetch_*()`, `compare_exchange_*()`: `O(1)`
- `clear_NOT_THREAD_SAFE()`: `O(n)`

## Example

```cpp
#include <iostream>
#include <thread>
#include <vector>

#include <ventra/vector/concurrent_atomic_vector.hpp>

int main() {
    ventra::concurrent_atomic_vector<int> values;

    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < 4; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            for (int i = 0; i < 100; ++i) {
                values.push_back(thread_id * 100 + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "size: " << values.size() << '\n';

    if (auto first = values.front()) {
        std::cout << "first published value: " << *first << '\n';
    }
}
```

## Notes

- This container is header-only.
- The implementation is append-only; there is no erase operation.
- The constructor allocates backing storage with `mmap()`, so construction can
  fail with `std::bad_alloc` if the mapping cannot be created.
- The current implementation is tuned for atomic-friendly value types rather
  than general-purpose objects.

## Benchmarks

<!-- AUTO-BENCHMARKS:BEGIN -->

### Int_push_back_reserved

![Benchmark Int_push_back_reserved](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back_reserved.png)


### Int_push_back

![Benchmark Int_push_back](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back.png)

<!-- AUTO-BENCHMARKS:END -->
