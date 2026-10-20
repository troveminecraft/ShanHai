#pragma once
#include "StructureRandom.h"
#include <algorithm>
#include <string_view>
namespace worldgen_rules {
inline bool villageBuilding(std::string_view name){return name.find("/houses/")!=name.npos;}
struct VillageGrowthBudget {int scale,buildings,pieces,depth;};
inline VillageGrowthBudget villageGrowthBudget(ChunkPosition anchor,uint32_t seed,int buildings,int pieces,int nativeDepth=6){
 StructureRandom random(placementSeed(anchor.x,anchor.z,VillageSalt+97,seed));
 int scale=3*(1+int(random.nextInt(3)));
 return {scale,std::max(1,buildings)*scale,std::clamp(pieces*scale*2,128,4096),std::min(24,nativeDepth+scale*2)};
}
}
