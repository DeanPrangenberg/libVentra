//
// Created by deanprangenberg on 04/04/2026.
//

#include <gtest/gtest.h>
#include <ventra/vector/concurrent_v1_vector.hpp>
#include <string>
#include <thread>

#include "ventra/vector/vector.hpp"

TEST(vector_concurrent_v1_vector_test, sizeChanging) {
    ventra::concurrent_v1_vector<int> vec;
    ASSERT_EQ(vec.size(), 0ULL);

    for (size_t i = 1; i < 100; ++i) {
        vec.push_back(static_cast<int>(i));
        EXPECT_EQ(vec.size(), i);
    }
}

TEST(vector_concurrent_v1_vector_test, atAndAccessOperator) {
    ventra::concurrent_v1_vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);

    int at1_return_val = 0;
    if (vec.fast_unpack_return_val(vec.at(0), at1_return_val)) {
        ASSERT_EQ(at1_return_val, 10);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }

    int at2_return_val = 0;
    if (vec.fast_unpack_return_val(vec.at(1), at2_return_val)) {
        ASSERT_EQ(at2_return_val, 20);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }
}

TEST(vector_concurrent_v1_vector_test, frontBack) {
    ventra::concurrent_v1_vector<int> vec;
    vec.push_back(100);

    int front = 0;
    if (vec.fast_unpack_return_val(vec.front(), front)) {
        ASSERT_EQ(front, 100);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }

    int back = 0;
    if (vec.fast_unpack_return_val(vec.back(), back)) {
        ASSERT_EQ(front, 100);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }

    vec.push_back(200);

    if (vec.fast_unpack_return_val(vec.front(), front)) {
        ASSERT_EQ(front, 100);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }

    if (vec.fast_unpack_return_val(vec.back(), back)) {
        ASSERT_EQ(back, 200);
    } else {
        FAIL() << "push_back did not update idx as intended";
    }
}

TEST(vector_concurrent_v1_vector_test, empty) {
    ventra::concurrent_v1_vector<int> vec;
    ASSERT_TRUE(vec.empty());

    vec.push_back(0);
    ASSERT_FALSE(vec.empty());
}

TEST(vector_concurrent_v1_vector_test, capacity) {
    ventra::concurrent_v1_vector<int> vec;
    // Min cap ist 32
    ASSERT_EQ(vec.capacity(), 32);

    for (size_t i = 1; i < 100; ++i) {
        vec.push_back(static_cast<int>(i));
    }

    ASSERT_TRUE(vec.capacity() >= 32);

}

TEST(vector_concurrent_v1_vector_test, reserve) {
    ventra::concurrent_v1_vector<int> vec;
    vec.reserve(200);
    ASSERT_EQ(vec.size(), 0);
    ASSERT_TRUE(vec.capacity() >= 200);
}

TEST(vector_concurrent_v1_vector_test, AtOutOfBoundsReturn) {
    ventra::concurrent_v1_vector<int> vec;
    vec.push_back(1);

    EXPECT_TRUE(vec.at(0).has_value());
    EXPECT_FALSE(vec.at(1).has_value());
    EXPECT_FALSE(vec.at(100).has_value());
}

TEST(vector_concurrent_v1_vector_test, complexTypesString) {
    ventra::concurrent_v1_vector<std::string> vec;

    vec.push_back("Hello");
    vec.push_back("World");
    vec.push_back("Ventra");

    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(*vec.at(0).value().get(), "Hello");
    ASSERT_EQ(*vec.at(1).value().get(), "World");
    ASSERT_EQ(*vec.at(2).value().get(), "Ventra");
}

TEST(vector_concurrent_v1_vector_test, fastUnpack) {
    ventra::concurrent_v1_vector<int> vec;
    vec.push_back(10);

    const auto out = vec.at(0);

    int val = 0;
    vec.fast_unpack_return_val(out, val);

    ASSERT_EQ(val, 10);
}

TEST(vector_concurrent_v1_vector_test, setAt) {
    ventra::concurrent_v1_vector<int> vec;

    vec.push_back(10);

    int set_return_val = 0;
    if (vec.fast_unpack_return_val(vec.set(0, 11), set_return_val)) {
        ASSERT_EQ(set_return_val, 11);
    } else {
        FAIL() << "set() did not work as intended";
    }


    int at_return_val = 0;
    if (vec.fast_unpack_return_val(vec.at(0), at_return_val)) {
        ASSERT_EQ(at_return_val, 11);
    } else {
        FAIL() << "set() did not update idx as intended";
    }
}

TEST(vector_concurrent_v1_vector_test, setOnOutOfRange) {
    ventra::concurrent_v1_vector<int> vec;

    int set_return_val = 0;
    if (vec.fast_unpack_return_val(vec.set(200, 11), set_return_val)) {
        FAIL() << "set() returned false positive after updating val out of range";
    }

    SUCCEED();
}

TEST(vector_concurrent_v1_vector_test, ReadWriteTornado) {
    ventra::concurrent_v1_vector<int> vec;
    const size_t num_writers = std::thread::hardware_concurrency() / 2;
    const size_t num_readers = std::thread::hardware_concurrency() / 2;
    constexpr size_t items_per_writer = 5000;

    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    // WRITER
    for (size_t t = 0; t < num_writers; ++t) {
        threads.emplace_back([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {}

            for (size_t i = 0; i < items_per_writer; ++i) {
                vec.push_back(42);
            }
        });
    }

    // READER
    std::atomic<size_t> successful_reads{0};
    for (size_t t = 0; t < num_readers; ++t) {
        threads.emplace_back([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {}

            for (size_t i = 0; i < items_per_writer * 2; ++i) {
                size_t current_size = vec.size();
                if (current_size > 0) {
                    size_t random_idx = i % current_size;
                    auto opt = vec.at(random_idx);

                    if (opt.has_value()) {
                        successful_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    // Start signal
    start_flag.store(true, std::memory_order_release);

    // Wait for threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(vec.size(), num_writers * items_per_writer);
    EXPECT_GT(successful_reads.load(), 0ULL);
}

TEST(vector_concurrent_v1_vector_test, ConcurrentPushBack) {
    ventra::concurrent_v1_vector<int> vec;
    const size_t num_threads = std::thread::hardware_concurrency();
    constexpr size_t items_per_thread = 10000;

    std::vector<std::thread> threads;

    std::atomic<bool> start_flag{false};

    // WRITER
    auto worker = [&](size_t start_val) {
        while (!start_flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // Pushe 10.000 aufsteigende Zahlen
        for (size_t i = 0; i < items_per_thread; ++i) {
            vec.push_back(static_cast<int>(start_val + i));
        }
    };


    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t * items_per_thread);
    }

    // Start event
    start_flag.store(true, std::memory_order_release);

    // Wait for threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(vec.size(), num_threads * items_per_thread);

    std::vector<int> validation;
    validation.reserve(vec.size());

    for (size_t i = 0; i < vec.size(); ++i) {
        auto opt = vec.at(i);
        ASSERT_TRUE(opt.has_value()) << "Element an Index " << i << " ist nullopt!";
        validation.push_back(*opt.value());
    }

    std::sort(validation.begin(), validation.end());

    for (size_t i = 0; i < validation.size(); ++i) {
        ASSERT_EQ(validation[i], i) << "Verlorenes oder überschriebenes Element entdeckt!";
    }
}