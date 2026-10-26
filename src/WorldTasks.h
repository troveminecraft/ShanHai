#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <unordered_map>

namespace terrain_runtime {
// Host shutdown must finish admitted work before releasing registries/caches.
// This does not cancel native Bedrock jobs or turn unfinished work into success.
class WorldTasks {
    mutable std::mutex mutex;
    std::condition_variable drained;
    bool accepting = false;
    uint64_t epoch = 0, running = 0;
    // Nested native callbacks on an admitted thread must finish even after
    // close begins (DB load -> terrain fill -> related callbacks).
    inline static thread_local std::unordered_map<WorldTasks*,uint64_t> depth;
    void release() {
        std::lock_guard lock(mutex);
        auto it=depth.find(this);
        if (it==depth.end()) std::terminate(); // Leases are thread-confined.
        if (--it->second==0) depth.erase(it);
        if (--running == 0) drained.notify_all();
    }
public:
    struct Snapshot { uint64_t epoch, running; bool accepting; };
    class Lease {
        WorldTasks* owner = nullptr;
        friend class WorldTasks;
        explicit Lease(WorldTasks& value) : owner(&value) {}
    public:
        Lease(Lease const&) = delete;
        Lease& operator=(Lease const&) = delete;
        Lease(Lease&& other) noexcept : owner(std::exchange(other.owner, nullptr)) {}
        ~Lease() { if (owner) owner->release(); }
    };
    void open() {
        std::lock_guard lock(mutex);
        if (accepting || running) throw std::logic_error("World tasks still active");
        ++epoch;
        accepting = true;
    }
    Lease enter() {
        std::lock_guard lock(mutex);
        if (!accepting && !depth.contains(this)) throw std::runtime_error("World session is closing; generation cannot be committed");
        ++depth[this];
        ++running;
        return Lease(*this);
    }
    std::optional<Lease> tryEnter() {
        std::lock_guard lock(mutex);
        if (!accepting && !depth.contains(this)) return std::nullopt;
        ++depth[this];
        ++running;
        return Lease(*this);
    }
    void closeAndWait() {
        if (depth.contains(this)) throw std::logic_error("Cannot close a world from its active task");
        std::unique_lock lock(mutex);
        accepting = false;
        drained.wait(lock, [this] { return running == 0; });
    }
    Snapshot snapshot() const {
        std::lock_guard lock(mutex);
        return {epoch, running, accepting};
    }
};
}
