// Reuse original small-feature definitions instead of approximate templates.
// Discovery is console-only; generation never scans the feature registry.
void nativeFeatureCatalog(Dimension& d){
 J rows=J::array();
 bedrock_compat::forEachFeature(bedrock_compat::level(d).getFeatureRegistry(),[&](HashedString const&name,WeakRef<IFeature> weak){
  if(!weak.mRegistry->has_value())return;
  auto& entries=*weak.mRegistry->value().get().mFeatureRegistry;
  auto index=uint64_t(weak.mIndex);if(index>=entries.size()||!entries[index])return;
  auto* ptr=entries[index].get();auto vtable=*reinterpret_cast<void***>(ptr);std::string kind;
  auto nativeTable=[](char const*symbol){return ll::memory::SymbolView(symbol).resolve(true);};
  if(vtable==nativeTable("??_7MonsterRoomFeature@@6B@"))kind="monster_room";
  else if(vtable==nativeTable("??_7DesertWellFeature@@6B@"))kind="desert_well";
  else if(vtable==nativeTable("??_7FossilFeature@@6B@"))kind="fossil";
  rows.push_back({{"name",name.getString()},{"nativeKind",kind},{"internal",bool(ptr->mInternal)}});
 });
 std::ofstream(folder/"small-feature-catalog.json")<<rows.dump(2);
}

bool smallFeaturesEnabled=false;
std::map<std::string,IFeature*> nativeSmallDefinitions;
std::set<std::pair<int,int>> completedSmallChunks;
std::set<std::pair<int,int>> recentSmallChunks;
std::deque<std::pair<int,int>> recentSmallOrder;
void initializeSmallHistory(){
 completedSmallChunks.clear();recentSmallChunks.clear();recentSmallOrder.clear();std::ifstream f(worldFolder()/"karma-native-small-features.jsonl");std::string line;
 while(std::getline(f,line)){auto r=J::parse(line,nullptr,false);if(r.is_discarded())continue;if(r.value("world",std::string{})!=activeWorld)throw std::runtime_error("Small-feature history world mismatch");auto const&cp=r.at("chunk");completedSmallChunks.emplace(cp[0].get<int>(),cp[1].get<int>());}
}
std::atomic_uint64_t smallAttempts=0,smallPlaced=0,smallNanos=0,smallRejectedWrites=0;
std::atomic_uint64_t smallProbeNanos=0,smallProbeCalls=0;
uint64_t smallSelector(ChunkPos cp,uint seed){return worldgen_rules::smallFeatureSelector({cp.x,cp.z},seed);}
bool smallPotential(ChunkPos cp,uint seed){return smallFeaturesEnabled&&worldgen_rules::smallFeaturePotential({cp.x,cp.z},seed);}
IFeature& smallDefinition(Level& level,std::string const& name){
 if(auto it=nativeSmallDefinitions.find(name);it!=nativeSmallDefinitions.end())return *it->second;
 auto weak=level.getFeatureRegistry().lookupByName(name);
 if(!weak.mRegistry->has_value())throw std::runtime_error("Native small feature missing: "+name);
 auto&entries=*weak.mRegistry->value().get().mFeatureRegistry;auto i=uint64_t(weak.mIndex);
 if(i>=entries.size()||!entries[i])throw std::runtime_error("Native small feature registry index invalid");
 auto* p=entries[i].get();if(!bedrock_compat::featureNameMatches(level.getFeatureRegistry(),name,i,p))throw std::runtime_error("Native small feature name/index mismatch");nativeSmallDefinitions.emplace(name,p);return *p;
}
struct SmallWriteScope {
 BoundingBox clip;BlockSource* source;std::map<std::array<int,3>,Block const*> before;unsigned rejected=0;J rejectedPos=J::array();
};
thread_local SmallWriteScope* smallWriteScope=nullptr;
// A native fossil is planned without mutating its neighbourhood. All final
// native writes (including rotation, damage and ore replacement) are frozen.
struct FossilCapture {BlockSource* source;std::map<std::array<int,3>,Block const*> writes;bool rejected=false;};
thread_local FossilCapture* fossilCapture=nullptr;
bool captureFossilWrite(BlockSource* source,BlockPos const&p,Block const&block){
 auto*s=fossilCapture;if(!s||s->source!=source)return false;
 if(p.y<-58||p.y>(field->generationHeight()-65)||!structureBoxInside(p.x,p.z,p.x,p.z))s->rejected=true;
 else if(block.getTypeName()!="minecraft:air")s->writes[{p.x,p.y,p.z}]=&block;
 return true;
}
bool captureSmallWrite(BlockSource* source,BlockPos const&p){
 if(outsidePortalFragment(source,p))return false;
 auto*s=smallWriteScope;if(!s||s->source!=source)return true;
 auto const&b=s->clip;
 if(!worldgen_rules::smallFeatureWriteOwned({chunk(b.min.x),chunk(b.min.z)},{p.x,p.y,p.z},false,b.max.y)||source->getBlock(p).getTypeName()=="minecraft:bedrock"){++s->rejected;++smallRejectedWrites;if(s->rejectedPos.size()<8)s->rejectedPos.push_back({p.x,p.y,p.z});return false;}
 s->before.try_emplace({p.x,p.y,p.z},&source->getBlock(p));return true;
}
LL_TYPE_INSTANCE_HOOK(SmallWrite,ll::memory::HookPriority::Normal,BlockSource,&BlockSource::$setBlock,bool,BlockPos const&p,Block const&block,int flags,ActorBlockSyncMessage const*sync,BlockChangeContext const&context){if(captureFossilWrite(this,p,block))return true;if(!captureSmallWrite(this,p))return false;bool done=origin(p,block,flags,sync,context);if(done)rememberStructureWall(p,block);return done;}
#ifndef SHANHAI_MC_26_51
LL_TYPE_INSTANCE_HOOK(SmallWriteSimple,ll::memory::HookPriority::Normal,BlockSource,&BlockSource::setBlockNoUpdate,bool,BlockPos const&p,Block const&block){if(captureFossilWrite(this,p,block))return true;if(!captureSmallWrite(this,p))return false;bool done=origin(p,block);if(done)rememberStructureWall(p,block);return done;}
LL_TYPE_INSTANCE_HOOK(SmallWriteFast,ll::memory::HookPriority::Normal,BlockSource,&BlockSource::setBlockSimple,bool,BlockPos const&p,Block const&block){if(captureFossilWrite(this,p,block))return true;if(!captureSmallWrite(this,p))return false;bool done=origin(p,block);if(done)rememberStructureWall(p,block);return done;}
#else
// The source's simple setter is now inlined. Catch the public target boundary
// before a missing chunk can reject a fossil capture, and the surviving chunk
// setter for native structure paths that do not use WorldBlockTarget.
LL_TYPE_INSTANCE_HOOK(SmallWriteFast,ll::memory::HookPriority::Normal,WorldBlockTarget,&WorldBlockTarget::$setBlockSimple,bool,BlockPos const&p,Block const&block){auto*source=&mBlockSource;if(captureFossilWrite(source,p,block))return true;if(!captureSmallWrite(source,p))return false;bool done=origin(p,block);if(done)rememberStructureWall(p,block);return done;}
BlockSource* nativeSimpleSource(){return fossilCapture?fossilCapture->source:smallWriteScope?smallWriteScope->source:portalWriteSource;}
BlockPos nativeSimplePosition(LevelChunk& chunk,ChunkBlockPos const& p){auto cp=*chunk.mPosition;return {cp.x*16+int(p.x),int(chunk.mMin->y)+int(p.y.mVal),cp.z*16+int(p.z)};}
bool captureNativeSimpleWrite(LevelChunk& c,ChunkBlockPos const& p,Block const& b){
 auto* source=nativeSimpleSource();if(!source)return false;auto world=nativeSimplePosition(c,p);
 if(source->getChunkAt(world)!=&c)return false;
 return captureFossilWrite(source,world,b)||!captureSmallWrite(source,world);
}
void rememberNativeSimpleWrite(LevelChunk& c,ChunkBlockPos const& p,Block const& b){if(nativeSimpleSource())rememberStructureWall(nativeSimplePosition(c,p),b);}
#endif
class SmallTarget final:public WorldBlockTarget {
public:
 SmallTarget(BlockSource&b,WorldGenContext const&c):WorldBlockTarget(b,c){}
 short getHeightmap(int x,int z)override{return short(field->surface(x+32768,z+32768).height+1);}
};
bool callSmallFeature(Dimension& d,BlockSource&blocks,ChunkPos cp,std::string const&kind,BlockPos pos,Random&rng){
 SmallWriteScope scope{{{cp.x*16,-58,cp.z*16},{cp.x*16+15,(field->generationHeight()-65),cp.z*16+15}},&blocks};
 smallWriteScope=&scope;struct Reset{~Reset(){smallWriteScope=nullptr;}}reset;
 auto began=std::chrono::steady_clock::now();++smallAttempts;bool placed=false;
 if(kind=="monster_room"){MonsterRoomFeature room;placed=room.$place(blocks,pos,rng);
  static unsigned diagnostics=0;if(!placed&&diagnostics++<8){J rings=J::array();for(int radius:{3,4}){int floor=0,roof3=0,roof4=0,doors=0;for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx){floor+=blocks.isSolidBlockingBlock({pos.x+dx,pos.y-1,pos.z+dz});roof3+=blocks.isSolidBlockingBlock({pos.x+dx,pos.y+3,pos.z+dz});roof4+=blocks.isSolidBlockingBlock({pos.x+dx,pos.y+4,pos.z+dz});if(std::abs(dx)==radius||std::abs(dz)==radius)doors+=blocks.getBlock({pos.x+dx,pos.y,pos.z+dz}).getTypeName()=="minecraft:air"&&blocks.getBlock({pos.x+dx,pos.y+1,pos.z+dz}).getTypeName()=="minecraft:air";}rings.push_back({{"radius",radius},{"floor",floor},{"roof3",roof3},{"roof4",roof4},{"doors",doors}});}std::ofstream(folder/"dungeon-fit-diagnostics.jsonl",std::ios::app)<<J{{"pos",{pos.x,pos.y,pos.z}},{"rings",rings}}.dump()<<'\n';}
 }
 else {
  WorldGenContext worldContext{};worldContext.mPreliminarySurfaceProvider=&provider;SmallTarget target(blocks,worldContext);RenderParams params;params.mLevel=&bedrock_compat::level(d);params.mBlockSource=&blocks;params.mBlockPlacementTarget=&target;
  FeaturePlacementFailureLog logger;
#ifdef SHANHAI_MC_26_51
  FeaturePlacementFailureLogProxy proxy{&logger};
#else
  FeaturePlacementFailureLogProxy proxy(logger);
#endif

  struct Context {IBlockWorldGenAPI& target;BlockPos pos;Random& random;RenderParams& params;FeaturePlacementFailureLogProxy logger;};
  static_assert(sizeof(Context)==sizeof(IFeature::PlacementContext));static_assert(offsetof(Context,logger)==offsetof(IFeature::PlacementContext,mLogger));
  Context storage{target,pos,rng,params,std::move(proxy)};auto&def=smallDefinition(bedrock_compat::level(d),"minecraft:"+kind+"_feature");
  placed=def.place(reinterpret_cast<IFeature::PlacementContext const&>(storage)).has_value();
 }
 if(placed&&!scope.rejected&&kind=="desert_well"){
  auto air=Block::tryGetFromRegistry("minecraft:air");for(int z=pos.z-3;z<=pos.z+3;++z)for(int x=pos.x-3;x<=pos.x+3;++x)for(int y=pos.y-2;y<=pos.y+6;++y){BlockPos p{x,y,z};auto const&v=blocks.getBlock(p);if((groundPlant(v.getTypeName())||v.getTypeName()=="minecraft:cactus")&&!v.canSurvive(blocks,p))bedrock_compat::setBlockNoUpdate(blocks,p,*air);}
 }
 smallWriteScope=nullptr;
 double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();smallNanos+=uint64_t(ms*1e6);
 if(scope.rejected||!placed){for(auto const&[p,old]:scope.before)bedrock_compat::setBlockNoUpdate(blocks,{p[0],p[1],p[2]},*old);if(scope.rejected)std::ofstream(folder/"small-feature-rejections.jsonl",std::ios::app)<<J{{"kind",kind},{"pos",{pos.x,pos.y,pos.z}},{"rejectedWrites",scope.rejected},{"firstRejected",scope.rejectedPos},{"rolledBack",scope.before.size()}}.dump()<<'\n';return false;}
 if(scope.before.empty())return false;
 ++smallPlaced;auto ch=blocks.getChunkAt(pos);bedrock_compat::requestSave(*ch);
 BoundingBox written{{32000,(field->generationHeight()-64),32000},{-32000,-64,-32000}};
 for(auto const&[p,old]:scope.before){written.min.x=std::min(written.min.x,p[0]);written.min.y=std::min(written.min.y,p[1]);written.min.z=std::min(written.min.z,p[2]);written.max.x=std::max(written.max.x,p[0]);written.max.y=std::max(written.max.y,p[1]);written.max.z=std::max(written.max.z,p[2]);}
 J row{{"world",activeWorld},{"kind",kind},{"pos",{pos.x,pos.y,pos.z}},{"chunk",{cp.x,cp.z}},{"box",boxJson(written)},{"writes",scope.before.size()},{"ms",ms}};
 auto path=worldFolder()/"karma-native-small-features.jsonl";std::ofstream history(path,std::ios::app);history<<row.dump()<<'\n';history.flush();if(!history)throw std::runtime_error("Cannot persist small-feature history");completedSmallChunks.emplace(cp.x,cp.z);return true;
}
bool fitsSmall(BoundingBox const&b,std::vector<BoundingBox> const&occupied){
 return worldgen_rules::smallFeatureFits(sharedBox(b),occupied);
}
bool wellCandidate(ChunkPos cp,uint seed){return worldgen_rules::wellCandidate({cp.x,cp.z},seed);}
std::optional<BlockPos> wellPosition(ChunkPos cp){
 auto p=worldgen_rules::wellPosition({cp.x,cp.z},[](int x,int z){return field->surface(x+32768,z+32768);});if(!p)return {};return BlockPos{p->x,p->y,p->z};
}
void placeSmallFeatures(Dimension&d,BlockSource&blocks,ChunkPos cp,uint seed,std::vector<BoundingBox>const&occupied){
 // AutoStage already rejects chunks with an on-disk version. A coordinate
 // visited earlier in this session is NOT proof that its unsaved chunk survived
 // eviction: generation can restart with a new bare chunk at the same position.
 // Re-evaluate native placement for that fresh chunk; never replay saved chunks.
 auto key=std::pair{cp.x,cp.z};completedSmallChunks.erase(key);
 auto probeBegan=std::chrono::steady_clock::now();++smallProbeCalls;struct Timer{std::chrono::steady_clock::time_point began;~Timer(){smallProbeNanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count();}}timer{probeBegan};
 Random rng(seed,true);bedrock_compat::seedStructure(rng,cp.x,cp.z,worldgen_rules::SmallFeatureSalt,seed);
 auto name=[&](worldgen_rules::StructurePoint p)->std::string_view{return blocks.getBlock({p.x,p.y,p.z}).getTypeName();};
 auto solid=[&](worldgen_rules::StructurePoint p){return blocks.isSolidBlockingBlock({p.x,p.y,p.z});};
 std::vector<worldgen_rules::StructureBox> reserved;reserved.reserve(occupied.size());for(auto const&b:occupied)reserved.push_back(sharedBox(b));
 worldgen_rules::executeSmallFeatures({cp.x,cp.z},seed,rng,[](int x,int z){return field->surface(x+32768,z+32768);},name,solid,reserved,[&](std::string_view kind,auto p,auto&r){return callSmallFeature(d,blocks,cp,std::string(kind),{p.x,p.y,p.z},r);},field->highMountainUndergroundEnabled(),field->generationHeight()-65);

}
void findSmallCandidates(Dimension&d,int x,int z){
 uint seed=bedrock_compat::seed(bedrock_compat::level(d).getLevelData()).to32BitRandomSeed();J rows=J::array();int cx=chunk(x),cz=chunk(z);
 for(int dz=-128;dz<=128;++dz)for(int dx=-128;dx<=128;++dx){ChunkPos cp{cx+dx,cz+dz};if(!structureChunkInside(cp.x,cp.z))continue;
  if(wellCandidate(cp,seed))if(auto p=wellPosition(cp);p&&!storedChunk(d,cp))rows.push_back({{"kind","desert_well"},{"pos",{p->x,p->y,p->z}},{"chunk",{cp.x,cp.z}}});
 }
 std::ofstream(folder/"small-feature-candidates.json")<<rows.dump(2);
}
void inspectSmallFeature(Dimension&d,BlockPos pos){
 BoundingBox b{{chunk(pos.x)*16,-58,chunk(pos.z)*16},{chunk(pos.x)*16+15,(field->generationHeight()-65),chunk(pos.z)*16+15}};ensureLoaded(d,b);auto&blocks=d.getBlockSourceFromMainChunkSource();J special=J::array(),counts=J::object();
 uint64_t hash=1469598103934665603ull;
 for(int z=b.min.z;z<=b.max.z;++z)for(int x=b.min.x;x<=b.max.x;++x)for(int y=-58;y<(field->generationHeight()-64);++y){BlockPos p{x,y,z};auto const&v=blocks.getBlock(p);auto n=v.getTypeName();counts[n]=counts.value(n,0)+1;uint64_t h=v.mSerializationIdHash;for(int shift=0;shift<64;shift+=8)hash=(hash^((h>>shift)&255))*1099511628211ull;
  if(n=="minecraft:chest"||n=="minecraft:mob_spawner"||n=="minecraft:suspicious_sand"||n=="minecraft:bone_block"){
   J row{{"pos",{x,y,z}},{"block",n}};auto actor=blocks.getBlockEntity(p);row["blockActor"]=actor!=nullptr;
   if(actor&&n=="minecraft:chest"){auto*c=static_cast<ChestBlockActor const*>(actor);row["lootTable"]=*c->mLootTable;row["lootSeed"]=c->mLootTableSeed;}
   if(actor&&n=="minecraft:mob_spawner"){auto*s=static_cast<MobSpawnerBlockActor const*>(actor);row["mob"]=*s->mSpawner->mActorDefId->mFullName;row["playerRange"]=int(s->mSpawner->mRequiredPlayerRange);}
   special.push_back(row);
  }
 }
 std::optional<BlockPos> safe;int64_t score=INT64_MAX;
 int referenceY=pos.y;bool fossil=std::any_of(special.begin(),special.end(),[](auto const&r){return r["block"]=="minecraft:bone_block";})&&counts.value("minecraft:mob_spawner",0)==0&&counts.value("minecraft:sandstone",0)<15;
 std::ifstream ledger(worldFolder()/"karma-native-small-features.jsonl");std::string line;while(std::getline(ledger,line)){auto r=J::parse(line,nullptr,false);if(!r.is_discarded()&&r.value("pos",J::array())==J::array({pos.x,pos.y,pos.z})){fossil=r.value("kind",std::string{}).find("fossil")!=std::string::npos;break;}}
 if(fossil)referenceY=field->surface(pos.x+32776,pos.z+32776).height+1;
 for(int z=b.min.z;z<=b.max.z;++z)for(int x=b.min.x;x<=b.max.x;++x)for(int y=std::max(-57,referenceY-3);y<=std::min((field->generationHeight()-74),referenceY+8);++y){BlockPos p{x,y,z};if(!blocks.isSolidBlockingBlock({x,y-1,z})||blocks.getBlock(p).getTypeName()!="minecraft:air"||blocks.getBlock({x,y+1,z}).getTypeName()!="minecraft:air")continue;int64_t dx=x-(b.min.x+8),dz=z-(b.min.z+8),dy=y-referenceY,value=dx*dx+dz*dz+dy*dy*16;if(value<score){score=value;safe=p;}}
 J destination=J::object();if(safe)destination={{"x",safe->x},{"y",safe->y},{"z",safe->z},{"kind",safe->y<32?"dry":"surface"},{"verifiedAir",true}};
 std::ofstream(folder/"small-feature-inspection.json")<<J{{"world",activeWorld},{"chunk",{chunk(pos.x),chunk(pos.z)}},{"counts",counts},{"special",special},{"hash",std::to_string(hash)},{"destination",destination},{"attempts",smallAttempts.load()},{"placed",smallPlaced.load()},{"nanos",smallNanos.load()},{"probeCalls",smallProbeCalls.load()},{"probeNanos",smallProbeNanos.load()},{"rejectedWrites",smallRejectedWrites.load()}}.dump(2);
}
