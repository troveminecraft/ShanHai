#pragma once
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
namespace terrain_runtime {
inline constexpr int RepairCoreRadius=2,RepairDependencyRadius=10,RepairSlots=1;
inline int recoveryViewRadius(int requested){return std::clamp(requested,2,64);}
struct ViewGapRecovery {
 struct Gap {int x,z;double since,attempted=-1;unsigned seen=0,attempts=0;int state=-1;};
 std::unordered_map<uint64_t,Gap> gaps;unsigned epoch=0;
 static uint64_t key(int x,int z){return uint64_t(uint32_t(x))<<32|uint32_t(z);}
 void begin(){++epoch;}
 // Missing/unstarted frontier chunks are normal streaming, not holes. An
 // absent chunk is eligible only when ready chunks enclose it on both axes.
 void observe(int x,int z,int state,bool enclosed,double now){
  auto k=key(x,z);if(state==13||(state<=0&&!enclosed)){gaps.erase(k);return;}
  auto [it,inserted]=gaps.try_emplace(k,Gap{x,z,now});auto&g=it->second;
  if(inserted||g.state!=state){g.since=now;g.state=state;}
  g.seen=epoch;
 }
 void end(){for(auto i=gaps.begin();i!=gaps.end();)if(i->second.seen!=epoch)i=gaps.erase(i);else ++i;}
 Gap* next(double now,int cx,int cz,std::vector<std::pair<int,int>> const& active={}){
  Gap* best=nullptr;int distance=0;
  for(auto&[_,g]:gaps){
   if(std::any_of(active.begin(),active.end(),[&](auto p){return std::max(std::abs(g.x-p.first),std::abs(g.z-p.second))<=2*RepairCoreRadius;}))continue;
   int d=(g.x-cx)*(g.x-cx)+(g.z-cz)*(g.z-cz);
   // A failed request used to permanently exclude this hole until the player
   // left the view. Retry at most twice, with backoff beyond the patch lifetime.
   if(now-g.since<8.||g.attempts>=3||
      (g.attempted>=0&&now-g.attempted<30.*g.attempts))continue;
   if(!best||d<distance||(d==distance&&(g.x<best->x||(g.x==best->x&&g.z<best->z)))){best=&g;distance=d;}
  }
  return best;
 }
 void requested(Gap& g,double now){g.attempted=now;++g.attempts;}
};
}
