//
// Created by deanprangenberg on 3/24/2026.
//

#include <gtest/gtest.h>
#include <string>

#include <../include/ventra/unordered_map/unordered_map.hpp>

TEST(unordered_map_test, InsertGetSinglePrimitiveHandlesLvalueAndRvalueInputs) {
    ventra::unordered_map<int, double> map;

    double lvalueVal = 42.0;
    int lvalueKey = 43;

    map.insert(42, lvalueVal);
    map.insert(lvalueKey, 43.0);
    map.insert(44, 44.0);

    ASSERT_EQ(lvalueVal, map.get(42));
    ASSERT_EQ(43.0, map.get(lvalueKey));
    ASSERT_EQ(44.0, map.get(44));
}

TEST(unordered_map_test, InsertGetSingleStringHandlesLvalueAndRvalueInputs) {
    ventra::unordered_map<std::string, std::string> map;

    std::string lvalueVal = "42.0";
    std::string lvalueKey = "44";

    map.insert("42", lvalueVal);
    map.insert("43", "43.0");
    map.insert(lvalueKey, std::string("44.0"));

    ASSERT_EQ(lvalueVal, map.get("42"));
    ASSERT_EQ("43.0", map.get("43"));
    ASSERT_EQ("44.0", map.get(lvalueKey));
}

TEST(unordered_map_test, GetAfterResizeSmall) {
    ventra::unordered_map<int, int> map(2);

    for (int i = 0; i < 3; ++i) {
        map.insert(i, i);
    }

    ASSERT_EQ(0, map.get(0));
    ASSERT_EQ(1, map.get(1));
    ASSERT_EQ(2, map.get(2));
}

TEST(unordered_map_test, GetAfterResizeBig) {
    ventra::unordered_map<int, int> map(2);

    ventra::vector<int> insertedValues;

    for (int i = 0; i < 1000; ++i) {
        map.insert(i, i);
        insertedValues.push_back(i);
    }


    for (int i = 0; i < 30; ++i) {
        ASSERT_EQ(insertedValues[i], map.get(i));
    }
}

TEST(unordered_map_test, GetNotFoundException) {
    ventra::unordered_map<int, double> map;

    ASSERT_THROW(map.get(1), std::invalid_argument);
}

TEST(unordered_map_test, GetDefaultReturn) {
    ventra::unordered_map<int, double> map;

    ASSERT_EQ(map.get(1, 5.0), 5.0);
}

TEST(unordered_map_test, GetAllKeys) {
    ventra::unordered_map<int, double> map;

    ventra::vector<double> insertedValues;

    for (int i = 0; i < 10; ++i) {
        map.insert(i, static_cast<double>(i));
        insertedValues.push_back(static_cast<double>(i));
    }

    auto allKeys = map.getKeys();

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(insertedValues[i], allKeys[i]);
    }
}

TEST(unordered_map_test, GetAllVals) {
    ventra::unordered_map<int, double> map;

    ventra::vector<double> insertedValues;

    for (int i = 0; i < 10; ++i) {
        map.insert(i, static_cast<double>(i));
        insertedValues.push_back(static_cast<double>(i));
    }

    auto allVals = map.getVals();

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(insertedValues[i], allVals[i]);
    }
}

TEST(unordered_map_test, GetAllPairs) {
    ventra::unordered_map<int, double> map;

    ventra::vector<std::pair<int, double>> insertedValues;

    for (int i = 0; i < 10; ++i) {
        std::pair<int, double> pair = {i, static_cast<double>(i)};
        
        map.insert(pair.first, pair.second);
        
        insertedValues.push_back(pair);
    }

    auto allPairs = map.getPairs();

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(insertedValues[i], allPairs[i]);
    }
}

TEST(unordered_map_test, GetRemovedElement) {
    ventra::unordered_map<int, double> map;

    map.insert(1, 1.0);

    map.remove(1);

    ASSERT_THROW(map.get(1.0), std::invalid_argument);
}

TEST(unordered_map_test, BracketOperatorExistingAndNewKeys) {
    ventra::unordered_map<std::string, int> map;

    ASSERT_EQ(0, map["neu"]);

    ASSERT_TRUE(map.isKey("neu"));

    map["neu"] = 42;
    ASSERT_EQ(42, map["neu"]);

    map["noch_neuer"] = 100;
    ASSERT_EQ(100, map["noch_neuer"]);
}

TEST(unordered_map_test, InsertHandlesRvalueKeys) {
    ventra::unordered_map<std::string, double> map;

    std::string lvalueKey = "LvalueKey";

    map.insert(lvalueKey, 1.0);

    map.insert(std::string("RvalueKey"), 2.0);

    std::string keyToMove = "MovedKey";
    map.insert(std::move(keyToMove), 3.0);

    ASSERT_EQ(1.0, map.get("LvalueKey"));
    ASSERT_EQ(2.0, map.get("RvalueKey"));
    ASSERT_EQ(3.0, map.get("MovedKey"));
}
