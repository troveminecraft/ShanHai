#pragma once
#include "StreamingBudget.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <deque>

namespace terrain_runtime {
// Watch only a small stalled sample. No filesystem access in worker hooks and
// no retained Level/Chunk/View pointers across calls or worlds.
class ChunkPipelineTrace {
public:
    struct Event {int kind,x,z,stage,otherX=0,otherZ=0,observed=-1;bool result=false;uint64_t micros=0;};
    static constexpr size_t Limit=256;
private:
    std::atomic<std::shared_ptr<std::unordered_set<uint64_t> const>> watched{
        std::make_shared<std::unordered_set<uint64_t> const>()};
    std::mutex mutex;
    std::deque<Event> events;
public:
    void watch(std::vector<uint64_t> const& keys){
        auto next=std::make_shared<std::unordered_set<uint64_t>>();
        for(auto key:keys){if(next->size()==16)break;next->insert(key);}
        watched.store(next);
    }
    bool contains(int x,int z)const{return watched.load()->contains(streamingKey(x,z));}
    void record(Event event){
        if(!contains(event.x,event.z))return;
        std::lock_guard lock(mutex);if(events.size()==Limit)events.pop_front();events.push_back(event);
    }
    std::vector<Event> snapshot(){std::lock_guard lock(mutex);return {events.begin(),events.end()};}
};
}
