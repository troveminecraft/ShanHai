#pragma once
#include "StructureRandom.h"
#include <cstdlib>
#include <vector>
#include <utility>
#include <algorithm>
#include <tuple>

namespace worldgen_rules {
// Original LLBDS order (family, cell Z, cell X), including each family's
// complete influence radius. Modern trial chambers reach beyond vanilla's
// fixed eight-chunk preparation window.
template<class Include, class Visit>
void forNearbyExtraCandidate(ChunkPosition owner,uint32_t seed,Include include,Visit visit){
 for(auto const&s:ExtraSpecs){
  if(!include(s.type))continue;
  for(int z=spacingCell(owner.z-s.radius,s.spacing);z<=spacingCell(owner.z+s.radius,s.spacing);++z)
   for(int x=spacingCell(owner.x-s.radius,s.spacing);x<=spacingCell(owner.x+s.radius,s.spacing);++x){
    auto c=candidateForCell(x,z,seed,s.spacing,s.separation,s.salt);
    if(std::abs(int64_t(c.x)-owner.x)<=s.radius&&std::abs(int64_t(c.z)-owner.z)<=s.radius)visit(s,c);
   }
 }
}
inline std::vector<std::pair<int,ChunkPosition>> nearbyExtraCandidates(ChunkPosition owner,uint32_t seed){
 std::vector<std::pair<int,ChunkPosition>> result;
 forNearbyExtraCandidate(owner,seed,[](int){return true;},[&](auto const&s,auto c){result.emplace_back(s.type,c);});
 return result;
}
struct ExtraSearchCandidate {int64_t distance;ChunkPosition anchor;};
inline std::vector<ExtraSearchCandidate> orderedExtraCandidates(ExtraSpec const&s,int originX,int originZ,uint32_t seed,int minX,int maxX,int minZ,int maxZ){
 std::vector<ExtraSearchCandidate> result;
 for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x){auto c=candidateForCell(x,z,seed,s.spacing,s.separation,s.salt);int64_t dx=int64_t(c.x)*16-originX,dz=int64_t(c.z)*16-originZ;result.push_back({dx*dx+dz*dz,c});}
 std::sort(result.begin(),result.end(),[](auto const&a,auto const&b){return std::tie(a.distance,a.anchor.z,a.anchor.x)<std::tie(b.distance,b.anchor.z,b.anchor.x);});
 return result;
}
}
