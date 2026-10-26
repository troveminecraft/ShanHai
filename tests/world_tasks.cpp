#include "WorldTasks.h"
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
// DLL/static owners are destroyed after thread-local destructors on exit.
// A second, already-drained close must remain safe in that order.
struct ExitOwner {
    terrain_runtime::WorldTasks tasks;
    ~ExitOwner() { tasks.closeAndWait(); }
} exitOwner;
int main() {
    exitOwner.tasks.open();
    { auto work=exitOwner.tasks.enter(); }
    exitOwner.tasks.closeAndWait();
    terrain_runtime::WorldTasks tasks;
    tasks.open();
    std::promise<void> entered, release;
    auto gate=release.get_future();
    auto work=std::async(std::launch::async,[&] {
        auto lease=tasks.enter();
        entered.set_value(); gate.wait();
        auto nested=tasks.enter(); // Already admitted work may finish its callbacks.
    });
    entered.get_future().wait();
    auto closing=std::async(std::launch::async,[&]{ tasks.closeAndWait(); });
    assert(closing.wait_for(20ms)==std::future_status::timeout);
    while(tasks.snapshot().accepting) std::this_thread::yield();
    bool rejected=false;
    try { auto late=tasks.enter(); } catch(std::runtime_error const&) { rejected=true; }
    assert(rejected && tasks.snapshot().running==1);
    release.set_value(); work.get(); closing.get();
    assert(!tasks.snapshot().accepting && tasks.snapshot().running==0);
    auto first=tasks.snapshot().epoch;
    tasks.open();
    assert(tasks.snapshot().epoch==first+1);
    {
        std::optional<terrain_runtime::WorldTasks::Lease> outer(tasks.enter());
        auto inner=tasks.tryEnter();
        auto moved=std::move(outer);
        outer.reset();
        moved.reset(); // Removing an older node must not invalidate the inner.
        assert(tasks.snapshot().running==1);
        bool selfClose=false;
        try { tasks.closeAndWait(); } catch(std::logic_error const&) { selfClose=true; }
        assert(selfClose);
    }
    try { auto lease=tasks.enter(); throw std::runtime_error("injected"); } catch(...) {}
    tasks.closeAndWait();
    assert(tasks.snapshot().running==0);
    std::cout<<"world admission, shutdown drain, lease moves, exception release and static-exit cleanup passed\n";
}
