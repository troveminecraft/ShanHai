#pragma once
#include "StructurePlan.h"
namespace worldgen_rules {
// Run after templates and loot markers, on the fresh owner's fragment only.
// Water lives in layer 0 for air, layer 1 for blocks the engine can waterlog.
template<class WaterLevel,class Environment>
unsigned submergeStructure(StructureBox bounds,StructureBox owner,WaterLevel level,Environment&env){
 unsigned writes=0;
 for(int z=std::max(bounds.min.z,owner.min.z);z<=std::min(bounds.max.z,owner.max.z);++z)
  for(int x=std::max(bounds.min.x,owner.min.x);x<=std::min(bounds.max.x,owner.max.x);++x){
   int top=std::min({bounds.max.y,owner.max.y,level(x,z)});
   for(int y=std::max(bounds.min.y,owner.min.y);y<=top;++y){
    StructurePoint p{x,y,z};auto block=env.get(p);
    if(env.air(block)){env.water(p,0);++writes;}
    else if(env.canContainWater(block)){env.water(p,1);++writes;}
   }
  }
 return writes;
}
}
