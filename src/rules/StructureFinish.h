#pragma once
#include "StructurePlan.h"
namespace worldgen_rules {
inline bool structureGroundPlant(std::string_view name){return name=="minecraft:poppy"||name=="minecraft:dandelion"||name=="minecraft:blue_orchid"||name=="minecraft:allium"||name=="minecraft:azure_bluet"||name.ends_with("_tulip")||name=="minecraft:oxeye_daisy"||name=="minecraft:cornflower"||name=="minecraft:lily_of_the_valley"||name=="minecraft:sunflower"||name=="minecraft:lilac"||name=="minecraft:rose_bush"||name=="minecraft:peony"||name=="minecraft:snow_layer"||name=="minecraft:short_grass"||name=="minecraft:tall_grass"||name=="minecraft:fern"||name=="minecraft:large_fern"||name=="minecraft:deadbush";}
inline std::optional<std::array<int,4>> structurePlantReservation(StructureBox const&b){
 if(b.max.x<b.min.x||b.max.z<b.min.z)return {};
 return std::array<int,4>{b.min.x+32756,b.min.z+32756,b.max.x+32780,b.max.z+32780};
}
// Preserve upstream X/Z/Y order: upper plants observe lower-block removals.
template<class Name,class Survives,class Clear>
unsigned clearUnsupportedStructurePlants(ChunkPosition cp,int lo,int hi,Name name,Survives survives,Clear clear,int maxY=319){
 unsigned count=0;
 for(int z=cp.z*16;z<cp.z*16+16;++z)for(int x=cp.x*16;x<cp.x*16+16;++x)for(int y=std::max(-63,lo);y<=std::min(maxY-1,hi);++y){
  StructurePoint p{x,y,z};if(!structureGroundPlant(name(p)))continue;
  if(name(StructurePoint{x,y-1,z})!="minecraft:grass_path"&&survives(p))continue;
  clear(p);++count;
 }
 return count;
}
}
