// Original FossilFeature chooses the complete skeleton once. Its output is
// persisted before any owning chunk receives a fragment. Saved chunks remain
// authoritative after restart; journal presence is not a disk-save commit.
struct FrozenFossil {ChunkPos anchor;std::string kind;BlockPos origin;BoundingBox bounds;
 std::map<std::array<int,3>,Block const*> writes;};
std::map<std::tuple<int,int,std::string>,FrozenFossil> frozenFossils;
std::map<std::tuple<int,int,std::string>,J> fossilJournalRows;
std::set<std::tuple<int,int,std::string>> rejectedFossils;
std::deque<std::tuple<int,int,std::string>> rejectedFossilOrder;
std::set<std::tuple<int,int,std::string,int,int>> recentFossilFragments;
std::deque<std::tuple<int,int,std::string,int,int>> recentFossilFragmentOrder;
bool fossilTemplatesReady=false;
std::atomic_uint64_t fossilPlanCalls=0,fossilPlanNanos=0,fossilFragments=0,fossilFragmentNanos=0,fossilSavedSkips=0;
std::atomic_uint64_t fossilFlushCalls=0,fossilFlushNanos=0;
uint64_t fossilHash(std::string const&text){return fossil_journal::checksum(text);}
std::filesystem::path fossilJournalPath(){return worldFolder()/"karma-native-fossil-plans.jsonl";}
std::string fossilProfile;
void durableFossilAppend(J const&payload){
 auto started=std::chrono::steady_clock::now();
 auto text=fossil_journal::encode(payload);
 auto path=fossilJournalPath();HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open fossil plan journal");
 DWORD written=0;bool ok=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file);CloseHandle(file);
 if(!ok)throw std::runtime_error("Cannot flush fossil plan before placement");
 ++fossilFlushCalls;fossilFlushNanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
}
void initializeFossilJournal(){
 frozenFossils.clear();fossilJournalRows.clear();rejectedFossils.clear();rejectedFossilOrder.clear();recentFossilFragments.clear();recentFossilFragmentOrder.clear();fossilProfile=std::to_string(fossilHash(input.dump()));
 auto path=fossilJournalPath();std::ifstream f(path,std::ios::binary);std::string data((std::istreambuf_iterator<char>(f)),{});
 auto decoded=fossil_journal::decode(data,worldIdentity(),fossilProfile);size_t end=decoded.validBytes;fossilJournalRows=std::move(decoded.plans);
 // Only an incomplete final line can be discarded; complete corrupt lines
 // never become permission to replay structures or overwrite player edits.
 if(end<data.size()){
  std::ofstream(folder/"fossil-journal-recovery.json")<<J{{"world",activeWorld},{"discardedIncompleteTailBytes",data.size()-end},{"validPlanCount",fossilJournalRows.size()}}.dump(2);
  std::filesystem::resize_file(path,end);
 }
}
void warmFossilTemplates(Level&level){
 if(fossilTemplatesReady||!smallFeaturesEnabled||level.getLevelData().getLevelName()!=activeWorld)return;
 std::lock_guard guard(stageMutex);if(fossilTemplatesReady)return;auto t=std::chrono::steady_clock::now();J names=J::array();
 // Same eight server-owned skull/spine models used by FossilFeature.
 for(auto type:{"skull","spine"})for(int i=1;i<=4;++i){auto name="fossils/fossil_"+std::string(type)+"_0"+std::to_string(i);auto s=level.getStructureManager()->getOrCreateLegacy(name).rawSize();if(s.x<=0||s.y<=0||s.z<=0)throw std::runtime_error("Missing native fossil template: "+name);names.push_back(name);}
 fossilTemplatesReady=true;std::ofstream(folder/"fossil-template-warmup.json")<<J{{"count",names.size()},{"names",names},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()}}.dump(2);
}
// Fossil placement must not use a tree canopy/live decorated neighbour to
// choose its height or decide empty corners. These bounded queries consult
// the existing seeded terrain; they never generate neighbouring chunks.
LL_TYPE_INSTANCE_HOOK(FossilHeightXZ,ll::memory::HookPriority::Normal,BlockSource,(short(BlockSource::*)(int,int))&BlockSource::getHeightmap,short,int x,int z){if(fossilCapture&&fossilCapture->source==this){int h=field->surface(x+32768,z+32768).height;return short((capturedVillageGround?villageGround(*capturedVillageGround,x,z,h):h)+1);}return origin(x,z);}
LL_TYPE_INSTANCE_HOOK(FossilHeightPos,ll::memory::HookPriority::Normal,BlockSource,(short(BlockSource::*)(BlockPos const&)const)&BlockSource::getHeightmap,short,BlockPos const&p){if(fossilCapture&&fossilCapture->source==this){int h=field->surface(p.x+32768,p.z+32768).height;return short((capturedVillageGround?villageGround(*capturedVillageGround,p.x,p.z,h):h)+1);}return origin(p);}
LL_STATIC_HOOK(FossilCorners,ll::memory::HookPriority::Normal,&FossilFeature::_countEmptyCorners,uint64,BlockSource const&source,BoundingBox const&b){
 if(!fossilCapture||fossilCapture->source!=&source)return origin(source,b);
 uint64 n=0;for(int x:{b.min.x,b.max.x})for(int z:{b.min.z,b.max.z})for(int y:{b.min.y,b.max.y}){auto s=field->surface(x+32768,z+32768);auto m=field->material(s,x+32768,y,z+32768);n+=m==0||m==8||m==43;}return n;
}
bool fossilPotential(ChunkPos cp,uint seed){return smallFeaturesEnabled&&worldgen_rules::fossilNeighbourPotential({cp.x,cp.z},seed);}
void rejectFossil(std::tuple<int,int,std::string> const&key){if(rejectedFossils.emplace(key).second){rejectedFossilOrder.push_back(key);if(rejectedFossilOrder.size()>4096){rejectedFossils.erase(rejectedFossilOrder.front());rejectedFossilOrder.pop_front();}}}
FrozenFossil* frozenFossil(Dimension&d,BlockSource&blocks,ChunkPos anchor,uint seed,std::string const&kind,std::vector<BoundingBox> const&occupied){
 auto key=std::tuple{anchor.x,anchor.z,kind};if(auto it=frozenFossils.find(key);it!=frozenFossils.end())return &it->second;if(rejectedFossils.contains(key)||!fossilTemplatesReady)return nullptr;
 FrozenFossil plan;plan.anchor=anchor;plan.kind=kind;
 if(auto it=fossilJournalRows.find(key);it!=fossilJournalRows.end()){
  auto const&r=it->second;if(r.at("seed")!=seed)throw std::runtime_error("Fossil journal world seed mismatch");plan.bounds=boxFromJson(r.at("box"));auto pos=r.at("origin");plan.origin={pos[0].get<int>(),pos[1].get<int>(),pos[2].get<int>()};
  for(auto const&w:r.at("blocks")){auto b=Block::tryGetFromRegistry(w[3].get<std::string>(),w[4].get<ushort>());if(!b||uint64_t(b->mSerializationIdHash)!=w[5].get<uint64_t>())throw std::runtime_error("Fossil saved block state mismatch");plan.writes.emplace(std::array<int,3>{w[0],w[1],w[2]},&*b);}
  return &frozenFossils.emplace(key,std::move(plan)).first->second;
 }
 auto s=field->surface(anchor.x*16+32776,anchor.z*16+32776);if(!worldgen_rules::fossilHabitat(s)){rejectFossil(key);return nullptr;}
 // Do not start a newly-added skeleton in previously saved terrain.
 if(storedChunk(d,anchor)){rejectFossil(key);return nullptr;}
 Random rng(seed,true);StructureFeature::setRandomSeedFor(rng,anchor.x,anchor.z,worldgen_rules::fossilPlacementSalt(kind!="fossil"),seed);
 auto origin=worldgen_rules::fossilOrigin({anchor.x,anchor.z},s.height,kind!="fossil",rng);
 plan.origin={origin.x,origin.y,origin.z};FossilCapture capture{&blocks};fossilCapture=&capture;
 struct Reset{~Reset(){fossilCapture=nullptr;}}reset;auto t=std::chrono::steady_clock::now();++fossilPlanCalls;
 WorldGenContext context{};context.mPreliminarySurfaceProvider=&provider;SmallTarget target(blocks,context);RenderParams params;params.mLevel=&d.getLevel();params.mBlockSource=&blocks;params.mBlockPlacementTarget=&target;FeaturePlacementFailureLog logger;FeaturePlacementFailureLogProxy proxy(logger);
 struct Context {IBlockWorldGenAPI& target;BlockPos pos;Random& random;RenderParams& params;FeaturePlacementFailureLogProxy logger;};static_assert(sizeof(Context)==sizeof(IFeature::PlacementContext));
 Context storage{target,plan.origin,rng,params,std::move(proxy)};bool ok=smallDefinition(d.getLevel(),"minecraft:"+kind+"_feature").place(reinterpret_cast<IFeature::PlacementContext const&>(storage)).has_value();fossilCapture=nullptr;
 fossilPlanNanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
 if(!ok||capture.rejected||capture.writes.empty()){rejectFossil(key);return nullptr;}
 plan.bounds=nativeBox(worldgen_rules::fossilOutputBounds({anchor.x,anchor.z},capture.writes,[](auto p){return worldgen_rules::StructurePoint{p[0],p[1],p[2]};},[](auto b){return b->getTypeName();}));
 if(!fitsSmall(plan.bounds,occupied)){rejectFossil(key);return nullptr;}
 for(int z=chunk(plan.bounds.min.z);z<=chunk(plan.bounds.max.z);++z)for(int x=chunk(plan.bounds.min.x);x<=chunk(plan.bounds.max.x);++x)if(storedChunk(d,{x,z})){rejectFossil(key);return nullptr;}
 plan.writes=std::move(capture.writes);J writes=J::array();for(auto const&[p,b]:plan.writes)writes.push_back({p[0],p[1],p[2],b->getTypeName(),b->getData(),uint64_t(b->mSerializationIdHash)});
 J record{{"schema",1},{"world",worldIdentity()},{"profile",fossilProfile},{"seed",seed},{"anchor",{anchor.x,anchor.z}},{"kind",kind},{"origin",{plan.origin.x,plan.origin.y,plan.origin.z}},{"box",boxJson(plan.bounds)},{"blocks",writes}};
 durableFossilAppend(record);fossilJournalRows.emplace(key,record);return &frozenFossils.emplace(key,std::move(plan)).first->second;
}
void placeFrozenFossils(Dimension&d,BlockSource&blocks,ChunkPos cp,uint seed,std::vector<BoundingBox>&occupied){
 if(!fossilTemplatesReady)return;
 for(int z=cp.z-1;z<=cp.z+1;++z)for(int x=cp.x-1;x<=cp.x+1;++x){ChunkPos anchor{x,z};if(!structureChunkInside(x,z)||!worldgen_rules::fossilCandidate({x,z},seed))continue;
  for(auto kind:{"deepslate_diamond_fossil","fossil"}){auto*plan=frozenFossil(d,blocks,anchor,seed,kind,occupied);if(!plan)continue;occupied.push_back(plan->bounds);auto t=std::chrono::steady_clock::now();unsigned writes=0;
   auto fragment=std::tuple{x,z,std::string(kind),cp.x,cp.z};if(!recentFossilFragments.emplace(fragment).second)continue;recentFossilFragmentOrder.push_back(fragment);if(recentFossilFragmentOrder.size()>4096){recentFossilFragments.erase(recentFossilFragmentOrder.front());recentFossilFragmentOrder.pop_front();}
   // Only this fresh chunk receives its portion. No neighbour write, forced
   // load, delayed post-generation repair or saved-block comparison/replay.
   writes=unsigned(worldgen_rules::publishFossilOwner({cp.x,cp.z},plan->writes,[](auto p){return worldgen_rules::StructurePoint{p[0],p[1],p[2]};},
    [&](auto p){BlockPos pos{p.x,p.y,p.z};auto const&old=blocks.getBlock(pos);if(old.getTypeName()=="minecraft:bedrock"||blocks.getBlockEntity(pos))throw std::runtime_error("Fossil fragment would overwrite protected block");return &old;},
    [&](auto p,auto b){BlockPos pos{p.x,p.y,p.z};return blocks.getBlock(pos)==*b||blocks.setBlockNoUpdate(pos,*b);},
    [&](auto p,auto b){return blocks.setBlockNoUpdate({p.x,p.y,p.z},*b);}));
   if(writes){++fossilFragments;auto*c=blocks.getChunkAt({cp.x*16,0,cp.z*16});c->setUnsaved();c->markSaveIfNeverSaved();c->markSaveNonActorDataIfDirty();std::ofstream(folder/"fossil-fragments.jsonl",std::ios::app)<<J{{"anchor",{x,z}},{"kind",kind},{"chunk",{cp.x,cp.z}},{"writes",writes},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()}}.dump()<<'\n';}
   fossilFragmentNanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
  }
 }
}
void fossilStatus(){J plans=J::array();for(auto const&[key,row]:fossilJournalRows){auto r=row;r.erase("blocks");r["writes"]=row["blocks"].size();plans.push_back(r);}std::ofstream(folder/"fossil-status.json")<<J{{"world",activeWorld},{"plans",plans},{"planCalls",fossilPlanCalls.load()},{"planNanos",fossilPlanNanos.load()},{"fragments",fossilFragments.load()},{"fragmentNanos",fossilFragmentNanos.load()},{"flushCalls",fossilFlushCalls.load()},{"flushNanos",fossilFlushNanos.load()},{"savedSkips",fossilSavedSkips.load()},{"negativeCache",rejectedFossils.size()},{"recentFragments",recentFossilFragments.size()}}.dump(2);}
void inspectFrozenFossil(Dimension&d,int x,int z,bool deep){
 auto key=std::tuple{chunk(x),chunk(z),std::string(deep?"deepslate_diamond_fossil":"fossil")};auto it=fossilJournalRows.find(key);if(it==fossilJournalRows.end())throw std::runtime_error("No admitted fossil plan");auto const&r=it->second;auto b=boxFromJson(r["box"]);ensureLoaded(d,b);auto&blocks=d.getBlockSourceFromMainChunkSource();J mismatches=J::array(),counts=J::object(),chunks=J::array();
 for(auto const&w:r["blocks"]){BlockPos p{w[0].get<int>(),w[1].get<int>(),w[2].get<int>()};auto const&v=blocks.getBlock(p);counts[v.getTypeName()]=counts.value(v.getTypeName(),0)+1;if(uint64_t(v.mSerializationIdHash)!=w[5].get<uint64_t>())mismatches.push_back({{"pos",{p.x,p.y,p.z}},{"actual",v.getTypeName()},{"expected",w[3]}});}
 for(int zz=chunk(b.min.z);zz<=chunk(b.max.z);++zz)for(int xx=chunk(b.min.x);xx<=chunk(b.max.x);++xx)chunks.push_back({xx,zz});
 std::optional<BlockPos> safe;int64_t score=INT64_MAX;int cx=chunk(x)*16+8,cz=chunk(z)*16+8;
 for(int zz=cz-7;zz<=cz+7;++zz)for(int xx=cx-7;xx<=cx+7;++xx){int h=field->surface(xx+32768,zz+32768).height+1;for(int yy=std::max(-57,h-2);yy<=std::min(310,h+8);++yy){BlockPos p{xx,yy,zz};if(!blocks.isSolidBlockingBlock({xx,yy-1,zz})||blocks.getBlock(p).getTypeName()!="minecraft:air"||blocks.getBlock({xx,yy+1,zz}).getTypeName()!="minecraft:air")continue;int64_t dx=xx-cx,dz=zz-cz,dy=yy-h,value=dx*dx+dz*dz+dy*dy*16;if(value<score){score=value;safe=p;}}}
 J destination=J::object();if(safe)destination={{"x",safe->x},{"y",safe->y},{"z",safe->z},{"kind","surface"},{"verifiedAir",true}};
 std::ofstream(folder/"fossil-inspection.json")<<J{{"world",activeWorld},{"plan",r},{"counts",counts},{"chunks",chunks},{"mismatches",mismatches},{"complete",mismatches.empty()},{"destination",destination}}.dump(2);
}
void findFrozenFossils(Dimension&d,int x,int z,int radius){
 if(radius<1||radius>64)throw std::runtime_error("Fossil search radius must be 1..64 chunks");uint seed=d.getLevel().getLevelData().getSeed().to32BitRandomSeed();J rows=J::array();
 for(int zz=chunk(z)-radius;zz<=chunk(z)+radius;++zz)for(int xx=chunk(x)-radius;xx<=chunk(x)+radius;++xx){ChunkPos cp{xx,zz};if(xx<-terrain_bounds::HalfChunks+1||xx>=terrain_bounds::HalfChunks-1||zz<-terrain_bounds::HalfChunks+1||zz>=terrain_bounds::HalfChunks-1||smallSelector(cp,seed)%64||storedChunk(d,cp))continue;auto s=field->surface(xx*16+32776,zz*16+32776);if((s.biome==11||s.biome==10)&&s.volcanic<.01&&s.iceHabitat<.01&&s.ocean<.5)rows.push_back({{"anchor",{xx*16,zz*16}},{"height",s.height},{"biome",s.biome}});}
 std::ofstream(folder/"fossil-candidates.json")<<rows.dump(2);
}
