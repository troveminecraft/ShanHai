// Scatter features must not read a half-built neighbour or write into it.
// Run the original definition against final seeded ground and the complete
// native layout, retain its output, then publish only each owning chunk.
class VillageDecorSource final:public bedrock_compat::BlockSourceView {
 VillagePlan const&plan;
 Block const*air;Block const*soil;Block const*grass;Block const*stone;
public:
 VillageDecorSource(BlockSource&source,VillagePlan const&p):BlockSourceView(source.getChunkSource(),false,true),plan(p){air=&*Block::tryGetFromRegistry("minecraft:air");soil=&*Block::tryGetFromRegistry("minecraft:dirt");grass=&*Block::tryGetFromRegistry("minecraft:grass_block");stone=&*Block::tryGetFromRegistry("minecraft:stone");}
 Block const&getBlock(BlockPos const&p)const override{
  static std::map<std::array<int,3>,Block const*> const empty;
  auto const&pending=fossilCapture?fossilCapture->writes:empty;
  return *worldgen_rules::villageDecorRead(worldgen_rules::StructurePoint{p.x,p.y,p.z},pending,[&](auto q)->Block const*{auto it=plan.templateBlocks.find({q.x,q.y,q.z});return it==plan.templateBlocks.end()?nullptr:it->second;},[&](int x,int z){return villageGround(plan,x,z,field->surface(x+32768,z+32768).height);},[&](int x,int z){auto s=field->surface(x+32768,z+32768);return field->material(s,x+32768,s.height,z+32768);},[&](std::string_view n){return n=="minecraft:air"?air:n=="minecraft:dirt"?soil:n=="minecraft:grass_block"?grass:stone;});
 }
 Block const&getBlock(BlockPos const&p,uint layer)const override{return layer?*air:getBlock(p);}
 bool isSolidBlockingBlock(BlockPos const&p)const override{return bedrock_compat::solidBlock(getBlock(p));}
 bool isSolidBlockingBlock(int x,int y,int z)const override{return bedrock_compat::solidBlock(getBlock({x,y,z}));}
};
class VillageDecorTarget final:public WorldBlockTarget {
 VillagePlan const&plan;
public:
 VillageDecorTarget(BlockSource&source,WorldGenContext const&ctx,VillagePlan const&p):WorldBlockTarget(source,ctx),plan(p){}
 bool canGetChunk()const override{return false;}
 LevelChunk*getChunk(ChunkPos const&)override{return nullptr;}
 Block const&getBlock(BlockPos const&p)const override{return mBlockSource.getBlock(p);}
 Block const&getBlockNoBoundsCheck(BlockPos const&p)const override{return mBlockSource.getBlock(p);}
 Block const&getExtraBlock(BlockPos const&p)const override{return mBlockSource.getBlock(p,1);}
 Biome const*getBiome(BlockPos const&p)const override{static std::vector<std::string> names;if(names.empty()){J metadata;std::ifstream((terrainFolder()/"terrain.json"))>>metadata;names=metadata["biomeNames"].get<std::vector<std::string>>();}auto s=field->surface(p.x+32768,p.z+32768);auto b=bedrock_compat::dimension(mBlockSource).getBiomeRegistry().lookupByName("minecraft:"+names.at(s.biome));return b?&*b:nullptr;}
 short getHeightmap(int x,int z)override{return short(villageGround(plan,x,z,field->surface(x+32768,z+32768).height)+1);}
};
void placeVillageDecor(BlockSource&blocks,ChunkPos cp,VillagePlan&plan,uint seed){
 if(!plan.decorReady){
  VillageDecorSource source(blocks,plan);WorldGenContext context{};context.mPreliminarySurfaceProvider=&provider;VillageDecorTarget target(source,context,plan);MolangVariableMap variables;RenderParams params;params.mVariables=&variables;params.mLevel=&bedrock_compat::level(blocks);params.mBlockSource=&source;params.mBlockPlacementTarget=&target;
  std::map<size_t,worldgen_rules::VillageDecorPlacement> commonDecor;
  if(plan.sharedLayout)for(auto op:worldgen_rules::villageDecorOrder(plan.sharedBlueprint,*plan.sharedLayout,[&](int x,int z){return villageGround(plan,x,z,field->surface(x+32768,z+32768).height);}))commonDecor.emplace(op.index,op);
  plan.diagnostics["frozenDecor"]=J::array();for(size_t i=0;i<plan.start->pieces->size();++i){auto const&ptr=(*plan.start->pieces)[i];auto&piece=*static_cast<PoolElementStructurePiece*>(ptr.get());if(piece.mElement.type()!=StructurePoolElementType::Feature||piece.mElement.mLocation->find("tree")!=std::string::npos||plan.blockedFeatures.contains(ptr.get()))continue;
   auto common=commonDecor.find(i);if(plan.sharedLayout&&common==commonDecor.end())continue;
   Random rng(common!=commonDecor.end()?common->second.seed:worldgen_rules::placementSeed(plan.start->chunkX,plan.start->chunkZ,10387380+int(i),seed),true);FossilCapture capture{&source};fossilCapture=&capture;capturedVillageGround=&plan;struct Reset{~Reset(){fossilCapture=nullptr;capturedVillageGround=nullptr;}}reset;
   FeaturePlacementFailureLog logger;
#ifdef SHANHAI_MC_26_51
  FeaturePlacementFailureLogProxy proxy{&logger};
#else
  FeaturePlacementFailureLogProxy proxy(logger);
#endif
struct Context{IBlockWorldGenAPI&target;BlockPos pos;Random&random;RenderParams&params;FeaturePlacementFailureLogProxy logger;};static_assert(sizeof(Context)==sizeof(IFeature::PlacementContext));
   auto pos=*piece.mPosition;pos.y=villageGround(plan,pos.x,pos.z,field->surface(pos.x+32768,pos.z+32768).height)+1;if(common!=commonDecor.end())pos={common->second.position.x,common->second.position.y,common->second.position.z};params.mBlockPos=pos;params.mRandomFunction=std::function<float()>([&rng]{return rng.nextFloat();});variables.setMolangVariable(HashedString("variable.worldx"),MolangScriptArg(float(pos.x)));variables.setMolangVariable(HashedString("variable.worldy"),MolangScriptArg(float(pos.y)));variables.setMolangVariable(HashedString("variable.worldz"),MolangScriptArg(float(pos.z)));if(auditPlanningStarts)std::ofstream(folder/"village-decor-pending.json")<<J{{"location",*piece.mElement.mLocation},{"position",{pos.x,pos.y,pos.z}},{"anchor",{plan.start->chunkX*16,plan.start->chunkZ*16}}}.dump(2);Context storage{target,pos,rng,params,std::move(proxy)};auto result=smallDefinition(bedrock_compat::level(blocks),*piece.mElement.mLocation).place(reinterpret_cast<IFeature::PlacementContext const&>(storage));
   plan.diagnostics["frozenDecor"].push_back({{"location",*piece.mElement.mLocation},{"success",result.has_value()},{"writes",capture.writes.size()}});if(capture.rejected)throw std::runtime_error("Village original scatter exceeds protected world bounds");if(result.has_value())for(auto const&[p,b]:capture.writes)plan.decorWrites[p]=b;
  }
  plan.decorWrites=worldgen_rules::groundVillageDecor(plan.decorWrites,[](auto b){return b->getTypeName();},[&](int x,int z){return villageGround(plan,x,z,field->surface(x+32768,z+32768).height);});
  plan.decorReady=true;
 }
 worldgen_rules::publishVillageDecor({cp.x,cp.z},plan.decorWrites,[&](auto p){auto const&b=blocks.getBlock({p.x,p.y,p.z});return b.isAir()||groundPlant(b.getTypeName());},[&](auto p){return blocks.getBlockEntity({p.x,p.y,p.z})!=nullptr;},[&](auto p,auto b){bedrock_compat::setBlockNoUpdate(blocks,{p.x,p.y,p.z},*b);},[&](auto p,auto b){return !worldgen_rules::villageGroundedDecor(b->getTypeName())||bedrock_compat::solidBlock(blocks.getBlock({p.x,p.y-1,p.z}));});
}
