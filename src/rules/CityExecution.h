#pragma once
#include "CityEnvironment.h"
namespace worldgen_rules {
inline constexpr int CityFragmentReach=104;
struct CityFragmentResult {unsigned carved=0,templates=0,ecologyAttempts=0,ecologyPlaced=0;};
// Original LLBDS order: carve natural rock, all intersecting non-feature native
// pieces (with the ORIGINAL piece index in their seed), then owned feature
// nodes, then attachment/cover finalization. Adapters supply only native calls.
template<class Carve,class Place,class Ecology,class Finish>
CityFragmentResult executeCityFragment(std::span<StructureBlueprintPiece const>pieces,ChunkPosition anchor,StructureBox owner,uint32_t seed,Carve carve,Place place,Ecology ecology,Finish finish){
 if(!owner.valid()||spacingCell(owner.min.x,16)!=spacingCell(owner.max.x,16)||spacingCell(owner.min.z,16)!=spacingCell(owner.max.z,16))throw std::runtime_error("City fragment must have one chunk owner");
 CityFragmentResult out;out.carved=carve();ChunkPosition cp{spacingCell(owner.min.x,16),spacingCell(owner.min.z,16)};
 for(size_t i=0;i<pieces.size();++i){auto const&p=pieces[i];if(p.kind==StructurePieceKind::Feature||!p.box.intersectsXZ(owner))continue;place(i,extraPieceSeed(8,i,anchor,cp,seed),owner);++out.templates;}
 for(auto const&node:cityEcologyNodes(pieces,anchor,owner,seed)){++out.ecologyAttempts;if(ecology(node,owner))++out.ecologyPlaced;}
 finish();return out;
}
}
