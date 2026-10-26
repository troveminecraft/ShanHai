#include "WorldTasks.h"
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int main() {
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
    try { auto lease=tasks.enter(); throw std::runtime_error("injected"); } catch(...) {}
    tasks.closeAndWait();
    assert(tasks.snapshot().running==0);
    std::cout<<"world admission, shutdown drain, exception release and reopening passed\n";
}
