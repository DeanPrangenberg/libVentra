//
// Created by deanprangenberg on 04/21/2026.
//

#include <atomic>
#include <barrier>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <ventra/deque/concurrent_deque.hpp>

template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
struct arena_test_config {
    template<typename N, std::size_t MaxThreads, std::uint32_t Capacity>
    using arena = Arena<N, MaxThreads, Capacity, 2>;

    template<
        typename V,
        std::size_t MaxThreads,
        std::size_t SlotsPerThread = 2,
        std::size_t Capacity = 65536
    >
    using deque = ventra::concurrent_deque<
        V,
        MaxThreads,
        SlotsPerThread,
        Capacity,
        Arena
    >;
};

struct arena_new_config : arena_test_config<memory_arena_new> {
    static constexpr const char* name = "New";
};

struct arena_allocator_config
    : arena_test_config<memory_arena_allocator> {
    static constexpr const char* name = "Allocator";
};

struct arena_mmap_config : arena_test_config<memory_arena_mmap> {
    static constexpr const char* name = "Mmap";
};

template<typename ArenaConfig>
class deque_concurrent_deque_test : public ::testing::Test {
protected:
    template<
        typename V,
        std::size_t MaxThreads,
        std::size_t SlotsPerThread = 2,
        std::size_t Capacity = 65536
    >
    using deque_type = typename ArenaConfig::template deque<
        V,
        MaxThreads,
        SlotsPerThread,
        Capacity
    >;
};

struct arena_name_generator {
    template<typename ArenaConfig>
    static std::string GetName(int) {
        return ArenaConfig::name;
    }
};

using concurrent_deque_arena_types = ::testing::Types<
    arena_new_config,
    arena_allocator_config,
    arena_mmap_config
>;

TYPED_TEST_SUITE(
    deque_concurrent_deque_test,
    concurrent_deque_arena_types,
    arena_name_generator
);

TYPED_TEST(deque_concurrent_deque_test, pop_empty_returns_false) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    int out = 0;

    EXPECT_FALSE(deque.pop_back(0, out));
    EXPECT_FALSE(deque.pop_front(0, out));
}

TYPED_TEST(deque_concurrent_deque_test, push_pop_back) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_back(0, 1));

    int out = 0;
    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, push_pop_front) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_front(0, 1));

    int out = 0;
    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, const_push_pop_back) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    const int in = 1;
    ASSERT_TRUE(deque.push_back(0, in));

    int out = 0;
    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, const_push_pop_front) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    const int in = 1;
    ASSERT_TRUE(deque.push_front(0, in));

    int out = 0;
    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, push_pop_back_sequence) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_back(0, 1));
    ASSERT_TRUE(deque.push_back(0, 2));
    ASSERT_TRUE(deque.push_back(0, 3));

    int out = 0;
    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 3);

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 2);

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, push_pop_front_sequence) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_front(0, 1));
    ASSERT_TRUE(deque.push_front(0, 2));
    ASSERT_TRUE(deque.push_front(0, 3));

    int out = 0;
    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 3);

    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 2);

    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 1);
}

TYPED_TEST(deque_concurrent_deque_test, push_front_pop_back_sequence) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_front(0, 1));
    ASSERT_TRUE(deque.push_front(0, 2));
    ASSERT_TRUE(deque.push_front(0, 3));

    int out = 0;
    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 1);

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 2);

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, 3);
}

TYPED_TEST(deque_concurrent_deque_test, push_back_pop_front_sequence) {
    typename TestFixture::template deque_type<int, 1, 2, 64> deque;

    ASSERT_TRUE(deque.push_back(0, 1));
    ASSERT_TRUE(deque.push_back(0, 2));
    ASSERT_TRUE(deque.push_back(0, 3));

    int out = 0;
    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 1);

    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 2);

    ASSERT_TRUE(deque.pop_front(0, out));
    ASSERT_EQ(out, 3);
}

TYPED_TEST(deque_concurrent_deque_test, complex_emplace) {
    typename TestFixture::template deque_type<std::string, 1, 2, 64>
        deque;

    ASSERT_TRUE(deque.emplace_front(0, "1"));
    ASSERT_TRUE(deque.emplace_back(0, "1"));
    ASSERT_TRUE(deque.emplace_back(0, "2"));

    std::string out;
    ASSERT_TRUE(deque.pop_front(0, out));

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, "2");

    ASSERT_TRUE(deque.pop_back(0, out));
    ASSERT_EQ(out, "1");
}

TYPED_TEST(deque_concurrent_deque_test, reuses_retired_nodes_after_capacity_is_hit) {
    constexpr std::size_t capacity = 64;
    constexpr std::size_t usable_capacity = capacity - 1;

    typename TestFixture::template deque_type<int, 1, 2, capacity> deque;

    for (std::size_t i = 0; i < usable_capacity; ++i) {
        ASSERT_TRUE(deque.push_back(0, static_cast<int>(i)));
    }

    EXPECT_FALSE(deque.push_back(0, 1234));

    int out = 0;
    for (std::size_t i = 0; i < usable_capacity; ++i) {
        ASSERT_TRUE(deque.pop_front(0, out));
        ASSERT_EQ(out, static_cast<int>(i));
    }

    for (std::size_t i = 0; i < usable_capacity; ++i) {
        ASSERT_TRUE(deque.push_back(0, static_cast<int>(i)));
    }
}

TYPED_TEST(deque_concurrent_deque_test, reuses_nodes_retired_by_idle_threads) {
    typename TestFixture::template deque_type<int, 4, 2, 64> deque;

    for (int i = 0; i < 63; ++i) {
        ASSERT_TRUE(deque.push_back(0, i));
    }

    int out = 0;
    for (std::size_t thread_id = 1; thread_id < 4; ++thread_id) {
        // Each list stays below the batch reclamation threshold.
        for (int i = 0; i < 21; ++i) {
            ASSERT_TRUE(deque.pop_front(thread_id, out));
        }
    }

    // Thread 0 has no retired nodes of its own. The other IDs remain idle.
    for (int i = 0; i < 63; ++i) {
        ASSERT_TRUE(deque.push_front(0, i)) << "refill index " << i;
    }
    EXPECT_FALSE(deque.push_front(0, 63));

    for (int i = 0; i < 63; ++i) {
        ASSERT_TRUE(deque.pop_back(0, out));
        EXPECT_EQ(out, i);
    }
    EXPECT_FALSE(deque.pop_back(0, out));
}

TYPED_TEST(deque_concurrent_deque_test, cross_thread_reclamation_respects_hazards) {
    struct arena_node {
        std::atomic<std::uint32_t> freelist_next{0};
    };
    typename TypeParam::template arena<arena_node, 4, 8> arena;

    const auto protected_idx = arena.allocate_idx(0);
    ASSERT_NE(protected_idx, arena.null_idx);
    for (int i = 0; i < 6; ++i) {
        ASSERT_NE(arena.allocate_idx(0), arena.null_idx);
    }

    arena.protect(2, 0, protected_idx);
    arena.retire_node(1, protected_idx);
    EXPECT_EQ(arena.allocate_idx(0), arena.null_idx);
    EXPECT_EQ(arena.allocate_idx(3), arena.null_idx);

    arena.clear_all_hazards(2);
    EXPECT_EQ(arena.allocate_idx(0), protected_idx);
    EXPECT_EQ(arena.allocate_idx(3), arena.null_idx);
}

TYPED_TEST(deque_concurrent_deque_test, concurrent_phased_near_capacity_reuses_nodes) {
    constexpr std::size_t num_threads = 24;
    constexpr std::size_t total_operations = 61440;
    constexpr std::size_t operations_per_thread = total_operations / num_threads;
    constexpr int rounds = 8;

    // Match the benchmark's arena sizing even though only 24 IDs are active.
    typename TestFixture::template deque_type<int, 64> deque;
    std::barrier phase_barrier(static_cast<std::ptrdiff_t>(num_threads));
    std::atomic<int> push_failures{0};
    std::atomic<int> pop_failures{0};
    std::atomic<int> invalid_values{0};
    std::vector<std::atomic<int>> seen(total_operations);
    std::vector<std::thread> threads;

    for (std::size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            for (int round = 0; round < rounds; ++round) {
                for (std::size_t i = 0; i < operations_per_thread; ++i) {
                    const int value = static_cast<int>(
                        thread_id * operations_per_thread + i
                    );
                    if (!deque.push_back(thread_id, value)) {
                        push_failures.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                phase_barrier.arrive_and_wait();

                for (std::size_t i = 0; i < operations_per_thread; ++i) {
                    int out = -1;
                    if (!deque.pop_front(thread_id, out)) {
                        pop_failures.fetch_add(1, std::memory_order_relaxed);
                    } else if (out < 0 || out >= static_cast<int>(total_operations)) {
                        invalid_values.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        seen[out].fetch_add(1, std::memory_order_relaxed);
                    }
                }
                phase_barrier.arrive_and_wait();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(push_failures.load(), 0);
    EXPECT_EQ(pop_failures.load(), 0);
    EXPECT_EQ(invalid_values.load(), 0);
    for (std::size_t i = 0; i < total_operations; ++i) {
        ASSERT_EQ(seen[i].load(), rounds) << "value " << i;
    }
    int out = 0;
    EXPECT_FALSE(deque.pop_front(0, out));
}

TYPED_TEST(deque_concurrent_deque_test, multithreaded_chaotic_random_ops) {
    constexpr int num_threads = 64;
    constexpr int ops_per_thread = 50'000;

    typename TestFixture::template deque_type<
        std::string,
        num_threads
    > deque;

    std::vector<std::thread> threads;
    std::atomic<int> successful_pushes{0};
    std::atomic<int> successful_pops{0};

    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::string out;

            for (int op = 0; op < ops_per_thread; ++op) {
                const int action = (i + op) % 2;

                bool success = false;
                switch (action) {
                    case 0:
                        success = deque.push_back(i, "Data");
                        break;
                    case 1:
                        success = deque.pop_back(i, out);
                        break;
                    default:
                        break;
                }

                if (success) {
                    if (action == 0) {
                        successful_pushes.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    } else {
                        successful_pops.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::string out;
    while (deque.pop_back(0, out)) {
        successful_pops.fetch_add(1, std::memory_order_relaxed);
    }

    ASSERT_EQ(successful_pushes.load(), successful_pops.load());
}
