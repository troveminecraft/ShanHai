#undef NDEBUG
#include "rules/StructureReservationCache.h"
#include <cassert>
#include <future>
#include <iostream>
using Cache=worldgen_rules::StructureReservationCache;
int main(){
    Cache cache;std::vector<Cache::Key> keys{{-2,-3,5},{1,4,5}};
    std::vector<Cache::Areas> groups;
    assert(!cache.get(keys,-3,5,groups));assert(!cache.allAbsent(keys));
    cache.put(keys[0],{});cache.put(keys[1],{});
    assert(cache.allAbsent(keys));assert(cache.get(keys,0,0,groups)&&groups.empty());
    Cache::Entry structure{std::array{-48,80,-17,111},{{32720,32848,32751,32879}}};
    cache.put(keys[0],structure);
    assert(!cache.allAbsent(keys));
    for(int z=-3;z<13;++z)for(int x=-10;x<7;++x){
        assert(cache.get(keys,x,z,groups));
        bool intersects=!(-17+28<x*16||-48-28>x*16+15||111+28<z*16||80-28>z*16+15);
        assert(groups.size()==size_t(intersects));if(intersects)assert(groups[0]==structure.areas);
    }
    // Hold the unrelated native registry lock throughout concurrent cached
    // reads. A cache hit has no dependency on a slow native placement.
    std::mutex registry;std::unique_lock held(registry);
    auto read=[&]{for(int n=0;n<10000;++n){std::vector<Cache::Areas> g;
        assert(cache.get(keys,-3,5,g)&&g.size()==1&&g[0]==structure.areas);}};
    auto a=std::async(std::launch::async,read),b=std::async(std::launch::async,read);
    a.get();b.get();held.unlock();
    // Bounded eviction/clear cannot turn unknown work into negative decisions.
    for(int i=0;i<5000;++i)cache.put({10,i,0},{});
    assert(!cache.get(keys,0,0,groups));assert(!cache.allAbsent(keys));
    cache.clear();assert(!cache.allAbsent({{10,4999,0}}));
    std::cout<<"PASS footprint parity, unknown/negative/positive plans, concurrent cached prefill independent of registry lock, eviction and world clear\n";
}
