#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <thread>

namespace terrain_runtime {
// Host shutdown must finish admitted work before releasing registries/caches.
// This does not cancel native Bedrock jobs or turn unfinished work into success.
class WorldTasks {
    mutable std::mutex mutex;
    std::condition_variable drained;
    bool accepting = false;
    uint64_t epoch = 0, running = 0;
    void release() {
        std::lock_guard lock(mutex);
        if (--running == 0) drained.notify_all();
    }
public:
    struct Snapshot { uint64_t epoch, running; bool accepting; };
    class Lease {
        WorldTasks* owner = nullptr;
        Lease* previous = nullptr;
        Lease* next = nullptr;
        std::thread::id thread;
        friend class WorldTasks;
        explicit Lease(WorldTasks& value) : owner(&value), next(first), thread(std::this_thread::get_id()) {
            if (next) next->previous=this;
            first=this;
        }
    public:
        Lease(Lease const&) = delete;
        Lease& operator=(Lease const&) = delete;
        Lease(Lease&& other) noexcept : owner(std::exchange(other.owner,nullptr)), previous(other.previous), next(other.next), thread(other.thread) {
            if (!owner) return;
            if (thread!=std::this_thread::get_id()) std::terminate();
            if (previous) previous->next=this; else first=this;
            if (next) next->previous=this;
        }
        ~Lease() {
            if (!owner) return;
            if (thread!=std::this_thread::get_id()) std::terminate();
            if (previous) previous->next=next; else first=next;
            if (next) next->previous=previous;
            owner->release();
        }
    };
private:
    // A trivial TLS head has no destructor. The nodes live in admitted leases,
    // so repeated close during DLL/static destruction cannot read a destroyed
    // thread-local container. Moves and non-LIFO optional resets relink nodes.
    inline static thread_local Lease* first = nullptr;
    bool enteredHere() const {
        for (auto* lease=first;lease;lease=lease->next)
            if (lease->owner==this) return true;
        return false;
    }
public:
    void open() {
        std::lock_guard lock(mutex);
        if (accepting || running) throw std::logic_error("World tasks still active");
        ++epoch;
        accepting = true;
    }
    Lease enter() {
        std::lock_guard lock(mutex);
        if (!accepting && !enteredHere()) throw std::runtime_error("World session is closing; generation cannot be committed");
        ++running;
        return Lease(*this);
    }
    std::optional<Lease> tryEnter() {
        std::lock_guard lock(mutex);
        if (!accepting && !enteredHere()) return std::nullopt;
        ++running;
        return Lease(*this);
    }
    void closeAndWait() {
        if (enteredHere()) throw std::logic_error("Cannot close a world from its active task");
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
