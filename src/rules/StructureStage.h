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
#include <span>
#include "../RuntimeMetrics.h"

namespace worldgen_rules {
// Clang-cl x64 emits these two virtual-call thunks for the supported SDKs.
// Decode the compiler's displacement; never bake a guessed vtable slot number.
inline std::optional<unsigned> generatorVirtualSlot(std::span<uint8_t const> code) {
    constexpr std::array<uint8_t,9> debugPrefix{0x50,0x48,0x89,0x0c,0x24,0x48,0x8b,0x0c,0x24};
    bool spilled=code.size()>=debugPrefix.size() && std::equal(debugPrefix.begin(),debugPrefix.end(),code.begin());
    if(spilled)code=code.subspan(debugPrefix.size());
    if(code.size()<10 || code[0]!=0x48 || code[1]!=0x8b || code[2]!=0x01 || code[3]!=0x48 || code[4]!=0x8b) return {};
    uint32_t displacement=0;size_t end=0;
    if(code[5]==0x40 && code[6]<0x80) {displacement=code[6];end=7;}
    else if(code[5]==0x80 && code.size()>=13) {
        for(unsigned i=0;i<4;++i)displacement|=uint32_t(code[6+i])<<(8*i);
        end=10;
    } else return {};
    if(spilled) {
        if(code.size()<end+5 || code[end]!=0x41 || code[end+1]!=0x5a)return {};
        end+=2;
    }
    if(code[end]!=0x48 || code[end+1]!=0xff || code[end+2]!=0xe0 ||
       displacement%sizeof(void*) || displacement>=128*sizeof(void*)) return {};
    return unsigned(displacement/sizeof(void*));
}
// Multiple generator classes may share a folded native stub. Install a detour
// once per address, and route only this mod's primary dimension through it.
inline bool secondaryGeneratorHook(uintptr_t primary, uintptr_t secondary) {
    if (!primary || !secondary)
        throw std::runtime_error("Missing native generator hook target");
    return primary != secondary;
}
template<size_t N, class Write>
void replaceGeneratorSlots(std::array<void**,N> const &slots, std::array<void*,N> const &expected,
                           std::array<void*,N> const &replacement, Write write) {
    for(size_t i=0;i<N;++i) {
        if(!slots[i] || !expected[i] || !replacement[i] || *slots[i]!=expected[i])
            throw std::runtime_error("Native generator vtable does not match SDK targets");
        for(size_t j=0;j<i;++j) if(slots[i]==slots[j])
            throw std::runtime_error("Duplicate native generator vtable slot");
    }
    size_t applied=0;
    try {
        for(;applied<N;++applied) write(slots[applied],replacement[applied]);
    } catch(...) {
        while(applied) {--applied;write(slots[applied],expected[applied]);}
        throw;
    }
}
template<size_t N, class Write>
void restoreGeneratorSlots(std::array<void**,N> const &slots,std::array<void*,N> const &expected,
                           std::array<void*,N> const &replacement,Write write) {
    for(size_t i=N;i>0;--i)
        if(slots[i-1] && *slots[i-1]==replacement[i-1]) write(slots[i-1],expected[i-1]);
}
template <class Origin, class Stage>
bool afterStructureDecoration(bool target, bool failed, Origin origin, Stage stage) {
    if (!target)
        return origin();
    if (failed || !origin())
        return false;
    return stage();
}
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
    static Chunk *ownerIf(View const &view) {
        return current && current->view == &view ? current->owner : nullptr;
    }
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
struct StructureStageSite {int phase,x,z;};
inline thread_local std::optional<StructureStageSite> structureStageSite;
struct StructureStageSiteScope {
    std::optional<StructureStageSite> previous=structureStageSite;
    StructureStageSiteScope(int phase,int x,int z){structureStageSite=StructureStageSite{phase,x,z};}
    ~StructureStageSiteScope(){structureStageSite=previous;}
};
class StructureStageMutex {
    using Clock = std::chrono::steady_clock;
    std::mutex mutex;
    Clock::time_point acquired;
    uint64_t currentWaitUs=0;
    std::mutex slowMutex;
    std::deque<std::array<int64_t,5>> slowSites;

  public:
    std::atomic_uint64_t acquisitions{0}, waitNanos{0}, holdNanos{0};
    terrain_runtime::Latency waitLatency,holdLatency;
    void lock() {
        auto began = Clock::now();
        mutex.lock();
        acquired = Clock::now();
        currentWaitUs=std::chrono::duration_cast<std::chrono::microseconds>(acquired-began).count();
        waitNanos += std::chrono::duration_cast<std::chrono::nanoseconds>(acquired - began).count();
        waitLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(acquired-began).count());
        ++acquisitions;
    }
    bool try_lock() {
        if (!mutex.try_lock())
            return false;
        acquired = Clock::now();
        currentWaitUs=0;
        ++acquisitions;
        return true;
    }
    void unlock() {
        auto holdUs=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-acquired).count();
        if((holdUs>=100000||currentWaitUs>=100000)&&structureStageSite){
            auto c=*structureStageSite;std::lock_guard lock(slowMutex);
            if(slowSites.size()==32)slowSites.pop_front();
            slowSites.push_back({c.phase,c.x,c.z,int64_t(currentWaitUs),holdUs});
        }
        holdLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-acquired).count());
        holdNanos += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - acquired).count();
        mutex.unlock();
    }
    auto slowSnapshot(){std::lock_guard lock(slowMutex);return slowSites;}
    void clearSlowSites(){std::lock_guard lock(slowMutex);slowSites.clear();}
};
} // namespace worldgen_rules
