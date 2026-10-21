#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace worldgen_rules {
// Native decoration already supplies its owner. Keep it scoped to the exact
// synchronous task/view; random chunk selection is not an ownership API.
template <class View, class Chunk> class DecorationTaskScope {
    inline static thread_local DecorationTaskScope const *current = nullptr;
    DecorationTaskScope const *previous;
    View const *view;
    Chunk *owner;

  public:
    DecorationTaskScope(View const &view, Chunk &owner)
        : previous(current), view(&view), owner(&owner) { current = this; }
    DecorationTaskScope(DecorationTaskScope const &) = delete;
    DecorationTaskScope &operator=(DecorationTaskScope const &) = delete;
    ~DecorationTaskScope() { current = previous; }
    static Chunk &ownerFor(View const &view) {
        if (!current || current->view != &view)
            throw std::runtime_error("Native decoration task context missing or view mismatched");
        return *current->owner;
    }
};

// A decoration view owns a complete 3x3 neighbourhood. Query the native view,
// rather than reading an undocumented GridArea layout. The probe can be any
// member of that neighbourhood; missing/extra members fail before placement.
template <class Exists> std::pair<int, int> decorationCenter(int probeX, int probeZ, Exists exists) {
    int loX = probeX, hiX = probeX, loZ = probeZ, hiZ = probeZ, count = 0;
    for (int z = probeZ - 2; z <= probeZ + 2; ++z) {
        for (int x = probeX - 2; x <= probeX + 2; ++x) {
            if (!exists(x, z))
                continue;
            ++count;
            loX = std::min(loX, x);
            hiX = std::max(hiX, x);
            loZ = std::min(loZ, z);
            hiZ = std::max(hiZ, z);
        }
    }
    if (count != 9 || hiX - loX != 2 || hiZ - loZ != 2)
        throw std::runtime_error("Incomplete or unexpected native decoration neighbourhood");
    return {loX + 1, loZ + 1};
}

template <class View, class Chunk, class Position, class Lookup>
Chunk &decorationTaskOwner(View const &view, Position position, Lookup lookup) {
    auto &owner = DecorationTaskScope<View, Chunk>::ownerFor(view);
    auto p = position(owner);
    auto center = decorationCenter(p.first, p.second,
                                   [&](int x, int z) { return lookup(x, z) != nullptr; });
    if (center != p || lookup(p.first, p.second) != &owner)
        throw std::runtime_error("Native decoration task owner disagrees with its neighbourhood");
    return owner;
}

// An absent hint only causes recomputation, never skipped terrain/structures.
// std::set alone leaked hints for generated chunks that never reached decoration.
template <class Key, size_t Capacity> class BoundedHints {
    static_assert(Capacity > 0);
    std::set<Key> keys;
    std::deque<Key> order;

  public:
    void insert(Key key) {
        if (!keys.insert(key).second)
            return;
        order.push_back(std::move(key));
        while (order.size() > Capacity) {
            keys.erase(order.front());
            order.pop_front();
        }
    }
    size_t erase(Key const &key) {
        auto removed = keys.erase(key);
        if (removed)
            std::erase(order, key);
        return removed;
    }
    bool contains(Key const &key) const {
        return keys.contains(key);
    }
    size_t size() const {
        return keys.size();
    }
    void clear() {
        keys.clear();
        order.clear();
    }
};

// Native structure caches are mutable. Keep their lock explicit, and measure
// wait separately from ownership so time waiting is not called "generation".
class StructureStageMutex {
    using Clock = std::chrono::steady_clock;
    std::mutex mutex;
    Clock::time_point acquired;

  public:
    std::atomic_uint64_t acquisitions{0}, waitNanos{0}, holdNanos{0};
    void lock() {
        auto began = Clock::now();
        mutex.lock();
        acquired = Clock::now();
        waitNanos += std::chrono::duration_cast<std::chrono::nanoseconds>(acquired - began).count();
        ++acquisitions;
    }
    bool try_lock() {
        if (!mutex.try_lock())
            return false;
        acquired = Clock::now();
        ++acquisitions;
        return true;
    }
    void unlock() {
        holdNanos += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - acquired).count();
        mutex.unlock();
    }
};
} // namespace worldgen_rules
