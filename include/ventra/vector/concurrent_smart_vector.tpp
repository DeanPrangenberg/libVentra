//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

template<typename V>
constexpr size_t concurrent_smart_vector<V>::calc_chunk_idx(const size_t idx) noexcept {
    if (idx < first_chunk_size_) {
        return 0;
    }

    size_t remaining_idx = idx - first_chunk_size_;

    size_t current_chunk_idx = 1;
    size_t current_chunk_capacity = first_chunk_size_ * 2;

    while (remaining_idx >= current_chunk_capacity) {
        remaining_idx -= current_chunk_capacity;
        current_chunk_capacity *= 2;
        current_chunk_idx++;
    }

    return current_chunk_idx;
}


template<typename V>
constexpr size_t concurrent_smart_vector<V>::calc_local_idx(const size_t idx, const size_t chunk_idx) noexcept {
    if (chunk_idx == 0) {
        return idx;
    }

    size_t remaining_idx = idx - first_chunk_size_;
    size_t current_chunk_capacity = first_chunk_size_ * 2;

    for (size_t i = 1; i < chunk_idx; ++i) {
        remaining_idx -= current_chunk_capacity;
        current_chunk_capacity *= 2;
    }

    return remaining_idx;
}


template<typename V>
concurrent_smart_vector<V>::concurrent_smart_vector() noexcept {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(
            static_cast<chunk_type>(nullptr),
            std::memory_order_relaxed
        );
    }

    ensure_capacity_for_index(first_chunk_size_ - 1);
}


template<typename V>
concurrent_smart_vector<V>::concurrent_smart_vector(const size_t initial_capacity) {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(
            static_cast<chunk_type>(nullptr),
            std::memory_order_relaxed
        );
    }

    ensure_capacity_for_index(initial_capacity);
}


template<typename V>
template<typename ValType>
concurrent_smart_vector<V>::concurrent_smart_vector(const size_t count, ValType &&val) {
    for (size_t i = 0; i < num_chunks_; ++i) {
        chunks_[i].store(
            static_cast<chunk_type>(nullptr),
            std::memory_order_relaxed
        );
    }

    ensure_capacity_for_index(count);

    for (size_t i = 0; i < count; ++i) {
        set(i, val);
    }
}


template<typename V>
concurrent_smart_vector<V>::~concurrent_smart_vector() {
    for (size_t i = 0; i < num_chunks_; ++i) {
        auto chunk_ptr = chunks_[i].load(std::memory_order_acquire);

        if (chunk_ptr != nullptr) {
            delete[] chunk_ptr;
        }
    }
}


template<typename V>
std::optional<std::shared_ptr<V>> concurrent_smart_vector<V>::at(const size_t idx) const {
    if (idx > size_.load(std::memory_order_acquire) - 1) {
        return std::nullopt;
    }

    const size_t chunk_idx = calc_chunk_idx(idx);
    const size_t local_idx = calc_local_idx(idx, chunk_idx);

    auto chunk_ptr = chunks_[chunk_idx].load(std::memory_order_acquire);

    if (chunk_ptr == nullptr) {
        return std::nullopt;
    }

    std::shared_ptr<V> out = chunk_ptr[local_idx].load(std::memory_order_acquire);

    if (out == nullptr) {
        return std::nullopt;
    }

    return out;
}


template<typename V>
std::optional<std::shared_ptr<V>> concurrent_smart_vector<V>::front() const {
    return at(0);
}


template<typename V>
std::optional<std::shared_ptr<V>> concurrent_smart_vector<V>::back() const {
    return at(size_ - 1);
}


template<typename V>
size_t concurrent_smart_vector<V>::size() const noexcept {
    return size_.load(std::memory_order_acquire);
}


template<typename V>
size_t concurrent_smart_vector<V>::capacity() const noexcept {
    return capacity_.load(std::memory_order_acquire);
}


template<typename V>
bool concurrent_smart_vector<V>::empty() const noexcept {
    return size_.load(std::memory_order_acquire) == 0;
}


template<typename V>
void concurrent_smart_vector<V>::reserve(const size_t new_capacity) {
    ensure_capacity_for_index(new_capacity);
}

// IN DER TPP:
template<typename V>
void concurrent_smart_vector<V>::push_back(const V& val) {
    const size_t idx = size_.fetch_add(1, std::memory_order_acq_rel);
    ensure_capacity_for_index(idx);

    std::shared_ptr<V> new_element = std::make_shared<V>(val);

    insert_at(idx, new_element);
}

template<typename V>
void concurrent_smart_vector<V>::push_back(V&& val) {
    const size_t idx = size_.fetch_add(1, std::memory_order_acq_rel);
    ensure_capacity_for_index(idx);

    std::shared_ptr<V> new_element = std::make_shared<V>(std::move(val));

    insert_at(idx, new_element);
}


template<typename V>
std::optional<std::shared_ptr<V>> concurrent_smart_vector<V>::set(const size_t idx, const V& val) {
    if (idx > size_.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    // Build element
    std::shared_ptr<V> new_element;
    new_element = std::make_shared<V>(val);

    insert_at(idx, new_element);

    return new_element;
}


template<typename V>
std::optional<std::shared_ptr<V> > concurrent_smart_vector<V>::set(const size_t idx, V&& val) {
    if (idx > size_.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    // Build element
    std::shared_ptr<V> new_element;
    new_element = std::make_shared<V>(std::move(val));

    insert_at(idx, new_element);

    return new_element;
}


template<typename V>
template<typename ... Args>
std::optional<std::shared_ptr<V>> concurrent_smart_vector<V>::set(const size_t idx, Args&&... args) {
    if (idx >= size_.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    std::shared_ptr<V> new_element = std::make_shared<V>(std::forward<Args>(args)...);

    insert_at(idx, new_element);

    return new_element;
}

template<typename V>
bool concurrent_smart_vector<V>::fast_unpack_return_val(std::optional<std::shared_ptr<V>> val, V& out) {
    if (!val.has_value()) {
        return false;
    }

    out = *val.value().get();

    return true;
}


template<typename V>
void concurrent_smart_vector<V>::insert_at(const size_t idx, std::shared_ptr<V> new_element) {
    const size_t chunk_idx = calc_chunk_idx(idx);
    const size_t local_idx = calc_local_idx(idx, chunk_idx);

    auto chunk_ptr = chunks_[chunk_idx].load(std::memory_order_acquire);
    chunk_ptr[local_idx].store(new_element, std::memory_order_release);
}

template<typename V>
void concurrent_smart_vector<V>::ensure_capacity_for_index(const size_t idx) {
    const size_t target_chunk_idx = calc_chunk_idx(idx);

    if (target_chunk_idx >= num_chunks_) {
        throw std::overflow_error("Vector capacity exceeded!");
    }

    for (size_t i = 0; i <= target_chunk_idx; ++i) {
        if (chunks_[i].load(std::memory_order_acquire) != nullptr) {
            continue;
        }

        const size_t chunk_cap = (i == 0) ? 32U : 32ULL << i;

        chunk_type new_chunk = new std::atomic<std::shared_ptr<V>>[chunk_cap];

        // fill array with nullptr_shared_ptrs
        for(size_t j = 0; j < chunk_cap; ++j) {
            new_chunk[j].store(std::shared_ptr<V>(nullptr), std::memory_order_relaxed);
        }

        chunk_type expected = nullptr;

        if (!chunks_[i].compare_exchange_strong(expected, new_chunk, std::memory_order_acq_rel)) {
            delete[] new_chunk;
        } else {
            capacity_.fetch_add(chunk_cap, std::memory_order_relaxed);
        }
    }
}
