#undef NDEBUG
#include "native/RegionalPlans.h"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace nativegen;
using namespace std::chrono_literals;
int main(){
 // A deliberately blocked planner must not block an unrelated cache hit/miss.
 std::promise<void> entered,release;auto gate=release.get_future().share();
 std::atomic_int calls=0;
 auto first=std::async(std::launch::async,[&]{return regional::get("test","water",0,0,[&]{++calls;entered.set_value();gate.wait();return J{{"ok",1}};});});
 entered.get_future().wait();
 auto second=std::async(std::launch::async,[&]{return regional::get("test","water",0,0,[&]{++calls;return J{};});});
 auto independent=std::async(std::launch::async,[&]{return regional::get("test","water",1,0,[&]{auto nested=regional::get("test","desert",0,0,[]{return J(7);});return *nested;});});
 bool parallel=independent.wait_for(2s)==std::future_status::ready;
 release.set_value();auto a=first.get(),b=second.get();assert(parallel&&calls==1&&a==b);assert(*independent.get()==7);
 // Failed production wakes all consumers and leaves a key retryable.
 std::promise<void> failEntered,failRelease;auto failGate=failRelease.get_future().share();
 auto failure=std::async(std::launch::async,[&]{try{regional::get("test","failure",0,0,[&]()->J{failEntered.set_value();failGate.wait();throw std::runtime_error("injected");});return false;}catch(...){return true;}});
 failEntered.get_future().wait();
 auto follower=std::async(std::launch::async,[&]{try{regional::get("test","failure",0,0,[]()->J{throw std::runtime_error("injected");});return false;}catch(...){return true;}});
 failRelease.set_value();assert(failure.get()&&follower.get());
 assert(*regional::get("test","failure",0,0,[]{return J(9);})==9);
 for(int i=0;i<500;++i)regional::get("evict","ice",i,0,[&]{return J(i);});
 assert(regional::plans.size()<=384&&regional::pendingPlans.empty());assert(*a==J({{"ok",1}}));
 std::cout<<"single-flight, independent/nested planners, exception retry and bounded residency passed\n";
}
