#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <sys/mman.h>

#include "concurrent_deque_hazard_pointer.hpp"
#include "concurrent_deque_memory_arena.hpp"

template<
    typename N,
    std::size_t MaxThreads,
    std::uint32_t Capacity = 65536,
    std::size_t HazardSlotsPerThread = 2
>
class memory_arena_mmap
    : public memory_arena<
        N,
        MaxThreads,
        Capacity,
        HazardSlotsPerThread
    > {
public:
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    static constexpr u32 null_idx = 0;

    static_assert(Capacity > 1);
    static_assert(
        Capacity <= 65536,
        "The freelist encoding only supports 16-bit indices"
    );

    memory_arena_mmap() {
        const std::size_t bytes =
            static_cast<std::size_t>(Capacity) * sizeof(N);

        void* ptr = mmap(
            nullptr,
            bytes,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
            -1,
            0
        );

        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        nodes_ = static_cast<N*>(ptr);

        u32 constructed = 0;

        try {
            for (; constructed < Capacity; ++constructed) {
                std::construct_at(&nodes_[constructed]);
            }
        } catch (...) {
            for (u32 i = 0; i < constructed; ++i) {
                std::destroy_at(&nodes_[i]);
            }

            munmap(static_cast<void*>(nodes_), bytes);
            nodes_ = nullptr;
            throw;
        }
    }

    ~memory_arena_mmap() {
        if (nodes_ == nullptr) {
            return;
        }

        // All worker threads must already be stopped here.
        for (u32 i = 0; i < Capacity; ++i) {
            std::destroy_at(&nodes_[i]);
        }

        munmap(
            static_cast<void*>(nodes_),
            static_cast<std::size_t>(Capacity) * sizeof(N)
        );
    }

    memory_arena_mmap(const memory_arena_mmap&) = delete;
    memory_arena_mmap& operator=(const memory_arena_mmap&) = delete;
    memory_arena_mmap(memory_arena_mmap&&) = delete;
    memory_arena_mmap& operator=(memory_arena_mmap&&) = delete;

    [[nodiscard]]
    u32 allocate_idx(std::size_t thread_id) noexcept {
        assert(thread_id < MaxThreads);

        if (const u32 reused = pop_freelist(); reused != null_idx) {
            return reused;
        }

        u32 current = next_idx_.load(std::memory_order_relaxed);

        while (current < Capacity) {
            if (next_idx_.compare_exchange_weak(
                    current,
                    current + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed
            )) {
                return current;
            }
        }

        // Retired nodes may belong to an idle thread. Claim its list before
        // reporting exhaustion, while still respecting every hazard pointer.
        for (std::size_t offset = 0; offset < MaxThreads; ++offset) {
            reclaim_retired((thread_id + offset) % MaxThreads);
            if (const u32 reused = pop_freelist(); reused != null_idx) {
                return reused;
            }
        }

        return null_idx;
    }

    [[nodiscard]]
    N* get(u32 idx) noexcept {
        assert(idx < Capacity);
        return &nodes_[idx];
    }

    [[nodiscard]]
    const N* get(u32 idx) const noexcept {
        assert(idx < Capacity);
        return &nodes_[idx];
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
            idx == null_idx ? nullptr : &nodes_[idx]
        );
    }

    void clear_hazard(
        std::size_t thread_id,
        std::size_t slot_id
    ) noexcept {
        hazards_.clear(thread_id, slot_id);
    }

    void clear_all_hazards(std::size_t thread_id) noexcept {
        hazards_.clear_all(thread_id);
    }

    // Use only for a node that was never published in the deque.
    void release_unpublished_idx(u32 idx) noexcept {
        assert(idx != null_idx);
        assert(idx < Capacity);
        release_idx(idx);
    }

    // The node was logically removed and must not be reused yet.
    void retire_node(
        std::size_t thread_id,
        u32 idx
    ) {
        assert(thread_id < MaxThreads);
        assert(idx != null_idx);
        assert(idx < Capacity);

        if (this->publish_retired_node(thread_id, idx, nodes_)) {
            reclaim_retired(thread_id);
        }
    }

    void reclaim_retired(std::size_t thread_id) noexcept {
        assert(thread_id < MaxThreads);

        this->reclaim_retired_nodes(
            thread_id,
            nodes_,
            hazards_,
            [this](u32 idx) noexcept { release_idx(idx); }
        );
    }

private:
    N* nodes_ = nullptr;

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
    u32 pop_freelist() noexcept {
        u64 head = freelist_head_.load(std::memory_order_acquire);

        while (decode_idx(head) != null_idx) {
            const u32 idx = decode_idx(head);
            const u32 next = nodes_[idx].freelist_next.load(
                std::memory_order_relaxed
            );

            const u64 new_head = encode(
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
        u64 head = freelist_head_.load(std::memory_order_relaxed);

        do {
            nodes_[idx].freelist_next.store(
                decode_idx(head),
                std::memory_order_relaxed
            );
        } while (!freelist_head_.compare_exchange_weak(
            head,
            encode(decode_version(head) + 1, idx),
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }

    static constexpr u64 encode(
        u64 version,
        u32 idx
    ) noexcept {
        return (version << 16) | static_cast<u64>(idx);
    }

    static constexpr u32 decode_idx(u64 data) noexcept {
        return static_cast<u32>(data & 0xFFFF);
    }

    static constexpr u64 decode_version(u64 data) noexcept {
        return data >> 16;
    }
};
