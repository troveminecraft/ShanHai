// Bedrock ships contain twenty native variants (including degraded halves).
// Freeze the selected model and its rotated, complete footprint before writing
// any chunk. The legacy procedural piece's nominal 20x20 box cannot do this.
class FrozenShipPiece final:public TemplateStructurePiece {
public:
 std::string modelName;
 std::optional<worldgen_rules::FrozenShipProgram> sharedProgram;
 FrozenShipPiece(Dimension& d,Random& rng,BlockPos pos,ChunkPos anchor,uint seed,bool unified):TemplateStructurePiece(0){
  std::optional<worldgen_rules::FrozenShipPlan> geometry;
  if(unified){
   for(size_t i=0;i<worldgen_rules::FrozenShipTemplates.size();++i)if(ShipwreckPiece::STRUCTURE_SHIPWRECK_TYPES()[i]!=worldgen_rules::FrozenShipTemplates[i])throw std::runtime_error("Native ship catalog differs from shared upstream");
   geometry=worldgen_rules::planFrozenShip({anchor.x,anchor.z},seed,[&](std::string_view name){auto&model=bedrock_compat::level(d).getStructureManager()->getOrCreateLegacy(std::string(name));auto s=model.rawSize();return worldgen_rules::StructurePoint{s.x,s.y,s.z};});
  }
  auto choice=geometry?geometry->selection:worldgen_rules::chooseFrozenShip(rng);
  modelName=ShipwreckPiece::STRUCTURE_SHIPWRECK_TYPES()[choice.variant];
  auto& model=bedrock_compat::level(d).getStructureManager()->getOrCreateLegacy(modelName);
  auto rotation=Rotation(choice.rotation);mTemplate=&model;*mTemplatePosition=pos;
  *mSettings=LegacyStructureSettings(Mirror::None,rotation,nullptr,{{terrain_bounds::MinBlock,-64,terrain_bounds::MinBlock},{terrain_bounds::MaxBlock,(field->generationHeight()-65),terrain_bounds::MaxBlock}});
  mSettings->mIgnoreStructureBlocks=true;mPostProcessed=false;mOrientation=-1;*mBoundingBox=templateBounds(model,pos,rotation);
  if(geometry)sharedProgram=worldgen_rules::compileFrozenShip(*geometry,nativeLegacyDataMarkers(model));
 }
 void syncSharedGeometry(){if(!sharedProgram)return;auto&g=sharedProgram->geometry;if(mTemplatePosition->x!=g.origin.x||mTemplatePosition->z!=g.origin.z)throw std::runtime_error("Unexpected frozen ship horizontal move");worldgen_rules::moveFrozenShipProgram(*sharedProgram,mTemplatePosition->y-g.origin.y);}
 bool postProcess(BlockSource&blocks,Random&random,BoundingBox const&clip)override{
  if(!sharedProgram)return TemplateStructurePiece::postProcess(blocks,random,clip);
  return worldgen_rules::executeFrozenShip(*sharedProgram,{{clip.min.x,clip.min.y,clip.min.z},{clip.max.x,clip.max.y,clip.max.z}},[&](auto const&g,uint32_t randomSeed,auto const&){LegacyStructureSettings settings(Mirror::None,Rotation(g.selection.rotation),nullptr,clip);settings.mIgnoreStructureBlocks=true;settings.mSeed=g.settingsSeed;Random rng(randomSeed,true);mTemplate->placeInWorld(blocks,{g.origin.x,g.origin.y,g.origin.z},settings,rng);},[&](worldgen_rules::StructurePoint p)->std::string_view{return blocks.getBlock({p.x,p.y,p.z}).getTypeName();},[&](worldgen_rules::StructurePoint p){bedrock_compat::setBlockNoUpdate(blocks,{p.x,p.y,p.z},*Block::tryGetFromRegistry("minecraft:air"));},[&](worldgen_rules::StructurePoint p,int facing){auto chest=Block::tryGetFromRegistry("minecraft:chest",static_cast<ushort>(facing));if(!chest)throw std::runtime_error("Frozen ship chest block unavailable");bedrock_compat::setBlockNoUpdate(blocks,{p.x,p.y,p.z},*chest);},[&](worldgen_rules::StructureLootAssignment const&a){auto actor=blocks.getBlockEntity({a.position.x,a.position.y,a.position.z});if(!actor)throw std::runtime_error("Frozen ship chest actor absent");static_cast<ChestBlockActor*>(actor)->setLootTable(a.table,int(a.seed));});
 }
 void _handleDataMarker(std::string const& label,BlockPos const& pos,BlockSource& blocks,Random& rng,BoundingBox const& clip)override{
  if(pos.x<clip.min.x||pos.x>clip.max.x||pos.z<clip.min.z||pos.z>clip.max.z)return;
  std::string loot(worldgen_rules::shipMarkerLoot(label));if(loot.empty())return;
  BlockPos chest=pos;if(blocks.getBlock({pos.x,pos.y-1,pos.z}).getTypeName()=="minecraft:chest")--chest.y;
  bedrock_compat::setBlockNoUpdate(blocks,pos,*Block::tryGetFromRegistry("minecraft:air"));
  if(blocks.getBlock(chest).getTypeName()=="minecraft:chest"){auto*actor=blocks.getBlockEntity(chest);if(actor)static_cast<ChestBlockActor*>(actor)->setLootTable(loot,rng.nextInt());}
  else StructureHelpers::createChest(*this,blocks,clip,rng,chest.x,chest.y,chest.z,2,loot);
 }
};
class FrozenShipStart final:public StructureStart {
public:
 FrozenShipStart(Dimension& d,Random& rng,ChunkPos cp,uint seed,bool unified){chunkX=cp.x;chunkZ=cp.z;maxYSpawnOffset=0;pieces->push_back(std::make_unique<FrozenShipPiece>(d,rng,BlockPos{cp.x*16,0,cp.z*16},cp,seed,unified));bedrock_compat::calculateBoundingBox(*this);}
 std::string_view getStructureName()const override{return "minecraft:shipwreck";}
};
