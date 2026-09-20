//
// Created by deanprangenberg on 04/04/2026.
//

#include <gtest/gtest.h>
#include <ventra/vector/vector.hpp>
#include <string>

namespace {
    struct no_default_construct {
        explicit no_default_construct(int initial_value) : value(initial_value) {}

        int value;
    };

    struct tracked_value {
        static inline int alive = 0;

        tracked_value() : value(0) {
            ++alive;
        }

        explicit tracked_value(int initial_value) : value(initial_value) {
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
}

TEST(vector_vector_test, sizeChanging) {
    ventra::vector<int> vec;
    ASSERT_EQ(vec.size(), 0);

    for (int i = 1; i < 100; i++) {
        vec.push_back(i);
        ASSERT_EQ(vec.size(), i);
    }
}

TEST(vector_vector_test, atAndAccessOperator) {
    ventra::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);

    ASSERT_EQ(vec.at(0), 10);
    ASSERT_EQ(vec.at(1), 20);

    ASSERT_EQ(vec[0], 10);
    ASSERT_EQ(vec[1], 20);

    // Testen ob Zuweisung klappt
    vec[0] = 99;
    ASSERT_EQ(vec[0], 99);
}

TEST(vector_vector_test, frontBack) {
    ventra::vector<int> vec;
    vec.push_back(100);
    ASSERT_EQ(vec.front(), 100);
    ASSERT_EQ(vec.back(), 100);

    vec.push_back(200);
    ASSERT_EQ(vec.front(), 100);
    ASSERT_EQ(vec.back(), 200);
}

TEST(vector_vector_test, dataIteration) {
    ventra::vector<int> vec;
    for (int i = 0; i < 5; ++i) vec.push_back(i);

    auto ptr = vec.data();
    for (int i = 0; i < vec.size(); ++i) {
        ASSERT_EQ(*(ptr + i), i);
    }
}

TEST(vector_vector_test, empty) {
    ventra::vector<int> vec;
    ASSERT_TRUE(vec.empty());

    vec.push_back(0);
    ASSERT_FALSE(vec.empty());

    vec.pop_back();
    ASSERT_TRUE(vec.empty());
}

TEST(vector_vector_test, capacityReserveSizeShrink) {
    ventra::vector<int> vec;
    ASSERT_EQ(vec.capacity(), 0);

    vec.push_back(0);
    ASSERT_TRUE(vec.capacity() >= 1);

    vec.reserve(100);
    ASSERT_EQ(vec.capacity(), 100);
    ASSERT_EQ(vec.size(), 1);

    vec.shrink_to_fit();
    ASSERT_EQ(vec.capacity(), 1);
    ASSERT_EQ(vec.size(), 1);
}

TEST(vector_vector_test, clear) {
    ventra::vector<int> vec;
    vec.push_back(0);
    vec.push_back(1);

    vec.clear();
    ASSERT_EQ(vec.size(), 0);
    ASSERT_TRUE(vec.empty());
}

TEST(vector_vector_test, resize) {
    ventra::vector<int> vec;
    vec.resize(10);
    ASSERT_EQ(vec.size(), 10);
    ASSERT_TRUE(vec.capacity() >= 10);

    vec.resize(5);
    ASSERT_EQ(vec.size(), 5);
}

TEST(vector_vector_test, reserveThenResize) {
    ventra::vector<int> vec;

    vec.reserve(10);
    vec.resize(5);

    ASSERT_EQ(vec.size(), 5);
    ASSERT_EQ(vec.capacity(), 10);

    for (size_t i = 0; i < vec.size(); ++i) {
        ASSERT_EQ(vec[i], 0);
    }
}

TEST(vector_vector_test, outOfBoundsThrows) {
    ventra::vector<int> vec;
    vec.push_back(1);

    EXPECT_THROW(vec.at(1), std::out_of_range);
    EXPECT_THROW(vec.at(100), std::out_of_range);
}

TEST(vector_vector_test, popBackOnEmpty) {
    ventra::vector<int> vec;
    // Sollte nicht abstürzen!
    EXPECT_NO_THROW(vec.pop_back());
    ASSERT_EQ(vec.size(), 0);
}

TEST(vector_vector_test, popOnInvalidIndex) {
    ventra::vector<int> vec;
    vec.push_back(1);

    vec.pop(5);
    ASSERT_EQ(vec.size(), 1);
}

TEST(vector_vector_test, complexTypesString) {
    ventra::vector<std::string> vec;

    vec.push_back("Hello");
    vec.push_back("World");
    vec.push_back("Ventra");

    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[0], "Hello");
    ASSERT_EQ(vec[2], "Ventra");

    vec.pop(1);

    ASSERT_EQ(vec.size(), 2);
    ASSERT_EQ(vec[1], "Ventra");
}

TEST(vector_vector_test, insertShiftCheck) {
    ventra::vector<int> vec;
    vec.push_back(10);
    vec.push_back(30);

    // Einfügen in die Mitte (Index 1)
    vec.insert(vec.data() + 1, 20);

    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[0], 10);
    ASSERT_EQ(vec[1], 20);
    ASSERT_EQ(vec[2], 30);

    // Einfügen am Anfang
    vec.insert(vec.data(), 0);

    ASSERT_EQ(vec.size(), 4);
    ASSERT_EQ(vec[0], 0);
    ASSERT_EQ(vec[1], 10);
}

TEST(vector_vector_test, eraseShiftCheck) {
    ventra::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    // Lösche das erste Element
    auto next = vec.erase(vec.data());

    ASSERT_EQ(vec.size(), 2);
    ASSERT_EQ(vec[0], 20);
    ASSERT_EQ(vec[1], 30);
    ASSERT_EQ(*next, 20);
}

TEST(vector_vector_test, eraseLast) {
    ventra::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    ASSERT_NO_THROW(vec.erase(vec.end() - 1));

    ASSERT_EQ(vec.at(0), 10);
    ASSERT_EQ(vec.at(1), 20);

    ASSERT_EQ(*(vec.end() - 2), 10);
    ASSERT_EQ(*(vec.end() - 1), 20);
}

TEST(vector_vector_test, rangeLoopTest) {
    ventra::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    int compare_val = 10;
    for (auto val : vec) {
        ASSERT_EQ(val, compare_val);
        compare_val += 10;
    }
}

TEST(vector_vector_test, insertAtEnd) {
    ventra::vector<int> vec;

    ASSERT_NO_THROW(vec.insert(vec.end(), 1));
    ASSERT_EQ(vec.size(), 1);
    ASSERT_EQ(vec[0], 1);

    ASSERT_NO_THROW(vec.insert(vec.end(), 2));
    ASSERT_EQ(vec.size(), 2);
    ASSERT_EQ(vec[1], 2);
}

TEST(vector_vector_test, emplaceBackSupportsNonDefaultConstructible) {
    ventra::vector<no_default_construct> vec;

    vec.emplace_back(10);
    vec.emplace_back(20);

    ASSERT_EQ(vec.size(), 2);
    ASSERT_EQ(vec[0].value, 10);
    ASSERT_EQ(vec[1].value, 20);
}

TEST(vector_vector_test, removeOperationsDestroyStoredValues) {
    tracked_value::alive = 0;

    {
        ventra::vector<tracked_value> vec;
        vec.emplace_back(10);
        vec.emplace_back(20);
        vec.emplace_back(30);

        ASSERT_EQ(tracked_value::alive, 3);

        vec.pop_back();
        ASSERT_EQ(tracked_value::alive, 2);

        vec.erase(vec.begin());
        ASSERT_EQ(tracked_value::alive, 1);

        vec.clear();
        ASSERT_EQ(tracked_value::alive, 0);
    }

    ASSERT_EQ(tracked_value::alive, 0);
}
