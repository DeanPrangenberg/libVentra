//
// Created by deanprangenberg on 4/1/2026.
//

#pragma once

template<typename K, typename V>
unordered_map<K, V>::unordered_map(const size_t &initSize) : numBuckets_(initSize), elementCounter_(0) {
    if (initSize < 1) {
        throw std::invalid_argument("unordered_map initSize must be > 0");
    }

    buckets_ = ventra::vector<ventra::vector<std::pair<K, V> > >(initSize);
}

template<typename K, typename V>
template<typename KeyType, typename ValType>
void unordered_map<K, V>::insert(KeyType&& key, ValType&& val) {
    size_t keyIndex = calcKeyIdx(key);

    bool found = false;

    for (auto& pair : buckets_[keyIndex]) {
        if (pair.first == key) {
            pair.second = std::forward<ValType>(val);
            found = true;
            break;
        }
    }

    if (!found) {
        buckets_[keyIndex].emplace_back(
            std::forward<KeyType>(key),
            std::forward<ValType>(val)
        );

        ++elementCounter_;
    }

    if (elementCounter_ >= numBuckets_) {
        rehash();
    }
}

template<typename K, typename V>
V& unordered_map<K, V>::get(const K &key) {
    size_t keyIndex = calcKeyIdx(key);

    // find value
    for (auto& pair : buckets_[keyIndex]) {
        if (pair.first == key) {
            return pair.second;
        }
    }

    throw std::invalid_argument("key not found");
}

template<typename K, typename V>
const V& unordered_map<K, V>::get(const K &key) const {
    size_t keyIndex = calcKeyIdx(key);

    // find value
    for (const auto& pair : buckets_[keyIndex]) {
        if (pair.first == key) {
            return pair.second;
        }
    }

    throw std::invalid_argument("key not found");
}

template<typename K, typename V>
V unordered_map<K, V>::get(const K &key, V&& ifNotFoundReturn) const {
    size_t keyIndex = calcKeyIdx(key);

    // find value
    for (const auto& pair : buckets_[keyIndex]) {
        if (pair.first == key) {
            return pair.second;
        }
    }

    return ifNotFoundReturn;
}

template<typename K, typename V>
void unordered_map<K, V>::remove(const K &key) {
    size_t keyIndex = calcKeyIdx(key);

    // find value
    for (size_t i = 0; i < buckets_[keyIndex].size(); ++i) {
        if (buckets_[keyIndex][i].first == key) {
            buckets_[keyIndex].erase(buckets_[keyIndex].begin() + i);
            elementCounter_--;
            return;
        }
    }
}

template<typename K, typename V>
ventra::vector<K> unordered_map<K, V>::getKeys() const {
    ventra::vector<K> out;
    out.reserve(elementCounter_);

    for (const auto& bucket : buckets_) {
        for (const auto& pair : bucket) {
            out.emplace_back(pair.first);
        }
    }

    return out;
}

template<typename K, typename V>
ventra::vector<V> unordered_map<K, V>::getVals() const {
    ventra::vector<V> out;
    out.reserve(elementCounter_);

    for (const auto& bucket : buckets_) {
        for (const auto& pair : bucket) {
            out.emplace_back(pair.second);
        }
    }

    return out;
}

template<typename K, typename V>
ventra::vector<std::pair<K, V>> unordered_map<K, V>::getPairs() const {
    ventra::vector<std::pair<K, V>> out;
    out.reserve(elementCounter_);

    for (const auto& bucket : buckets_) {
        for (const auto& pair : bucket) {
            out.emplace_back(pair);
        }
    }

    return out;
}

template<typename K, typename V>
bool unordered_map<K, V>::isKey(const K &key) const {
    size_t idx = calcKeyIdx(key);
    for (const auto& pair : buckets_[idx]) {
        if (pair.first == key) return true;
    }

    return false;
}

template<typename K, typename V>
bool unordered_map<K, V>::isVal(const V &val) const {
    for (const auto& bucket : buckets_) {
        for (const auto& pair : bucket) {
            if (pair.second == val) {
                return true;
            }
        }
    }

    return false;
}

template<typename K, typename V>
size_t unordered_map<K, V>::calcKeyIdx(const K& key) const {
    return std::hash<K>{}(key) % numBuckets_;
}

template<typename K, typename V>
void unordered_map<K, V>::rehash() {
    size_t newNumBuckets = numBuckets_ * 2;
    ventra::vector<ventra::vector<std::pair<K, V>>> newBuckets(newNumBuckets);

    for (auto& oldBucket : buckets_) {
        for (auto& item : oldBucket) {
            size_t newIndex = std::hash<K>{}(item.first) % newNumBuckets;

            newBuckets[newIndex].emplace_back(std::move(item));
        }
    }

    buckets_ = std::move(newBuckets);
    numBuckets_ = newNumBuckets;
}

template<typename K, typename V>
V& unordered_map<K, V>::operator[](const K &key) {
    size_t keyIndex = calcKeyIdx(key);

    for (auto& pair : buckets_[keyIndex]) {
        if (pair.first == key) {
            return pair.second;
        }
    }

    if (elementCounter_ >= numBuckets_) {
        rehash();
        keyIndex = calcKeyIdx(key);
    }

    buckets_[keyIndex].emplace_back(key, V{});
    elementCounter_++;

    return buckets_[keyIndex].back().second;
}
