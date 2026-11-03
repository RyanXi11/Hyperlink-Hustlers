#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Open-addressing hash map from article title to article id.
// Linear probing, FNV-1a hashing, power-of-two capacity, load factor <= 0.5.
class TitleMap {
public:
    TitleMap() = default;

    void reserve(std::size_t expectedKeys);

    // Returns false and leaves the existing value in place if the key is already present.
    bool insert(const std::string& key, int value);

    // Returns -1 if the key is not present.
    int find(const std::string& key) const;

    void clear();
    std::size_t size() const { return keys_.size(); }
    std::size_t capacity() const { return slots_.size(); }
    std::size_t memoryBytes() const;

private:
    struct Slot {
        std::uint32_t hash = 0;
        std::int32_t entry = -1;  // index into keys_/values_, -1 when empty
    };

    static std::uint32_t hashOf(const std::string& key);
    void rehash(std::size_t newCapacity);
    std::size_t probe(const std::string& key, std::uint32_t hash) const;

    std::vector<Slot> slots_;
    std::vector<std::string> keys_;
    std::vector<int> values_;
};
