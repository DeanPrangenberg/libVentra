//
// Created by deanprangenberg on 04/16/2026.
//

#pragma once

template<typename V, size_t init_size>
array<V, init_size>::array(std::initializer_list<V> init_list) {
    size_t idx = 0;
    for (const V& val : init_list) {
        data_[idx] = val;
        ++idx;
    }
}

template<typename V, size_t init_size>
void array<V, init_size>::fill(const V &val) {
    for (size_t i = 0; i < size_; ++i) {
        data_[i] = val;
    }
}

template<typename V, size_t init_size>
void array<V, init_size>::swap(array<V, init_size>& other_array) {
    for (size_t i = 0; i < size_; ++i) {
        std::swap(other_array.data_[i], data_[i]);
    }
}

template<typename V, size_t init_size>
V array<V, init_size>::front() const {
    return data_[0];
}

template<typename V, size_t init_size>
V array<V, init_size>::back() const {
    return data_[size_ - 1];
}

template<typename V, size_t init_size>
V& array<V, init_size>::operator[](size_t idx) {
    return data_[idx];
}

template<typename V, size_t init_size>
V array<V, init_size>::operator[](size_t idx) const {
    return data_[idx];
}

template<typename V, size_t init_size>
V& array<V, init_size>::at(size_t idx) {
    if (idx > size_ - 1 || size_ == 0) {
        throw std::out_of_range("Index out of bounds");
    }

    return data_[idx];
}

template<typename V, size_t init_size>
V array<V, init_size>::at(size_t idx) const {
    if (idx > size_ - 1 || size_ == 0) {
        throw std::out_of_range("Index out of bounds");
    }

    return data_[idx];
}

template<typename V, size_t init_size>
V* array<V, init_size>::data() {
    return &data_[0];
}

template<typename V, size_t init_size>
size_t array<V, init_size>::size() {
    return size_;
}

template<typename V, size_t init_size>
size_t array<V, init_size>::max_size() {
    return size_;
}

template<typename V, size_t init_size>
bool array<V, init_size>::empty() {
    return (size_ == 0);
}

template<typename V, size_t init_size>
V* array<V, init_size>::begin() {
    return &data_[0];
}

template<typename V, size_t init_size>
V* array<V, init_size>::end() {
    if (size_ == 0) {
        return &data_[0];
    }

    return &data_[size_];
}

template<typename V, size_t init_size>
V* array<V, init_size>::cbegin() const {
    return &data_[0];
}

template<typename V, size_t init_size>
V* array<V, init_size>::cend() const {
    return &data_[size_];
}
