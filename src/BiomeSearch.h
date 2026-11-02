#pragma once
#include "TerrainKernel.h"
#include "BiomeCatalog.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>

namespace terrain_runtime {
struct BiomeSearchResult {
    std::optional<std::array<int,4>> found; // world X, surface Y, world Z, palette biome
    int samples=0,radius=0;
    bool interrupted=false;
    std::string error;
};
struct BiomeSearchLimits {int radius=8192,step=64,samples=70000;};

// Bounded nearest-ring sampling. A result is always checked with the exact
// frozen recipe; this is not an exhaustive nearest-block proof. Injecting the
// sampler keeps cancellation, bounds and negative coordinates testable.
template<class Sample,class Valid,class Stop>
BiomeSearchResult searchBiomes(int x,int z,std::vector<int> const& ids,
                              Sample sample,Valid valid,Stop stop,BiomeSearchLimits limits={}){
    BiomeSearchResult result;
    if(limits.radius<0||limits.step<=0||limits.samples<=0)throw std::invalid_argument("Invalid biome search limits");
    if(ids.empty())return result;
    auto at=[&](int dx,int dz){
        if(stop()||result.samples>=limits.samples){result.interrupted=true;return false;}
        int X=x+dx,Z=z+dz;
        if(!valid(X,Z))return true;
        ++result.samples;
        auto s=sample(X,Z);
        if(std::find(ids.begin(),ids.end(),s.biome)!=ids.end())
            result.found=std::array{X,std::max(s.height,s.water)+1,Z,s.biome};
        return !result.found;
    };
    if(!at(0,0))return result;
    for(int r=limits.step;r<=limits.radius;r+=limits.step){
        result.radius=r;
        for(int d=-r;d<=r;d+=limits.step){
            if(!at(d,-r)||!at(d,r))return result;
            if(d!=-r&&d!=r&&(!at(-r,d)||!at(r,d)))return result;
        }
    }
    return result;
}

class BiomeSearchJob {
    BiomeSearchResult result;
    std::atomic_bool ready=false;
    // Last member: joins before any worker-owned result fields are destroyed.
    std::jthread worker;
public:
    BiomeSearchJob(terrain_kernel::J recipe,int x,int z,std::vector<int> ids,bool infinite,
                   std::function<void()> configureThread={},bool catalogQuery=false)
      :worker([this,recipe=std::move(recipe),x,z,ids=std::move(ids),infinite,configureThread=std::move(configureThread),catalogQuery](std::stop_token stop){
        try{
            if(configureThread)configureThread();
            // Private caches: a user search must not hold the generation pool,
            // plan-store locks or any game-owned object while traversing terrain.
            terrain_kernel::Field field(recipe,std::make_shared<terrain_kernel::Context>());
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(45);
            int samples=0;
            result=searchBiomes(x,z,ids,[&](int X,int Z){
                if(++samples%16==0)std::this_thread::sleep_for(std::chrono::milliseconds(2));
                auto s=field.surface(X+32768,Z+32768,false);
                if(catalogQuery){
                    int found=-1;
                    for(int id:ids)if(id>=0&&size_t(id)<biome_catalog::entries().size()
                        &&biome_catalog::matches(biome_catalog::entries()[id],s,recipe.at("config").value("elevationRevision",0)>=1)){
                        found=id;break;
                    }
                    s.biome=found;
                }
                return s;
            },[infinite](int X,int Z){
                int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
                return infinite?terrain_bounds::gameContainsChunk(cx,cz):terrain_bounds::containsChunk(cx,cz);
            },[&]{return stop.stop_requested()||std::chrono::steady_clock::now()>=deadline;});
        }catch(std::exception const&e){result.error=e.what();}
        ready.store(true,std::memory_order_release);
    }){}
    bool done()const{return ready.load(std::memory_order_acquire);}
    BiomeSearchResult const& get()const{return result;} // only after done()
    void cancel(){worker.request_stop();}
};
}
