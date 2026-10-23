#pragma once
#include "StructureFinish.h"
#include "StructurePlan.h"
#include <optional>
#include <set>

// LLBDS small structures use native definitions. Candidate selection, terrain
// probes and admission are shared; adapters supply reads and native placement.
namespace worldgen_rules {


inline constexpr int SmallFeatureSalt=146731108;
inline constexpr unsigned DungeonProbeLimit=16;
inline uint64_t smallFeatureSelector(ChunkPosition cp,uint32_t seed){
 uint64_t v=uint64_t(uint32_t(cp.x))*0x9e3779b97f4a7c15ull^uint64_t(uint32_t(cp.z))*0xbf58476d1ce4e5b9ull^seed^0x146731108ull;
 v=(v^(v>>30))*0xbf58476d1ce4e5b9ull;v=(v^(v>>27))*0x94d049bb133111ebull;return v^(v>>31);
}
inline bool smallFeaturePotential(ChunkPosition cp,uint32_t seed){return smallFeatureSelector(cp,seed)%4==0;}
inline bool wellCandidate(ChunkPosition cp,uint32_t seed){return smallFeatureSelector(cp,seed)%500==0;}
inline bool fossilCandidate(ChunkPosition cp,uint32_t seed){return smallFeatureSelector(cp,seed)%64==0;}
inline bool fossilNeighbourPotential(ChunkPosition cp,uint32_t seed){
 for(int z=cp.z-1;z<=cp.z+1;++z)for(int x=cp.x-1;x<=cp.x+1;++x)if(fossilCandidate({x,z},seed))return true;return false;
}
template<class Surface> bool smallFeatureHabitat(Surface const&s){return s.volcanic<=.01&&s.iceHabitat<=.01&&s.ocean<=.5;}
template<class Surface> bool fossilHabitat(Surface const&s){return (s.biome==11||s.biome==10)&&smallFeatureHabitat(s);}
template<class Sample> std::optional<StructurePoint> wellPosition(ChunkPosition cp,Sample sample){
 int x=cp.x*16+8,z=cp.z*16+8;auto s=sample(x,z);
 if(s.biome!=11||s.water>=s.height||s.river||s.lake||s.volcanic>.01||s.iceHabitat>.01)return {};
 for(int dz=-3;dz<=3;dz+=3)for(int dx=-3;dx<=3;dx+=3){auto p=sample(x+dx,z+dz);if(p.biome!=11||p.river||p.lake||p.water>=p.height||p.height!=s.height)return {};}
 return StructurePoint{x,s.height+1,z};
}
inline StructureBox wellBounds(StructurePoint p){return {{p.x-3,p.y-3,p.z-3},{p.x+3,p.y+4,p.z+3}};}
inline StructureBox dungeonBounds(StructurePoint p){return {{p.x-5,p.y-2,p.z-5},{p.x+5,p.y+5,p.z+5}};}
template<class Boxes> bool smallFeatureFits(StructureBox const&b,Boxes const&occupied){
 for(auto const&o:occupied)if(b.max.x>=o.min.x&&b.min.x<=o.max.x&&b.max.z>=o.min.z&&b.min.z<=o.max.z&&b.max.y>=o.min.y-4&&b.min.y<=o.max.y+4)return false;return true;
}
// This bounded scan does not carve a room or connect a tunnel. The native room
// definition still decides floor/roof/entrances, mob type and chest contents.
template<class Get,class Solid> std::vector<int> dungeonProbeLevels(ChunkPosition cp,int height,Get name,Solid solid){
 int high=std::min(104,height-16);if(high< -40)return {};
 std::set<int> floors;
 for(auto[dx,dz]:{VillageXZ{0,0},{3,0},{-3,0},{0,3},{0,-3}})for(int y=high;y>=-50;--y){StructurePoint p{cp.x*16+8+dx,y,cp.z*16+8+dz};if(name(p)=="minecraft:air"&&solid(StructurePoint{p.x,y-1,p.z}))floors.insert(y);}
 return {floors.begin(),floors.end()};
}
template<class Get,class Solid> bool dungeonProbeSupported(StructurePoint p,Get name,Solid solid){
 if(!solid(StructurePoint{p.x,p.y-1,p.z})||!solid(StructurePoint{p.x,p.y+4,p.z}))return false;
 for(int r:{3,4})for(auto[dx,dz]:{VillageXZ{r,0},{-r,0},{0,r},{0,-r}})
  if(name(StructurePoint{p.x+dx,p.y,p.z+dz})=="minecraft:air"&&name(StructurePoint{p.x+dx,p.y+1,p.z+dz})=="minecraft:air")return true;
 return false;
}
inline int fossilPlacementSalt(bool deep){return deep?146731110:146731109;}
template<class Random> StructurePoint fossilOrigin(ChunkPosition anchor,int height,bool deep,Random&rng){
 int y=deep?-32+rng.nextInt(16):8+rng.nextInt(std::max(1,std::min(48,height-14)-7));return {anchor.x*16+8,y,anchor.z*16+8};
}
inline bool fossilBlockAllowed(std::string_view name){return name=="minecraft:bone_block"||name=="minecraft:coal_ore"||name=="minecraft:deepslate_diamond_ore";}
inline bool fossilWriteInReach(ChunkPosition anchor,StructurePoint p,int maxY=319){return p.y>=-58&&p.y<=maxY&&std::abs(spacingCell(p.x,16)-anchor.x)<=1&&std::abs(spacingCell(p.z,16)-anchor.z)<=1;}
inline bool smallFeatureWriteOwned(ChunkPosition owner,StructurePoint p,bool bedrock,int maxY=319){return !bedrock&&p.y>=-58&&p.y<=maxY&&spacingCell(p.x,16)==owner.x&&spacingCell(p.z,16)==owner.z;}
// One execution order and one random stream for all platforms. Adapters only
// provide terrain/blocks and invoke the original native feature implementation.
template<class Random,class Sample,class Get,class Solid,class Boxes,class Place>
void executeSmallFeatures(ChunkPosition cp,uint32_t seed,Random&rng,Sample sample,Get name,Solid solid,Boxes const&occupied,Place place){
 auto surface=sample(cp.x*16+8,cp.z*16+8);if(!smallFeatureHabitat(surface))return;
 if(wellCandidate(cp,seed))if(auto p=wellPosition(cp,sample);p&&smallFeatureFits(wellBounds(*p),occupied))place("desert_well",*p,rng);
 auto levels=dungeonProbeLevels(cp,surface.height,name,solid);
 if(!levels.empty())std::rotate(levels.begin(),levels.begin()+rng.nextInt(int(levels.size())),levels.end());
 unsigned checked=0;
 for(int y:levels){if(checked++>=DungeonProbeLimit)break;StructurePoint p{cp.x*16+8,y,cp.z*16+8};
  if(dungeonProbeSupported(p,name,solid)&&smallFeatureFits(dungeonBounds(p),occupied)&&place("monster_room",p,rng))break;
 }
}
}
