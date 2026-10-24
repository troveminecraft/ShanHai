#pragma once
#include <mutex>
#include <set>
#include <vector>

namespace worldgen_rules {
// Native GC may run on a worker without the adapter's stage lock. Owned
// features retire starts only in the stage-locked collector, which also
// retires every adapter reference. Foreign features retain native behaviour.
template<class Handle> class StructureCacheOwnership {
    mutable std::mutex mutex;
    std::set<Handle> owned;
    inline static thread_local Handle collecting{};
public:
    void insert(Handle h) { std::lock_guard lock(mutex); owned.insert(h); }
    bool contains(Handle h) const { std::lock_guard lock(mutex); return owned.contains(h); }
    void clear() { std::lock_guard lock(mutex); owned.clear(); }
    std::vector<Handle> snapshot() const { std::lock_guard lock(mutex); return {owned.begin(),owned.end()}; }
    bool permitNativeCollection(Handle h) const { return collecting==h || !contains(h); }
    template<class Collect> void collect(Handle h, Collect invoke) {
        struct Scope { Handle previous; ~Scope(){collecting=previous;} } scope{collecting};
        collecting=h;
        invoke();
    }
};
}
