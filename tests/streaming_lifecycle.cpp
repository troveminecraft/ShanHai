#undef NDEBUG
#include "StreamingBudget.h"
#include "StreamingPriority.h"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <limits>

// A dependency-graph regression, not an engine performance benchmark. All
// policies see the same jobs, costs, worker limit and neighbour predicates.
// Terrain (four calculators) -> structures -> decoration -> light -> ready.
struct Chunk { int x,z,stage=0; bool busy=false; double end=0; };
struct Result { double near=0,view=0; int finished=0; };
Result simulate(bool revised,int radius,bool jungle){
    constexpr std::array<int,5> states{0,2,4,6,13};
    std::vector<Chunk> chunks;
    int side=2*radius+1;
    for(int z=-radius;z<=radius;++z)for(int x=-radius;x<=radius;++x)chunks.push_back({x,z});
    terrain_runtime::ReadyAges ages;
    std::unordered_map<uint64_t,terrain_runtime::ReadyPriority> priority;
    struct Old {int stage; double since,last;};std::map<uint64_t,Old> old;
    std::vector<terrain::StreamFocus> focus{{.03125,.03125,.03125,.03125,0}};
    auto at=[&](int x,int z)->Chunk*{return std::abs(x)>radius||std::abs(z)>radius?nullptr:&chunks[(z+radius)*side+x+radius];};
    auto eligible=[&](Chunk const& c){
        if(c.busy||c.stage==4)return false;
        if(!c.stage)return true;
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto n=at(c.x+dx,c.z+dz);if(n&&n->stage<c.stage)return false;}
        return true;
    };
    Result result;double now=0,nextSample=0;std::array<double,4> calculatorFree{};
    while(result.finished<int(chunks.size())){
        int workers=0;bool near=true,view=true;result.finished=0;
        for(auto& c:chunks){
            if(c.busy&&c.end<=now+1e-9){c.busy=false;++c.stage;}
            if(c.busy)++workers;
            result.finished+=c.stage==4;
            if(c.stage!=4&&std::max(std::abs(c.x),std::abs(c.z))<=12)view=false;
            if(c.stage!=4&&std::max(std::abs(c.x),std::abs(c.z))<=1)near=false;
        }
        if(near&&!result.near)result.near=now;
        if(view&&!result.view)result.view=now;
        if(now>=nextSample){
            nextSample=now+.2;std::vector<terrain_runtime::ReadyChunk> ready;
            for(auto const& c:chunks)if(eligible(c))ready.push_back({c.x,c.z,states[c.stage]});
            if(revised)priority=ages.update(ready,now);
            else{
                priority.clear();
                for(auto c:ready){auto key=terrain_runtime::streamingKey(c.x,c.z);auto [it,added]=old.try_emplace(key,Old{c.state,now,now});
                    auto& e=it->second;if(!added&&e.stage!=c.state)e={c.state,now,now};e.last=now;
                    priority[key]={int(std::clamp(now-e.since,0.,30.)*128.),c.state};}
                std::erase_if(old,[&](auto const& e){return now-e.second.last>1.5;});
            }
        }
        while(workers<8){
            Chunk* best=nullptr;int bestScore=std::numeric_limits<int>::max();
            for(auto& c:chunks)if(eligible(c)){
                auto it=priority.find(terrain_runtime::streamingKey(c.x,c.z));auto p=it==priority.end()?terrain_runtime::ReadyPriority{}:it->second;
                bool critical=revised?terrain::inDependencyHalo(c.x,c.z,.03125,.03125,8):terrain::chunkDistance(c.x,c.z,.03125,.03125)<=8;
                int distance=terrain::streamingScore(c.x,c.z,focus);
                int score=revised?terrain_runtime::scheduledStreamingScore(distance,p.ageCredit,critical,p.state):
                    (critical?-8192:0)+std::min(distance,2048)-p.ageCredit;
                if(score<bestScore){best=&c;bestScore=score;}
            }
            if(!best)break;
            ++workers;best->busy=true;
            if(best->stage==0){
                // Native load jobs can occupy all worker slots while waiting
                // for the four calculators, as observed in the flight log.
                auto lane=std::min_element(calculatorFree.begin(),calculatorFree.end());
                best->end=*lane=std::max(now,*lane)+(jungle?.10:.035);
            }else best->end=now+(best->stage==1?.002:best->stage==2?.004:.006);
        }
        if(result.finished==int(chunks.size()))break;
        double next=nextSample;for(auto const& c:chunks)if(c.busy)next=std::min(next,c.end);
        assert(next>now&&next<1000);now=next;
    }
    return result;
}
int main(){
    for(bool jungle:{false,true})for(int radius:{24,48}){
        auto before=simulate(false,radius,jungle),after=simulate(true,radius,jungle);
        std::cout<<"synthetic "<<(jungle?"jungle":"plain")<<" radius="<<radius
                 <<" near "<<before.near<<" -> "<<after.near<<" view12 "<<before.view<<" -> "<<after.view
                 <<" completed="<<after.finished<<std::endl;
        assert(before.finished==after.finished);
        assert(after.near<=before.near+.2);
        assert(after.view<before.view);
    }
}
