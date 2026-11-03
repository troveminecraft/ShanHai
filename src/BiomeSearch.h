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
#include <unordered_set>
#include <unordered_map>
#include <mutex>

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

// Visit the immediate neighbourhood first, then survey large biomes on a
// coarse lattice. Only refine around a verified hit (or when a coarse pass
// misses). No chunks are generated and every reported point is an exact
// surface sample. As before, locating is not a nearest-block guarantee.
template<class Sample,class Valid,class Stop>
BiomeSearchResult searchBiomesAdaptive(int x,int z,std::vector<int> const& ids,
                                      Sample sample,Valid valid,Stop stop,BiomeSearchLimits limits={}){
    BiomeSearchResult result;
    if(limits.radius<0||limits.step<=0||limits.samples<=0)throw std::invalid_argument("Invalid biome search limits");
    if(ids.empty())return result;
    std::unordered_set<uint64_t> visited;
    auto at=[&](int dx,int dz){
        if(stop()||result.samples>=limits.samples){result.interrupted=true;return false;}
        if(std::abs(dx)>limits.radius||std::abs(dz)>limits.radius)return true;
        uint64_t key=(uint64_t(uint32_t(dx))<<32)|uint32_t(dz);
        if(!visited.insert(key).second||!valid(x+dx,z+dz))return true;
        ++result.samples;result.radius=std::max({result.radius,std::abs(dx),std::abs(dz)});
        auto s=sample(x+dx,z+dz);
        if(std::find(ids.begin(),ids.end(),s.biome)!=ids.end()){
            auto distance=[&](int X,int Z){return int64_t(X-x)*(X-x)+int64_t(Z-z)*(Z-z);};
            if(!result.found||distance(x+dx,z+dz)<distance((*result.found)[0],(*result.found)[2]))
                result.found=std::array{x+dx,std::max(s.height,s.water)+1,z+dz,s.biome};
        }
        return true;
    };
    auto ring=[&](int r,int step){
        for(int d=-r;d<=r;d+=step){
            if(!at(d,-r)||!at(d,r))return false;
            if(d!=-r&&d!=r&&(!at(-r,d)||!at(r,d)))return false;
        }return true;
    };
    if(!at(0,0)||result.found)return result;
    int coarse=limits.step*4;
    for(int r=limits.step;r<=std::min(coarse,limits.radius);r+=limits.step){
        if(!ring(r,limits.step)||result.found)return result;
    }
    for(int step:{coarse,limits.step*2,limits.step}){
        for(int r=step;r<=limits.radius;r+=step){
            if(!ring(r,step))return result;
            if(result.found){
                int cx=(*result.found)[0]-x,cz=(*result.found)[2]-z;
                for(int dz=-step;dz<=step;dz+=limits.step)
                for(int dx=-step;dx<=step;dx+=limits.step)
                    if(!at(cx+dx,cz+dz))return result;
                return result;
            }
        }
    }
    return result;
}

// Owned by the current world's command service, separate from its generation
// context. A single command worker retains its bounded Field caches between
// searches; teardown joins that worker before releasing this state.
struct BiomeSearchState {
    std::mutex mutex;
    std::unique_ptr<terrain_kernel::Field> field,hints;
    std::unordered_map<int,std::array<int,4>> lastHits;
};
inline bool biomeSearchCanUseHints(std::vector<int> const& ids,bool catalogQuery){
    for(int id:ids){
        auto entry=catalogQuery?&biome_catalog::entries().at(id):nullptr;
        int biome=entry?entry->biome:id;
        // Catalog snow entries retain their parent's numeric biome, so test
        // the snow flag as well as legacy expanded palette indices.
        if(biome==10||biome==18||biome==19||biome==47||biome==48)return false;
    }
    return true;
}

class BiomeSearchJob {
    BiomeSearchResult result;
    std::atomic_bool ready=false;
    // Last member: joins before any worker-owned result fields are destroyed.
    std::jthread worker;
public:
    BiomeSearchJob(terrain_kernel::J recipe,int x,int z,std::vector<int> ids,bool infinite,
                   std::function<void()> configureThread={},bool catalogQuery=false,
                   std::shared_ptr<BiomeSearchState> state={})
      :worker([this,recipe=std::move(recipe),x,z,ids=std::move(ids),infinite,configureThread=std::move(configureThread),catalogQuery,state=std::move(state)](std::stop_token stop) mutable {
        try{
            if(configureThread)configureThread();
            // Private caches: a user search must not hold the generation pool,
            // plan-store locks or any game-owned object while traversing terrain.
            if(!state)state=std::make_shared<BiomeSearchState>();
            std::unique_lock stateLock(state->mutex);
            if(stop.stop_requested()){result.interrupted=true;ready.store(true,std::memory_order_release);return;}
            if(!state->field)state->field=std::make_unique<terrain_kernel::Field>(recipe,std::make_shared<terrain_kernel::Context>());
            if(!state->hints)state->hints=std::make_unique<terrain_kernel::Field>(recipe,std::make_shared<terrain_kernel::Context>(),true);
            auto& field=*state->field;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(45);
            int samples=0;
            auto classify=[&](terrain_kernel::Surface s){
                if(catalogQuery){
                    int found=-1;
                    for(int id:ids)if(id>=0&&size_t(id)<biome_catalog::entries().size()
                        &&biome_catalog::matches(biome_catalog::entries()[id],s,recipe.at("config").value("elevationRevision",0)>=1)){
                        found=id;break;
                    }
                    s.biome=found;
                }
                return s;
            };
            auto valid=[infinite](int X,int Z){
                int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
                return infinite?terrain_bounds::gameContainsChunk(cx,cz):terrain_bounds::containsChunk(cx,cz);
            };
            auto stopped=[&]{return stop.stop_requested()||std::chrono::steady_clock::now()>=deadline;};
            auto anchorGrid=[&](int id){return field.biomeSearchGrid(catalogQuery?biome_catalog::entries().at(id).biome:id);};
            bool anchorsOnly=recipe.contains("regionalGeneration")&&std::all_of(ids.begin(),ids.end(),[&](int id){return anchorGrid(id)>0;});
            int searchRadius=anchorsOnly?16384:8192;
            auto exact=[&](int X,int Z){return classify(field.surface(X+32768,Z+32768,false));};
            auto hit=[&](int X,int Z){
                if(stopped()||!valid(X,Z)||std::max(std::abs(X-x),std::abs(Z-z))>searchRadius)return false;
                ++samples;auto s=exact(X,Z);
                if(std::find(ids.begin(),ids.end(),s.biome)==ids.end())return false;
                result.found=std::array{X,std::max(s.height,s.water)+1,Z,s.biome};
                result.radius=std::max(std::abs(X-x),std::abs(Z-z));return true;
            };
            // Preserve an immediate exact answer at the player and reuse a
            // verified prior result. Never return an approximate habitat hint.
            bool found=hit(x,z);
            if(!found)for(int id:ids){auto i=state->lastHits.find((catalogQuery?1000:0)+id);
                if(i!=state->lastHits.end()&&hit(i->second[0],i->second[2])){found=true;break;}}
            // Small rare habitats have authoritative region anchors. Sampling
            // an 8,192-square grid first was both slower and liable to miss them.
            if(!found)for(int id:ids){
                int biome=catalogQuery?biome_catalog::entries().at(id).biome:id;
                int grid=field.biomeSearchGrid(biome);if(!grid)continue;
                int offset=biome==60?32768:0;
                int gx=int(std::floor((x+offset)/double(grid))),gz=int(std::floor((z+offset)/double(grid)));
                int reach=int(std::ceil(double(searchRadius)/grid))+1;
                for(int ring=0;ring<=reach&&!found&&!stopped();++ring)
                for(int dz=-ring;dz<=ring&&!found&&!stopped();++dz)
                for(int dx=-ring;dx<=ring&&!found&&!stopped();++dx){
                    if(std::max(std::abs(dx),std::abs(dz))!=ring)continue;
                    auto points=state->hints->biomeSearchAnchors(biome,gx+dx,gz+dz);
                    std::sort(points.begin(),points.end(),[&](auto a,auto b){
                        auto d=[&](auto p){return int64_t(p[0]-x)*(p[0]-x)+int64_t(p[1]-z)*(p[1]-z);};return d(a)<d(b);});
                    for(auto p:points){if(stopped())break;
                        auto hint=classify(state->hints->surface(p[0]+32768,p[1]+32768,false));
                        if(std::find(ids.begin(),ids.end(),hint.biome)!=ids.end()&&hit(p[0],p[1])){found=true;break;}}
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                if(found)break;
            }
            bool hintable=biomeSearchCanUseHints(ids,catalogQuery);
            auto run=[&](bool hints){
                auto hintDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                return searchBiomesAdaptive(x,z,ids,[&](int X,int Z){
                    if(++samples%16==0)std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    // A cheap natural surface only orders the search. Regional
                    // waterways can override it; never report this hint as a hit.
                    if(hints){auto hint=classify(state->hints->surface(X+32768,Z+32768,false));
                        if(std::find(ids.begin(),ids.end(),hint.biome)==ids.end()){hint.biome=-999;return hint;}}
                    return exact(X,Z);
                },valid,[&]{return stopped()||(hints&&std::chrono::steady_clock::now()>=hintDeadline);},
                BiomeSearchLimits{8192,64,hints?12000:70000});
            };
            // Region-owned habitats cannot appear between rejected sites.
            // Do not spend another 45 seconds traversing a dense grid after
            // their authoritative candidates were exhausted.
            if(!found&&!anchorsOnly){result=run(hintable);
                if(hintable&&!result.found&&!stopped())result=run(false);
            }
            if(!found&&anchorsOnly)result.radius=searchRadius;
            result.samples=samples;
            if(result.found)state->lastHits[(catalogQuery?1000:0)+(*result.found)[3]]=*result.found;
            else if(stopped())result.interrupted=true;
        }catch(std::exception const&e){result.error=e.what();}
        ready.store(true,std::memory_order_release);
    }){}
    bool done()const{return ready.load(std::memory_order_acquire);}
    BiomeSearchResult const& get()const{return result;} // only after done()
    void cancel(){worker.request_stop();}
};
}
