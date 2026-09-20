//
// Created by deanprangenberg on 4/27/26.
//

#pragma once

#include <atomic>
#include <cstdint>

#if !defined(__x86_64__)
#error "Anchor requires x86-64"
#endif

#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error "Anchor requires CMPXCHG16B support. Compile with -mcx16."
#endif

class alignas(16) anchor {
public:
    using u128 = unsigned __int128;

    static constexpr std::uint32_t null_idx = 0;

    enum class State : std::uint8_t {
        stable = 0,
        frontPush = 1,
        backPush = 2,
    };

    struct View {
        std::uint32_t first_node_idx;
        std::uint32_t last_node_idx;
        State state;
        std::uint64_t version;
    };

    anchor() noexcept
        : raw_(pack(null_idx, null_idx, State::stable, 0)) {
    }

    anchor(const std::uint32_t first_node_idx,
        const std::uint32_t last_node_idx,
        const State state,
        const std::uint64_t version
        ) noexcept : raw_(pack(first_node_idx, last_node_idx, state, version)) {

    }

    anchor(const anchor&) = delete;
    anchor(anchor&&) = delete;
    anchor& operator=(anchor&&) = delete;
    anchor& operator=(const anchor&) = delete;

    static u128 pack(const std::uint32_t first_node_idx,
                     const std::uint32_t last_node_idx,
                     State state,
                     const std::uint64_t version) noexcept {
        u128 value = 0;

        value |= (static_cast<u128>(first_node_idx) & FIRST_NODE_IDX_MASK) << FIRST_NODE_IDX_SHIFT;

        value |= (static_cast<u128>(last_node_idx) & LAST_NODE_IDX_MASK) << LAST_NODE_IDX_SHIFT;

        value |= (static_cast<u128>(static_cast<std::uint8_t>(state)) & STATE_MASK) << STATE_SHIFT;

        value |= (static_cast<u128>(version) & VERSION_MASK) << VERSION_SHIFT;

        return value;
    }

    static u128 pack(const View& view) noexcept {
        return pack(
            view.first_node_idx,
            view.last_node_idx,
            view.state,
            view.version
        );
    }

    static View unpack(const u128 value) noexcept {
        return View{
            .first_node_idx = static_cast<std::uint32_t>(
                (value >> FIRST_NODE_IDX_SHIFT) & FIRST_NODE_IDX_MASK
            ),
            .last_node_idx = static_cast<std::uint32_t>(
                (value >> LAST_NODE_IDX_SHIFT) & LAST_NODE_IDX_MASK
            ),
            .state = static_cast<State>(
                static_cast<std::uint8_t>((value >> STATE_SHIFT) & STATE_MASK)
            ),
            .version = static_cast<std::uint64_t>(
                (value >> VERSION_SHIFT) & VERSION_MASK
            ),
        };
    }

    [[nodiscard]] u128 load_raw(const std::memory_order order = std::memory_order_acquire) const noexcept {
        return __atomic_load_n(&raw_, to_builtin_order(order));
    }

    [[nodiscard]] View load_view(const std::memory_order order = std::memory_order_acquire) const noexcept {
        return unpack(load_raw(order));
    }

    bool compare_exchange_raw(
        u128& expected,
        const u128 desired,
        const std::memory_order success = std::memory_order_acq_rel,
        const std::memory_order failure = std::memory_order_acquire
    ) noexcept {
        return __atomic_compare_exchange_n(
            &raw_,
            &expected,
            desired,
            false,
            to_builtin_order(success),
            to_builtin_order(failure)
        );
    }

    bool compare_exchange_view(
        View& expected,
        const View& desired,
        const std::memory_order success = std::memory_order_acq_rel,
        const std::memory_order failure = std::memory_order_acquire
    ) noexcept {
        u128 expected_raw = pack(expected);
        const u128 desired_raw = pack(desired);

        const bool ok = compare_exchange_raw(
            expected_raw,
            desired_raw,
            success,
            failure
        );

        if (!ok) {
            expected = unpack(expected_raw);
        }

        return ok;
    }

    static constexpr std::uint64_t next_version(std::uint64_t version) noexcept {
        return (version + 1) & MAX_VERSION;
    }

private:
    static constexpr int FIRST_NODE_IDX_BITS = 32;
    static constexpr int LAST_NODE_IDX_BITS = 32;
    static constexpr int STATE_BITS = 2;
    static constexpr int VERSION_BITS = 62;

    static constexpr int FIRST_NODE_IDX_SHIFT = 0;
    static constexpr int LAST_NODE_IDX_SHIFT = FIRST_NODE_IDX_SHIFT + FIRST_NODE_IDX_BITS;
    static constexpr int STATE_SHIFT = LAST_NODE_IDX_SHIFT + LAST_NODE_IDX_BITS;
    static constexpr int VERSION_SHIFT = STATE_SHIFT + STATE_BITS;

    static constexpr std::uint64_t MAX_VERSION = (std::uint64_t{1} << VERSION_BITS) - 1;

    static_assert(VERSION_SHIFT + VERSION_BITS == 128);

    static constexpr u128 FIRST_NODE_IDX_MASK =
            (u128{1u} << FIRST_NODE_IDX_BITS) - 1;

    static constexpr u128 LAST_NODE_IDX_MASK =
            (u128{1u} << LAST_NODE_IDX_BITS) - 1;

    static constexpr u128 STATE_MASK =
            (u128{1u} << STATE_BITS) - 1;

    static constexpr u128 VERSION_MASK =
            (u128{1u} << VERSION_BITS) - 1;

    alignas(16) u128 raw_;

    static constexpr int to_builtin_order(const std::memory_order order) noexcept {
        switch (order) {
            case std::memory_order_relaxed:
                return __ATOMIC_RELAXED;
            case std::memory_order_consume:
                return __ATOMIC_CONSUME;
            case std::memory_order_acquire:
                return __ATOMIC_ACQUIRE;
            case std::memory_order_release:
                return __ATOMIC_RELEASE;
            case std::memory_order_acq_rel:
                return __ATOMIC_ACQ_REL;
            case std::memory_order_seq_cst:
                return __ATOMIC_SEQ_CST;
        }

        return __ATOMIC_SEQ_CST;
    }
};

static_assert(sizeof(anchor::u128) == 16);
static_assert(sizeof(anchor) == 16);
static_assert(alignof(anchor) == 16);