#pragma once
#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace worldgen_rules {
// Value-only prefill decisions. Readers never retain native starts, pieces or
// the shared planning Field, and never wait for native placement's stage lock.
class StructureReservationCache {
public:
    using Key = std::array<int,3>; // family, anchor x, anchor z
    using Areas = std::vector<std::array<int,4>>;
    struct Entry { std::optional<std::array<int,4>> bounds; Areas areas; };
private:
    std::mutex mutex;
    std::map<Key,Entry> entries;
public:
    void clear() { std::lock_guard lock(mutex); entries.clear(); }
    void put(Key key, Entry value) {
        std::lock_guard lock(mutex);
        if(entries.size() >= 4096 && !entries.contains(key)) entries.erase(entries.begin());
        entries.insert_or_assign(key,std::move(value));
    }
    bool allAbsent(std::vector<Key> const& keys) {
        std::lock_guard lock(mutex);
        for(auto const& key:keys) {
            auto it=entries.find(key);
            if(it==entries.end() || it->second.bounds) return false;
        }
        return true;
    }
    bool get(std::vector<Key> const& keys,int x,int z,std::vector<Areas>& groups) {
        std::lock_guard lock(mutex);
        for(auto const& key:keys) if(!entries.contains(key)) return false;
        groups.clear();
        for(auto const& key:keys) {
            auto const& e=entries.at(key);
            if(!e.bounds || e.areas.empty()) continue;
            auto b=*e.bounds;
            if(b[2]+28<x*16 || b[0]-28>x*16+15 || b[3]+28<z*16 || b[1]-28>z*16+15) continue;
            groups.push_back(e.areas);
        }
        return true;
    }
};
}
