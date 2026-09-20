#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#include "concurrent_deque_memory_arena.hpp"
#include "concurrent_deque_hazard_pointer.hpp"

template<
    typename N,
    std::size_t MaxThreads,
    std::uint32_t Capacity = 65536,
    std::size_t HazardSlotsPerThread = 2
>
class memory_arena_allocator
    : public memory_arena<
        N,
        MaxThreads,
        Capacity,
        HazardSlotsPerThread
    > {
public:
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using allocator_type = std::allocator<N>;
    using allocator_traits =
        std::allocator_traits<allocator_type>;

    static constexpr u32 null_idx = 0;

    static_assert(Capacity > 1);
    static_assert(
        Capacity <= 65536,
        "The freelist encoding only supports 16-bit indices"
    );

    memory_arena_allocator()
        : slots_(std::make_unique<slot[]>(Capacity)) {
    }

    ~memory_arena_allocator() {
        if (slots_ == nullptr) {
            return;
        }

        // All worker threads must already be stopped here.
        for (u32 i = 0; i < Capacity; ++i) {
            if (N* node = slots_[i].node.exchange(
                    nullptr,
                    std::memory_order_acq_rel
            )) {
                destroy_node(node);
            }
        }
    }

    memory_arena_allocator(
        const memory_arena_allocator&
    ) = delete;

    memory_arena_allocator& operator=(
        const memory_arena_allocator&
    ) = delete;

    memory_arena_allocator(
        memory_arena_allocator&&
    ) = delete;

    memory_arena_allocator& operator=(
        memory_arena_allocator&&
    ) = delete;

    [[nodiscard]]
    u32 allocate_idx(std::size_t thread_id) noexcept {
        assert(thread_id < MaxThreads);

        if (const u32 reused = allocate_reused_idx();
            reused != null_idx) {
            return reused;
        }

        u32 current =
            next_idx_.load(std::memory_order_relaxed);

        while (current < Capacity) {
            if (next_idx_.compare_exchange_weak(
                    current,
                    current + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed
            )) {
                if (allocate_node_at(current)) {
                    return current;
                }

                push_free_slot(current);
                return null_idx;
            }
        }

        // Retired nodes may belong to an idle thread. Claim its list before
        // reporting exhaustion, while still respecting every hazard pointer.
        for (std::size_t offset = 0; offset < MaxThreads; ++offset) {
            reclaim_retired((thread_id + offset) % MaxThreads);
            if (const u32 reused = allocate_reused_idx();
                reused != null_idx) {
                return reused;
            }
        }

        return null_idx;
    }

    [[nodiscard]]
    N* get(u32 idx) noexcept {
        assert(idx < Capacity);

        N* node = node_at(idx);
        assert(node != nullptr);
        return node;
    }

    [[nodiscard]]
    const N* get(u32 idx) const noexcept {
        assert(idx < Capacity);

        const N* node = node_at(idx);
        assert(node != nullptr);
        return node;
    }

    void protect(
        std::size_t thread_id,
        std::size_t slot_id,
        u32 idx
    ) noexcept {
        assert(idx < Capacity);

        hazards_.protect(
            thread_id,
            slot_id,
            idx == null_idx ? nullptr : node_at(idx)
        );
    }

    void clear_hazard(
        std::size_t thread_id,
        std::size_t slot_id
    ) noexcept {
        hazards_.clear(
            thread_id,
            slot_id
        );
    }

    void clear_all_hazards(
        std::size_t thread_id
    ) noexcept {
        hazards_.clear_all(thread_id);
    }

    void release_unpublished_idx(u32 idx) noexcept {
        assert(idx != null_idx);
        assert(idx < Capacity);

        release_idx(idx);
    }

    void retire_node(
        std::size_t thread_id,
        u32 idx
    ) {
        assert(thread_id < MaxThreads);
        assert(idx != null_idx);
        assert(idx < Capacity);

        if (this->publish_retired_node_by_index(
                thread_id,
                idx,
                [this](u32 node_idx) noexcept {
                    return node_at(node_idx);
                }
        )) {
            reclaim_retired(thread_id);
        }
    }

    void reclaim_retired(std::size_t thread_id) noexcept {
        assert(thread_id < MaxThreads);

        this->reclaim_retired_nodes_by_index(
            thread_id,
            hazards_,
            [this](u32 idx) noexcept {
                return node_at(idx);
            },
            [this](u32 idx) noexcept {
                release_idx(idx);
            }
        );
    }

private:
    struct slot {
        std::atomic<N*> node{nullptr};
        std::atomic<u32> free_next{0};
    };

    allocator_type allocator_{};

    std::unique_ptr<slot[]> slots_;

    hazard_pointer<
        N,
        MaxThreads,
        HazardSlotsPerThread
    > hazards_{};

    alignas(64) std::atomic<u64>
        freelist_head_{encode(0, null_idx)};

    alignas(64) std::atomic<u32>
        next_idx_{1};

    [[nodiscard]]
    N* node_at(u32 idx) noexcept {
        assert(idx < Capacity);

        return slots_[idx].node.load(
            std::memory_order_acquire
        );
    }

    [[nodiscard]]
    const N* node_at(u32 idx) const noexcept {
        assert(idx < Capacity);

        return slots_[idx].node.load(
            std::memory_order_acquire
        );
    }

    [[nodiscard]]
    u32 allocate_reused_idx() noexcept {
        const u32 idx = pop_free_slot();

        if (idx == null_idx) {
            return null_idx;
        }

        if (allocate_node_at(idx)) {
            return idx;
        }

        push_free_slot(idx);
        return null_idx;
    }

    [[nodiscard]]
    bool allocate_node_at(u32 idx) noexcept {
        assert(idx != null_idx);
        assert(idx < Capacity);
        assert(node_at(idx) == nullptr);

        N* node = create_node();
        if (node == nullptr) {
            return false;
        }

        node->freelist_next.store(
            null_idx,
            std::memory_order_relaxed
        );

        slots_[idx].node.store(
            node,
            std::memory_order_release
        );

        return true;
    }

    [[nodiscard]]
    N* create_node() noexcept {
        N* node = nullptr;

        try {
            node = allocator_traits::allocate(
                allocator_,
                1
            );

            allocator_traits::construct(
                allocator_,
                std::addressof(*node)
            );

            return node;
        } catch (...) {
            if (node != nullptr) {
                allocator_traits::deallocate(
                    allocator_,
                    node,
                    1
                );
            }

            return nullptr;
        }
    }

    void destroy_node(N* node) noexcept {
        allocator_traits::destroy(
            allocator_,
            std::addressof(*node)
        );

        allocator_traits::deallocate(
            allocator_,
            node,
            1
        );
    }

    [[nodiscard]]
    u32 pop_free_slot() noexcept {
        u64 head =
            freelist_head_.load(
                std::memory_order_acquire
            );

        while (decode_idx(head) != null_idx) {
            const u32 idx =
                decode_idx(head);

            const u32 next =
                slots_[idx].free_next.load(
                    std::memory_order_relaxed
                );

            const u64 new_head =
                encode(
                    decode_version(head) + 1,
                    next
                );

            if (freelist_head_.compare_exchange_weak(
                    head,
                    new_head,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
            )) {
                return idx;
            }
        }

        return null_idx;
    }

    void release_idx(u32 idx) noexcept {
        N* node = slots_[idx].node.exchange(
            nullptr,
            std::memory_order_acq_rel
        );

        assert(node != nullptr);
        if (node == nullptr) {
            return;
        }

        destroy_node(node);
        push_free_slot(idx);
    }

    void push_free_slot(u32 idx) noexcept {
        u64 head =
            freelist_head_.load(
                std::memory_order_relaxed
            );

        do {
            slots_[idx].free_next.store(
                decode_idx(head),
                std::memory_order_relaxed
            );
        } while (
            !freelist_head_.compare_exchange_weak(
                head,
                encode(
                    decode_version(head) + 1,
                    idx
                ),
                std::memory_order_release,
                std::memory_order_relaxed
            )
        );
    }

    static constexpr u64 encode(
        u64 version,
        u32 idx
    ) noexcept {
        return (version << 16)
             | static_cast<u64>(idx);
    }

    static constexpr u32 decode_idx(
        u64 data
    ) noexcept {
        return static_cast<u32>(
            data & 0xFFFF
        );
    }

    static constexpr u64 decode_version(
        u64 data
    ) noexcept {
        return data >> 16;
    }
};
