#pragma once
#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
namespace terrain_runtime {
// Value-only observations of the existing builder hook. Bounded, no file I/O,
// no access to game objects on the diagnostic thread and no renderer retries.
class MeshSectionTrace {
    struct Column {int x=0,z=0;uint64_t stamp=0,seen=0,finished=0,built=0,renderable=0,calls=0,maxUs=0;};
    std::array<Column,512> columns{};std::mutex mutex;uint64_t sequence=0;
    std::atomic_uint64_t skipped=0;
    static int chunk(int p){return p>=0?p/16:(p-15)/16;}
public:
    void record(int x,int y,int z,bool complete,unsigned flags,uint64_t us=0)noexcept{
        int sy=chunk(y)+4;if(sy<0||sy>=36)return;
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock){++skipped;return;}
        int cx=chunk(x),cz=chunk(z);auto& c=columns[(uint32_t(cx)*73856093u^uint32_t(cz)*19349663u)%columns.size()];
        if(!c.stamp||c.x!=cx||c.z!=cz)c=Column{cx,cz};
        uint64_t bit=uint64_t(1)<<sy;c.stamp=++sequence;c.seen|=bit;
        if(!complete){c.finished&=~bit;++c.calls;return;}
        c.finished|=bit;c.maxUs=std::max(c.maxUs,us);
        c.built=(c.built&~bit)|((flags&2)?bit:0);c.renderable=(c.renderable&~bit)|((flags&1)?bit:0);
    }
    nlohmann::json snapshot(){
        std::vector<Column> copy;{std::lock_guard lock(mutex);for(auto c:columns)if(c.stamp)copy.push_back(c);}
        std::sort(copy.begin(),copy.end(),[](auto a,auto b){return a.stamp>b.stamp;});
        nlohmann::json rows=nlohmann::json::array();
        for(size_t i=0;i<std::min<size_t>(64,copy.size());++i){auto c=copy[i];rows.push_back({
            {"chunk",{c.x,c.z}},{"seenMask",c.seen},{"builderReturnedMask",c.finished},
            {"nativeBuiltMask",c.built},{"nativeCanRenderMask",c.renderable},{"calls",c.calls},{"maxMs",c.maxUs/1000.}});}
        return {{"scope","recent builder sections; bit 0 is Y -64..-49; native flags sampled before GPU publication"},
            {"capacity",512},{"skipped",skipped.load()},{"columns",std::move(rows)}};
    }
};
}
