#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "concurrent_deque_anchor.hpp"
#include "concurrent_deque_memory_arena_mmap.hpp"
#include "concurrent_deque_memory_arena_new.hpp"
#include "concurrent_deque_memory_arena_std_aloc.hpp"

namespace ventra {

template<
    typename V,
    std::size_t MaxThreads,
    std::size_t SlotsPerThread = 2,
    std::size_t Capacity = 65536,
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena = memory_arena_new
>
class concurrent_deque {
    struct node;

    using arena_type = Arena<
        node,
        MaxThreads,
        static_cast<std::uint32_t>(Capacity),
        SlotsPerThread
    >;

public:
    concurrent_deque();

    template<typename... ArenaArgs>
        requires std::is_constructible_v<arena_type, ArenaArgs...>
    explicit concurrent_deque(
        std::in_place_t,
        ArenaArgs&&... arena_args
    );
    ~concurrent_deque();

    concurrent_deque(const concurrent_deque&) = delete;
    concurrent_deque& operator=(const concurrent_deque&) = delete;
    concurrent_deque(concurrent_deque&&) = delete;
    concurrent_deque& operator=(concurrent_deque&&) = delete;

    bool push_back(std::size_t thread_id, V&& val);
    bool push_back(std::size_t thread_id, const V& val);

    template<typename... Args>
        requires std::is_constructible_v<V, Args...>
    bool emplace_back(std::size_t thread_id, Args&&... args);

    bool push_front(std::size_t thread_id, V&& val);
    bool push_front(std::size_t thread_id, const V& val);

    template<typename... Args>
        requires std::is_constructible_v<V, Args...>
    bool emplace_front(std::size_t thread_id, Args&&... args);

    bool pop_back(std::size_t thread_id, V& out);
    bool pop_front(std::size_t thread_id, V& out);

private:
    struct node {
        std::atomic<std::uint32_t> previous_node_idx{0};
        std::atomic<std::uint32_t> next_node_idx{0};
        std::atomic<std::uint32_t> freelist_next{0};
        std::uint64_t version{0};

    private:
        alignas(V) std::byte val_store_[sizeof(V)];

    public:
        [[nodiscard]]
        V* val_ptr() noexcept {
            return std::launder(
                reinterpret_cast<V*>(val_store_)
            );
        }

        [[nodiscard]]
        const V* val_ptr() const noexcept {
            return std::launder(
                reinterpret_cast<const V*>(val_store_)
            );
        }

        template<typename... Args>
        void construct_val(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<V, Args...>) {
            std::construct_at(
                val_ptr(),
                std::forward<Args>(args)...
            );
        }

        void destroy_value() noexcept {
            std::destroy_at(val_ptr());
        }
    };

    static_assert(MaxThreads > 0, "MaxThreads must be greater than zero");
    static_assert(SlotsPerThread >= 2, "The deque requires at least two hazard slots per thread");
    static_assert(Capacity > 1, "Capacity must be greater than one");
    static_assert(Capacity <= 65536, "The arena uses 16-bit node indices");
    static_assert(
        (Capacity & (Capacity - 1)) == 0,
        "Capacity must be a power of two"
    );

    anchor anchor_{};

    arena_type nodes_{};

    static bool same_anchor(
        const anchor::View& left,
        const anchor::View& right
    ) noexcept;

    template<typename Constructor>
    bool push_back_impl(
        std::size_t thread_id,
        std::uint32_t node_idx,
        Constructor&& constructor
    );

    template<typename Constructor>
    bool push_front_impl(
        std::size_t thread_id,
        std::uint32_t node_idx,
        Constructor&& constructor
    );

    void stabilize(
        std::size_t thread_id,
        anchor::View input_anchor
    );

    void stabilize_front(
        std::size_t thread_id,
        anchor::View input_anchor
    );

    void stabilize_back(
        std::size_t thread_id,
        anchor::View input_anchor
    );
};

#include "concurrent_deque.tpp"

} // namespace ventra
