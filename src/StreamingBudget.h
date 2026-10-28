#pragma once
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace terrain_runtime {
inline uint64_t streamingKey(int x,int z){return (uint64_t(uint32_t(x))<<32)|uint32_t(z);}
struct ReadyChunk {int x,z,state;};
// Only age jobs seen in the engine's dependency-ready queues. A blocked job
// cannot manufacture eligibility; its expected state remains engine-owned.
class ReadyAges {
    struct Entry {int state;double since,last;};
    std::unordered_map<uint64_t,Entry> entries;
public:
    void clear(){entries.clear();}
    std::unordered_map<uint64_t,int> update(std::vector<ReadyChunk> const& ready,double now){
        std::unordered_map<uint64_t,int> result;
        for(auto c:ready){
            auto key=streamingKey(c.x,c.z);
            auto [it,added]=entries.try_emplace(key,Entry{c.state,now,now});
            auto& e=it->second;
            if(!added&&(e.state!=c.state||now-e.last>1.5))e={c.state,now,now};
            e.last=now;
            // All visible ready tasks eventually outrank newly arriving local
            // jobs. Age resets at each stage and when leaving the queue.
            result[key]=int(std::clamp(now-e.since,0.,30.)*128.);
        }
        std::erase_if(entries,[&](auto const& e){return now-e.second.last>1.5;});
        if(entries.size()>32768)entries.clear();
        return result;
    }
};
inline int fairStreamingScore(int distanceScore,int ageCredit){return std::min(distanceScore,2048)-ageCredit;}
struct PipelinePressure {
    unsigned queued=0,postprocess=0,unfinished=0;
    bool queueSampleValid=false;
};
// Hysteresis prevents repeatedly destroying/recreating an ahead view as one
// worker finishes. Budgets apply to extra requests only, never native progress.
struct SupplementalBudget {
    bool open=false;
    bool update(PipelinePressure p,unsigned waiting,bool busy){
        if(!p.queueSampleValid||busy||waiting>=4||p.queued>=256||p.postprocess>=128||p.unfinished>=512)open=false;
        else if(waiting<=1&&p.queued<=64&&p.postprocess<=32&&p.unfinished<=128)open=true;
        return open;
    }
};
struct ActiveStreamingClock {
    double seconds=0,previous=-1,excluded=0;
    void tick(double wall){
        if(previous>=0){double delta=std::max(0.,wall-previous),used=std::min(delta,.25);seconds+=used;excluded+=delta-used;}
        previous=wall;
    }
};
}
