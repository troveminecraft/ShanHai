// The native jigsaw builder still owns road connections, collision checks and
// template selection. Only the shared size budget changes for a planned village.
struct VillageGrowthContext {
 worldgen_rules::VillageGrowthBudget budget;size_t scanned=0;int buildings=0;
 void count(std::vector<std::unique_ptr<StructurePiece>> const&pieces){
  for(;scanned<pieces.size();++scanned){auto const&p=*static_cast<PoolElementStructurePiece*>(pieces[scanned].get());buildings+=worldgen_rules::villageBuilding(*p.mElement.mLocation);}
 }
};
thread_local VillageGrowthContext* villageGrowth=nullptr;
LL_TYPE_INSTANCE_HOOK(VillageBuilderDepth,ll::memory::HookPriority::Normal,LegacyJigsawPlacement,&LegacyJigsawPlacement::$ctor,void*,int depth,LegacyJigsawPlacement::PieceFactory factory){
 return origin(villageGrowth?std::max(depth,villageGrowth->budget.depth):depth,std::move(factory));
}
LL_TYPE_INSTANCE_HOOK(VillageBuilderBudget,ll::memory::HookPriority::Normal,LegacyJigsawPlacement,&LegacyJigsawPlacement::_tryPlacingPiece,bool,
 std::vector<std::unique_ptr<StructurePiece>>&pieces,PoolElementStructurePiece const&source,Random&random,JigsawBlockInfo const&jigsaw,BoundingBox const&sourceBox,std::vector<BoundingBox>&internal,BlockPos const&attach,StructureTemplatePool const*pool,JigsawStructureRegistry const&pools,Dimension&dimension,BlockVolume&volume,BlockPos const&reference){
 if(villageGrowth){villageGrowth->count(pieces);if(villageGrowth->buildings>=villageGrowth->budget.buildings||pieces.size()>=size_t(villageGrowth->budget.pieces))return false;}
 return origin(pieces,source,random,jigsaw,sourceBox,internal,attach,pool,pools,dimension,volume,reference);
}
