#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>

template<
    typename N,
    std::size_t MaxThreads,
    std::size_t SlotsPerThread = 2
>
class hazard_pointer {
public:
    struct alignas(64) hazard_slot {
        std::atomic<N*> ptr{nullptr};
    };

    void protect(
        std::size_t thread_id,
        std::size_t slot_id,
        N* ptr
    ) noexcept {
        assert(thread_id < MaxThreads);
        assert(slot_id < SlotsPerThread);

        slots_[index(thread_id, slot_id)].ptr.store(
            ptr,
            std::memory_order_seq_cst
        );
    }

    void clear(
        std::size_t thread_id,
        std::size_t slot_id
    ) noexcept {
        assert(thread_id < MaxThreads);
        assert(slot_id < SlotsPerThread);

        slots_[index(thread_id, slot_id)].ptr.store(
            nullptr,
            std::memory_order_release
        );
    }

    void clear_all(std::size_t thread_id) noexcept {
        assert(thread_id < MaxThreads);

        for (std::size_t slot_id = 0; slot_id < SlotsPerThread; ++slot_id) {
            clear(thread_id, slot_id);
        }
    }

    [[nodiscard]]
    bool is_protected(const N* ptr) const noexcept {
        for (const auto& slot : slots_) {
            if (slot.ptr.load(std::memory_order_acquire) == ptr) {
                return true;
            }
        }

        return false;
    }

private:
    static constexpr std::size_t index(
        std::size_t thread_id,
        std::size_t slot_id
    ) noexcept {
        return thread_id * SlotsPerThread + slot_id;
    }

    std::array<hazard_slot, MaxThreads * SlotsPerThread> slots_{};
};
