#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>

template<
    typename N,
    std::size_t MaxThreads,
    std::uint32_t Capacity = 65536,
    std::size_t HazardSlotsPerThread = 2
>
class memory_arena {
protected:
    static constexpr std::size_t reclaim_threshold =
        MaxThreads * HazardSlotsPerThread * 2 + 16;

    // Only the owning thread updates its retirement counter. The list itself
    // can be detached by any allocator when the shared freelist runs dry.
    bool publish_retired_node(
        std::size_t thread_id,
        std::uint32_t idx,
        N* nodes
    ) noexcept {
        push_retired(thread_id, idx, nodes);

        auto& count = retired_[thread_id].retire_count;
        if (++count == reclaim_threshold) {
            count = 0;
            return true;
        }

        return false;
    }

    template<typename NodeAt>
    bool publish_retired_node_by_index(
        std::size_t thread_id,
        std::uint32_t idx,
        NodeAt&& node_at
    ) noexcept {
        push_retired_by_index(thread_id, idx, node_at);

        auto& count = retired_[thread_id].retire_count;
        if (++count == reclaim_threshold) {
            count = 0;
            return true;
        }

        return false;
    }

    template<typename Hazards, typename Release>
    void reclaim_retired_nodes(
        std::size_t thread_id,
        N* nodes,
        const Hazards& hazards,
        Release&& release
    ) noexcept {
        // Exchange transfers exclusive ownership of this chain to the scanner.
        // Concurrent retirements and scanners operate on a new chain.
        auto idx = retired_[thread_id].head.exchange(
            0,
            std::memory_order_acquire
        );

        while (idx != 0) {
            const auto next = nodes[idx].freelist_next.load(
                std::memory_order_relaxed
            );

            if (hazards.is_protected(&nodes[idx])) {
                push_retired(thread_id, idx, nodes);
            } else {
                release(idx);
            }

            // Do not access the node after publishing it to either list.
            idx = next;
        }
    }

    template<typename Hazards, typename NodeAt, typename Release>
    void reclaim_retired_nodes_by_index(
        std::size_t thread_id,
        const Hazards& hazards,
        NodeAt&& node_at,
        Release&& release
    ) noexcept {
        // Exchange transfers exclusive ownership of this chain to the scanner.
        // Concurrent retirements and scanners operate on a new chain.
        auto idx = retired_[thread_id].head.exchange(
            0,
            std::memory_order_acquire
        );

        while (idx != 0) {
            N* node = node_at(idx);
            assert(node != nullptr);

            const auto next = node->freelist_next.load(
                std::memory_order_relaxed
            );

            if (hazards.is_protected(node)) {
                push_retired_by_index(thread_id, idx, node_at);
            } else {
                release(idx);
            }

            // Do not access the node after publishing it to either list.
            idx = next;
        }
    }

private:
    struct alignas(64) retired_list {
        std::atomic<std::uint32_t> head{0};
        std::size_t retire_count{0};
    };

    std::array<retired_list, MaxThreads> retired_{};

    void push_retired(
        std::size_t thread_id,
        std::uint32_t idx,
        N* nodes
    ) noexcept {
        auto& head = retired_[thread_id].head;
        auto expected = head.load(std::memory_order_relaxed);

        do {
            nodes[idx].freelist_next.store(
                expected,
                std::memory_order_relaxed
            );
        } while (!head.compare_exchange_weak(
            expected,
            idx,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }

    template<typename NodeAt>
    void push_retired_by_index(
        std::size_t thread_id,
        std::uint32_t idx,
        NodeAt& node_at
    ) noexcept {
        auto& head = retired_[thread_id].head;
        auto expected = head.load(std::memory_order_relaxed);

        N* node = node_at(idx);
        assert(node != nullptr);

        do {
            node->freelist_next.store(
                expected,
                std::memory_order_relaxed
            );
        } while (!head.compare_exchange_weak(
            expected,
            idx,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }
};
