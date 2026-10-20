#pragma once
#include "SmallFeaturePlan.h"
#include <type_traits>
namespace worldgen_rules {
template<class Visit>void forEachFossilCandidate(ChunkPosition owner,uint32_t seed,Visit visit){
 for(int z=owner.z-1;z<=owner.z+1;++z)for(int x=owner.x-1;x<=owner.x+1;++x)if(fossilCandidate({x,z},seed))for(bool deep:{true,false})visit(ChunkPosition{x,z},deep);
}
template<class Writes,class Position,class Name>
StructureBox fossilOutputBounds(ChunkPosition anchor,Writes const&writes,Position position,Name name){
 if(writes.empty())throw std::runtime_error("Empty native fossil output");
 StructureBox box{{INT_MAX,INT_MAX,INT_MAX},{INT_MIN,INT_MIN,INT_MIN}};
 for(auto const&[key,value]:writes){auto p=position(key);if(!fossilBlockAllowed(name(value))||!fossilWriteInReach(anchor,p))throw std::runtime_error("Invalid original fossil output");
  box.min.x=std::min(box.min.x,p.x);box.min.y=std::min(box.min.y,p.y);box.min.z=std::min(box.min.z,p.z);box.max.x=std::max(box.max.x,p.x);box.max.y=std::max(box.max.y,p.y);box.max.z=std::max(box.max.z,p.z);
 }return box;
}
// Preflight the whole local fragment before the first write. Adapters provide
// native state reads/writes, including their block-actor protection check.
template<class Writes,class Position,class Read,class Apply,class Restore>
size_t publishFossilOwner(ChunkPosition owner,Writes const&writes,Position position,Read read,Apply apply,Restore restore){
 using State=std::invoke_result_t<Read,StructurePoint>;
 using Value=typename Writes::mapped_type;
 struct Change{StructurePoint p;State before;Value after;};std::vector<Change>changes;
 for(auto const&[key,value]:writes){auto p=position(key);if(spacingCell(p.x,16)==owner.x&&spacingCell(p.z,16)==owner.z)changes.push_back({p,read(p),value});}
 try{for(auto const&c:changes)if(!apply(c.p,c.after))throw std::runtime_error("Native fossil fragment write rejected");}
 catch(...){for(auto const&c:changes)if(!restore(c.p,c.before))throw std::runtime_error("Native fossil rollback failed");throw;}
 return changes.size();
}
}
