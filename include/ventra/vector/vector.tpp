//
// Created by deanprangenberg on 04/04/2026.
//

#pragma once

template<typename V>
vector<V>::vector() noexcept : data_(nullptr), size_(0), capacity_(0) {
}

template<typename V>
vector<V>::vector(size_t size) : data_(allocate(size)), size_(0), capacity_(size) {
    size_t constructed = 0;

    try {
        for (; constructed < size; ++constructed) {
            allocator_traits::construct(allocator_, data_ + constructed);
        }
    } catch (...) {
        for (size_t i = 0; i < constructed; ++i) {
            allocator_traits::destroy(allocator_, data_ + i);
        }
        deallocate(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
        throw;
    }

    size_ = size;
}

template<typename V>
vector<V>::vector(const vector<V> &other) : data_(allocate(other.capacity_)), size_(0), capacity_(other.capacity_) {
    size_t constructed = 0;

    try {
        for (; constructed < other.size_; ++constructed) {
            allocator_traits::construct(allocator_, data_ + constructed, other.data_[constructed]);
        }
    } catch (...) {
        for (size_t i = 0; i < constructed; ++i) {
            allocator_traits::destroy(allocator_, data_ + i);
        }
        deallocate(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
        throw;
    }

    size_ = other.size_;
}

template<typename V>
vector<V>::vector(vector<V> &&other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)) {
}

template<typename V>
template<typename ValType>
vector<V>::vector(size_t count, ValType &&val) : data_(allocate(count)), size_(0), capacity_(count) {
    V copy_value(std::forward<ValType>(val));
    size_t constructed = 0;

    try {
        for (; constructed < count; ++constructed) {
            allocator_traits::construct(allocator_, data_ + constructed, copy_value);
        }
    } catch (...) {
        for (size_t i = 0; i < constructed; ++i) {
            allocator_traits::destroy(allocator_, data_ + i);
        }
        deallocate(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
        throw;
    }

    size_ = count;
}

template<typename V>
vector<V>::vector(std::initializer_list<V> init_list)
    : data_(allocate(init_list.size())), size_(0), capacity_(init_list.size()) {
    size_t constructed = 0;

    try {
        for (const V& element: init_list) {
            allocator_traits::construct(allocator_, data_ + constructed, element);
            ++constructed;
        }
    } catch (...) {
        for (size_t i = 0; i < constructed; ++i) {
            allocator_traits::destroy(allocator_, data_ + i);
        }
        deallocate(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
        throw;
    }

    size_ = init_list.size();
}

template<typename V>
vector<V>::~vector() {
    clear();
    deallocate(data_, capacity_);
}

template<typename V>
V* vector<V>::allocate(size_t count) {
    if (count == 0) {
        return nullptr;
    }

    return allocator_traits::allocate(allocator_, count);
}

template<typename V>
void vector<V>::deallocate(V* ptr, size_t count) noexcept {
    if (ptr != nullptr) {
        allocator_traits::deallocate(allocator_, ptr, count);
    }
}

template<typename V>
void vector<V>::destroy_elements(size_t first, size_t last) noexcept {
    for (size_t i = first; i < last; ++i) {
        allocator_traits::destroy(allocator_, data_ + i);
    }
}

template<typename V>
void vector<V>::reserve_exact(size_t new_capacity) {
    V* new_data = allocate(new_capacity);
    size_t constructed = 0;

    try {
        if constexpr (std::is_trivially_copyable_v<V>) {
            if (size_ > 0) {
                std::memmove(new_data, data_, size_ * sizeof(V));
            }
            constructed = size_;
        } else {
            for (; constructed < size_; ++constructed) {
                allocator_traits::construct(allocator_, new_data + constructed, std::move_if_noexcept(data_[constructed]));
            }
        }
    } catch (...) {
        if constexpr (!std::is_trivially_copyable_v<V>) {
            for (size_t i = 0; i < constructed; ++i) {
                allocator_traits::destroy(allocator_, new_data + i);
            }
        }
        deallocate(new_data, new_capacity);
        throw;
    }

    if constexpr (!std::is_trivially_copyable_v<V>) {
        destroy_elements(0, size_);
    }

    deallocate(data_, capacity_);

    data_ = new_data;
    capacity_ = new_capacity;
}

template<typename V>
size_t vector<V>::growth_capacity(size_t min_capacity) const noexcept {
    size_t new_capacity = (capacity_ == 0) ? 1 : capacity_;

    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    return new_capacity;
}

template<typename V>
bool vector<V>::is_valid_insert_position(const V* iter_pos) const noexcept {
    if (size_ == 0) {
        return iter_pos == end();
    }

    return iter_pos >= data_ && iter_pos <= data_ + size_;
}

template<typename V>
void vector<V>::swap(vector<V> &other) noexcept {
    using std::swap;

    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
}

template<typename V>
V& vector<V>::at(size_t idx) {
    if (idx >= size_) {
        throw std::out_of_range(std::format("ventra::vector.at(): Called with out of bounds idx={}", idx));
    }

    return data_[idx];
}

template<typename V>
const V& vector<V>::at(size_t idx) const {
    if (idx >= size_) {
        throw std::out_of_range(std::format("ventra::vector.at(): Called with out of bounds idx={}", idx));
    }

    return data_[idx];
}

template<typename V>
V& vector<V>::front() {
    return data_[0];
}

template<typename V>
const V& vector<V>::front() const {
    return data_[0];
}

template<typename V>
V& vector<V>::back() {
    return data_[size_ - 1];
}

template<typename V>
const V& vector<V>::back() const {
    return data_[size_ - 1];
}

template<typename V>
V* vector<V>::data() noexcept {
    return data_;
}

template<typename V>
const V* vector<V>::data() const noexcept {
    return data_;
}

template<typename V>
bool vector<V>::empty() const noexcept {
    return size_ == 0;
}

template<typename V>
size_t vector<V>::size() const noexcept {
    return size_;
}

template<typename V>
size_t vector<V>::capacity() const noexcept {
    return capacity_;
}

template<typename V>
void vector<V>::reserve(size_t new_reserved_size) {
    if (new_reserved_size > capacity_) {
        reserve_exact(new_reserved_size);
    }
}

template<typename V>
void vector<V>::shrink_to_fit() {
    if (capacity_ != size_) {
        reserve_exact(size_);
    }
}

template<typename V>
template<typename... Args>
V& vector<V>::emplace_back(Args &&... args) {
    if (size_ == capacity_) {
        reserve_exact(growth_capacity(size_ + 1));
    }

    allocator_traits::construct(allocator_, data_ + size_, std::forward<Args>(args)...);
    ++size_;

    return data_[size_ - 1];
}

template<typename V>
template<typename ValType>
void vector<V>::push_back(ValType &&val) {
    emplace_back(std::forward<ValType>(val));
}

template<typename V>
void vector<V>::pop_back() {
    if (size_ == 0) {
        return;
    }

    --size_;
    allocator_traits::destroy(allocator_, data_ + size_);
}

template<typename V>
void vector<V>::pop(size_t idx) {
    if (idx >= size_) {
        return;
    }

    erase(data_ + idx);
}

template<typename V>
template<typename ValType>
void vector<V>::insert(V* iter_pos, ValType &&val) {
    if (!is_valid_insert_position(iter_pos)) {
        throw std::out_of_range("ventra::vector.insert(): Cannot insert at iter_pos is out of vector bound");
    }

    const size_t idx = (size_ == 0) ? 0 : static_cast<size_t>(iter_pos - data_);

    if (size_ == capacity_) {
        const size_t new_capacity = growth_capacity(size_ + 1);
        V* new_data = allocate(new_capacity);
        size_t constructed = 0;

        try {
            for (; constructed < idx; ++constructed) {
                allocator_traits::construct(allocator_, new_data + constructed,
                                            std::move_if_noexcept(data_[constructed]));
            }

            allocator_traits::construct(allocator_, new_data + idx, std::forward<ValType>(val));
            ++constructed;

            for (size_t i = idx; i < size_; ++i, ++constructed) {
                allocator_traits::construct(allocator_, new_data + constructed, std::move_if_noexcept(data_[i]));
            }
        } catch (...) {
            for (size_t i = 0; i < constructed; ++i) {
                allocator_traits::destroy(allocator_, new_data + i);
            }
            deallocate(new_data, new_capacity);
            throw;
        }

        destroy_elements(0, size_);
        deallocate(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
    } else if (idx == size_) {
        allocator_traits::construct(allocator_, data_ + size_, std::forward<ValType>(val));
    } else {
        allocator_traits::construct(allocator_, data_ + size_, std::move(data_[size_ - 1]));


        if constexpr (std::is_trivially_copyable_v<V>) {
            std::memmove(data_ + idx + 1, data_ + idx, (size_ - idx) * sizeof(V));
        } else {
            for (size_t i = size_ - 1; i > idx; --i) {
                data_[i] = std::move(data_[i - 1]);
            }
        }

        data_[idx] = std::forward<ValType>(val);
    }

    ++size_;
}

template<typename V>
V* vector<V>::erase(V* iter_pos) {
    if (size_ == 0 || iter_pos < data_ || iter_pos >= data_ + size_) {
        return iter_pos;
    }

    const size_t idx = static_cast<size_t>(iter_pos - data_);

    if constexpr (std::is_trivially_copyable_v<V>) {
        std::memmove(data_ + idx + 1, data_ + idx, (size_ - idx) * sizeof(V));
    } else {
        for (size_t i = idx + 1; i < size_; ++i) {
            data_[i - 1] = std::move(data_[i]);
        }
    }

    pop_back();

    return data_ + idx;
}

template<typename V>
void vector<V>::clear() {
    destroy_elements(0, size_);
    size_ = 0;
}

template<typename V>
void vector<V>::resize(size_t new_size) {
    if (new_size < size_) {
        destroy_elements(new_size, size_);
        size_ = new_size;
        return;
    }

    if (new_size > capacity_) {
        reserve_exact(growth_capacity(new_size));
    }

    size_t constructed = size_;

    try {
        for (; constructed < new_size; ++constructed) {
            allocator_traits::construct(allocator_, data_ + constructed);
        }
    } catch (...) {
        for (size_t i = size_; i < constructed; ++i) {
            allocator_traits::destroy(allocator_, data_ + i);
        }
        throw;
    }

    size_ = new_size;
}

template<typename V>
vector<V> &vector<V>::operator=(const vector<V> &other) {
    if (this == &other) {
        return *this;
    }

    vector<V> tmp(other);
    swap(tmp);
    return *this;
}

template<typename V>
vector<V> &vector<V>::operator=(vector<V> &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    clear();
    deallocate(data_, capacity_);

    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    capacity_ = std::exchange(other.capacity_, 0);

    return *this;
}

template<typename V>
bool vector<V>::operator==(const vector<V> &other) const {
    if (size_ != other.size_) {
        return false;
    }

    for (size_t i = 0; i < size_; ++i) {
        if (data_[i] != other.data_[i]) {
            return false;
        }
    }

    return true;
}

template<typename V>
V& vector<V>::operator[](size_t idx) {
    if (idx >= size_) {
        throw std::out_of_range(std::format("ventra::vector.at(): Called with out of bounds idx={}", idx));
    }

    return data_[idx];
}

template<typename V>
const V& vector<V>::operator[](size_t idx) const {
    if (idx >= size_) {
        throw std::out_of_range(std::format("ventra::vector.at(): Called with out of bounds idx={}", idx));
    }

    return data_[idx];
}
