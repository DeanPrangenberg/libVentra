//
// Created by deanprangenberg on 4/1/2026.
//

#pragma once

#include <functional>
#include <stdexcept>

#include "../vector/vector.hpp"

namespace ventra {

template<typename K, typename V>
class unordered_map {
public:
    explicit unordered_map(const size_t &initSize = 16);
    ~unordered_map() = default;

    template<typename KeyType, typename ValType>
    void insert(KeyType &&key, ValType &&val);

    V& get(const K &key);
    const V& get(const K &key) const;

    V get(const K &key, V&& ifNotFoundReturn) const;

    void remove(const K &key);

    ventra::vector<K> getKeys() const;
    ventra::vector<V> getVals() const;
    ventra::vector<std::pair<K, V>> getPairs() const;

    bool isKey(const K &key) const;
    bool isVal(const V &val) const;

    V& operator[](const K &key);

private:
    ventra::vector<ventra::vector<std::pair<K, V>>> buckets_;
    size_t numBuckets_;
    size_t elementCounter_;

    size_t calcKeyIdx(const K &key) const;

    void rehash();
};

#include "unordered_map.tpp"

}
