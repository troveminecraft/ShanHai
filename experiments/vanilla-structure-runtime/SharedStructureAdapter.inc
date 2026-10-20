// Only SDK translation belongs here. StructurePlan.h owns terrain decisions.
worldgen_rules::StructureBox sharedBox(BoundingBox const&b){return {{b.min.x,b.min.y,b.min.z},{b.max.x,b.max.y,b.max.z}};}
BoundingBox nativeBox(worldgen_rules::StructureBox const&b){return {{b.min.x,b.min.y,b.min.z},{b.max.x,b.max.y,b.max.z}};}
worldgen_rules::StructureBlueprintPiece sharedPoolPiece(PoolElementStructurePiece&p){
 using namespace worldgen_rules;StructureBlueprintPiece out;out.box=sharedBox(*p.mBoundingBox);out.origin={p.mPosition->x,p.mPosition->y,p.mPosition->z};out.rotation=int(p.mRotation);out.rigid=p.mElement.getProjection()==Projection::Rigid;out.location=*p.mElement.mLocation;
 out.kind=p.mElement.type()==StructurePoolElementType::Feature?StructurePieceKind::Feature:p.mElement.type()==StructurePoolElementType::Single?StructurePieceKind::Template:StructurePieceKind::Other;
 if(out.kind!=StructurePieceKind::Template)return out;
 auto const&optional=*p.mElement.mTemplate;if(!optional)throw std::runtime_error("Native pool template unavailable");
 auto legacy=std::get_if<std::reference_wrapper<LegacyStructureTemplate>>(&*optional->mStructureVariant);if(!legacy)throw std::runtime_error("Native pool template representation unavailable");
 out.cells.reserve(legacy->get().mBlockInfo->size());
 for(auto const&c:*legacy->get().mBlockInfo){std::string_view final,marker;auto const&name=c.mBlock.getTypeName();if(name=="minecraft:jigsaw"&&c.mTag&&c.mTag->contains("final_state",Tag::String))final=c.mTag->at("final_state").get<StringTag>();
  if(name=="minecraft:structure_block"&&c.mTag){std::string_view mode,metadata;if(c.mTag->contains("mode",Tag::String))mode=c.mTag->at("mode").get<StringTag>();if(c.mTag->contains("metadata",Tag::String))metadata=c.mTag->at("metadata").get<StringTag>();if(auto label=legacyDataMarker(name,mode,metadata))marker=*label;}
  out.cells.push_back({{c.mPos->x,c.mPos->y,c.mPos->z},{name,reinterpret_cast<uintptr_t>(&c.mBlock)},final,marker});
 }return out;
}
Block const* sharedBlock(worldgen_rules::StructureBlock const&b){
 if(b.handle)return reinterpret_cast<Block const*>(b.handle);
 auto found=Block::tryGetFromRegistry(std::string(b.name));if(!found)throw std::runtime_error("Missing structure block "+std::string(b.name));return &*found;
}
