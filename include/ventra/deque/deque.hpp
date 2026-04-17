//
// Created by deanprangenberg on 04/14/2026.
//

#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <ventra/vector/vector.hpp>

namespace ventra {
    template<typename V, typename Alloc = std::allocator<V>>
    class deque {
    public:
        class iterator;
        class const_iterator;

        explicit deque(const size_t chunk_size = 64);
        deque(const deque& other);
        deque(deque&& other);
        deque(const std::initializer_list<V> init_list);
        ~deque();

        deque& operator=(const deque& other);
        deque& operator=(deque&& other);

        // Modifier
        void assign(const size_t amount, const V& val);
        void push_back(V&& val);
        void push_back(const V& val);
        void push_front(V&& val);
        void push_front(const V& val);
        void pop_back();
        void pop_front();
        void insert(iterator pos, V&& val);
        void insert(iterator pos, const V& val);
        void erase(iterator pos);
        void swap(deque<V, Alloc>& other_deque);
        void clear();

        template<typename... Args>
        V& emplace(iterator pos, Args&&... args);

        template<typename... Args>
        V& emplace_back(Args&&... args);

        template<typename... Args>
        V& emplace_front(Args&&... args);

        // Access
        V& front();
        V& back();
        V& operator[](const size_t idx);
        V& at(const size_t idx);
        const V& front() const;
        const V& back() const;
        const V& operator[](const size_t idx) const;
        const V& at(const size_t idx) const;

        // Capacity
        size_t size() const;
        void resize(const size_t new_size);
        void resize(const size_t new_size, const V& val);
        bool empty() const;
        size_t capacity() const;
        void shrink_to_fit();

        // Iterator
        iterator begin() { return iterator(this, 0); }

        iterator end() { return iterator(this, size_); }

        const_iterator begin() const { return const_iterator(this, 0); }

        const_iterator end() const { return const_iterator(this, size_); }

        const_iterator cbegin() const { return const_iterator(this, 0); }

        const_iterator cend() const { return const_iterator(this, size_); }

    private:
        void rebuild_chunks();
        void ensure_space_push_back();
        void ensure_space_push_front();
        void calc_idx(const size_t idx, size_t& chunk_idx, size_t& offset_idx) const;
        bool is_valid_idx(const size_t idx) const;
        void make_space_at(const size_t idx);

        using allocator_type = Alloc;
        using traits = std::allocator_traits<allocator_type>;

        using chunk_pointer = V*;
        using chunk_allocator_type = typename traits::template rebind_alloc<chunk_pointer>;
        using chunk_traits = std::allocator_traits<chunk_allocator_type>;

        chunk_pointer* chunks_ = nullptr;

        size_t chunk_size_ = 0;
        size_t chunk_counter_ = 0;
        size_t max_chunk_amount_ = 0;
        size_t size_ = 0;
        size_t capacity_ = 0;

        size_t start_chunk_idx_ = 0;
        size_t start_offset_idx_ = 0;
        size_t end_chunk_idx_ = 0;
        size_t end_offset_idx_ = 0;

        allocator_type alloc_;
        chunk_allocator_type chunk_alloc_;

        static constexpr size_t init_chunks_capacity = 16;

    public:
        class iterator {
        private:
            friend class const_iterator;

            deque* deque_ = nullptr;
            size_t idx_ = 0;

        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = V;
            using difference_type = std::ptrdiff_t;
            using pointer = V*;
            using reference = V&;

            iterator() = default;


            iterator(deque* deq, const size_t idx) : deque_(deq), idx_(idx) {
            }

            reference operator*() const {
                return (*deque_)[idx_];
            }

            pointer operator->() const {
                return &(*deque_)[idx_];
            }

            iterator& operator++() {
                ++idx_;
                return *this;
            }

            iterator operator++(int) {
                iterator temp = *this;
                ++(*this);
                return temp;
            }

            iterator& operator--() {
                --idx_;
                return *this;
            }

            iterator operator--(int) {
                iterator temp = *this;
                --(*this);
                return temp;
            }

            iterator& operator+=(difference_type n) {
                idx_ = static_cast<size_t>(static_cast<difference_type>(idx_) + n);
                return *this;
            }

            iterator& operator-=(difference_type n) {
                return *this += -n;
            }

            iterator operator+(difference_type n) const {
                iterator temp = *this;
                temp += n;
                return temp;
            }

            iterator operator-(difference_type n) const {
                iterator temp = *this;
                temp -= n;
                return temp;
            }


            difference_type operator-(const iterator& other) const {
                return static_cast<difference_type>(idx_) - static_cast<difference_type>(other.idx_);
            }

            reference operator[](difference_type n) const {
                return *(*this + n);
            }

            bool operator==(const iterator& other) const {
                return deque_ == other.deque_ && idx_ == other.idx_;
            }

            bool operator!=(const iterator& other) const {
                return !(*this == other);
            }

            bool operator<(const iterator& other) const {
                return idx_ < other.idx_;
            }

            bool operator<=(const iterator& other) const {
                return idx_ <= other.idx_;
            }

            bool operator>(const iterator& other) const {
                return idx_ > other.idx_;
            }

            bool operator>=(const iterator& other) const {
                return idx_ >= other.idx_;
            }

            size_t index() const {
                return idx_;
            }
        };

        class const_iterator {
        private:
            const deque* deque_ = nullptr;
            size_t idx_ = 0;

        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = V;
            using difference_type = std::ptrdiff_t;
            using pointer = const V*;
            using reference = const V&;

            const_iterator() = default;

            const_iterator(const deque* deq, const size_t idx) : deque_(deq), idx_(idx) {
            }

            const_iterator(const iterator& it) : deque_(it.deque_), idx_(it.idx_) {
            }

            reference operator*() const {
                return (*deque_)[idx_];
            }

            pointer operator->() const {
                return &(*deque_)[idx_];
            }

            const_iterator& operator++() {
                ++idx_;
                return *this;
            }

            const_iterator operator++(int) {
                const_iterator temp = *this;
                ++(*this);
                return temp;
            }

            const_iterator& operator--() {
                --idx_;
                return *this;
            }

            const_iterator operator--(int) {
                const_iterator temp = *this;
                --(*this);
                return temp;
            }

            const_iterator& operator+=(difference_type n) {
                idx_ = static_cast<size_t>(static_cast<difference_type>(idx_) + n);
                return *this;
            }

            const_iterator& operator-=(difference_type n) {
                return *this += -n;
            }

            const_iterator operator+(difference_type n) const {
                const_iterator temp = *this;
                temp += n;
                return temp;
            }

            const_iterator operator-(difference_type n) const {
                const_iterator temp = *this;
                temp -= n;
                return temp;
            }

            difference_type operator-(const const_iterator& other) const {
                return static_cast<difference_type>(idx_) - static_cast<difference_type>(other.idx_);
            }

            reference operator[](difference_type n) const {
                return *(*this + n);
            }

            bool operator==(const const_iterator& other) const {
                return deque_ == other.deque_ && idx_ == other.idx_;
            }

            bool operator!=(const const_iterator& other) const {
                return !(*this == other);
            }

            bool operator<(const const_iterator& other) const {
                return idx_ < other.idx_;
            }

            bool operator<=(const const_iterator& other) const {
                return idx_ <= other.idx_;
            }

            bool operator>(const const_iterator& other) const {
                return idx_ > other.idx_;
            }

            bool operator>=(const const_iterator& other) const {
                return idx_ >= other.idx_;
            }

            size_t index() const {
                return idx_;
            }
        };
    };

#include "deque.tpp"
} // namespace ventra
