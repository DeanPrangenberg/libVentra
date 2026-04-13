//
// Created by deanprangenberg on 04/04/2026.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <ventra/vector/concurrent_atomic_vector.hpp>

namespace {
template<typename T>
std::vector<T> snapshot_values(const ventra::concurrent_atomic_vector<T>& vec) {
    std::vector<T> out;
    out.reserve(vec.size());

    for (size_t i = 0; i < vec.size(); ++i) {
        out.push_back(vec.load(i));
    }

    return out;
}
}

TEST(vector_concurrent_atomic_vector_test, defaultConstruction) {
    ventra::concurrent_atomic_vector<int> vec;

    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0U);
    EXPECT_GE(vec.capacity(), 32U);
    EXPECT_FALSE(vec.front().has_value());
    EXPECT_FALSE(vec.back().has_value());
    EXPECT_FALSE(vec.try_load(0).has_value());
}

TEST(vector_concurrent_atomic_vector_test, countConstructorInitializesAllValues) {
    ventra::concurrent_atomic_vector<int> vec(5, 7);

    ASSERT_EQ(vec.size(), 5U);
    ASSERT_GE(vec.capacity(), 5U);

    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.load(i), 7);
    }

    ASSERT_TRUE(vec.front().has_value());
    ASSERT_TRUE(vec.back().has_value());
    EXPECT_EQ(vec.front().value(), 7);
    EXPECT_EQ(vec.back().value(), 7);
}

TEST(vector_concurrent_atomic_vector_test, reserveAndPushBackAcrossChunkBoundaries) {
    ventra::concurrent_atomic_vector<int> vec;
    vec.reserve(130);

    EXPECT_EQ(vec.size(), 0U);
    EXPECT_GE(vec.capacity(), 130U);

    for (int i = 0; i < 150; ++i) {
        vec.push_back(i * 3);
    }

    ASSERT_EQ(vec.size(), 150U);
    ASSERT_GE(vec.capacity(), 150U);

    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.load(i), static_cast<int>(i * 3));
    }

    ASSERT_TRUE(vec.front().has_value());
    ASSERT_TRUE(vec.back().has_value());
    EXPECT_EQ(vec.front().value(), 0);
    EXPECT_EQ(vec.back().value(), 447);
}

TEST(vector_concurrent_atomic_vector_test, storeAndTryStoreRespectBounds) {
    ventra::concurrent_atomic_vector<int> vec(3, 10);

    vec.store(0, 11);
    vec.store(2, 42);

    EXPECT_EQ(vec.load(0), 11);
    EXPECT_EQ(vec.load(1), 10);
    EXPECT_EQ(vec.load(2), 42);

    EXPECT_FALSE(vec.try_store(3, 99));
    EXPECT_THROW(vec.store(3, 99), std::out_of_range);
}

TEST(vector_concurrent_atomic_vector_test, tryLoadAndTryMethodsReturnFailureForInvalidIndex) {
    ventra::concurrent_atomic_vector<std::uint32_t> vec(2, 0U);
    std::uint32_t expected = 0U;

    EXPECT_FALSE(vec.try_load(2).has_value());
    EXPECT_FALSE(vec.try_store(2, 1U));
    EXPECT_FALSE(vec.try_fetch_add(2, 1U));
    EXPECT_FALSE(vec.try_fetch_sub(2, 1U));
    EXPECT_FALSE(vec.try_fetch_and(2, 1U));
    EXPECT_FALSE(vec.try_fetch_or(2, 1U));
    EXPECT_FALSE(vec.try_fetch_xor(2, 1U));
    EXPECT_FALSE(vec.try_compare_exchange_weak(2, expected, 1U).has_value());
    EXPECT_FALSE(vec.try_compare_exchange_strong(2, expected, 1U).has_value());
}

TEST(vector_concurrent_atomic_vector_test, fetchOperationsMutateValues) {
    ventra::concurrent_atomic_vector<int> arithmetic_vec(1, 10);
    arithmetic_vec.fetch_add(0, 5);
    EXPECT_EQ(arithmetic_vec.load(0), 15);

    arithmetic_vec.fetch_sub(0, 3);
    EXPECT_EQ(arithmetic_vec.load(0), 12);

    ventra::concurrent_atomic_vector<std::uint32_t> bit_vec(1, 0b1010U);
    bit_vec.fetch_and(0, 0b1100U);
    EXPECT_EQ(bit_vec.load(0), 0b1000U);

    bit_vec.fetch_or(0, 0b0011U);
    EXPECT_EQ(bit_vec.load(0), 0b1011U);

    bit_vec.fetch_xor(0, 0b0010U);
    EXPECT_EQ(bit_vec.load(0), 0b1001U);
}

TEST(vector_concurrent_atomic_vector_test, compareExchangeOperationsReflectSuccessAndFailure) {
    ventra::concurrent_atomic_vector<int> vec(1, 41);

    int expected = 41;
    auto strong_success = vec.compare_exchange_strong(0, expected, 42);
    ASSERT_TRUE(strong_success.has_value());
    EXPECT_TRUE(strong_success.value());
    EXPECT_EQ(vec.load(0), 42);

    expected = 41;
    auto strong_fail = vec.compare_exchange_strong(0, expected, 99);
    ASSERT_TRUE(strong_fail.has_value());
    EXPECT_FALSE(strong_fail.value());
    EXPECT_EQ(expected, 42);
    EXPECT_EQ(vec.load(0), 42);

    expected = 42;
    bool weak_succeeded = false;
    for (int attempt = 0; attempt < 8 && !weak_succeeded; ++attempt) {
        auto weak_result = vec.compare_exchange_weak(0, expected, 77);
        ASSERT_TRUE(weak_result.has_value());
        weak_succeeded = weak_result.value();
        if (!weak_succeeded) {
            expected = 42;
        }
    }

    EXPECT_TRUE(weak_succeeded);
    EXPECT_EQ(vec.load(0), 77);
}

TEST(vector_concurrent_atomic_vector_test, concurrentPushBackPreservesAllInsertedValues) {
    ventra::concurrent_atomic_vector<int> vec;

    const size_t num_threads = std::max(2U, std::thread::hardware_concurrency());
    constexpr size_t items_per_thread = 1000;

    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
        threads.emplace_back([&, thread_idx]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t item_idx = 0; item_idx < items_per_thread; ++item_idx) {
                vec.push_back(static_cast<int>(thread_idx * items_per_thread + item_idx));
            }
        });
    }

    start_flag.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(vec.size(), num_threads * items_per_thread);

    auto actual = snapshot_values(vec);
    std::sort(actual.begin(), actual.end());

    std::vector<int> expected;
    expected.reserve(num_threads * items_per_thread);
    for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
        for (size_t item_idx = 0; item_idx < items_per_thread; ++item_idx) {
            expected.push_back(static_cast<int>(thread_idx * items_per_thread + item_idx));
        }
    }

    EXPECT_EQ(actual, expected);
}

TEST(vector_concurrent_atomic_vector_test, concurrentFetchAddAccumulatesCorrectly) {
    ventra::concurrent_atomic_vector<int> vec(1, 0);

    const size_t num_threads = std::max(2U, std::thread::hardware_concurrency());
    constexpr int increments_per_thread = 5000;

    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
        threads.emplace_back([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < increments_per_thread; ++i) {
                vec.fetch_add(0, 1);
            }
        });
    }

    start_flag.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(vec.load(0), static_cast<int>(num_threads) * increments_per_thread);
}
