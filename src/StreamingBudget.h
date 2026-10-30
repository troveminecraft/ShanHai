#pragma once
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace terrain_runtime {
inline uint64_t streamingKey(int x,int z){return (uint64_t(uint32_t(x))<<32)|uint32_t(z);}
struct ReadyChunk {int x,z,state;};
struct ReadyPriority {int ageCredit=0,state=0;};
// Only age jobs seen in the engine's dependency-ready queues. A blocked job
// cannot manufacture eligibility; its expected state remains engine-owned.
class ReadyAges {
    struct Entry {int state;double since,last;};
    std::unordered_map<uint64_t,Entry> entries;
public:
    void clear(){entries.clear();}
    std::unordered_map<uint64_t,ReadyPriority> update(std::vector<ReadyChunk> const& ready,double now){
        std::unordered_map<uint64_t,ReadyPriority> result;
        // The two native queues may briefly mention the same coordinate.
        // Keep its furthest stage regardless of snapshot iteration order.
        for(auto c:ready){auto& p=result[streamingKey(c.x,c.z)];p.state=std::max(p.state,c.state);}
        for(auto& [key,priority]:result){
            auto [it,added]=entries.try_emplace(key,Entry{priority.state,now,now});
            auto& e=it->second;
            // Progress must not turn an old chunk into a fresh job. Keep its
            // age across stages and short dependency waits. A backwards state
            // or a long absence identifies a new request for these coordinates.
            if(!added&&(priority.state<e.state||now-e.last>30.))e={priority.state,now,now};
            e.state=priority.state;e.last=now;
            // Bound fairness to six distance steps. An old view-edge load
            // cannot outrank a newly ready nearby structure/decorating job.
            priority.ageCredit=int(std::clamp(now-e.since,0.,6.)*16.);
        }
        std::erase_if(entries,[&](auto const& e){return now-e.second.last>30.;});
        // Discard inactive history first; never erase the current queue's age.
        if(entries.size()>32768)std::erase_if(entries,[&](auto const& e){return e.second.last<now;});
        return result;
    }
};
inline int completionCredit(int state){
    // Canonical states describe the NEXT eligible transition's input. Engine
    // dependency predicates remain authoritative; priority cannot bypass them.
    return state>=10?256:state>=6?224:state>=4?192:state>=2?128:0;
}
inline int fairStreamingScore(int distanceScore,int ageCredit){return std::min(distanceScore,2048)-std::clamp(ageCredit,0,96);}
// An old view-distance job must not overtake the complete near dependency halo.
// Fairness still applies between noncritical jobs; the halo moves with players.
inline int scheduledStreamingScore(int distanceScore,int ageCredit,bool critical,int state=0){
    return (critical?-8192:0)+fairStreamingScore(distanceScore,ageCredit)-completionCredit(state);
}
struct IdleQueuePump {
    double idleSince=-1,next=0;
    bool poll(double now,bool valid,unsigned queued,unsigned inFlight){
        if(!valid||!queued||inFlight){idleSince=-1;return false;}
        if(idleSince<0)idleSince=now;
        if(now-idleSince<.5||now<next)return false;
        next=now+1.;return true;
    }
};
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
