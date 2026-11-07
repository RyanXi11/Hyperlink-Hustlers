#pragma once

#include <cstddef>
#include <utility>
#include <vector>

template <typename Key, typename Value>
class MinHeap {
public:
    struct Entry {
        Key key;
        Value value;
    };

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }

    void push(const Key& key, const Value& value) {
        data_.push_back({key, value});
        siftUp(data_.size() - 1);
    }

    // undefined if the heap is empty
    const Entry& top() const { return data_[0]; }

    // removes and returns entry w/ smallest key
    Entry pop() {
        Entry smallest = data_[0];
        data_[0] = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return smallest;
    }

private:
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!(data_[i].key < data_[parent].key)) break;
            std::swap(data_[i], data_[parent]);
            i = parent;
        }
    }

    void siftDown(std::size_t i) {
        const std::size_t n = data_.size();
        while (true) {
            std::size_t left = 2 * i + 1;
            std::size_t right = left + 1;
            std::size_t smallest = i;
            if (left < n && data_[left].key < data_[smallest].key) smallest = left;
            if (right < n && data_[right].key < data_[smallest].key) smallest = right;
            if (smallest == i) break;
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }

    std::vector<Entry> data_;
};