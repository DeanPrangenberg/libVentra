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
        explicit concurrent_atomic_vector() noexcept;
        explicit concurrent_atomic_vector(size_t initial_capacity);

        concurrent_atomic_vector(size_t count, const V& val);

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
        [[nodiscard]] size_t capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        void reserve(size_t new_capacity);

        // Modifiers
        void push_back(const V& val, std::memory_order order = std::memory_order_release);
        void push_back(V&& val, std::memory_order order = std::memory_order_release);

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

    private:
        // Constants
        static constexpr size_t first_chunk_size_ = 32uz;
        static constexpr size_t num_chunks_ = 32uz;

        // Internal state
        struct alignas(CACHE_LINE_SIZE) padded_atomic {
            std::atomic<V> val;

            padded_atomic() noexcept = default;

            explicit padded_atomic(V v) noexcept : val(v) {}
        };

        // Internal Data
        std::atomic<padded_atomic*> chunks_[num_chunks_];
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> size_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> capacity_{0};

        // Allocation Spinlock
        alignas(CACHE_LINE_SIZE) std::atomic_flag allocation_lock_ = ATOMIC_FLAG_INIT;

        // helper functions
        [[nodiscard]] static constexpr size_t calc_chunk_idx(size_t idx) noexcept;
        [[nodiscard]] static constexpr size_t calc_local_idx(size_t idx) noexcept;
        [[nodiscard]] static constexpr size_t calc_local_idx(size_t idx, size_t chunk_idx) noexcept;

        [[nodiscard]] padded_atomic* load_chunk_ptr_unchecked(size_t idx) const;

        [[nodiscard]] bool is_valid_idx(size_t idx) const noexcept;

        void ensure_capacity_for_index(size_t idx);
    };

#include "concurrent_atomic_vector.tpp"
}
