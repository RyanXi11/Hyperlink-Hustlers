#include "graph/TitleMap.h"

namespace {

constexpr std::size_t kMinCapacity = 16;

std::size_t capacityFor(std::size_t keys) {
    std::size_t capacity = kMinCapacity;
    while (capacity < keys * 2) {
        capacity *= 2;
    }
    return capacity;
}

}  // namespace

void TitleMap::reserve(std::size_t expectedKeys) {
    std::size_t capacity = capacityFor(expectedKeys);
    if (capacity > slots_.size()) {
        rehash(capacity);
    }
    keys_.reserve(expectedKeys);
    values_.reserve(expectedKeys);
}

bool TitleMap::insert(const std::string& key, int value) {
    if ((keys_.size() + 1) * 2 > slots_.size()) {
        rehash(slots_.empty() ? kMinCapacity : slots_.size() * 2);
    }
    std::uint32_t hash = hashOf(key);
    std::size_t index = probe(key, hash);
    if (slots_[index].entry != -1) {
        return false;
    }
    slots_[index].hash = hash;
    slots_[index].entry = static_cast<std::int32_t>(keys_.size());
    keys_.push_back(key);
    values_.push_back(value);
    return true;
}

int TitleMap::find(const std::string& key) const {
    if (slots_.empty()) {
        return -1;
    }
    std::int32_t entry = slots_[probe(key, hashOf(key))].entry;
    return entry == -1 ? -1 : values_[entry];
}

void TitleMap::clear() {
    slots_ = {};
    keys_ = {};
    values_ = {};
}

std::size_t TitleMap::memoryBytes() const {
    std::size_t bytes = slots_.capacity() * sizeof(Slot) + keys_.capacity() * sizeof(std::string) +
                        values_.capacity() * sizeof(int);
    for (const std::string& key : keys_) {
        if (key.capacity() > std::string().capacity()) {
            bytes += key.capacity() + 1;
        }
    }
    return bytes;
}

std::uint32_t TitleMap::hashOf(const std::string& key) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

void TitleMap::rehash(std::size_t newCapacity) {
    std::vector<Slot> fresh(newCapacity);
    std::size_t mask = newCapacity - 1;
    for (const Slot& slot : slots_) {
        if (slot.entry == -1) {
            continue;
        }
        std::size_t index = slot.hash & mask;
        while (fresh[index].entry != -1) {
            index = (index + 1) & mask;
        }
        fresh[index] = slot;
    }
    slots_.swap(fresh);
}

// Returns the slot holding key, or the empty slot where it would be inserted.
std::size_t TitleMap::probe(const std::string& key, std::uint32_t hash) const {
    std::size_t mask = slots_.size() - 1;
    std::size_t index = hash & mask;
    while (slots_[index].entry != -1) {
        const Slot& slot = slots_[index];
        if (slot.hash == hash && keys_[slot.entry] == key) {
            return index;
        }
        index = (index + 1) & mask;
    }
    return index;
}
