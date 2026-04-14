//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

template<typename V>
constexpr size_t concurrent_atomic_vector<V>::calc_chunk_idx(const size_t idx) noexcept {
    if (idx < first_chunk_size_) {
        return 0;
    }

    return std::bit_width((idx / first_chunk_size_) + 1) - 1;
}

template<typename V>
constexpr size_t concurrent_atomic_vector<V>::calc_local_idx(const size_t idx) noexcept {
    return calc_local_idx(idx, calc_chunk_idx(idx));
}

template<typename V>
constexpr size_t concurrent_atomic_vector<V>::calc_local_idx(const size_t idx, const size_t chunk_idx) noexcept {
    return idx - first_chunk_size_ * ((1ULL << chunk_idx) - 1);
}

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector() noexcept {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(nullptr, std::memory_order_relaxed);
    }

    ensure_capacity_for_index(first_chunk_size_ - 1);
}

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector(const size_t initial_capacity) {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(nullptr, std::memory_order_relaxed);
    }

    if (initial_capacity > 0) {
        ensure_capacity_for_index(initial_capacity - 1);
    }
}

template<typename V>
concurrent_atomic_vector<V>::concurrent_atomic_vector(const size_t count, const V& val) {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(nullptr, std::memory_order_relaxed);
    }

    if (count == 0) {
        return;
    }

    ensure_capacity_for_index(count - 1);
    size_.store(count, std::memory_order_release);

    for (size_t i = 0; i < count; ++i) {
        store(i, val, std::memory_order_relaxed);
    }
}

template<typename V>
concurrent_atomic_vector<V>::~concurrent_atomic_vector() {
    for (size_t i = 0; i < num_chunks_; ++i) {
        auto* chunk_ptr = chunks_[i].load(std::memory_order_acquire);
        if (chunk_ptr == nullptr) {
            break;
        }

        delete[] chunk_ptr;
    }
}

template<typename V>
V concurrent_atomic_vector<V>::load(const size_t idx, const std::memory_order order) const {
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    return chunk_ptr[local_idx].val.load(order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::try_load(const size_t idx, const std::memory_order order) const {
    if (!is_valid_idx(idx)) {
        return std::nullopt;
    }

    return load(idx, order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::front(const std::memory_order order) const {
    if (empty()) {
        return std::nullopt;
    }

    return load(0, order);
}

template<typename V>
std::optional<V> concurrent_atomic_vector<V>::back(const std::memory_order order) const {
    if (empty()) {
        return std::nullopt;
    }

    return load(size_.load(std::memory_order_acquire) - 1, order);
}

template<typename V>
size_t concurrent_atomic_vector<V>::size() const noexcept {
    return size_.load(std::memory_order_acquire);
}

template<typename V>
size_t concurrent_atomic_vector<V>::capacity() const noexcept {
    return capacity_.load(std::memory_order_acquire);
}

template<typename V>
bool concurrent_atomic_vector<V>::empty() const noexcept {
    return size_.load(std::memory_order_acquire) == 0;
}

template<typename V>
void concurrent_atomic_vector<V>::reserve(const size_t new_capacity) {
    if (new_capacity == 0 || new_capacity <= capacity()) {
        return;
    }

    ensure_capacity_for_index(new_capacity - 1);
}

template<typename V>
void concurrent_atomic_vector<V>::push_back(const V& val, const std::memory_order order) {
    const size_t idx = size_.fetch_add(1, std::memory_order_relaxed);
    ensure_capacity_for_index(idx);
    store(idx, val, order);
}

template<typename V>
void concurrent_atomic_vector<V>::push_back(V&& val, std::memory_order order) {
    const size_t idx = size_.fetch_add(1, std::memory_order_relaxed);
    ensure_capacity_for_index(idx);
    store(idx, std::move(val), order);
}

template<typename V>
void concurrent_atomic_vector<V>::store(const size_t idx, V&& val, const std::memory_order order) {
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.store(std::move(val), order);
}

template<typename V>
void concurrent_atomic_vector<V>::store(const size_t idx, const V& val, const std::memory_order order) {
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.store(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.fetch_add(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.fetch_sub(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.fetch_and(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.fetch_or(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    chunk_ptr[local_idx].val.fetch_xor(val, order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    return chunk_ptr[local_idx].val.compare_exchange_weak(expected, desired, store_order, load_order);
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
    auto* chunk_ptr = load_chunk_ptr_unchecked(idx);
    const size_t local_idx = calc_local_idx(idx);

    return chunk_ptr[local_idx].val.compare_exchange_strong(expected, desired, store_order, load_order);
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
typename concurrent_atomic_vector<V>::padded_atomic * concurrent_atomic_vector<V>::load_chunk_ptr_unchecked(size_t idx) const {
    const size_t chunk_idx = calc_chunk_idx(idx);
    auto* chunk_ptr = chunks_[chunk_idx].load(std::memory_order_acquire);

    if (chunk_ptr == nullptr) {
        throw std::out_of_range("Chunk not allocated");
    }

    return chunk_ptr;
}

template<typename V>
bool concurrent_atomic_vector<V>::is_valid_idx(const size_t idx) const noexcept {
    return idx < size_.load(std::memory_order_acquire);
}

template<typename V>
void concurrent_atomic_vector<V>::ensure_capacity_for_index(const size_t idx) {
    const size_t target_chunk_idx = calc_chunk_idx(idx);

    if (target_chunk_idx >= num_chunks_) {
        throw std::overflow_error("Vector capacity exceeded!");
    }

    for (size_t i = 0; i <= target_chunk_idx; ++i) {
        if (chunks_[i].load(std::memory_order_acquire) != nullptr) {
            continue;
        }

        // Spinlock
        while (allocation_lock_.test_and_set(std::memory_order_acquire)) {}

        if (chunks_[i].load(std::memory_order_acquire) == nullptr) {
            size_t chunk_cap;
            if (i == 0) {
                chunk_cap = first_chunk_size_;
            } else {
                chunk_cap = first_chunk_size_ << i;
            }

            auto* new_chunk = new padded_atomic[chunk_cap];

            chunks_[i].store(new_chunk, std::memory_order_release);
            capacity_.fetch_add(chunk_cap, std::memory_order_relaxed);
        }

        allocation_lock_.clear(std::memory_order_release);
    }
}
