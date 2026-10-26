#pragma once
#include <unordered_map>
#include <cstdint>
namespace terrain_runtime {
struct ViewGapRecovery {
 struct Gap {int x,z;double since,attempted=-1;unsigned seen=0,attempts=0;};
 std::unordered_map<uint64_t,Gap> gaps;unsigned epoch=0;
 static uint64_t key(int x,int z){return uint64_t(uint32_t(x))<<32|uint32_t(z);}
 void begin(){++epoch;}
 void observe(int x,int z,bool ready,double now){
  auto k=key(x,z);if(ready){gaps.erase(k);return;}
  auto [it,_]=gaps.try_emplace(k,Gap{x,z,now});it->second.seen=epoch;
 }
 void end(){for(auto i=gaps.begin();i!=gaps.end();)if(i->second.seen!=epoch)i=gaps.erase(i);else ++i;}
 Gap* next(double now,int cx,int cz){
  Gap* best=nullptr;int distance=0;
  for(auto&[_,g]:gaps){
   int d=(g.x-cx)*(g.x-cx)+(g.z-cz)*(g.z-cz);
   // A failed request used to permanently exclude this hole until the player
   // left the view. Retry at most twice, with backoff beyond the patch lifetime.
   if(now-g.since<(d<=16?2.:5.)||g.attempts>=3||
      (g.attempted>=0&&now-g.attempted<30.*g.attempts))continue;
   if(!best||d<distance||(d==distance&&(g.x<best->x||(g.x==best->x&&g.z<best->z)))){best=&g;distance=d;}
  }
  return best;
 }
 void requested(Gap& g,double now){g.attempted=now;++g.attempts;}
};
}
