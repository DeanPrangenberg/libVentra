//
// Created by deanprangenberg on 04/14/2026.
//

#include <gtest/gtest.h>

#include <deque>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

#include <ventra/deque/deque.hpp>

namespace {
    template<typename T>
    void expect_sequence(const ventra::deque<T>& deque, const std::initializer_list<T> expected) {
        ASSERT_EQ(deque.size(), expected.size());

        size_t idx = 0;
        for (const auto& value : expected) {
            EXPECT_EQ(deque.at(idx), value);
            ++idx;
        }
    }

    template<typename T>
    void expect_same_as_std_deque(const ventra::deque<T>& actual, const std::deque<T>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        EXPECT_EQ(static_cast<size_t>(actual.end() - actual.begin()), expected.size());

        for (size_t idx = 0; idx < expected.size(); ++idx) {
            EXPECT_EQ(actual.at(idx), expected[idx]) << "idx=" << idx;
        }

        if (expected.empty()) {
            EXPECT_TRUE(actual.empty());
            return;
        }

        EXPECT_FALSE(actual.empty());
        EXPECT_EQ(actual.front(), expected.front());
        EXPECT_EQ(actual.back(), expected.back());
    }

    struct tracked_value {
        static inline int alive = 0;

        tracked_value() : value(0) {
            ++alive;
        }

        explicit tracked_value(const int initial_value) : value(initial_value) {
            ++alive;
        }

        tracked_value(const tracked_value& other) : value(other.value) {
            ++alive;
        }

        tracked_value(tracked_value&& other) noexcept : value(other.value) {
            ++alive;
            other.value = -1;
        }

        tracked_value& operator=(const tracked_value&) = default;
        tracked_value& operator=(tracked_value&&) noexcept = default;

        ~tracked_value() {
            --alive;
        }

        int value;
    };

    struct throwing_copy_value {
        static inline int alive = 0;
        static inline int copies_until_throw = -1;

        throwing_copy_value() : value(0) {
            ++alive;
        }

        explicit throwing_copy_value(const int initial_value) : value(initial_value) {
            ++alive;
        }

        throwing_copy_value(const throwing_copy_value& other) : value(other.value) {
            if (copies_until_throw == 0) {
                throw std::runtime_error("copy construction failed");
            }

            if (copies_until_throw > 0) {
                --copies_until_throw;
            }

            ++alive;
        }

        throwing_copy_value(throwing_copy_value&& other) noexcept : value(other.value) {
            ++alive;
            other.value = -1;
        }

        throwing_copy_value& operator=(const throwing_copy_value& other) {
            if (this == &other) {
                return *this;
            }

            if (copies_until_throw == 0) {
                throw std::runtime_error("copy assignment failed");
            }

            if (copies_until_throw > 0) {
                --copies_until_throw;
            }

            value = other.value;
            return *this;
        }

        throwing_copy_value& operator=(throwing_copy_value&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        ~throwing_copy_value() {
            --alive;
        }

        int value;
    };
}

TEST(deque_deque_test, defaultConstructedDequeIsEmpty) {
    ventra::deque<int> deque;

    EXPECT_TRUE(deque.empty());
    EXPECT_EQ(deque.size(), 0);
}

TEST(deque_deque_test, zeroChunkSizeThrowsInvalidArgument) {
    EXPECT_THROW(ventra::deque<int>(0), std::invalid_argument);
}

TEST(deque_deque_test, pushFrontAndBackAcrossChunksPreserveOrder) {
    ventra::deque<int> deque(3);

    deque.push_back(3);
    deque.push_back(4);
    deque.push_front(2);
    deque.push_front(1);
    deque.push_back(5);
    deque.push_back(6);

    EXPECT_EQ(deque.front(), 1);
    EXPECT_EQ(deque.back(), 6);
    expect_sequence(deque, {1, 2, 3, 4, 5, 6});
}

TEST(deque_deque_test, emptyDequeOperationsThrowWhereSupported) {
    ventra::deque<int> deque;

    EXPECT_THROW(deque.pop_back(), std::out_of_range);
    EXPECT_THROW(deque.pop_front(), std::out_of_range);
    EXPECT_THROW(deque.back(), std::out_of_range);
    EXPECT_THROW(deque.at(0), std::out_of_range);
}

TEST(deque_deque_test, pushBackAfterBecomingEmptyResetsLogicalStart) {
    ventra::deque<int> deque(2);

    deque.push_front(1);
    deque.pop_front();

    deque.push_back(2);
    int& emplaced = deque.emplace_back(3);

    EXPECT_EQ(deque.front(), 2);
    EXPECT_EQ(deque.at(0), 2);
    EXPECT_EQ(deque.back(), 3);
    EXPECT_EQ(emplaced, 3);
    expect_sequence(deque, {2, 3});
}

TEST(deque_deque_test, assignReplacesContentAndSupportsZeroAmount) {
    ventra::deque<int> deque(2);

    deque.push_back(1);
    deque.push_back(2);
    deque.push_back(3);

    deque.assign(5, 7);
    expect_sequence(deque, {7, 7, 7, 7, 7});

    deque.assign(0, 1);
    EXPECT_TRUE(deque.empty());
}

TEST(deque_deque_test, insertInMiddleShiftsElementsAcrossChunks) {
    ventra::deque<int> deque(2);

    deque.push_back(1);
    deque.push_back(2);
    deque.push_back(4);
    deque.push_back(5);

    deque.insert(deque.begin() + 2, 3);

    expect_sequence(deque, {1, 2, 3, 4, 5});
}

TEST(deque_deque_test, eraseRemovesMiddleAndLastElement) {
    ventra::deque<int> deque = {1, 2, 3, 4, 5};

    deque.erase(deque.begin() + 2);
    expect_sequence(deque, {1, 2, 4, 5});

    deque.erase(deque.end() - 1);
    expect_sequence(deque, {1, 2, 4});

    EXPECT_THROW(deque.erase(deque.end()), std::out_of_range);
}

TEST(deque_deque_test, emplaceVariantsReturnInsertedElementAndPreserveSequence) {
    ventra::deque<std::string> deque(2);

    std::string& front = deque.emplace_front(2, 'a');
    EXPECT_EQ(front, "aa");

    std::string& back = deque.emplace_back(2, 'c');
    EXPECT_EQ(back, "cc");

    std::string& middle = deque.emplace(deque.begin() + 1, 2, 'b');

    EXPECT_EQ(middle, "bb");
    expect_sequence<std::string>(deque, {"aa", "bb", "cc"});
}

TEST(deque_deque_test, copyOperationsCreateIndependentDeques) {
    ventra::deque<int> original(2);
    for (int value = 1; value <= 5; ++value) {
        original.push_back(value);
    }

    ventra::deque<int> copied(original);
    expect_sequence(copied, {1, 2, 3, 4, 5});

    original.at(1) = 20;
    copied.push_back(6);

    expect_sequence(original, {1, 20, 3, 4, 5});
    expect_sequence(copied, {1, 2, 3, 4, 5, 6});

    ventra::deque<int> assigned(3);
    assigned.push_back(-1);
    assigned.push_back(-2);
    assigned = copied;
    expect_sequence(assigned, {1, 2, 3, 4, 5, 6});

    assigned = assigned;
    expect_sequence(assigned, {1, 2, 3, 4, 5, 6});
}

TEST(deque_deque_test, moveOperationsTransferContentAndLeaveSourceReusable) {
    ventra::deque<int> source(2);
    for (int value = 1; value <= 5; ++value) {
        source.push_back(value);
    }

    ventra::deque<int> moved(std::move(source));
    expect_sequence(moved, {1, 2, 3, 4, 5});
    EXPECT_TRUE(source.empty());

    source.push_back(9);
    source.push_front(8);
    expect_sequence(source, {8, 9});

    ventra::deque<int> target(3);
    target.push_back(-1);
    target.push_back(-2);
    target = std::move(moved);
    expect_sequence(target, {1, 2, 3, 4, 5});
    EXPECT_TRUE(moved.empty());

    moved.push_back(42);
    expect_sequence(moved, {42});
}

TEST(deque_deque_test, atSupportsConstAndNonConstAccess) {
    ventra::deque<int> deque = {10, 20, 30};

    EXPECT_EQ(deque.at(1), 20);
    deque.at(1) = 25;
    EXPECT_EQ(deque.at(1), 25);
    EXPECT_THROW(deque.at(3), std::out_of_range);

    const ventra::deque<int>& const_deque = deque;
    EXPECT_EQ(const_deque.front(), 10);
    EXPECT_EQ(const_deque.back(), 30);
    EXPECT_EQ(const_deque.at(1), 25);
    EXPECT_THROW(const_deque.at(3), std::out_of_range);
}

TEST(deque_deque_test, swapExchangesContentsAndKeepsBothDequesUsable) {
    ventra::deque<int> left(2);
    left.push_front(2);
    left.push_front(1);
    left.push_back(3);
    left.push_back(4);

    ventra::deque<int> right(5);
    right.push_back(10);
    right.push_back(20);

    left.swap(right);

    expect_sequence(left, {10, 20});
    expect_sequence(right, {1, 2, 3, 4});

    left.push_front(5);
    right.push_back(5);

    expect_sequence(left, {5, 10, 20});
    expect_sequence(right, {1, 2, 3, 4, 5});
}

TEST(deque_deque_test, iteratorArithmeticAndConstIteratorConversionWork) {
    ventra::deque<int> deque = {10, 20, 30, 40};

    auto it = deque.begin();
    EXPECT_EQ(*(it + 2), 30);
    EXPECT_EQ(deque.end() - deque.begin(), 4);

    it += 3;
    EXPECT_EQ(*it, 40);
    it -= 2;
    EXPECT_EQ(it[1], 30);
    EXPECT_TRUE(it > deque.begin());

    ventra::deque<int>::const_iterator converted = deque.begin() + 1;
    EXPECT_EQ(*converted, 20);

    const ventra::deque<int>& const_deque = deque;
    auto cit = const_deque.cbegin();
    EXPECT_EQ(*(cit + 3), 40);
    EXPECT_TRUE(cit < const_deque.cend());
}

TEST(deque_deque_test, resizeShrinksAndFillsWithDefaultOrCustomValues) {
    ventra::deque<int> deque = {1, 2, 3};

    deque.resize(5);
    expect_sequence(deque, {1, 2, 3, 0, 0});

    deque.resize(7, -1);
    expect_sequence(deque, {1, 2, 3, 0, 0, -1, -1});

    deque.resize(2);
    expect_sequence(deque, {1, 2});
}

TEST(deque_deque_test, shrinkToFitReducesCapacityAndKeepsContent) {
    ventra::deque<int> deque(4);

    for (int i = 0; i < 20; ++i) {
        deque.push_back(i);
    }

    const size_t grown_capacity = deque.capacity();

    for (int i = 0; i < 14; ++i) {
        deque.pop_back();
    }

    deque.shrink_to_fit();

    expect_sequence(deque, {0, 1, 2, 3, 4, 5});
    EXPECT_LT(deque.capacity(), grown_capacity);
    EXPECT_GE(deque.capacity(), deque.size());
}

TEST(deque_deque_test, shrinkToFitOnEmptyDequeRestoresSingleChunkCapacity) {
    ventra::deque<int> deque(4);

    for (int i = 0; i < 10; ++i) {
        deque.push_back(i);
    }

    deque.clear();
    deque.shrink_to_fit();

    EXPECT_TRUE(deque.empty());
    EXPECT_EQ(deque.capacity(), 4);

    deque.push_back(42);
    EXPECT_EQ(deque.front(), 42);
    EXPECT_EQ(deque.back(), 42);
}

TEST(deque_deque_test, randomizedOperationsMatchStdDequeModel) {
    ventra::deque<int> actual(2);
    std::deque<int> expected;

    for (int value = 0; value < 20; ++value) {
        actual.push_back(value);
        expected.push_back(value);
    }

    for (int value = 1; value <= 20; ++value) {
        actual.push_front(-value);
        expected.push_front(-value);
    }

    expect_same_as_std_deque(actual, expected);

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> op_dist(0, 8);
    std::uniform_int_distribution<int> value_dist(-100, 100);
    std::uniform_int_distribution<int> size_dist(0, 30);

    for (int step = 0; step < 400; ++step) {
        const int op = op_dist(rng);
        SCOPED_TRACE(::testing::Message() << "step=" << step << " op=" << op);

        switch (op) {
            case 0: {
                const int value = value_dist(rng);
                actual.push_back(value);
                expected.push_back(value);
                break;
            }
            case 1: {
                const int value = value_dist(rng);
                actual.push_front(value);
                expected.push_front(value);
                break;
            }
            case 2: {
                if (expected.empty()) {
                    const int value = value_dist(rng);
                    actual.push_back(value);
                    expected.push_back(value);
                    break;
                }

                actual.pop_back();
                expected.pop_back();
                break;
            }
            case 3: {
                if (expected.empty()) {
                    const int value = value_dist(rng);
                    actual.push_front(value);
                    expected.push_front(value);
                    break;
                }

                actual.pop_front();
                expected.pop_front();
                break;
            }
            case 4: {
                const int value = value_dist(rng);
                const size_t idx = static_cast<size_t>(std::uniform_int_distribution<int>(
                    0,
                    static_cast<int>(expected.size())
                )(rng));
                actual.insert(actual.begin() + static_cast<std::ptrdiff_t>(idx), value);
                expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(idx), value);
                break;
            }
            case 5: {
                if (expected.empty()) {
                    const int value = value_dist(rng);
                    actual.push_back(value);
                    expected.push_back(value);
                    break;
                }

                const size_t idx = static_cast<size_t>(std::uniform_int_distribution<int>(
                    0,
                    static_cast<int>(expected.size() - 1)
                )(rng));
                actual.erase(actual.begin() + static_cast<std::ptrdiff_t>(idx));
                expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(idx));
                break;
            }
            case 6: {
                const size_t new_size = static_cast<size_t>(size_dist(rng));
                actual.resize(new_size);
                expected.resize(new_size);
                break;
            }
            case 7: {
                const size_t new_size = static_cast<size_t>(size_dist(rng));
                const int fill_value = value_dist(rng);
                actual.resize(new_size, fill_value);
                expected.resize(new_size, fill_value);
                break;
            }
            case 8: {
                actual.shrink_to_fit();
                break;
            }
        }

        expect_same_as_std_deque(actual, expected);
    }
}

TEST(deque_deque_test, clearLeavesDequeReusable) {
    ventra::deque<int> deque(2);

    deque.push_back(2);
    deque.push_front(1);
    deque.push_back(3);

    deque.clear();
    EXPECT_TRUE(deque.empty());

    deque.push_back(9);
    deque.push_front(8);
    expect_sequence(deque, {8, 9});
}

TEST(deque_deque_test, copyExceptionLeavesDequeUsableAndDoesNotLeak) {
    throwing_copy_value::alive = 0;
    throwing_copy_value::copies_until_throw = -1;

    {
        ventra::deque<throwing_copy_value> deque(2);
        deque.emplace_back(1);
        deque.emplace_back(2);

        throwing_copy_value value(3);
        throwing_copy_value::copies_until_throw = 0;

        EXPECT_THROW(deque.push_back(value), std::runtime_error);

        ASSERT_EQ(deque.size(), 2);
        EXPECT_EQ(deque.at(0).value, 1);
        EXPECT_EQ(deque.at(1).value, 2);

        throwing_copy_value::copies_until_throw = -1;
        deque.push_back(value);

        ASSERT_EQ(deque.size(), 3);
        EXPECT_EQ(deque.at(2).value, 3);
    }

    EXPECT_EQ(throwing_copy_value::alive, 0);
}

TEST(deque_deque_test, destructorDestroysContainedValues) {
    tracked_value::alive = 0;

    {
        ventra::deque<tracked_value> deque(2);
        deque.emplace_back(1);
        deque.emplace_back(2);
        deque.emplace_front(0);

        EXPECT_EQ(tracked_value::alive, 3);
    }

    EXPECT_EQ(tracked_value::alive, 0);
}
