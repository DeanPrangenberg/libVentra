# `ventra::vector<T>`

`ventra::vector<T>` is a small header-only dynamic array with contiguous storage.
It offers a focused subset of `std::vector` and exposes raw pointers as iterators.

## Header

```cpp
#include <ventra/vector/vector.hpp>
```

## Overview

- Contiguous storage for `T`
- Copy and move support
- Bounds-checked access via `at()`
- Raw-pointer iteration via `begin()` and `end()`
- Automatic growth for `push_back()` and `emplace_back()`
- Manual capacity management via `reserve()` and `shrink_to_fit()`

The container lives in namespace `ventra` and is implemented entirely in
`include/ventra/vector/vector.hpp` and `include/ventra/vector/vector.tpp`.

## Basic Properties

- `data()` points to contiguous memory.
- `begin()` and `end()` return `T*` / `const T*`.
- For an empty vector, `data()`, `begin()`, and `end()` return `nullptr`.
- The class is not thread-safe.
- The API is intentionally smaller than `std::vector`.

## Type Requirements

Some operations require additional capabilities from `T`:

- `vector(size_t size)` and growing `resize()` require `T` to be default-constructible.
- `vector(size_t count, value)`, `push_back()`, and `insert()` require `T` to be
  constructible from the provided value.
- `operator==` requires `T` to support equality comparison.

## Construction

### Default construction

```cpp
ventra::vector<int> values;
```

Creates an empty vector with `size() == 0` and `capacity() == 0`.

### Size construction

```cpp
ventra::vector<int> values(5);
```

Creates `5` default-initialized elements.

### Fill construction

```cpp
ventra::vector<std::string> values(3, "ventra");
```

Creates `count` copies of the provided value.

### Initializer-list construction

```cpp
ventra::vector<int> values{1, 2, 3, 4};
```

Creates a vector from the given elements.

### Copy and move

- Copy construction duplicates all elements.
- Move construction transfers ownership of the internal storage.

## Element Access

### `at(size_t idx)`

Returns a reference to the element at `idx`.
Throws `std::out_of_range` if `idx >= size()`.

### `operator[](size_t idx)`

Returns a reference without bounds checking.
Access with an invalid index is undefined behavior.

### `front()` / `back()`

Return references to the first or last element.
Calling these on an empty vector is undefined behavior.

### `data()`

Returns the underlying contiguous memory block.
Returns `nullptr` when the vector is empty.

## Capacity

### `empty()`

Returns `true` when `size() == 0`.

### `size()`

Returns the number of constructed elements.

### `capacity()`

Returns the number of elements that can be stored without reallocation.

### `reserve(size_t new_capacity)`

Increases capacity when `new_capacity > capacity()`.
Does not change `size()`.

### `shrink_to_fit()`

Reduces capacity to `size()`.

### `resize(size_t new_size)`

- If `new_size < size()`, trailing elements are destroyed.
- If `new_size > size()`, new elements are default-constructed.
- Capacity grows automatically when needed.

## Modifiers

### `emplace_back(Args&&... args)`

Constructs a new element at the end and returns a reference to it.
This is the preferred append operation for non-default-constructible or
expensive-to-copy types.

### `push_back(value)`

Appends a value to the end of the vector.

### `pop_back()`

Removes the last element.
If the vector is empty, the function does nothing.

### `pop(size_t idx)`

Removes the element at index `idx`.
If `idx` is out of range, the function does nothing.

### `insert(T* pos, value)`

Inserts a new element before `pos`.

Requirements:

- `pos` must be a pointer obtained from this vector.
- `pos` must be in the range `[begin(), end()]`.

Behavior:

- Inserting at `end()` appends the element.
- An invalid position throws `std::out_of_range`.

### `erase(T* pos)`

Removes the element at `pos` and returns a pointer to the next element.

Behavior:

- If `pos` is invalid or the vector is empty, the function returns `pos`
  unchanged.
- Erasing the last element returns `end()` after removal.

### `clear()`

Destroys all elements and sets `size()` to `0`.
Allocated capacity is kept.

## Iteration

The vector can be used directly in range-based `for` loops:

```cpp
ventra::vector<int> values{10, 20, 30};

for (int value : values) {
    // ...
}
```

Because iterators are raw pointers:

- any reallocation invalidates all iterators, references, and pointers
- `insert()`, `erase()`, and `resize()` may invalidate iterators at or after the
  modified position
- `clear()` invalidates references and pointers to stored elements

## Comparison And Assignment

- Copy assignment uses deep copy semantics.
- Move assignment transfers ownership of the internal buffer.
- `operator==` compares size first, then elements one by one.

## Complexity

- `at()`, `operator[]`, `front()`, `back()`, `data()`, `empty()`, `size()`,
  `capacity()`: `O(1)`
- `push_back()` / `emplace_back()`: amortized `O(1)`, worst case `O(n)`
- `insert()` / `erase()` / `pop(idx)`: `O(n)`
- `reserve()` / `shrink_to_fit()` / copy construction / copy assignment: `O(n)`
- Move construction / move assignment: `O(1)`

## Example

```cpp
#include <iostream>
#include <string>

#include <ventra/vector/vector.hpp>

int main() {
    ventra::vector<std::string> names;

    names.emplace_back("Ada");
    names.push_back("Linus");
    names.insert(names.begin() + 1, "Grace");

    for (const auto& name : names) {
        std::cout << name << '\n';
    }

    names.erase(names.begin());
    std::cout << "first: " << names.front() << '\n';
    std::cout << "size: " << names.size() << '\n';
}
```

## Notes

- This container is header-only and intended for C++23 builds.
- `front()`, `back()`, and `operator[]` do not perform bounds checks.
- `at()` is the safe access function when invalid indexes are possible.
