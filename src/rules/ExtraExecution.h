#pragma once
#include "StructurePlan.h"
namespace worldgen_rules {
inline bool extraPopulationRequired(int type){return type==0||type==5||type==7||type==9||type>=10;}
// Preserve original piece indices: dropping non-intersecting pieces must not
// change the random stream of the next piece in another owner chunk.
template<class Bounds,class Place>
unsigned executeExtraPieces(int type,ChunkPosition anchor,StructureBox const&owner,uint32_t seed,size_t count,Bounds bounds,Place place){
 ChunkPosition cp{spacingCell(owner.min.x,16),spacingCell(owner.min.z,16)};
 unsigned placed=0;
 for(size_t i=0;i<count;++i)if(bounds(i).intersectsXZ(owner)){
  place(i,extraPieceSeed(type,i,anchor,cp,seed),owner);++placed;
 }
 return placed;
}
}
