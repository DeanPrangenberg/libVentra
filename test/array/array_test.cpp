//
// Created by deanprangenberg on 04/16/2026.
//

#include <gtest/gtest.h>
#include <ventra/array/array.hpp>


TEST(array_array_test, size) {
    ventra::array<int, 3> arr{};

    ASSERT_EQ(arr.size(), 3);
}

TEST(array_array_test, constructWithDefaultAt) {
    ventra::array<int, 3> arr = {1, 1, 1};

    ASSERT_EQ(arr.size(), 3);

    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(arr.at(i), 1);
    }
}

TEST(array_array_test, fill) {
    ventra::array<int, 3> arr{};

    arr.fill(1);

    ASSERT_EQ(arr.size(), 3);

    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(arr.at(i), 1);
    }
}

TEST(array_array_test, swap) {
    ventra::array<int, 3> arr1 = {1, 1, 1};
    ventra::array<int, 3> arr2 = {2, 2, 2};

    arr1.swap(arr2);

    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(arr1.at(i), 2);
        ASSERT_EQ(arr2.at(i), 1);
    }
}

TEST(array_array_test, frontBackAtOperator) {
    ventra::array<int, 2> arr{};

    arr[0] = 1;
    arr[1] = 2;

    ASSERT_EQ(arr[0], arr.front());
    ASSERT_EQ(arr[1], arr.back());
}

TEST(array_array_test, data) {
    ventra::array<int, 2> arr{};

    arr[0] = 1;
    arr[1] = 2;

    int* data_ptr = arr.data();

    ASSERT_EQ(*data_ptr, 1);
    ASSERT_EQ(*data_ptr + 1, 2);
}

TEST(array_array_test, max_size) {
    ventra::array<int, 3> arr{};

    ASSERT_EQ(arr.size(), arr.max_size());
}

TEST(array_array_test, empty) {
    ventra::array<int, 3> arr1{};

    ASSERT_FALSE(arr1.empty());

    ventra::array<int, 0> arr2{};

    ASSERT_TRUE(arr2.empty());
}

TEST(array_array_test, iterator) {
    ventra::array<int, 4> arr = {0, 1, 2, 3};

    int idx = 0;
    for (auto val : arr) {
        ASSERT_EQ(val, idx);
        idx++;
    }
}

TEST(array_array_test, reverse_iterator) {
    ventra::array<int, 4> arr = {0, 1, 2, 3};

    int idx = 0;
    for (auto val : arr) {
        ASSERT_EQ(val, idx);
        idx++;
    }
}

TEST(array_array_test, const_iterator) {
    ventra::array<int, 4> arr = {0, 1, 2, 3};

    int idx = 0;
    for (const auto val : arr) {
        ASSERT_EQ(val, idx);
        idx++;
    }
}

TEST(array_array_test, at_out_of_bounds) {
    ventra::array<int, 3> arr{};
    EXPECT_THROW(arr.at(3), std::out_of_range);
}

TEST(array_array_test, zero_sized_array) {
    ventra::array<int, 0> arr{};
    ASSERT_EQ(arr.size(), 0);
    ASSERT_EQ(arr.max_size(), 0);
    EXPECT_THROW(arr.at(0), std::out_of_range);

    ASSERT_EQ(arr.begin(), arr.end());
}

TEST(array_array_test, const_access) {
    const ventra::array<int, 3> arr = {1, 2, 3};

    ASSERT_EQ(arr.front(), 1);
    ASSERT_EQ(arr.back(), 3);
    ASSERT_EQ(arr[1], 2);
    ASSERT_EQ(arr.at(2), 3);
}

TEST(array_array_test, complex_types) {
    ventra::array<std::string, 2> arr = {"Hallo", "Welt"};
    ASSERT_EQ(arr[0], "Hallo");

    ventra::array<std::string, 2> arr2{};
    arr.swap(arr2);
    ASSERT_EQ(arr2[1], "Welt");
}

TEST(array_array_test, iterators_bounds) {
    ventra::array<int, 3> arr = {10, 20, 30};
    ASSERT_EQ(*arr.begin(), 10);

    auto it = arr.end();
    it--;
    ASSERT_EQ(*it, 30);

    ASSERT_EQ(std::distance(arr.begin(), arr.end()), 3);
}

TEST(array_array_test, swap_reference_behavior) {
    ventra::array<int, 3> arr1 = {1, 2, 3};
    ventra::array<int, 3> arr2 = {7, 8, 9};

    int& ref = arr1[0];
    int* ptr = arr1.data();

    ASSERT_EQ(ref, 1);
    ASSERT_EQ(*ptr, 1);

    arr1.swap(arr2);

    ASSERT_EQ(ref, 7);
    ASSERT_EQ(*ptr, 7);

    ASSERT_EQ(arr1[0], 7);
}