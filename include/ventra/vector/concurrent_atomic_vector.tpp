//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector() {
    const size_t max_bytes = MAX_CAPACITY * sizeof(element);

    // Create virtual space with linux syscalls
    void* ptr = mmap(
        nullptr,
        max_bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
        -1, 0
    );

    // Check if failed
    if (ptr == MAP_FAILED) {
        throw std::bad_alloc();
    }

    data_ = static_cast<element*>(ptr);
}

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector(const size_t initial_capacity) : concurrent_atomic_vector() {

}

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector(const size_t count, const V& val) : concurrent_atomic_vector() {
    if (count == 0) {
        return;
    }

    // set size
    size_.store(count, std::memory_order_release);

    // fill with val
    for (size_t i = 0; i < count; ++i) {
        new (&data_[i].val) std::atomic<V>(val);
        data_[i].ready.store(true, std::memory_order_release);
    }
}

template<typename V>
concurrent_atomic_vector<V>::~concurrent_atomic_vector() {

    // Deconstruct all elements if not trivially destructible
    if constexpr (!std::is_trivially_destructible_v<std::atomic<V>>) {
        const size_t current_size = size_.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_size; ++i) {
            std::destroy_at(&data_[i].val);
        }
    }

    const size_t max_bytes = MAX_CAPACITY * sizeof(element);
    munmap(data_, max_bytes);
}

template<typename V>
V concurrent_atomic_vector<V>::load(const size_t idx, const std::memory_order order) const {
    return data_[idx].val.load(order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::try_load(const size_t idx, const std::memory_order order) const {
    if (!is_valid_idx(idx)) {
        return std::nullopt;
    }

    if (!data_[idx].ready.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    return load(idx, order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::front(const std::memory_order order) const {
    if (empty()) {
        return std::nullopt;
    }

    return try_load(0, order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::back(const std::memory_order order) const {
    if (empty()) {
        return std::nullopt;
    }

    return try_load(size_.load(std::memory_order_acquire) - 1, order);
}

template<typename V>
size_t concurrent_atomic_vector<V>::size() const noexcept {
    return size_.load(std::memory_order_acquire);
}

template<typename V>
size_t concurrent_atomic_vector<V>::capacity() noexcept {
    return MAX_CAPACITY;
}

template<typename V>
bool concurrent_atomic_vector<V>::empty() const noexcept {
    return size_.load(std::memory_order_acquire) == 0;
}

template<typename V>
void concurrent_atomic_vector<V>::reserve(const size_t new_capacity) {
    if (new_capacity > MAX_CAPACITY) {
        throw std::length_error("Vector capacity exceeded MAX_CAPACITY of 64GB!");
    }
}

template<typename V>
void concurrent_atomic_vector<V>::push_back(const V& val) {
    const size_t idx = size_.fetch_add(1, std::memory_order_relaxed);

    new (&data_[idx].val) std::atomic<V>(val);

    data_[idx].ready.store(true, std::memory_order_release);
}

template<typename V>
void concurrent_atomic_vector<V>::push_back(V&& val) {
    const size_t idx = size_.fetch_add(1, std::memory_order_relaxed);

    new (&data_[idx].val) std::atomic<V>(std::move(val));

    data_[idx].ready.store(true, std::memory_order_release);
}

template<typename V>
void concurrent_atomic_vector<V>::store(const size_t idx, V&& val, const std::memory_order order) {

    data_[idx].val.store(std::move(val), order);
}

template<typename V>
void concurrent_atomic_vector<V>::store(const size_t idx, const V& val, const std::memory_order order) {

    data_[idx].val.store(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_store(const size_t idx, V&& val, const std::memory_order order) {
    if (!is_valid_idx(idx)) {
        return false;
    }

    store(idx, std::move(val), order);
    return true;
}

template<typename V>
bool concurrent_atomic_vector<V>::try_store(const size_t idx, const V& val, const std::memory_order order) {
    if (!is_valid_idx(idx)) {
        return false;
    }

    store(idx, val, order);
    return true;
}

template<typename V>
void concurrent_atomic_vector<V>::fetch_add(const size_t idx, V val, const std::memory_order order) requires SupportsFetchAdd<V> {

    data_[idx].val.fetch_add(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_fetch_add(const size_t idx, V val, const std::memory_order order) requires SupportsFetchAdd<V> {
    if (!is_valid_idx(idx)) {
        return false;
    }

    fetch_add(idx, val, order);
    return true;
}

template<typename V>
void concurrent_atomic_vector<V>::fetch_sub(const size_t idx, V val, const std::memory_order order) requires SupportsFetchSub<V> {

    data_[idx].val.fetch_sub(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_fetch_sub(const size_t idx, V val, const std::memory_order order) requires SupportsFetchSub<V> {
    if (!is_valid_idx(idx)) {
        return false;
    }

    fetch_sub(idx, val, order);
    return true;
}

template<typename V>
void concurrent_atomic_vector<V>::fetch_and(const size_t idx, V val, const std::memory_order order) requires SupportsFetchAnd<V> {

    data_[idx].val.fetch_and(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_fetch_and(const size_t idx, V val, const std::memory_order order) requires SupportsFetchAnd<V> {
    if (!is_valid_idx(idx)) {
        return false;
    }

    fetch_and(idx, val, order);
    return true;
}

template<typename V>
void concurrent_atomic_vector<V>::fetch_or(const size_t idx, V val, const std::memory_order order) requires SupportsFetchOr<V> {

    data_[idx].val.fetch_or(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_fetch_or(const size_t idx, V val, const std::memory_order order) requires SupportsFetchOr<V> {
    if (!is_valid_idx(idx)) {
        return false;
    }

    fetch_or(idx, val, order);
    return true;
}

template<typename V>
void concurrent_atomic_vector<V>::fetch_xor(const size_t idx, V val, const std::memory_order order) requires SupportsFetchXor<V> {
    data_[idx].val.fetch_xor(val, order);
}

template<typename V>
bool concurrent_atomic_vector<V>::try_fetch_xor(const size_t idx, V val, const std::memory_order order) requires SupportsFetchXor<V> {
    if (!is_valid_idx(idx)) {
        return false;
    }

    fetch_xor(idx, val, order);
    return true;
}

template<typename V>
std::optional<bool> concurrent_atomic_vector<V>::compare_exchange_weak(
    const size_t idx,
    V& expected,
    V desired,
    const std::memory_order load_order,
    const std::memory_order store_order
) {

    return data_[idx].val.compare_exchange_weak(expected, desired, store_order, load_order);
}

template<typename V>
std::optional<bool> concurrent_atomic_vector<V>::try_compare_exchange_weak(
    const size_t idx,
    V& expected,
    V desired,
    const std::memory_order load_order,
    const std::memory_order store_order
) {
    if (!is_valid_idx(idx)) {
        return std::nullopt;
    }

    return compare_exchange_weak(idx, expected, desired, load_order, store_order);
}

template<typename V>
std::optional<bool> concurrent_atomic_vector<V>::compare_exchange_strong(
    const size_t idx,
    V& expected,
    V desired,
    const std::memory_order load_order,
    const std::memory_order store_order
) {

    return data_[idx].val.compare_exchange_strong(expected, desired, store_order, load_order);
}

template<typename V>
std::optional<bool> concurrent_atomic_vector<V>::try_compare_exchange_strong(
    const size_t idx,
    V& expected,
    V desired,
    const std::memory_order load_order,
    const std::memory_order store_order
) {
    if (!is_valid_idx(idx)) {
        return std::nullopt;
    }

    return compare_exchange_strong(idx, expected, desired, load_order, store_order);
}

template<typename V>
bool concurrent_atomic_vector<V>::is_valid_idx(const size_t idx) const noexcept {
    return idx < size_.load(std::memory_order_acquire);
}
