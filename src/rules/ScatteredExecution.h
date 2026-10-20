#pragma once
#include "StructurePlan.h"
#include <map>
#include <tuple>
namespace worldgen_rules {
// Original LLBDS jungle/hut semantics: a fresh native start for each owner,
// frozen whole-footprint floor, owner seed, immutable seeded neighbour reads.
// Native object construction, registered states and world writes are services.
struct ScatteredFragmentRecipe {int biome;ChunkPosition anchor;uint32_t worldSeed;StructureBox bounds;int floor;};
inline ScatteredFragmentRecipe planScatteredFragments(int biome,ChunkPosition anchor,uint32_t seed,StructureBox bounds,int floor){
 if((biome!=13&&biome!=10)||!bounds.valid()||bounds.min.y< -58||bounds.max.y>319||floor!=bounds.min.y||bounds.min.x< -30000000||bounds.min.z< -30000000||bounds.max.x>=30000000||bounds.max.z>=30000000||int64_t(bounds.max.x)-bounds.min.x>128||int64_t(bounds.max.z)-bounds.min.z>128)throw std::runtime_error("Invalid scattered fragment recipe");
 return {biome,anchor,seed,bounds,floor};
}
inline std::string_view scatteredNativeBiome(ScatteredFragmentRecipe const&r){return r.biome==13?"minecraft:jungle":"minecraft:swampland";}
inline bool scatteredSeededRead(StructureBox const&owner,StructurePoint p){return p.x<owner.min.x||p.x>owner.max.x||p.z<owner.min.z||p.z>owner.max.z;}
inline bool scatteredFragmentWriteOwned(StructureBox const&owner,StructurePoint p){return !scatteredSeededRead(owner,p)&&p.y>=-58&&p.y<=319;}
inline int scatteredFreshMove(StructureBox expected,int floor,StructureBox initial){
 if(!initial.valid())throw std::runtime_error("Native scattered start has no geometry");int64_t shift=int64_t(floor)-initial.min.y;if(shift<INT_MIN||shift>INT_MAX||int64_t(initial.max.y)+shift<INT_MIN||int64_t(initial.max.y)+shift>INT_MAX)throw std::runtime_error("Native scattered floor shift overflow");auto moved=initial.moved(int(shift));if(moved.min!=expected.min||moved.max!=expected.max)throw std::runtime_error("Fresh scattered geometry differs from native record");return int(shift);
}
inline int scatteredFreshMove(ScatteredFragmentRecipe const&r,StructureBox initial){return scatteredFreshMove(r.bounds,r.floor,initial);}
struct ScatteredOwnerFragment {ChunkPosition owner;StructureBox clip;uint32_t seed;};
template<class Create,class Place> bool executeScatteredFragments(ScatteredFragmentRecipe const&r,StructureBox caller,Create create,Place place){
 if(!r.bounds.intersectsXZ(caller))return false;bool any=false;
 int x0=spacingCell(std::max(r.bounds.min.x,caller.min.x),16),x1=spacingCell(std::min(r.bounds.max.x,caller.max.x),16),z0=spacingCell(std::max(r.bounds.min.z,caller.min.z),16),z1=spacingCell(std::min(r.bounds.max.z,caller.max.z),16);
 for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){StructureBox owner{{std::max(x*16,caller.min.x),caller.min.y,std::max(z*16,caller.min.z)},{std::min(x*16+15,caller.max.x),caller.max.y,std::min(z*16+15,caller.max.z)}};
  ScatteredOwnerFragment fragment{{x,z},owner,placementSeed(x,z,14357617,r.worldSeed)};auto fresh=create(r,fragment);bool done=place(fresh,r,fragment);any=done||any;
 }return any;
}
template<class State> class ScatteredSeededReadCache {
 std::map<std::tuple<int,int,int>,State>values;
public:
 template<class Query> State get(StructurePoint p,Query query){auto key=std::tuple{p.x,p.y,p.z};if(auto it=values.find(key);it!=values.end())return it->second;if(values.size()>=65536)throw std::runtime_error("Scattered seeded-read limit exceeded");auto value=query(p);values.emplace(key,value);return value;}
 size_t size()const{return values.size();}
};
}
