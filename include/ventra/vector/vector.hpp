//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <cstring>

namespace ventra {
    template<typename V>
    class vector {
    public:
        explicit vector() noexcept; // Default
        explicit vector(size_t size); // Init Size
        vector(const vector& other); // Copy
        vector(vector&& other) noexcept; // Move

        template<typename ValType>
        vector(size_t count, ValType&& val); // Fill

        vector(std::initializer_list<V> init_list); // init

        ~vector();

        // Element Access
        V& at(size_t idx);
        const V& at(size_t idx) const;
        V& front();
        const V& front() const;
        V& back();
        const V& back() const;
        V* data() noexcept;
        const V* data() const noexcept;

        // Capacity
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] size_t capacity() const noexcept;
        void reserve(size_t new_reserved_size);
        void shrink_to_fit();

        // Modifiers
        template<typename... Args>
        V& emplace_back(Args&&... args);

        template<typename ValType>
        void push_back(ValType&& val);
        void pop_back();
        void pop(size_t idx);

        template<typename ValType>
        void insert(V* iter_pos, ValType&& val);
        V* erase(V* iter_pos);
        void clear();
        void resize(size_t new_size);

        // Iterators
        V* begin() noexcept {
            return data_;
        }

        const V* begin() const noexcept {
            return data_;
        }

        V* end() noexcept {
            return data_ == nullptr ? nullptr : data_ + size_;
        }

        const V* end() const noexcept {
            return data_ == nullptr ? nullptr : data_ + size_;
        }

        // Operator
        vector& operator=(const vector& other);
        vector& operator=(vector&& other) noexcept;
        bool operator==(const vector &other) const;
        V& operator[](size_t idx);
        const V& operator[](size_t idx) const;

    private:
        using allocator_traits = std::allocator_traits<std::allocator<V>>;

        V* allocate(size_t count);
        void deallocate(V* ptr, size_t count) noexcept;
        void destroy_elements(size_t first, size_t last) noexcept;

        void reserve_exact(size_t new_capacity);
        [[nodiscard]] size_t growth_capacity(size_t min_capacity) const noexcept;
        bool is_valid_insert_position(const V* iter_pos) const noexcept;
        void swap(vector& other) noexcept;

        std::allocator<V> allocator_;
        V* data_;
        size_t size_;
        size_t capacity_;
    };

#include "vector.tpp"
}
