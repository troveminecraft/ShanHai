#pragma once
#include "StructurePlan.h"
namespace worldgen_rules {
// Each native scatter sees its own pending writes, then the complete planned
// templates, then fitted seeded ground. It never consults a live neighbour.
template<class Captured,class Template,class Ground,class Material,class Resolve>
auto villageDecorRead(StructurePoint p,Captured const&captured,Template templ,Ground ground,Material material,Resolve resolve)->typename Captured::mapped_type{
 if(auto it=captured.find({p.x,p.y,p.z});it!=captured.end())return it->second;
 if(auto b=templ(p))return b;
 int floor=ground(p.x,p.z);
 if(p.y>floor)return resolve("minecraft:air");
 if(p.y<floor)return resolve("minecraft:dirt");
 return resolve(material(p.x,p.z)==4?"minecraft:grass_block":"minecraft:stone");
}
inline bool villageGroundedDecor(std::string_view n){return n=="minecraft:hay_block"||n=="minecraft:melon_block"||n=="minecraft:pumpkin";}
// Native scatter height conventions differ from template origins. Anchor each
// stack to the completed ground, retaining the stack's internal vertical shape.
template<class Writes,class Name,class Ground>
Writes groundVillageDecor(Writes const&writes,Name name,Ground ground){
 std::map<VillageXZ,int> bottoms;
 for(auto const&[q,b]:writes)if(villageGroundedDecor(name(b)))mergeMinimum(bottoms,{q[0],q[2]},q[1]);
 Writes result;
 for(auto const&[q,b]:writes){auto p=q;if(villageGroundedDecor(name(b)))p[1]+=ground(q[0],q[2])+1-bottoms.at({q[0],q[2]});result.emplace(p,b);}
 return result;
}
template<class Writes,class Air,class Entity,class Write,class Support>
size_t publishVillageDecor(ChunkPosition owner,Writes const&writes,Air air,Entity entity,Write write,Support support){
 size_t count=0;for(auto const&[q,b]:writes){StructurePoint p{q[0],q[1],q[2]};if(spacingCell(p.x,16)!=owner.x||spacingCell(p.z,16)!=owner.z||!air(p)||entity(p)||!support(p,b))continue;write(p,b);++count;}return count;
}
}
