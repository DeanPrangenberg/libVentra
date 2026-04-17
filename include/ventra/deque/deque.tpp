//
// Created by deanprangenberg on 04/14/2026.
//

#pragma once

template<typename V, typename Alloc>
deque<V, Alloc>::deque(const size_t chunk_size)
    : chunk_size_(chunk_size), max_chunk_amount_(init_chunks_capacity), chunk_alloc_(alloc_) {
    if (chunk_size_ == 0) {
        throw std::invalid_argument("chunk_size must be > 0");
    }

    const size_t mid = max_chunk_amount_ / 2;

    // alloc chunks
    chunks_ = chunk_traits::allocate(chunk_alloc_, max_chunk_amount_);
    std::uninitialized_value_construct_n(chunks_, max_chunk_amount_);

    // alloc mid chunk
    chunks_[mid] = traits::allocate(alloc_, chunk_size_);
    ++chunk_counter_;

    // set default vals
    capacity_ = chunk_size_;
    size_ = 0;

    start_chunk_idx_ = mid;
    start_offset_idx_ = chunk_size_ / 2;

    end_chunk_idx_ = mid;
    end_offset_idx_ = start_offset_idx_;
}

template<typename V, typename Alloc>
deque<V, Alloc>::deque(const deque& other) : deque(other.chunk_size_ == 0 ? 64 : other.chunk_size_) {
    for (const V& value : other) {
        push_back(value);
    }
}

template<typename V, typename Alloc>
deque<V, Alloc>::deque(deque&& other) : deque(other.chunk_size_ == 0 ? 64 : other.chunk_size_) {
    swap(other);
}

template<typename V, typename Alloc>
deque<V, Alloc>::deque(const std::initializer_list<V> init_list) : deque() {
    for (const V& val : init_list) {
        push_back(val);
    }
}

template<typename V, typename Alloc>
deque<V, Alloc>::~deque() {
    while (!empty()) {
        pop_back();
    }

    for (size_t i = 0; i < max_chunk_amount_; ++i) {
        if (chunks_[i] != nullptr) {
            traits::deallocate(alloc_, chunks_[i], chunk_size_);
        }
    }

    chunk_traits::deallocate(chunk_alloc_, chunks_, max_chunk_amount_);
}

template<typename V, typename Alloc>
deque<V, Alloc>& deque<V, Alloc>::operator=(const deque& other) {
    if (this == &other) {
        return *this;
    }

    deque copy(other);
    swap(copy);
    return *this;
}

template<typename V, typename Alloc>
deque<V, Alloc>& deque<V, Alloc>::operator=(deque&& other) {
    if (this == &other) {
        return *this;
    }

    deque moved(std::move(other));
    swap(moved);
    return *this;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::assign(const size_t amount, const V& val) {
    clear();

    for (size_t i = 0; i < amount; ++i) {
        push_back(val);
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::push_back(V&& val) {
    ensure_space_push_back();

    if (empty()) {
        start_chunk_idx_ = end_chunk_idx_;
        start_offset_idx_ = end_offset_idx_;
    }

    V* pos = chunks_[end_chunk_idx_] + end_offset_idx_;
    traits::construct(alloc_, pos, std::move(val));

    ++end_offset_idx_;
    ++size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::push_back(const V& val) {
    ensure_space_push_back();

    if (empty()) {
        start_chunk_idx_ = end_chunk_idx_;
        start_offset_idx_ = end_offset_idx_;
    }

    V* pos = chunks_[end_chunk_idx_] + end_offset_idx_;
    traits::construct(alloc_, pos, val);

    ++end_offset_idx_;
    ++size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::push_front(V&& val) {
    ensure_space_push_front();

    --start_offset_idx_;

    V* pos = chunks_[start_chunk_idx_] + start_offset_idx_;
    traits::construct(alloc_, pos, std::move(val));

    ++size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::push_front(const V& val) {
    ensure_space_push_front();

    --start_offset_idx_;

    V* pos = chunks_[start_chunk_idx_] + start_offset_idx_;
    traits::construct(alloc_, pos, val);

    ++size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::pop_back() {
    if (empty()) {
        throw std::out_of_range("deque::pop_back on empty deque");
    }

    if (end_offset_idx_ == 0) {
        --end_chunk_idx_;
        end_offset_idx_ = chunk_size_;
    }

    --end_offset_idx_;

    V* pos = chunks_[end_chunk_idx_] + end_offset_idx_;
    traits::destroy(alloc_, pos);

    --size_;

    if (size_ == 0) {
        start_chunk_idx_ = end_chunk_idx_;
        start_offset_idx_ = end_offset_idx_;
        return;
    }

    if (end_offset_idx_ == 0) {
        --end_chunk_idx_;
        end_offset_idx_ = chunk_size_;
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::pop_front() {
    if (empty()) {
        throw std::out_of_range("deque::pop_front on empty deque");
    }

    V* pos = chunks_[start_chunk_idx_] + start_offset_idx_;
    traits::destroy(alloc_, pos);

    ++start_offset_idx_;
    --size_;

    if (size_ > 0 && start_offset_idx_ >= chunk_size_) {
        ++start_chunk_idx_;
        start_offset_idx_ = 0;
    }

    if (size_ == 0) {
        end_chunk_idx_ = start_chunk_idx_;
        end_offset_idx_ = start_offset_idx_;
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::insert(iterator pos, const V& val) {
    const size_t idx = pos.index();

    if (idx > size_) {
        throw std::out_of_range("out of range");
    }

    if (idx == 0) {
        push_front(val);
        return;
    }

    if (idx == size_) {
        push_back(val);
        return;
    }

    make_space_at(idx);
    (*this)[idx] = val;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::insert(iterator pos, V&& val) {
    const size_t idx = pos.index();

    if (idx > size_) {
        throw std::out_of_range("out of range");
    }

    if (idx == 0) {
        push_front(std::move(val));
        return;
    }

    if (idx == size_) {
        push_back(std::move(val));
        return;
    }

    make_space_at(idx);
    (*this)[idx] = std::move(val);
}

template<typename V, typename Alloc>
void deque<V, Alloc>::erase(iterator pos) {
    const size_t idx = pos.index();

    if (idx >= size_) {
        throw std::out_of_range("deque::erase out of range");
    }

    for (size_t i = idx; i + 1 < size_; ++i) {
        (*this)[i] = std::move((*this)[i + 1]);
    }

    pop_back();
}

template<typename V, typename Alloc>
void deque<V, Alloc>::swap(deque<V, Alloc>& other_deque) {
    using std::swap;

    swap(chunks_, other_deque.chunks_);

    swap(chunk_size_, other_deque.chunk_size_);
    swap(chunk_counter_, other_deque.chunk_counter_);
    swap(max_chunk_amount_, other_deque.max_chunk_amount_);
    swap(size_, other_deque.size_);
    swap(capacity_, other_deque.capacity_);

    swap(start_chunk_idx_, other_deque.start_chunk_idx_);
    swap(start_offset_idx_, other_deque.start_offset_idx_);
    swap(end_chunk_idx_, other_deque.end_chunk_idx_);
    swap(end_offset_idx_, other_deque.end_offset_idx_);

    swap(alloc_, other_deque.alloc_);
    swap(chunk_alloc_, other_deque.chunk_alloc_);
}

template<typename V, typename Alloc>
void deque<V, Alloc>::clear() {
    while (!empty()) {
        pop_back();
    }

    const size_t mid = max_chunk_amount_ / 2;

    start_chunk_idx_ = mid;
    start_offset_idx_ = chunk_size_ / 2;

    end_chunk_idx_ = mid;
    end_offset_idx_ = start_offset_idx_;
}

template<typename V, typename Alloc>
template<typename... Args>
V& deque<V, Alloc>::emplace(iterator pos, Args&&... args) {
    const size_t idx = pos.index();

    if (idx > size_) {
        throw std::out_of_range("out of range");
    }

    if (idx == 0) {
        emplace_front(std::forward<Args>(args)...);
        return (*this)[idx];
    }

    if (idx == size_) {
        emplace_back(std::forward<Args>(args)...);
        return (*this)[idx];
    }

    make_space_at(idx);
    (*this)[idx] = V(std::forward<Args>(args)...);

    return (*this)[idx];
}

template<typename V, typename Alloc>
template<typename... Args>
V& deque<V, Alloc>::emplace_back(Args&&... args) {
    ensure_space_push_back();

    if (empty()) {
        start_chunk_idx_ = end_chunk_idx_;
        start_offset_idx_ = end_offset_idx_;
    }

    V* pos = chunks_[end_chunk_idx_] + end_offset_idx_;
    traits::construct(alloc_, pos, std::forward<Args>(args)...);

    ++end_offset_idx_;
    ++size_;

    return *pos;
}

template<typename V, typename Alloc>
template<typename... Args>
V& deque<V, Alloc>::emplace_front(Args&&... args) {
    ensure_space_push_front();

    --start_offset_idx_;

    V* pos = chunks_[start_chunk_idx_] + start_offset_idx_;
    traits::construct(alloc_, pos, std::forward<Args>(args)...);

    ++size_;

    return *pos;
}

template<typename V, typename Alloc>
V& deque<V, Alloc>::front() {
    return chunks_[start_chunk_idx_][start_offset_idx_];
}

template<typename V, typename Alloc>
const V& deque<V, Alloc>::front() const {
    return chunks_[start_chunk_idx_][start_offset_idx_];
}

template<typename V, typename Alloc>
V& deque<V, Alloc>::back() {
    if (empty()) {
        throw std::out_of_range("deque::back on empty deque");
    }

    if (end_offset_idx_ == 0) {
        return chunks_[end_chunk_idx_ - 1][chunk_size_ - 1];
    }

    return chunks_[end_chunk_idx_][end_offset_idx_ - 1];
}

template<typename V, typename Alloc>
const V& deque<V, Alloc>::back() const {
    if (empty()) {
        throw std::out_of_range("deque::back on empty deque");
    }

    if (end_offset_idx_ == 0) {
        return chunks_[end_chunk_idx_ - 1][chunk_size_ - 1];
    }

    return chunks_[end_chunk_idx_][end_offset_idx_ - 1];
}

template<typename V, typename Alloc>
V& deque<V, Alloc>::operator[](const size_t idx) {
    size_t chunk_idx = 0;
    size_t offset_idx = 0;
    calc_idx(idx, chunk_idx, offset_idx);

    return chunks_[chunk_idx][offset_idx];
}

template<typename V, typename Alloc>
const V& deque<V, Alloc>::operator[](const size_t idx) const {
    size_t chunk_idx = 0;
    size_t offset_idx = 0;
    calc_idx(idx, chunk_idx, offset_idx);

    return chunks_[chunk_idx][offset_idx];
}

template<typename V, typename Alloc>
V& deque<V, Alloc>::at(const size_t idx) {
    if (idx >= size_) {
        throw std::out_of_range("deque::at out of range");
    }

    return (*this)[idx];
}

template<typename V, typename Alloc>
const V& deque<V, Alloc>::at(const size_t idx) const {
    if (idx >= size_) {
        throw std::out_of_range("deque::at out of range");
    }

    return (*this)[idx];
}

template<typename V, typename Alloc>
size_t deque<V, Alloc>::size() const {
    return size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::resize(const size_t new_size) {
    if (new_size == size_) {
        return;
    } else if (new_size < size_) {
        while (new_size < size_) {
            pop_back();
        }
        return;
    } else if (new_size > size_) {
        while (new_size > size_) {
            push_back(V());
        }
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::resize(const size_t new_size, const V& val) {
    if (new_size == size_) {
        return;
    } else if (new_size < size_) {
        while (new_size < size_) {
            pop_back();
        }
        return;
    } else if (new_size > size_) {
        while (new_size > size_) {
            push_back(val);
        }
    }
}

template<typename V, typename Alloc>
bool deque<V, Alloc>::empty() const {
    return (size_ == 0);
}

template<typename V, typename Alloc>
size_t deque<V, Alloc>::capacity() const {
    return capacity_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::shrink_to_fit() {
    if (size_ == 0) {
        for (size_t i = 0; i < max_chunk_amount_; ++i) {
            if (chunks_[i] != nullptr) {
                traits::deallocate(alloc_, chunks_[i], chunk_size_);
                chunks_[i] = nullptr;
            }
        }

        chunk_traits::deallocate(chunk_alloc_, chunks_, max_chunk_amount_);

        max_chunk_amount_ = init_chunks_capacity;
        chunks_ = chunk_traits::allocate(chunk_alloc_, max_chunk_amount_);
        std::uninitialized_value_construct_n(chunks_, max_chunk_amount_);

        const size_t mid = max_chunk_amount_ / 2;
        chunks_[mid] = traits::allocate(alloc_, chunk_size_);

        chunk_counter_ = 1;
        capacity_ = chunk_size_;

        start_chunk_idx_ = mid;
        start_offset_idx_ = chunk_size_ / 2;

        end_chunk_idx_ = mid;
        end_offset_idx_ = start_offset_idx_;
        return;
    }

    const size_t old_start_chunk_idx = start_chunk_idx_;
    const size_t old_start_offset_idx = start_offset_idx_;

    size_t old_last_chunk_idx = 0;
    size_t old_last_offset_idx = 0;
    calc_idx(size_ - 1, old_last_chunk_idx, old_last_offset_idx);

    const size_t used_chunk_count = old_last_chunk_idx - old_start_chunk_idx + 1;

    size_t new_max_chunk_amount = init_chunks_capacity;
    while (new_max_chunk_amount < used_chunk_count + 2) {
        new_max_chunk_amount *= 2;
    }

    chunk_pointer* new_chunks = chunk_traits::allocate(chunk_alloc_, new_max_chunk_amount);
    std::uninitialized_value_construct_n(new_chunks, new_max_chunk_amount);

    const size_t new_start_chunk_idx = (new_max_chunk_amount / 2) - (used_chunk_count / 2);

    for (size_t i = 0; i < used_chunk_count; ++i) {
        new_chunks[new_start_chunk_idx + i] = chunks_[old_start_chunk_idx + i];
    }

    for (size_t i = 0; i < max_chunk_amount_; ++i) {
        if (i < old_start_chunk_idx || i > old_last_chunk_idx) {
            if (chunks_[i] != nullptr) {
                traits::deallocate(alloc_, chunks_[i], chunk_size_);
                chunks_[i] = nullptr;
            }
        }
    }

    chunk_traits::deallocate(chunk_alloc_, chunks_, max_chunk_amount_);

    chunks_ = new_chunks;
    max_chunk_amount_ = new_max_chunk_amount;
    chunk_counter_ = used_chunk_count;
    capacity_ = used_chunk_count * chunk_size_;

    start_chunk_idx_ = new_start_chunk_idx;
    start_offset_idx_ = old_start_offset_idx;

    size_t new_last_chunk_idx = 0;
    size_t new_last_offset_idx = 0;
    {
        const size_t absolute_offset = start_offset_idx_ + (size_ - 1);
        new_last_chunk_idx = start_chunk_idx_ + (absolute_offset / chunk_size_);
        new_last_offset_idx = absolute_offset % chunk_size_;
    }

    end_chunk_idx_ = new_last_chunk_idx;
    end_offset_idx_ = new_last_offset_idx + 1;

    end_chunk_idx_ = new_last_chunk_idx;
    end_offset_idx_ = new_last_offset_idx + 1;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::rebuild_chunks() {
    ventra::vector<V*> pointer_store;
    pointer_store.reserve(chunk_counter_);

    size_t first_allocated_idx = max_chunk_amount_;

    for (size_t i = 0; i < max_chunk_amount_; ++i) {
        if (chunks_[i]) {
            if (first_allocated_idx == max_chunk_amount_) {
                first_allocated_idx = i;
            }

            pointer_store.push_back(chunks_[i]);
        }
    }

    chunk_traits::deallocate(chunk_alloc_, chunks_, max_chunk_amount_);

    max_chunk_amount_ *= 2;

    chunks_ = chunk_traits::allocate(chunk_alloc_, max_chunk_amount_);
    std::uninitialized_value_construct_n(chunks_, max_chunk_amount_);

    const size_t start_idx = (max_chunk_amount_ / 2) - (pointer_store.size() / 2);
    const size_t start_chunk_offset = start_chunk_idx_ - first_allocated_idx;
    const size_t end_chunk_offset = end_chunk_idx_ - first_allocated_idx;

    for (size_t i = 0; i < pointer_store.size(); ++i) {
        chunks_[start_idx + i] = pointer_store[i];
    }

    start_chunk_idx_ = start_idx + start_chunk_offset;
    end_chunk_idx_ = start_idx + end_chunk_offset;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::ensure_space_push_back() {
    // Needs next chunk
    if (end_offset_idx_ >= chunk_size_) {
        // No chunk space left
        if (end_chunk_idx_ + 1 >= max_chunk_amount_) {
            rebuild_chunks();
        }

        ++end_chunk_idx_;

        // chunk not constructed
        if (!chunks_[end_chunk_idx_]) {
            chunks_[end_chunk_idx_] = traits::allocate(alloc_, chunk_size_);
            capacity_ += chunk_size_;
            ++chunk_counter_;
        }

        end_offset_idx_ = 0;
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::ensure_space_push_front() {
    // Needs next chunk
    if (start_offset_idx_ == 0) {
        // No chunk space left
        if (start_chunk_idx_ == 0) {
            rebuild_chunks();
        }

        --start_chunk_idx_;

        // chunk not constructed
        if (!chunks_[start_chunk_idx_]) {
            chunks_[start_chunk_idx_] = traits::allocate(alloc_, chunk_size_);
            capacity_ += chunk_size_;
            ++chunk_counter_;
        }

        start_offset_idx_ = chunk_size_;
    }
}

template<typename V, typename Alloc>
void deque<V, Alloc>::calc_idx(const size_t idx, size_t& chunk_idx, size_t& offset_idx) const {
    size_t absolute_offset = start_offset_idx_ + idx;
    chunk_idx = start_chunk_idx_ + (absolute_offset / chunk_size_);
    offset_idx = absolute_offset % chunk_size_;
}

template<typename V, typename Alloc>
bool deque<V, Alloc>::is_valid_idx(const size_t idx) const {
    return idx < size_;
}

template<typename V, typename Alloc>
void deque<V, Alloc>::make_space_at(const size_t idx) {
    push_back(back());

    for (size_t i = size_ - 1; i > idx; --i) {
        (*this)[i] = std::move((*this)[i - 1]);
    }
}
