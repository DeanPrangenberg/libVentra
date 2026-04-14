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
#include <sys/mman.h>
#include <cstring>

// define cache_line_size needed for aligning
#ifdef __cpp_lib_hardware_interference_size
    constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
constexpr size_t CACHE_LINE_SIZE = 64;
#endif

template<typename T>
concept SupportsFetchAdd = requires(std::atomic<T>& a, T val, std::memory_order order) {
    { a.fetch_add(val, order) };
};

template<typename T>
concept SupportsFetchSub = requires(std::atomic<T>& a, T val, std::memory_order order) {
    { a.fetch_sub(val, order) };
};

template<typename T>
concept SupportsFetchAnd = requires(std::atomic<T>& a, T val, std::memory_order order) {
    { a.fetch_and(val, order) };
};

template<typename T>
concept SupportsFetchOr = requires(std::atomic<T>& a, T val, std::memory_order order) {
    { a.fetch_or(val, order) };
};

template<typename T>
concept SupportsFetchXor = requires(std::atomic<T>& a, T val, std::memory_order order) {
    { a.fetch_xor(val, order) };
};

namespace ventra {
    template<typename V>
    class concurrent_atomic_vector {
    public:
        // Constructors
        explicit concurrent_atomic_vector();
        explicit concurrent_atomic_vector(size_t initial_capacity);

        concurrent_atomic_vector(size_t count, const V& val);

        concurrent_atomic_vector(std::initializer_list<V> init_list);

        ~concurrent_atomic_vector();

        concurrent_atomic_vector(const concurrent_atomic_vector&) = delete;
        concurrent_atomic_vector& operator=(const concurrent_atomic_vector&) = delete;

        // Element Access
        [[nodiscard]] V load(size_t idx, std::memory_order order = std::memory_order_acquire) const;
        [[nodiscard]] std::optional<V> try_load(size_t idx, std::memory_order order = std::memory_order_acquire) const;
        [[nodiscard]] std::optional<V> front(std::memory_order order = std::memory_order_acquire) const;
        [[nodiscard]] std::optional<V> back(std::memory_order order = std::memory_order_acquire) const;

        // Capacity
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] static size_t capacity() noexcept;
        [[nodiscard]] bool empty() const noexcept;

        static void reserve(size_t new_capacity);

        // Modifiers
        void push_back(const V& val);
        void push_back(V&& val);

        void store(size_t idx, V&& val, std::memory_order order = std::memory_order_release);
        void store(size_t idx, const V& val, std::memory_order order = std::memory_order_release);
        [[nodiscard]] bool try_store(size_t idx, V&& val, std::memory_order order = std::memory_order_release);
        [[nodiscard]] bool try_store(size_t idx, const V& val, std::memory_order order = std::memory_order_release);

        void fetch_add(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchAdd<V>;
        [[nodiscard]] bool try_fetch_add(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchAdd<V>;

        void fetch_sub(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchSub<V>;
        [[nodiscard]] bool try_fetch_sub(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchSub<V>;

        void fetch_and(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchAnd<V>;
        [[nodiscard]] bool try_fetch_and(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchAnd<V>;

        void fetch_or(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchOr<V>;
        [[nodiscard]] bool try_fetch_or(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchOr<V>;

        void fetch_xor(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchXor<V>;
        [[nodiscard]] bool try_fetch_xor(size_t idx, V val, std::memory_order order = std::memory_order_acq_rel) requires SupportsFetchXor<V>;

        [[nodiscard]] std::optional<bool> compare_exchange_weak(size_t idx, V& expected, V desired,
            std::memory_order load_order = std::memory_order_acquire,
            std::memory_order store_order = std::memory_order_release);
        [[nodiscard]] std::optional<bool> try_compare_exchange_weak(size_t idx, V& expected, V desired,
            std::memory_order load_order = std::memory_order_acquire,
            std::memory_order store_order = std::memory_order_release);

        [[nodiscard]] std::optional<bool> compare_exchange_strong(size_t idx, V& expected, V desired,
            std::memory_order load_order = std::memory_order_acquire,
            std::memory_order store_order = std::memory_order_release);
        [[nodiscard]] std::optional<bool> try_compare_exchange_strong(size_t idx, V& expected, V desired,
            std::memory_order load_order = std::memory_order_acquire,
            std::memory_order store_order = std::memory_order_release);

        void clear_NOT_THREAD_SAFE();

        class iterator {
        private:
            const concurrent_atomic_vector* vec_;
            size_t idx_;

        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = V;
            using difference_type = std::ptrdiff_t;
            using pointer = V*;
            using reference = V;

            iterator(const concurrent_atomic_vector* vec, size_t idx) : vec_(vec), idx_(idx) {}

            V operator*() const {
                return vec_->load(idx_);
            }

            iterator& operator++() {
                idx_++;
                return *this;
            }

            iterator operator++(int) {
                iterator temp = *this;
                idx_++;
                return temp;
            }

            bool operator==(const iterator& other) const { return idx_ == other.idx_; }
            bool operator!=(const iterator& other) const { return idx_ != other.idx_; }

            iterator operator+(difference_type n) const { return iterator(vec_, idx_ + n); }
            iterator operator-(difference_type n) const { return iterator(vec_, idx_ - n); }
            difference_type operator-(const iterator& other) const { return idx_ - other.idx_; }
        };

        iterator begin() const {
            return iterator(this, 0);
        }

        iterator end() const {
            return iterator(this, size_.load(std::memory_order_acquire));
        }

    private:
        // Constants
        static constexpr size_t first_chunk_size_ = 32uz;
        static constexpr size_t num_chunks_ = 32uz;

        // Element struct
        struct element {
            std::atomic<V> val;
            std::atomic<bool> ready;
        };

        // Internal Data
        element* data_;
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> size_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> capacity_{0};

        static constexpr size_t MAX_CAPACITY = 64ULL * 1024 * 1024 * 1024 / sizeof(V);

        [[nodiscard]] bool is_valid_idx(size_t idx) const noexcept;
    };

#include "concurrent_atomic_vector.tpp"
}
