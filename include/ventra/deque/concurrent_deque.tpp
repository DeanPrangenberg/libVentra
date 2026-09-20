#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "concurrent_deque_anchor.hpp"

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::concurrent_deque() = default;

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
template<typename... ArenaArgs>
    requires std::is_constructible_v<
        typename concurrent_deque<
            V,
            MaxThreads,
            SlotsPerThread,
            Capacity,
            Arena
        >::arena_type,
        ArenaArgs...
    >
concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::concurrent_deque(
    std::in_place_t,
    ArenaArgs&&... arena_args
)
    : nodes_(std::forward<ArenaArgs>(arena_args)...) {
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::~concurrent_deque() = default;

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::same_anchor(
    const anchor::View& left,
    const anchor::View& right
) noexcept {
    return left.first_node_idx == right.first_node_idx &&
           left.last_node_idx == right.last_node_idx &&
           left.state == right.state &&
           left.version == right.version;
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_back(
    std::size_t thread_id,
    V&& val
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_back_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(std::move(val));
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_back(
    std::size_t thread_id,
    const V& val
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_back_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(val);
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
template<typename... Args>
    requires std::is_constructible_v<V, Args...>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::emplace_back(
    std::size_t thread_id,
    Args&&... args
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_back_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(std::forward<Args>(args)...);
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_front(
    std::size_t thread_id,
    V&& val
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_front_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(std::move(val));
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_front(
    std::size_t thread_id,
    const V& val
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_front_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(val);
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
template<typename... Args>
    requires std::is_constructible_v<V, Args...>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::emplace_front(
    std::size_t thread_id,
    Args&&... args
) {
    const uint32_t node_idx = nodes_.allocate_idx(thread_id);

    if (node_idx == nodes_.null_idx) {
        return false;
    }

    return push_front_impl(thread_id, node_idx, [&](node* n) {
        n->construct_val(std::forward<Args>(args)...);
    });
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::pop_back(
    std::size_t thread_id,
    V& out
) {
    while (true) {
        anchor::View current_anchor =
            anchor_.load_view(std::memory_order_acquire);

        if (current_anchor.state != anchor::State::stable) {
            stabilize(thread_id, current_anchor);
            continue;
        }

        if (current_anchor.first_node_idx == nodes_.null_idx &&
            current_anchor.last_node_idx == nodes_.null_idx) {
            return false;
        }

        const uint32_t last_node_idx =
            current_anchor.last_node_idx;

        // Protect the node before dereferencing it.
        nodes_.protect(thread_id, 0, last_node_idx);

        // The anchor may have changed between load and protect.
        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                current_anchor
        )) {
            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        node* last_node = nodes_.get(last_node_idx);

        if (current_anchor.first_node_idx ==
            current_anchor.last_node_idx) {
            anchor::View empty_anchor{
                nodes_.null_idx,
                nodes_.null_idx,
                anchor::State::stable,
                anchor::next_version(current_anchor.version)
            };

            if (anchor_.compare_exchange_view(
                    current_anchor,
                    empty_anchor,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
            )) {
                out = std::move(*last_node->val_ptr());
                last_node->destroy_value();

                nodes_.clear_hazard(thread_id, 0);
                nodes_.retire_node(thread_id, last_node_idx);
                return true;
            }

            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        const uint32_t new_last_node_idx =
            last_node->previous_node_idx.load(
                std::memory_order_acquire
            );

        if (new_last_node_idx == nodes_.null_idx) {
            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        nodes_.protect(thread_id, 1, new_last_node_idx);

        // Revalidate both the root and the link used to reach slot 1.
        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                current_anchor
            ) ||
            last_node->previous_node_idx.load(
                std::memory_order_acquire
            ) != new_last_node_idx) {
            nodes_.clear_all_hazards(thread_id);
            continue;
        }

        node* new_last_node = nodes_.get(new_last_node_idx);

        anchor::View new_anchor{
            current_anchor.first_node_idx,
            new_last_node_idx,
            anchor::State::stable,
            anchor::next_version(current_anchor.version)
        };

        if (anchor_.compare_exchange_view(
                current_anchor,
                new_anchor,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            out = std::move(*last_node->val_ptr());
            last_node->destroy_value();

            uint32_t expected = last_node_idx;
            new_last_node->next_node_idx.compare_exchange_strong(
                expected,
                nodes_.null_idx,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );

            nodes_.clear_all_hazards(thread_id);
            nodes_.retire_node(thread_id, last_node_idx);
            return true;
        }

        nodes_.clear_all_hazards(thread_id);
    }
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::pop_front(
    std::size_t thread_id,
    V& out
) {
    while (true) {
        anchor::View current_anchor =
            anchor_.load_view(std::memory_order_acquire);

        if (current_anchor.state != anchor::State::stable) {
            stabilize(thread_id, current_anchor);
            continue;
        }

        if (current_anchor.first_node_idx == nodes_.null_idx &&
            current_anchor.last_node_idx == nodes_.null_idx) {
            return false;
        }

        const uint32_t first_node_idx =
            current_anchor.first_node_idx;

        nodes_.protect(thread_id, 0, first_node_idx);

        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                current_anchor
        )) {
            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        node* first_node = nodes_.get(first_node_idx);

        if (current_anchor.first_node_idx ==
            current_anchor.last_node_idx) {
            anchor::View empty_anchor{
                nodes_.null_idx,
                nodes_.null_idx,
                anchor::State::stable,
                anchor::next_version(current_anchor.version)
            };

            if (anchor_.compare_exchange_view(
                    current_anchor,
                    empty_anchor,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
            )) {
                out = std::move(*first_node->val_ptr());
                first_node->destroy_value();

                nodes_.clear_hazard(thread_id, 0);
                nodes_.retire_node(thread_id, first_node_idx);
                return true;
            }

            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        const uint32_t new_first_node_idx =
            first_node->next_node_idx.load(
                std::memory_order_acquire
            );

        if (new_first_node_idx == nodes_.null_idx) {
            nodes_.clear_hazard(thread_id, 0);
            continue;
        }

        nodes_.protect(thread_id, 1, new_first_node_idx);

        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                current_anchor
            ) ||
            first_node->next_node_idx.load(
                std::memory_order_acquire
            ) != new_first_node_idx) {
            nodes_.clear_all_hazards(thread_id);
            continue;
        }

        node* new_first_node = nodes_.get(new_first_node_idx);

        anchor::View new_anchor{
            new_first_node_idx,
            current_anchor.last_node_idx,
            anchor::State::stable,
            anchor::next_version(current_anchor.version)
        };

        if (anchor_.compare_exchange_view(
                current_anchor,
                new_anchor,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            out = std::move(*first_node->val_ptr());
            first_node->destroy_value();

            uint32_t expected = first_node_idx;
            new_first_node->previous_node_idx.compare_exchange_strong(
                expected,
                nodes_.null_idx,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );

            nodes_.clear_all_hazards(thread_id);
            nodes_.retire_node(thread_id, first_node_idx);
            return true;
        }

        nodes_.clear_all_hazards(thread_id);
    }
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
template<typename Constructor>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_back_impl(
    std::size_t thread_id,
    uint32_t node_idx,
    Constructor&& constructor
) {
    node* new_node = nodes_.get(node_idx);

    try {
        constructor(new_node);
    } catch (...) {
        // Not published yet, therefore no retirement delay is needed.
        nodes_.release_unpublished_idx(node_idx);
        throw;
    }

    while (true) {
        auto current_anchor =
            anchor_.load_view(std::memory_order_acquire);

        new_node->version =
            anchor::next_version(current_anchor.version);

        if (current_anchor.state != anchor::State::stable) {
            stabilize(thread_id, current_anchor);
            continue;
        }

        if (current_anchor.first_node_idx == nodes_.null_idx &&
            current_anchor.last_node_idx == nodes_.null_idx) {
            anchor::View init_anchor_view{
                node_idx,
                node_idx,
                anchor::State::stable,
                anchor::next_version(current_anchor.version)
            };

            new_node->previous_node_idx.store(
                nodes_.null_idx,
                std::memory_order_relaxed
            );

            new_node->next_node_idx.store(
                nodes_.null_idx,
                std::memory_order_relaxed
            );

            if (anchor_.compare_exchange_view(
                    current_anchor,
                    init_anchor_view,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
            )) {
                return true;
            }

            continue;
        }

        anchor::View new_anchor_view{
            current_anchor.first_node_idx,
            node_idx,
            anchor::State::backPush,
            anchor::next_version(current_anchor.version)
        };

        new_node->previous_node_idx.store(
            current_anchor.last_node_idx,
            std::memory_order_relaxed
        );

        new_node->next_node_idx.store(
            nodes_.null_idx,
            std::memory_order_relaxed
        );

        if (anchor_.compare_exchange_view(
                current_anchor,
                new_anchor_view,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            stabilize_back(thread_id, new_anchor_view);
            return true;
        }
    }
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
template<typename Constructor>
bool concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::push_front_impl(
    std::size_t thread_id,
    uint32_t node_idx,
    Constructor&& constructor
) {
    node* new_node = nodes_.get(node_idx);

    try {
        constructor(new_node);
    } catch (...) {
        nodes_.release_unpublished_idx(node_idx);
        throw;
    }

    while (true) {
        auto current_anchor =
            anchor_.load_view(std::memory_order_acquire);

        new_node->version =
            anchor::next_version(current_anchor.version);

        if (current_anchor.state != anchor::State::stable) {
            stabilize(thread_id, current_anchor);
            continue;
        }

        if (current_anchor.first_node_idx == nodes_.null_idx &&
            current_anchor.last_node_idx == nodes_.null_idx) {
            anchor::View init_anchor_view{
                node_idx,
                node_idx,
                anchor::State::stable,
                anchor::next_version(current_anchor.version)
            };

            new_node->previous_node_idx.store(
                nodes_.null_idx,
                std::memory_order_relaxed
            );

            new_node->next_node_idx.store(
                nodes_.null_idx,
                std::memory_order_relaxed
            );

            if (anchor_.compare_exchange_view(
                    current_anchor,
                    init_anchor_view,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
            )) {
                return true;
            }

            continue;
        }

        anchor::View new_anchor_view{
            node_idx,
            current_anchor.last_node_idx,
            anchor::State::frontPush,
            anchor::next_version(current_anchor.version)
        };

        new_node->previous_node_idx.store(
            nodes_.null_idx,
            std::memory_order_relaxed
        );

        new_node->next_node_idx.store(
            current_anchor.first_node_idx,
            std::memory_order_relaxed
        );

        if (anchor_.compare_exchange_view(
                current_anchor,
                new_anchor_view,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            stabilize_front(thread_id, new_anchor_view);
            return true;
        }
    }
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
void concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::stabilize(
    std::size_t thread_id,
    anchor::View input_anchor
) {
    if (input_anchor.state == anchor::State::frontPush) {
        stabilize_front(thread_id, input_anchor);
    } else if (input_anchor.state == anchor::State::backPush) {
        stabilize_back(thread_id, input_anchor);
    }
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
void concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::stabilize_front(
    std::size_t thread_id,
    anchor::View input_anchor
) {
    if (input_anchor.first_node_idx == nodes_.null_idx) {
        return;
    }

    nodes_.protect(
        thread_id,
        0,
        input_anchor.first_node_idx
    );

    if (!same_anchor(
            anchor_.load_view(std::memory_order_acquire),
            input_anchor
    )) {
        nodes_.clear_hazard(thread_id, 0);
        return;
    }

    node* new_front_node =
        nodes_.get(input_anchor.first_node_idx);

    const uint32_t old_front_idx =
        new_front_node->next_node_idx.load(
            std::memory_order_acquire
        );

    if (old_front_idx == nodes_.null_idx) {
        nodes_.clear_hazard(thread_id, 0);
        return;
    }

    nodes_.protect(thread_id, 1, old_front_idx);

    if (!same_anchor(
            anchor_.load_view(std::memory_order_acquire),
            input_anchor
    ) ||
        new_front_node->next_node_idx.load(
            std::memory_order_acquire
        ) != old_front_idx) {
        nodes_.clear_all_hazards(thread_id);
        return;
    }

    node* old_front_node = nodes_.get(old_front_idx);

    while (true) {
        uint32_t current_previous =
            old_front_node->previous_node_idx.load(
                std::memory_order_acquire
            );

        if (current_previous == input_anchor.first_node_idx) {
            break;
        }

        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                input_anchor
        )) {
            nodes_.clear_all_hazards(thread_id);
            return;
        }

        if (old_front_node->previous_node_idx.compare_exchange_weak(
                current_previous,
                input_anchor.first_node_idx,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            break;
        }
    }

    anchor::View stable_anchor{
        .first_node_idx = input_anchor.first_node_idx,
        .last_node_idx = input_anchor.last_node_idx,
        .state = anchor::State::stable,
        .version = anchor::next_version(input_anchor.version)
    };

    anchor_.compare_exchange_view(
        input_anchor,
        stable_anchor,
        std::memory_order_acq_rel,
        std::memory_order_acquire
    );

    nodes_.clear_all_hazards(thread_id);
}

template<
    typename V,
    size_t MaxThreads,
    size_t SlotsPerThread,
    size_t Capacity,
    template<typename, std::size_t, std::uint32_t, std::size_t> typename Arena
>
void concurrent_deque<V, MaxThreads, SlotsPerThread, Capacity, Arena>::stabilize_back(
    std::size_t thread_id,
    anchor::View input_anchor
) {
    if (input_anchor.last_node_idx == nodes_.null_idx) {
        return;
    }

    nodes_.protect(
        thread_id,
        0,
        input_anchor.last_node_idx
    );

    if (!same_anchor(
            anchor_.load_view(std::memory_order_acquire),
            input_anchor
    )) {
        nodes_.clear_hazard(thread_id, 0);
        return;
    }

    node* new_back_node =
        nodes_.get(input_anchor.last_node_idx);

    const uint32_t previous_idx =
        new_back_node->previous_node_idx.load(
            std::memory_order_acquire
        );

    if (previous_idx == nodes_.null_idx) {
        nodes_.clear_hazard(thread_id, 0);
        return;
    }

    nodes_.protect(thread_id, 1, previous_idx);

    if (!same_anchor(
            anchor_.load_view(std::memory_order_acquire),
            input_anchor
    ) ||
        new_back_node->previous_node_idx.load(
            std::memory_order_acquire
        ) != previous_idx) {
        nodes_.clear_all_hazards(thread_id);
        return;
    }

    node* previous_back_node = nodes_.get(previous_idx);

    while (true) {
        uint32_t current_next =
            previous_back_node->next_node_idx.load(
                std::memory_order_acquire
            );

        if (current_next == input_anchor.last_node_idx) {
            break;
        }

        if (!same_anchor(
                anchor_.load_view(std::memory_order_acquire),
                input_anchor
        )) {
            nodes_.clear_all_hazards(thread_id);
            return;
        }

        if (previous_back_node->next_node_idx.compare_exchange_weak(
                current_next,
                input_anchor.last_node_idx,
                std::memory_order_acq_rel,
                std::memory_order_acquire
        )) {
            break;
        }
    }

    anchor::View stable_anchor{
        .first_node_idx = input_anchor.first_node_idx,
        .last_node_idx = input_anchor.last_node_idx,
        .state = anchor::State::stable,
        .version = anchor::next_version(input_anchor.version)
    };

    anchor_.compare_exchange_view(
        input_anchor,
        stable_anchor,
        std::memory_order_acq_rel,
        std::memory_order_acquire
    );

    nodes_.clear_all_hazards(thread_id);
}
