// Native test chunks are not inserted into the main chunk source or saved.
// This harness exercises original placement against its own block source.
class AuditChunkSource final:public ChunkSource {
 std::map<std::pair<int,int>,std::shared_ptr<LevelChunk>> chunks;
public:
 AuditChunkSource(Dimension&d,BoundingBox const&box):ChunkSource(&d,16){
  int count=(chunk(box.max.x)-chunk(box.min.x)+1)*(chunk(box.max.z)-chunk(box.min.z)+1);if(count<1||count>2048)throw std::runtime_error("Memory audit chunk bound exceeded");
  for(int z=chunk(box.min.z);z<=chunk(box.max.z);++z)for(int x=chunk(box.min.x);x<=chunk(box.max.x);++x){auto c=std::make_shared<LevelChunk>(d,ChunkPos{x,z},false,SubChunkInitMode::ReplaceWithAllAir,false,LevelChunkBlockActorStorage::TrackingMode::None);const_cast<std::atomic<ChunkState>&>(c->getState()).store(ChunkState::Loaded);chunks.emplace(std::pair{x,z},std::move(c));}
 }
 std::shared_ptr<LevelChunk> getExistingChunk(ChunkPos const&p)override{auto i=chunks.find({p.x,p.z});return i==chunks.end()?nullptr:i->second;}
 std::shared_ptr<LevelChunk> getOrLoadChunk(ChunkPos const&p,LoadMode,bool)override{return getExistingChunk(p);}
 std::shared_ptr<LevelChunk> createNewChunk(ChunkPos const&p,LoadMode,bool)override{return getExistingChunk(p);}
 bool isChunkKnown(ChunkPos const&p)override{return bool(getExistingChunk(p));}
 bool isChunkSaved(ChunkPos const&)override{return false;}
 bool saveLiveChunk(LevelChunk&)override{return false;}
 void writeEntityChunkTransfer(LevelChunk&)override{}
 void deserializeActorStorageToLevelChunk(LevelChunk&)override{}
 bool canCreateViews()const override{return false;}
 bool canLaunchTasks()const override{return false;}
 void shutdown()override{chunks.clear();}
 bool isShutdownDone()override{return true;}
 std::vector<LevelChunkBlockActorAccessToken> access(){std::vector<LevelChunkBlockActorAccessToken> tokens;for(auto const&[p,c]:chunks)tokens.push_back(c->enableBlockEntityAccessForThisThread());return tokens;}
};
void memoryAuditProbe(Dimension&d){
 bool human=false;d.getLevel().forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human||interactionAuditActive)throw std::runtime_error("Memory audit requires an empty test session");
 BlockPos p{8,80,8};auto original=&d.getBlockSourceFromMainChunkSource().getBlock(p);
 J result;{
  AuditChunkSource source(d,{{0,-64,0},{15,field->generationHeight()-65,15}});auto tokens=source.access();BlockSource blocks(source,false,true);
  auto stone=Block::tryGetFromRegistry("minecraft:stone"),chest=Block::tryGetFromRegistry("minecraft:chest");
  bool first=blocks.setBlockNoUpdate(p,*stone);bool second=blocks.setBlockNoUpdate(p,*chest);auto*actor=blocks.getBlockEntity(p);
  if(!first||!second||!actor||blocks.getBlock(p).getTypeName()!="minecraft:chest")throw std::runtime_error("Memory native block source did not retain blocks/actor");
  static_cast<ChestBlockActor*>(actor)->setLootTable("loot_tables/chests/desert_pyramid.json",123);
  CompoundTag tag;SaveContext context;bool saved=actor->save(tag,context);result={{"saved",saved},{"data",J::parse(tag.toSnbt(SnbtFormat::Jsonify),nullptr,false,true)}};
 }
 bool unchanged=*original==d.getBlockSourceFromMainChunkSource().getBlock(p);if(!unchanged)throw std::runtime_error("Memory probe changed a live block");result["liveBlockUnchanged"]=unchanged;std::ofstream(folder/"memory-audit-probe.json")<<result.dump(2);
}

struct AuditPlanScope {
 std::optional<int> oldVillageSpacing=auditVillageSpacing;
 J oldInput;std::unique_ptr<nativegen::Field> oldField;
 decltype(automaticBlueprints) oldScattered;decltype(villages) oldVillages;
 decltype(cityLinks) oldCityLinks;decltype(cityNeighbourCache) oldCityNeighbours;
 decltype(extraPlans) oldExtras;decltype(extraDelegates) oldDelegates;
 decltype(vegetationPairs) oldVegetation;
 std::vector<std::unique_ptr<StructureStart>> owned;
 std::unique_ptr<TerrainVillageFeature> villageDelegate;
 AuditPlanScope(Dimension&d,J const&data,uint seed){
  auto next=std::make_unique<nativegen::Field>(data);villageDelegate=std::make_unique<TerrainVillageFeature>(seed,d.getBiomeRegistry());
  auditVillageSpacing=data.at("config").value("villageSpacing",32)==20?20:32;
  oldInput=std::move(input);input=data;oldField=std::move(field);field=std::move(next);
  oldCityLinks.swap(cityLinks);oldCityNeighbours.swap(cityNeighbourCache);oldScattered.swap(automaticBlueprints);oldVillages.swap(villages);oldExtras.swap(extraPlans);oldDelegates.swap(extraDelegates);oldVegetation.swap(vegetationPairs);
  auditPlanningSeed=seed;auditPlanningStarts=&owned;auditVillageDelegate=villageDelegate.get();
 }
 ~AuditPlanScope(){
  cityLinks.clear();cityNeighbourCache.clear();cityLinks.swap(oldCityLinks);cityNeighbourCache.swap(oldCityNeighbours);
  automaticBlueprints.clear();iglooPopulationPlans.clear();villages.clear();extraPlans.clear();extraDelegates.clear();vegetationPairs.clear();owned.clear();
  automaticBlueprints.swap(oldScattered);villages.swap(oldVillages);extraPlans.swap(oldExtras);extraDelegates.swap(oldDelegates);vegetationPairs.swap(oldVegetation);
  auditVillageSpacing=oldVillageSpacing;
  input=std::move(oldInput);field=std::move(oldField);auditPlanningStarts=nullptr;auditPlanningSeed.reset();auditVillageDelegate=nullptr;adaptHeightMap=false;planningHeights=nullptr;
 }
};
std::vector<Block const*> auditPalette(){
 J metadata;std::ifstream((terrainFolder()/"terrain.json"))>>metadata;std::vector<Block const*> result;
 for(auto const&entry:metadata["palette"]){std::string name;J states=J::object();
  if(entry.is_object()){name=entry.at("name");states=entry.at("states");}
  else {name=entry.get<std::string>();
   if(name=="kelp")states["kelp_age"]=25;
   if(name.ends_with("_leaves")){states["persistent_bit"]=true;states["update_bit"]=false;}
   if(name=="tall_grass_lower"||name=="tall_grass_upper"||name=="large_fern_lower"||name=="large_fern_upper"){states["upper_block_bit"]=name.ends_with("_upper");name=name.starts_with("tall_grass")?"tall_grass":"large_fern";}
   if(name=="oak_log_x"||name=="oak_log_z"||name=="spruce_log_x"||name=="spruce_log_z"){states["pillar_axis"]=name.ends_with("_x")?"x":"z";name=name.substr(0,name.size()-2);}
   if(name.starts_with("pointed_dripstone_up_")||name.starts_with("pointed_dripstone_down_")){bool down=name.starts_with("pointed_dripstone_down_");states["hanging"]=down;states["dripstone_thickness"]=name.substr(std::string(down?"pointed_dripstone_down_":"pointed_dripstone_up_").size());name="pointed_dripstone";}
   name="minecraft:"+name;
  }
  auto base=Block::tryGetFromRegistry(name);if(!base)throw std::runtime_error("Audit palette block missing: "+name);
  if(!states.empty()){auto parsed=CompoundTag::fromSnbt(states.dump());if(!parsed)throw std::runtime_error("Audit palette states invalid");auto tag=base->getSerializationId();tag["states"]=std::move(*parsed);base=Block::tryGetFromRegistry(tag);if(!base)throw std::runtime_error("Audit palette permutation missing: "+name+states.dump());}
  result.push_back(&*base);
 }return result;
}
int auditVolumeHeight(Dimension&dimension){
 auto const&range=dimension.getHeightRange();
 return terrain_runtime::BuildHeight::nativeVolumeHeight(range.mMin,range.mMax,field->generationHeight());
}
void auditFillChunk(LevelChunk&c,std::vector<uint8_t>const&bytes,std::vector<Block const*>const&palette,VegetationPair*vegetation){
 if(bytes.size()<260||bytes[0]!=75||bytes[1]!=87||bytes[2]!=50)throw std::runtime_error("Audit terrain encoding mismatch");
 int height=auditVolumeHeight(c.getDimension()),generatedHeight=field->generationHeight();
 std::vector<Block const*> data(16*16*height,palette[0]);BlockVolume volume({data.data(),data.data()+data.size()},16,height,16,*palette[0],-64);size_t offset=260;
 auto word=[&](){if(offset+2>bytes.size())throw std::runtime_error("Audit terrain truncated");unsigned v=bytes[offset]|(bytes[offset+1]<<8);offset+=2;return v;};
 for(int col=0;col<256;++col){unsigned count=word(),y=0;for(unsigned i=0;i<count;++i){unsigned n=word(),id=word();if(n==0||y+n>unsigned(generatedHeight)||id>=palette.size())throw std::runtime_error("Audit terrain run invalid");for(unsigned k=0;k<n;++k)data[volume.indexNoBoundsCheck(BlockPos{col%16,int(y+k),col/16})]=palette[id];y+=n;}if(y!=unsigned(generatedHeight))throw std::runtime_error("Audit terrain column incomplete");}
 if(offset!=bytes.size())throw std::runtime_error("Audit terrain trailing bytes");
 if(vegetation)filterVegetationVolume(volume,c.getPosition(),*vegetation);
 J metadata;std::ifstream((terrainFolder()/"terrain.json"))>>metadata;std::vector<Biome const*> biomes;for(std::string name:metadata["biomeNames"]){auto b=c.getDimension().getBiomeRegistry().lookupByName("minecraft:"+name);if(!b)throw std::runtime_error("Audit biome missing");biomes.push_back(b);}
 ChunkVolume::VolumeOf<Biome> biomeVolume;biomeVolume.mNumSubChunks=height/16;std::vector<Biome const*> sub(4096);for(int x=0;x<16;++x)for(int z=0;z<16;++z)for(int y=0;y<16;++y)sub[(x<<8)|(z<<4)|y]=biomes.at(bytes[4+z*16+x]);biomeVolume.mSubChunkVectors.assign(height/16,sub);c.setBiomeFromVolume(biomeVolume);
 c.setBlockVolume(volume,0);c.setPreWorldGenHeightMap(volume.computeHeightMap());c.recomputeHeightMap(true);
 // Match the production secondary-water layer without scheduling real ticks.
 offset=260;for(int col=0;col<256;++col){unsigned count=word(),y=0;for(unsigned i=0;i<count;++i){unsigned n=word(),id=word();if(id==49||id==50||(id>=56&&id<=60))for(unsigned k=0;k<n;++k)c.setExtraBlock(ChunkBlockPos{BlockPos{col%16,int(y+k)-64,col/16},-64},*palette[8],nullptr);y+=n;}}
}
// RuinedPortalPiece is a whole-feature native program, not a clipped template:
// it tests one owner chunk and also spreads terrain outside that chunk.
// Run it once against bounded seeded native chunks, then publish local deltas.
void fillPortalGround(LevelChunk&c,std::vector<Block const*>const&palette){
 // Native structural planning needs seeded geology and climate, not another
 // complete vegetation/ore/landscape decoration pass for 36 neighbours.
 int height=auditVolumeHeight(c.getDimension()),generatedHeight=field->generationHeight();
 std::vector<Block const*> data(16*height*16,palette[0]);BlockVolume volume({data.data(),data.data()+data.size()},16,height,16,*palette[0],-64);
 J metadata;std::ifstream((terrainFolder()/"terrain.json"))>>metadata;std::vector<Biome const*> biomes;for(std::string name:metadata["biomeNames"]){auto b=c.getDimension().getBiomeRegistry().lookupByName("minecraft:"+name);if(!b)throw std::runtime_error("Portal biome missing");biomes.push_back(&*b);}
 std::vector<Biome const*> sub(4096);auto cp=c.getPosition();
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){int wx=cp.x*16+x+32768,wz=cp.z*16+z+32768;auto s=field->surface(wx,wz);std::vector<uint16_t> column(generatedHeight);std::fill_n(column.begin(),4,uint16_t(1));field->materialColumn(s,wx,wz,generatedHeight-65,column.data());for(int y=0;y<generatedHeight;++y)data[volume.indexNoBoundsCheck(BlockPos{x,y,z})]=palette.at(column[y]);for(int y=0;y<16;++y)sub[(x<<8)|(z<<4)|y]=biomes.at(s.biome);}
 ChunkVolume::VolumeOf<Biome> biomeVolume;biomeVolume.mNumSubChunks=height/16;biomeVolume.mSubChunkVectors.assign(height/16,sub);c.setBiomeFromVolume(biomeVolume);c.setBlockVolume(volume,0);c.setPreWorldGenHeightMap(volume.computeHeightMap());c.recomputeHeightMap(true);
}
std::vector<Biome const*> nativeCaptureBiomes(Dimension&d){J metadata;std::ifstream((terrainFolder()/"terrain.json"))>>metadata;std::vector<Biome const*>result;for(std::string name:metadata["biomeNames"]){auto b=d.getBiomeRegistry().lookupByName("minecraft:"+name);if(!b)throw std::runtime_error("Native capture biome missing");result.push_back(&*b);}return result;}
void fillSharedCaptureGround(LevelChunk&c,std::vector<Block const*>const&palette,std::vector<Biome const*>const&biomes){
 int height=auditVolumeHeight(c.getDimension());
 auto cp=c.getPosition();auto ground=worldgen_rules::buildFrozenCaptureGround(*field,{cp.x,cp.z});std::vector<Block const*>data(16*height*16,palette[0]);BlockVolume volume({data.data(),data.data()+data.size()},16,height,16,*palette[0],-64);std::vector<Biome const*>sub(4096);
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){size_t col=z*16+x;for(int y=0;y<ground.height;++y)data[volume.indexNoBoundsCheck(BlockPos{x,y,z})]=palette.at(ground.blocks[col*ground.height+y]);for(int y=0;y<16;++y)sub[(x<<8)|(z<<4)|y]=biomes.at(ground.biomes[col]);}
 ChunkVolume::VolumeOf<Biome>biomeVolume;biomeVolume.mNumSubChunks=height/16;biomeVolume.mSubChunkVectors.assign(height/16,sub);c.setBiomeFromVolume(biomeVolume);c.setBlockVolume(volume,0);c.setPreWorldGenHeightMap(volume.computeHeightMap());c.recomputeHeightMap(true);
}
worldgen_rules::FrozenCaptureState nativeCaptureRead(BlockSource&blocks,worldgen_rules::StructurePoint p){BlockPos q{p.x,p.y,p.z};return {uintptr_t(&blocks.getBlock(q)),uintptr_t(&blocks.getExtraBlock(q))};}
bool nativeCaptureEqual(worldgen_rules::FrozenCaptureState a,worldgen_rules::FrozenCaptureState b){return *reinterpret_cast<Block const*>(a.primary)==*reinterpret_cast<Block const*>(b.primary)&&*reinterpret_cast<Block const*>(a.secondary)==*reinterpret_cast<Block const*>(b.secondary);}
std::string_view nativeCaptureName(uintptr_t handle){return reinterpret_cast<Block const*>(handle)->getTypeName();}
std::optional<worldgen_rules::FrozenCaptureLoot> nativeCaptureLoot(BlockSource&blocks,worldgen_rules::StructurePoint p,worldgen_rules::FrozenLootKind kind){
 auto*actor=blocks.getBlockEntity({p.x,p.y,p.z});if(!actor)return {};std::string table;uint32_t seed;
 if(kind==worldgen_rules::FrozenLootKind::Chest){auto*a=static_cast<ChestBlockActor*>(actor);table=*a->mLootTable;seed=a->mLootTableSeed;}else{auto*a=static_cast<BrushableBlockActor*>(actor);table=*a->mLootTable;seed=a->mLootTableSeed;}
 return worldgen_rules::FrozenCaptureLoot{{p,std::move(table),seed},kind};
}
bool publishNativeCapture(BlockSource&blocks,worldgen_rules::FrozenCaptureProgram const&program,BoundingBox const&clip){
 return worldgen_rules::executeFrozenCapture(program,sharedBox(clip),[&](auto p){return blocks.getBlock({p.x,p.y,p.z}).getTypeName()=="minecraft:bedrock";},[&](auto const&w){BlockPos p{w.position.x,w.position.y,w.position.z};auto const&primary=*reinterpret_cast<Block const*>(w.state.primary);auto const&secondary=*reinterpret_cast<Block const*>(w.state.secondary);if(blocks.getBlock(p)!=primary&&!blocks.setBlockNoUpdate(p,primary))return false;return blocks.getExtraBlock(p)==secondary||blocks.setExtraBlockSimple(p,secondary);},[&](auto const&l){auto const&a=l.assignment;auto*actor=blocks.getBlockEntity({a.position.x,a.position.y,a.position.z});if(!actor)throw std::runtime_error("Native captured fragment loot actor missing");if(l.kind==worldgen_rules::FrozenLootKind::Chest)static_cast<ChestBlockActor*>(actor)->setLootTable(a.table,a.seed);else static_cast<BrushableBlockActor*>(actor)->setLootTable(a.table,a.seed);});
}
void placeSharedFrozenPortal(Dimension&d,BlockSource&blocks,ChunkPos cp,ExtraPlan&plan,ChunkPos anchor,uint seed){
 if(!plan.portal){
  auto began=std::chrono::steady_clock::now();auto spec=worldgen_rules::planFrozenCapture(worldgen_rules::FrozenCaptureKind::Portal,{anchor.x,anchor.z},seed,sharedBox(*plan.start->boundingBox),plan.floor,plan.start->pieces->size(),field->generationHeight()-65);
  AuditChunkSource source(d,nativeBox(spec.chunks));auto tokens=source.access();BlockSource native(source,false,true);auto palette=auditPalette();auto biomes=nativeCaptureBiomes(d);
  worldgen_rules::forEachFrozenCaptureChunk(spec,[&](auto cp){fillSharedCaptureGround(*source.getExistingChunk({cp.x,cp.z}),palette,biomes);villageTerrain(native,{cp.x,cp.z},plan.terrain);});
  auto program=worldgen_rules::captureFrozenStructure(spec,[&](auto p){return nativeCaptureRead(native,p);},[&](size_t i,uint32_t partSeed,auto const&full){Random rng(partSeed,true);return (*plan.start->pieces)[i]->postProcess(native,rng,nativeBox(full));},nativeCaptureEqual,nativeCaptureName,[&](auto p,auto kind){return nativeCaptureLoot(native,p,kind);});
  plan.diagnostics["frozenPortal"]={{"chunks",spec.chunkCount},{"writes",program.writes},{"chests",program.chests},{"classifiedStates",program.classifiedStates},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}};
  plan.portal=std::make_shared<worldgen_rules::FrozenCaptureProgram>(std::move(program));
 }
 publishNativeCapture(blocks,*plan.portal,{{cp.x*16,-64,cp.z*16},{cp.x*16+15,field->generationHeight()-65,cp.z*16+15}});
}
void placeFrozenPortal(Dimension&d,BlockSource&blocks,ChunkPos cp,ExtraPlan&plan,ChunkPos anchor,uint seed){
 if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{}))){placeSharedFrozenPortal(d,blocks,cp,plan,anchor,seed);return;}
 if(!plan.portalReady){
  auto began=std::chrono::steady_clock::now();auto halo=*plan.start->boundingBox;halo.min.x-=16;halo.min.z-=16;halo.max.x+=16;halo.max.z+=16;
  int n=(chunk(halo.max.x)-chunk(halo.min.x)+1)*(chunk(halo.max.z)-chunk(halo.min.z)+1);if(n>64)throw std::runtime_error("Portal frozen footprint exceeds 64 chunks");
  AuditChunkSource source(d,halo);auto tokens=source.access();BlockSource native(source,false,true);auto palette=auditPalette();
  for(int z=chunk(halo.min.z);z<=chunk(halo.max.z);++z)for(int x=chunk(halo.min.x);x<=chunk(halo.max.x);++x){fillPortalGround(*source.getExistingChunk({x,z}),palette);villageTerrain(native,{x,z},plan.terrain);}
  halo.min.x=chunk(halo.min.x)*16;halo.min.z=chunk(halo.min.z)*16;halo.max.x=chunk(halo.max.x)*16+15;halo.max.z=chunk(halo.max.z)*16+15;
  halo.min.y=std::max(-58,plan.floor-24);halo.max.y=std::min(field->generationHeight()-65,plan.floor+48);
  std::vector<std::pair<Block const*,Block const*>> original;original.reserve((halo.max.x-halo.min.x+1)*(halo.max.z-halo.min.z+1)*(halo.max.y-halo.min.y+1));
  for(int z=halo.min.z;z<=halo.max.z;++z)for(int x=halo.min.x;x<=halo.max.x;++x)for(int y=halo.min.y;y<=halo.max.y;++y){BlockPos q{x,y,z};original.emplace_back(&native.getBlock(q),&native.getExtraBlock(q));}
  BoundingBox full=halo;full.min.y=-64;full.max.y=field->generationHeight()-65;
  for(size_t i=0;i<plan.start->pieces->size();++i){Random rng(seed,true);StructureFeature::setRandomSeedFor(rng,anchor.x,anchor.z,extraSpec(1)->salt+int(i),seed);if(!(*plan.start->pieces)[i]->postProcess(native,rng,full))throw std::runtime_error("Native whole portal placement refused seeded ground");}
  size_t offset=0;unsigned frames=0;
  for(int z=halo.min.z;z<=halo.max.z;++z)for(int x=halo.min.x;x<=halo.max.x;++x)for(int y=halo.min.y;y<=halo.max.y;++y){BlockPos q{x,y,z};auto const&a=native.getBlock(q);auto const&extra=native.getExtraBlock(q);auto before=original.at(offset++);if(a!=*before.first||extra!=*before.second)plan.portalWrites[{x,y,z}]={&a,&extra};if(a.getTypeName()=="minecraft:obsidian"||a.getTypeName()=="minecraft:crying_obsidian")++frames;if(a.getTypeName()=="minecraft:chest"){auto*actor=native.getBlockEntity(q);if(!actor)throw std::runtime_error("Native portal chest actor missing");auto*chest=static_cast<ChestBlockActor*>(actor);plan.portalChests[{x,y,z}]={*chest->mLootTable,chest->mLootTableSeed};}}
  if(frames<4||plan.portalChests.empty())throw std::runtime_error("Native portal is missing frame or original loot chest");
  plan.portalReady=true;plan.diagnostics["frozenPortal"]={{"chunks",n},{"writes",plan.portalWrites.size()},{"chests",plan.portalChests.size()},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}};
 }
 for(auto const&[p,states]:plan.portalWrites)if(chunk(p[0])==cp.x&&chunk(p[2])==cp.z){BlockPos q{p[0],p[1],p[2]};if(blocks.getBlock(q).getTypeName()=="minecraft:bedrock")throw std::runtime_error("Portal fragment would replace bedrock");blocks.setBlockNoUpdate(q,*states.first);blocks.setExtraBlockSimple(q,*states.second);}
 for(auto const&[p,loot]:plan.portalChests)if(chunk(p[0])==cp.x&&chunk(p[2])==cp.z){auto*actor=blocks.getBlockEntity({p[0],p[1],p[2]});if(!actor)throw std::runtime_error("Portal fragment chest actor missing");static_cast<ChestBlockActor*>(actor)->setLootTable(loot.first,loot.second);}
}
bool placeSharedFrozenTemple(BlockSource&blocks,AutoBlueprint&plan,BoundingBox const&clip){
 if(!plan.temple){
  auto&d=blocks.getDimension();auto cp=ChunkPos{plan.start->chunkX,plan.start->chunkZ};uint seed=structureSeed(d);auto began=std::chrono::steady_clock::now();
  auto spec=worldgen_rules::planFrozenCapture(worldgen_rules::FrozenCaptureKind::Pyramid,{cp.x,cp.z},seed,sharedBox(*plan.start->boundingBox),0,plan.start->pieces->size(),field->generationHeight()-65);
  AuditChunkSource source(d,nativeBox(spec.chunks));auto tokens=source.access();BlockSource native(source,false,true);auto palette=auditPalette();auto biomes=nativeCaptureBiomes(d);
  worldgen_rules::forEachFrozenCaptureChunk(spec,[&](auto cp){fillSharedCaptureGround(*source.getExistingChunk({cp.x,cp.z}),palette,biomes);});
  auto biome=d.getBiomeRegistry().lookupByName("minecraft:desert");FixedBiomeSource biomeSource(*biome);Start whole(biomeSource,field->seaLevel(),cp.x,cp.z,provider);int floor=static_cast<ScatteredFeaturePiece*>((*plan.start->pieces)[0].get())->mHeightPosition;
  whole.moveBoundingBoxes(floor-whole.boundingBox->min.y);for(auto&piece:*whole.pieces)static_cast<ScatteredFeaturePiece*>(piece.get())->mHeightPosition=floor;whole.calculateBoundingBox();
  auto program=worldgen_rules::captureFrozenStructure(spec,[&](auto p){return nativeCaptureRead(native,p);},[&](size_t,uint32_t nativeSeed,auto const&full){Random rng(nativeSeed,true);return whole.postProcess(native,rng,nativeBox(full));},nativeCaptureEqual,nativeCaptureName,[&](auto p,auto kind){return nativeCaptureLoot(native,p,kind);});
  if(!auditPlanningStarts)std::ofstream(folder/"temple-plan-metrics.jsonl",std::ios::app)<<J{{"anchor",{cp.x*16,cp.z*16}},{"writes",program.writes},{"chests",program.chests},{"brushable",program.brushable},{"classifiedStates",program.classifiedStates},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}}.dump()<<'\n';
  plan.temple=std::make_shared<worldgen_rules::FrozenCaptureProgram>(std::move(program));
 }
 publishNativeCapture(blocks,*plan.temple,clip);return true;
}
bool placeFrozenTemple(BlockSource&blocks,AutoBlueprint&plan,Random&,BoundingBox const&clip){
 if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{})))return placeSharedFrozenTemple(blocks,plan,clip);
 if(!plan.templeReady){
  auto&d=blocks.getDimension();auto cp=ChunkPos{plan.start->chunkX,plan.start->chunkZ};auto b=*plan.start->boundingBox;auto halo=b;halo.min.x-=8;halo.min.z-=8;halo.max.x+=8;halo.max.z+=8;
  auto began=std::chrono::steady_clock::now();AuditChunkSource source(d,halo);auto tokens=source.access();BlockSource native(source,false,true);auto palette=auditPalette();
  for(int z=chunk(halo.min.z);z<=chunk(halo.max.z);++z)for(int x=chunk(halo.min.x);x<=chunk(halo.max.x);++x)fillPortalGround(*source.getExistingChunk({x,z}),palette);
  halo.min.x=chunk(halo.min.x)*16;halo.min.z=chunk(halo.min.z)*16;halo.max.x=chunk(halo.max.x)*16+15;halo.max.z=chunk(halo.max.z)*16+15;halo.min.y=std::max(-58,b.min.y-24);halo.max.y=std::min(field->generationHeight()-65,b.max.y+8);
  std::vector<std::pair<Block const*,Block const*>> before;for(int z=halo.min.z;z<=halo.max.z;++z)for(int x=halo.min.x;x<=halo.max.x;++x)for(int y=halo.min.y;y<=halo.max.y;++y){BlockPos p{x,y,z};before.emplace_back(&native.getBlock(p),&native.getExtraBlock(p));}
  auto biome=d.getBiomeRegistry().lookupByName("minecraft:desert");FixedBiomeSource biomes(*biome);Start whole(biomes,field->seaLevel(),cp.x,cp.z,provider);int floor=static_cast<ScatteredFeaturePiece*>((*plan.start->pieces)[0].get())->mHeightPosition;whole.moveBoundingBoxes(floor-whole.boundingBox->min.y);for(auto&piece:*whole.pieces)static_cast<ScatteredFeaturePiece*>(piece.get())->mHeightPosition=floor;whole.calculateBoundingBox();
  Random random(structureSeed(d),true);StructureFeature::setRandomSeedFor(random,cp.x,cp.z,14357617,structureSeed(d));auto full=halo;full.min.y=-64;full.max.y=field->generationHeight()-65;if(!whole.postProcess(native,random,full))throw std::runtime_error("Whole native pyramid placement refused");
  size_t offset=0;unsigned chests=0,brushable=0;for(int z=halo.min.z;z<=halo.max.z;++z)for(int x=halo.min.x;x<=halo.max.x;++x)for(int y=halo.min.y;y<=halo.max.y;++y){BlockPos p{x,y,z};auto const&block=native.getBlock(p);auto const&extra=native.getExtraBlock(p);auto old=before.at(offset++);if(block!=*old.first||extra!=*old.second)plan.templeWrites[{x,y,z}]={&block,&extra};if(block.getTypeName()=="minecraft:chest"||block.getTypeName()=="minecraft:suspicious_sand"){auto*actor=native.getBlockEntity(p);if(!actor)throw std::runtime_error("Pyramid native loot actor missing");if(block.getTypeName()=="minecraft:chest"){auto*a=static_cast<ChestBlockActor*>(actor);plan.templeLoot[{x,y,z}]={*a->mLootTable,a->mLootTableSeed};++chests;}else{auto*a=static_cast<BrushableBlockActor*>(actor);plan.templeLoot[{x,y,z}]={*a->mLootTable,a->mLootTableSeed};++brushable;}}}
  if(chests!=4||!brushable)throw std::runtime_error("Pyramid trap/archaeology loot is incomplete");plan.templeReady=true;
  if(!auditPlanningStarts)std::ofstream(folder/"temple-plan-metrics.jsonl",std::ios::app)<<J{{"anchor",{cp.x*16,cp.z*16}},{"writes",plan.templeWrites.size()},{"chests",chests},{"brushable",brushable},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}}.dump()<<'\n';
 }
 for(auto const&[p,states]:plan.templeWrites)if(p[0]>=clip.min.x&&p[0]<=clip.max.x&&p[2]>=clip.min.z&&p[2]<=clip.max.z){BlockPos q{p[0],p[1],p[2]};if(blocks.getBlock(q).getTypeName()=="minecraft:bedrock")throw std::runtime_error("Temple fragment would replace bedrock");blocks.setBlockNoUpdate(q,*states.first);blocks.setExtraBlockSimple(q,*states.second);}
 for(auto const&[p,loot]:plan.templeLoot)if(p[0]>=clip.min.x&&p[0]<=clip.max.x&&p[2]>=clip.min.z&&p[2]<=clip.max.z){BlockPos q{p[0],p[1],p[2]};auto*actor=blocks.getBlockEntity(q);if(!actor)throw std::runtime_error("Temple fragment loot actor missing");if(blocks.getBlock(q).getTypeName()=="minecraft:chest")static_cast<ChestBlockActor*>(actor)->setLootTable(loot.first,loot.second);else static_cast<BrushableBlockActor*>(actor)->setLootTable(loot.first,loot.second);}
 return true;
}
void memoryTempleAudit(Dimension&d,int order){
 if(order<0||order>2)throw std::runtime_error("Audit order must be 0 forward, 1 reverse, 2 whole");
 bool human=false;d.getLevel().forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human||interactionAuditActive)throw std::runtime_error("Memory audit requires empty session");std::lock_guard guard(stageMutex);
 J data=input;uint seed=structureSeed(d);AuditPlanScope scope(d,data,seed);auto&plan=autoBlueprint(d,{chunk(-15728),chunk(-15328)},seed);if(!plan.start)throw std::runtime_error("Temple sample rejected");auto box=*plan.start->boundingBox;auto halo=box;halo.min.x-=16;halo.min.z-=16;halo.max.x+=16;halo.max.z+=16;AuditChunkSource source(d,halo);auto tokens=source.access();BlockSource blocks(source,false,true);auto palette=auditPalette();nativegen::Engine engine(input,{},false,field.get());
 for(int z=chunk(halo.min.z);z<=chunk(halo.max.z);++z)for(int x=chunk(halo.min.x);x<=chunk(halo.max.x);++x)auditFillChunk(*source.getExistingChunk({x,z}),engine.chunk(x,z),palette,nullptr);
 std::vector<ChunkPos> chunks;for(int z=chunk(box.min.z);z<=chunk(box.max.z);++z)for(int x=chunk(box.min.x);x<=chunk(box.max.x);++x)chunks.push_back({x,z});if(order==1)std::reverse(chunks.begin(),chunks.end());
 if(order==2){Random rng(seed,true);plan.start->postProcess(blocks,rng,box);}else for(auto cp:chunks){Random rng(seed,true);StructureFeature::setRandomSeedFor(rng,cp.x,cp.z,14357617,seed);plan.start->postProcess(blocks,rng,{{cp.x*16,-64,cp.z*16},{cp.x*16+15,field->generationHeight()-65,cp.z*16+15}});}
 box.min.y=std::max(-58,box.min.y-16);box.max.y=std::min(field->generationHeight()-65,box.max.y+8);
 uint64_t hash=1469598103934665603ull;J actors=J::array(),counts=J::object(),invalid=J::array();unsigned invalidTotal=0;for(int z=box.min.z;z<=box.max.z;++z)for(int x=box.min.x;x<=box.max.x;++x)for(int y=box.min.y;y<=box.max.y;++y){BlockPos p{x,y,z};auto const&b=blocks.getBlock(p);uint64_t h=b.mSerializationIdHash;for(int s=0;s<64;s+=8)hash=(hash^((h>>s)&255))*1099511628211ull;counts[b.getTypeName()]=counts.value(b.getTypeName(),0)+1;auto const&name=b.getTypeName();bool bad=(groundPlant(name)||name=="minecraft:sculk_vein")&&!b.canSurvive(blocks,p);bad|=name=="minecraft:hay_block"&&blocks.getBlock({x,y-1,z}).isAir();if(bad){++invalidTotal;if(invalid.size()<100)invalid.push_back({{"pos",{x,y,z}},{"name",name}});}if(auto*a=blocks.getBlockEntity(p)){CompoundTag tag;SaveContext context;a->save(tag,context);actors.push_back(J::parse(tag.toSnbt(SnbtFormat::Jsonify),nullptr,false,true));}}
 std::ofstream(folder/"memory-temple-audit.json")<<J{{"order",order},{"box",boxJson(box)},{"stateHash",std::to_string(hash)},{"counts",counts},{"blockActors",actors},{"chunks",chunks.size()}}.dump(2);
}

struct AuditInput {J data;uint seed;std::string worldSeed;};
AuditInput loadAuditInput(Dimension&d,int index){
 if(index==0)return {input,structureSeed(d),d.getLevel().getLevelData().getSeed().toUiString()};
 J profiles;std::ifstream(folder/"audit-inputs.json")>>profiles;auto const&r=profiles.at("profiles").at(index-1);J data;auto pathText=r.at("path").get<std::string>();std::ifstream f(std::filesystem::path(std::u8string(pathText.begin(),pathText.end())));f>>data;
 if(data.at("version")!=input.at("version")||!data.at("seedManifest").value("ready",false))throw std::runtime_error("Audit input not a current complete seed package");
 std::string ui=r.at("worldSeed");LevelSeed64 native;native.mValue=uint64_t(std::stoll(ui));return {std::move(data),native.to32BitRandomSeed(),ui};
}
void auditSessionGuard(Dimension&d){bool human=false;d.getLevel().forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human||interactionAuditActive)throw std::runtime_error("Audit requires empty session");}
StructureStart* auditPlan(Dimension&d,int type,ChunkPos cp,uint seed){if(type==12)return villagePlan(d,cp,seed).start;if(type==13)return autoBlueprint(d,cp,seed).start;if(type<0||type>11)throw std::runtime_error("Audit family must be 0..13");return extraPlan(d,type,cp).start;}
void surveyAuditPlans(Dimension&d,int profile,int type,int offset,int count){
 if(count<1||count>64||offset<0||type<0||type>13)throw std::runtime_error("Audit survey batch outside bounds");auditSessionGuard(d);std::lock_guard guard(stageMutex);auto data=loadAuditInput(d,profile);AuditPlanScope scope(d,data.data,data.seed);J rows=J::array();int spacing=type==12?villageSpacing():type==13?32:extraSpec(type)->spacing;
 int low=spacingCell(-terrain_bounds::HalfChunks,spacing),high=spacingCell(terrain_bounds::HalfChunks-1,spacing),side=high-low+1,total=side*side;
 for(int i=offset;i<std::min(offset+count,total);++i){int x=low+i%side,z=low+i/side;ChunkPos cp=type==12?villageCandidate(x,z,data.seed):type==13?nativeCandidateForCell(x,z,data.seed):extraCandidate(*extraSpec(type),x,z,data.seed);auto*p=auditPlan(d,type,cp,data.seed);if(!p)continue;auto surface=field->surface(cp.x*16+32768,cp.z*16+32768);rows.push_back({{"family",p->getStructureName()},{"anchor",{cp.x*16,cp.z*16}},{"box",boxJson(*p->boundingBox)},{"biome",surface.biome},{"pieces",p->pieces->size()}});}
 std::ofstream(folder/"audit-survey.json")<<J{{"profile",profile},{"type",type},{"worldSeed",data.worldSeed},{"nativeSeed",data.seed},{"offset",offset},{"count",count},{"total",total},{"rows",rows}}.dump(2);
}
J readAuditVolume(BlockSource&blocks,BoundingBox box){
 uint64_t hash=1469598103934665603ull;J actors=J::array(),counts=J::object(),states=J::object(),invalid=J::array();unsigned invalidTotal=0;std::ofstream raw(folder/"memory-plan-audit.bin",std::ios::binary);
 for(int z=box.min.z;z<=box.max.z;++z)for(int x=box.min.x;x<=box.max.x;++x)for(int y=box.min.y;y<=box.max.y;++y){BlockPos p{x,y,z};auto const&b=blocks.getBlock(p);for(auto const*state:{&b,&blocks.getExtraBlock(p)}){uint64_t h=state->mSerializationIdHash;auto key=std::to_string(h);if(!states.contains(key))states[key]=J::parse(state->mSerializationId->toSnbt(SnbtFormat::Jsonify),nullptr,false,true);raw.write(reinterpret_cast<char const*>(&h),8);for(int shift=0;shift<64;shift+=8)hash=(hash^((h>>shift)&255))*1099511628211ull;}counts[b.getTypeName()]=counts.value(b.getTypeName(),0)+1;auto const&name=b.getTypeName();bool bad=(groundPlant(name)||name=="minecraft:sculk_vein")&&!b.canSurvive(blocks,p);bad|=name=="minecraft:hay_block"&&blocks.getBlock({x,y-1,z}).isAir();if(bad){++invalidTotal;if(invalid.size()<100)invalid.push_back({{"pos",{x,y,z}},{"name",name}});}if(auto*a=blocks.getBlockEntity(p)){CompoundTag tag;SaveContext context;if(!a->save(tag,context))throw std::runtime_error("Audit block actor save failed");actors.push_back(J::parse(tag.toSnbt(SnbtFormat::Jsonify),nullptr,false,true));}}
 if(!raw)throw std::runtime_error("Audit raw volume write failed");
 return {{"box",boxJson(box)},{"stateHash",std::to_string(hash)},{"counts",counts},{"blockActors",actors},{"states",states},{"invalidPlacementCount",invalidTotal},{"invalidPlacements",invalid}};
}
void runMemoryPlanAudit(Dimension&d,int profile,int type,int x,int z,int order){
 if(order<0||order>1)throw std::runtime_error("Memory plan order must be forward/reverse");auditSessionGuard(d);std::lock_guard guard(stageMutex);auto data=loadAuditInput(d,profile);AuditPlanScope scope(d,data.data,data.seed);ChunkPos anchor{chunk(x),chunk(z)};auto*start=auditPlan(d,type,anchor,data.seed);if(!start)throw std::runtime_error("Audit plan rejected actual terrain");auto box=*start->boundingBox;auto halo=box;halo.min.x-=32;halo.min.z-=32;halo.max.x+=32;halo.max.z+=32;
 auto began=std::chrono::steady_clock::now();AuditChunkSource source(d,halo);auto tokens=source.access();BlockSource blocks(source,false,true);auto palette=auditPalette();nativegen::Engine engine(input,{},false,field.get());VegetationPair*reservation=nullptr;
 if(type==12)reservation=&villageVegetationPair(villages.at({anchor.x,anchor.z}));else if(type<12&&extraSurface(type))reservation=&extraVegetationPair(extraPlans.at({type,anchor.x,anchor.z}));else if(type==13&&start->getStructureName()!="minecraft:desert_pyramid")reservation=&structureVegetationPair(automaticBlueprints.at({anchor.x,anchor.z}));
 for(int zz=chunk(halo.min.z);zz<=chunk(halo.max.z);++zz)for(int xx=chunk(halo.min.x);xx<=chunk(halo.max.x);++xx)auditFillChunk(*source.getExistingChunk({xx,zz}),engine.chunk(xx,zz),palette,reservation);
 std::set<int64_t> originalActors;for(auto*a:d.getLevel().getRuntimeActorList())originalActors.insert(a->getOrCreateUniqueID().rawID);
 struct ActorCleanup{Level&level;std::set<int64_t> const&baseline;~ActorCleanup(){std::vector<Actor*>drop;for(auto*a:level.getRuntimeActorList())if(!baseline.contains(a->getOrCreateUniqueID().rawID))drop.push_back(a);for(auto*a:drop)a->remove();}} actorCleanup{d.getLevel(),originalActors};
 std::vector<ChunkPos> orderChunks;for(int zz=chunk(halo.min.z);zz<=chunk(halo.max.z);++zz)for(int xx=chunk(halo.min.x);xx<=chunk(halo.max.x);++xx)orderChunks.push_back({xx,zz});if(order)std::reverse(orderChunks.begin(),orderChunks.end());
 for(auto cp:orderChunks){BoundingBox clip{{cp.x*16,-64,cp.z*16},{cp.x*16+15,field->generationHeight()-65,cp.z*16+15}};if(type==12)placeVillage(blocks,cp,villages.at({anchor.x,anchor.z}),data.seed);else if(type<12)placeExtras(d,blocks,cp,{{type,anchor}},data.seed,true);else {auto&plan=automaticBlueprints.at({anchor.x,anchor.z});Random rng(data.seed,true);StructureFeature::setRandomSeedFor(rng,cp.x,cp.z,14357617,data.seed);if(plan.igloo)placeIgloo(blocks,*plan.igloo,clip);else if(!(box.max.x<clip.min.x||box.min.x>clip.max.x||box.max.z<clip.min.z||box.min.z>clip.max.z))placeScatteredNative(blocks,plan,rng,clip);}}
 auto sample=halo;sample.min.y=std::max(-58,box.min.y-20);sample.max.y=std::min(field->generationHeight()-65,box.max.y+12);J result=readAuditVolume(blocks,sample);result.update(J{{"profile",profile},{"family",start->getStructureName()},{"type",type},{"anchor",{x,z}},{"worldSeed",data.worldSeed},{"nativeSeed",data.seed},{"order",order},{"chunks",orderChunks.size()},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}});if(type==8)result["cityConnectionSpans"]=cityConnections(*start,data.seed,cityNeighbours(d,*start,data.seed)).size();if(type==12){auto const&v=villages.at({anchor.x,anchor.z});result["villageDiagnostics"]=v.diagnostics;result["oasisColumns"]=v.oasis.size();}
 if(type<12)result["planDiagnostics"]=extraPlans.at({type,anchor.x,anchor.z}).diagnostics;std::ofstream(folder/"memory-plan-audit.json")<<result.dump(2);
}
