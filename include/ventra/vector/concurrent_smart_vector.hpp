//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <atomic>
#include <optional>
#include <bit>

namespace ventra {
    template<typename V>
    class concurrent_smart_vector {
    public:
        // Constructors
        explicit concurrent_smart_vector() noexcept;
        explicit concurrent_smart_vector(size_t initial_capacity);

        template<typename ValType>
        concurrent_smart_vector(size_t count, ValType&& val);

        ~concurrent_smart_vector();

        concurrent_smart_vector(const concurrent_smart_vector&) = delete;
        concurrent_smart_vector& operator=(const concurrent_smart_vector&) = delete;

        // Element Access
        [[nodiscard]] std::optional<std::shared_ptr<V>> at(size_t idx) const;
        [[nodiscard]] std::optional<std::shared_ptr<V>> front() const;
        [[nodiscard]] std::optional<std::shared_ptr<V>> back() const;

        // Capacity
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] size_t capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        void reserve(size_t new_capacity);

        // Modifiers
        void push_back(const V& val);
        void push_back(V&& val);

        std::optional<std::shared_ptr<V>> set(size_t idx, const V& val);
        std::optional<std::shared_ptr<V>> set(size_t idx, V&& val);

        template<typename... Args>
        std::optional<std::shared_ptr<V>> set(size_t idx, Args&&... args);

        static bool fast_unpack_return_val(std::optional<std::shared_ptr<V>> val, V& out);
    private:
        // Constants
        static constexpr size_t first_chunk_size_ = 32uz;
        static constexpr size_t num_chunks_ = 32uz;

        // helper types, so my fingers don't hurt that much ;)
        using chunk_type = std::atomic<std::shared_ptr<V>>*;

        // Internal state
        std::atomic<chunk_type> chunks_[num_chunks_];
        std::atomic<size_t> size_{0};
        std::atomic<size_t> capacity_{0};

        // helper functions
        [[nodiscard]] static constexpr size_t calc_chunk_idx(size_t idx) noexcept;
        [[nodiscard]] static constexpr size_t calc_local_idx(size_t idx, size_t chunk_idx) noexcept;

        void insert_at(size_t idx, std::shared_ptr<V> new_element);

        void ensure_capacity_for_index(size_t idx);
    };

#include "concurrent_smart_vector.tpp"
}