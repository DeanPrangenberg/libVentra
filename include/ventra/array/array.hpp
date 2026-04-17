//
// Created by deanprangenberg on 04/16/2026.
//

#pragma once

#include <cstddef>
#include <initializer_list>
#include <algorithm>
#include <stdexcept>

namespace ventra {
    template<typename V, size_t init_size>
    class array {
    public:
        array(std::initializer_list<V> init_list);
        ~array() = default;

        // Modifier
        void fill(const V& val);
        void swap(array<V, init_size>& other_array);

        // Access
        V front() const;
        V back() const;
        V& operator[](size_t idx);
        V operator[](size_t idx) const;
        V& at(size_t idx);
        V at(size_t idx) const;
        V* data();

        // Capacity
        size_t size();
        size_t max_size();
        bool empty();

        // Iterator
        V* begin();
        V* end();
        V* cbegin() const;
        V* cend() const;

    private:
        V data_[init_size];
        static const size_t size_ = init_size;
    };



#include "array.tpp"
}
