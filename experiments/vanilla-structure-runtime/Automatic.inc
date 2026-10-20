// Research adapter: native chunk decoration phase, before the chunk becomes Loaded.
// Do not access GridArea fields: the distributed SDK layout differs from this BDS.
std::mutex stageMutex;
extern J entityLedger;
std::atomic_bool entityLedgerReady=false;
bool villageActorChunk(StructureStart*,BoundingBox const&);
using NativeEntityJob=worldgen_rules::StructureEntityJob<StructureStart*,BoundingBox>;
worldgen_rules::StructureEntityQueue<StructureStart*,BoundingBox> nativeEntityQueue;
auto&nativeEntityJobs=nativeEntityQueue.pending;
auto&queuedEntityKeys=nativeEntityQueue.queued;
void queueStructureEntities(StructureStart* start,BoundingBox clip,std::string const& family,ChunkPos anchor,uint seed){
 if(!entityLedgerReady)return;
 nativeEntityQueue.enqueue(entityLedger,start,clip,family,{anchor.x,anchor.z},seed,family!="village"||villageActorChunk(start,clip));
}
unsigned storageLogRows=0;
std::atomic_bool automaticStructures=false;
std::atomic_uint64_t stageCalls=0,stageErrors=0,savedStructureSkips=0;
std::atomic_uint64_t adapterNanos=0,adapterPeakNanos=0,spacingRejectCalls=0,spacingRejectNanos=0;
std::array<std::atomic_uint64_t,16> adapterHistogram{};
struct StageTimer {
 std::chrono::steady_clock::time_point began=std::chrono::steady_clock::now();
 ~StageTimer(){uint64_t ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count();adapterNanos+=ns;auto peak=adapterPeakNanos.load();while(peak<ns&&!adapterPeakNanos.compare_exchange_weak(peak,ns)){};size_t bin=0;for(uint64_t limit=1000;ns>limit&&bin<15;limit*=2)++bin;++adapterHistogram[bin];}
};
int structureCell(int v){return v>=0?v/32:(v-31)/32;}
ChunkPos nativeCandidateForCell(int x,int z,uint seed){
 struct Slot{int x=0,z=0,cx=0,cz=0;uint seed=0;bool valid=false;};
 thread_local std::array<Slot,64> cache{};
 auto& entry=cache[(uint(x)*73856093u^uint(z)*19349663u)&63u];
 if(!entry.valid||entry.x!=x||entry.z!=z||entry.seed!=seed){auto c=worldgen_rules::candidateForCell(x,z,seed,32,8,14357617);entry={x,z,c.x,c.z,seed,true};}
 return {entry.cx,entry.cz};
}
struct AutoBlueprint {Start* start=nullptr;int lo=0,hi=0;std::unique_ptr<IglooLayout> igloo;bool templeReady=false;std::map<std::array<int,3>,std::pair<Block const*,Block const*>> templeWrites;std::map<std::array<int,3>,std::pair<std::string,int>> templeLoot;std::shared_ptr<worldgen_rules::FrozenCaptureProgram const>temple;std::optional<worldgen_rules::ScatteredFragmentRecipe>fragments;};
bool placeFrozenTemple(BlockSource&,AutoBlueprint&,Random&,BoundingBox const&);
extern thread_local BlockSource* portalWriteSource;
extern thread_local BoundingBox const* portalWriteClip;
extern thread_local BlockSource* seededReadSource;
extern thread_local BoundingBox const* seededReadClip;
extern thread_local LevelChunk* nativeFragmentOwner;
extern thread_local worldgen_rules::ScatteredSeededReadCache<Block const*>* scatteredReadCache;
worldgen_rules::StructureBox sharedBox(BoundingBox const&);
BoundingBox nativeBox(worldgen_rules::StructureBox const&);
bool placeScatteredNative(BlockSource&blocks,AutoBlueprint&plan,Random&rng,BoundingBox const&clip){
 if(plan.igloo){bool placed=placeIgloo(blocks,*plan.igloo,clip);if(placed&&plan.igloo->program){auto seed=blocks.getDimension().getLevel().getLevelData().getSeed().to32BitRandomSeed();for(auto const&[owner,markers]:plan.igloo->program->population){if(markers.empty()||owner.first*16>clip.max.x||owner.first*16+15<clip.min.x||owner.second*16>clip.max.z||owner.second*16+15<clip.min.z)continue;queueStructureEntities(plan.start,{{owner.first*16,-64,owner.second*16},{owner.first*16+15,319,owner.second*16+15}},"igloo",{int(plan.start->chunkX),int(plan.start->chunkZ)},seed);}}return placed;}
 if(plan.start->getStructureName()=="minecraft:desert_pyramid")return placeFrozenTemple(blocks,plan,rng,clip);
 if(plan.fragments)return worldgen_rules::executeScatteredFragments(*plan.fragments,sharedBox(clip),[&](auto const&recipe,auto const&fragment){
  auto&d=blocks.getDimension();auto biome=d.getBiomeRegistry().lookupByName(std::string(worldgen_rules::scatteredNativeBiome(recipe)));if(!biome)throw std::runtime_error("Native scattered biome missing");FixedBiomeSource source(*biome);auto local=std::make_unique<Start>(source,31,recipe.anchor.x,recipe.anchor.z,provider);
  int dy=worldgen_rules::scatteredFreshMove(recipe,sharedBox(*local->boundingBox));local->moveBoundingBoxes(dy);for(auto&piece:*local->pieces)static_cast<ScatteredFeaturePiece*>(piece.get())->mHeightPosition=recipe.floor;local->calculateBoundingBox();return local;
 },[&](auto&local,auto const&,auto const&fragment){
  auto owner=nativeBox(fragment.clip);auto*oldWriteSource=portalWriteSource;auto*oldWriteClip=portalWriteClip;auto*oldReadSource=seededReadSource;auto*oldReadClip=seededReadClip;auto*oldOwner=nativeFragmentOwner;auto*oldCache=scatteredReadCache;
  worldgen_rules::ScatteredSeededReadCache<Block const*> cache;portalWriteSource=seededReadSource=&blocks;portalWriteClip=seededReadClip=&owner;nativeFragmentOwner=blocks.getChunkAt({owner.min.x,0,owner.min.z});scatteredReadCache=&cache;
  struct Restore{BlockSource*w;BoundingBox const*wc;BlockSource*r;BoundingBox const*rc;LevelChunk*owner;worldgen_rules::ScatteredSeededReadCache<Block const*>*cache;~Restore(){portalWriteSource=w;portalWriteClip=wc;seededReadSource=r;seededReadClip=rc;nativeFragmentOwner=owner;scatteredReadCache=cache;}}restore{oldWriteSource,oldWriteClip,oldReadSource,oldReadClip,oldOwner,oldCache};
  Random random(fragment.seed,true);return local->postProcess(blocks,random,owner);
 });
 auto&d=blocks.getDimension();auto cp=ChunkPos{plan.start->chunkX,plan.start->chunkZ};auto s=field->surface(cp.x*16+32768,cp.z*16+32768);
 auto b=d.getBiomeRegistry().lookupByName(s.biome==11?"minecraft:desert":s.biome==13?"minecraft:jungle":"minecraft:swampland");if(!b)throw std::runtime_error("Native scattered biome missing");FixedBiomeSource source(*b);Start local(source,31,cp.x,cp.z,provider);
 int floor=static_cast<ScatteredFeaturePiece*>((*plan.start->pieces)[0].get())->mHeightPosition;
 local.moveBoundingBoxes(floor-local.boundingBox->min.y);for(auto&piece:*local.pieces)static_cast<ScatteredFeaturePiece*>(piece.get())->mHeightPosition=floor;local.calculateBoundingBox();
 if(boxJson(*local.boundingBox)!=boxJson(*plan.start->boundingBox))throw std::runtime_error("Scattered fragment layout diverged from native record");
 // Native per-piece placed-chest flags must not change the RNG consumed by
 // a different fresh fragment (including the desert archaeology chamber).
 auto*oldWriteSource=portalWriteSource;auto*oldWriteClip=portalWriteClip;auto*oldReadSource=seededReadSource;auto*oldReadClip=seededReadClip;auto*oldOwner=nativeFragmentOwner;portalWriteSource=&blocks;portalWriteClip=&clip;seededReadSource=&blocks;seededReadClip=&clip;nativeFragmentOwner=blocks.getChunkAt({clip.min.x,0,clip.min.z});
 struct Reset{BlockSource*w;BoundingBox const*wc;BlockSource*r;BoundingBox const*rc;LevelChunk*owner;~Reset(){portalWriteSource=w;portalWriteClip=wc;seededReadSource=r;seededReadClip=rc;nativeFragmentOwner=owner;}}reset{oldWriteSource,oldWriteClip,oldReadSource,oldReadClip,oldOwner};
 return local.postProcess(blocks,rng,clip);
}
std::map<std::pair<int,int>,AutoBlueprint> automaticBlueprints;
std::vector<std::pair<int,int>> iglooPopulationPlans;
AutoBlueprint& autoBlueprint(Dimension&,ChunkPos,uint);
bool storedChunk(Dimension& dim,ChunkPos cp){
 std::string key(8,'\0');std::memcpy(key.data(),&cp.x,4);std::memcpy(key.data()+4,&cp.z,4);key.push_back(char(LevelChunkTag::Version));
 auto& db=dim.getLevel().getLevelStorage();if(db.hasKey(key,DBHelpers::Category::Chunk))return true;
 key.back()=char(LevelChunkTag::LegacyVersion);return db.hasKey(key,DBHelpers::Category::Chunk);
}
class TerrainScatteredFeature final:public RandomScatteredLargeFeature {
public:
 explicit TerrainScatteredFeature(uint seed):RandomScatteredLargeFeature(seed){}
 bool getNearestGeneratedFeature(Dimension& dim,BiomeSource const&,BlockPos const& from,BlockPos& result,IPreliminarySurfaceProvider const&,bool fresh,std::optional<HashedString> const& wanted)override{
  uint seed=structureSeed(dim);
  struct Candidate{ChunkPos cp;int64_t distance;};std::vector<Candidate> ordered;
  // Finite configured terrain bounds. Query candidates only; never load chunks.
  for(int z=structureInfinite()?structureCell(from.z/16)-32:structureCell(-terrain_bounds::HalfChunks);z<=(structureInfinite()?structureCell(from.z/16)+32:structureCell(terrain_bounds::HalfChunks-1));++z)for(int x=structureInfinite()?structureCell(from.x/16)-32:structureCell(-terrain_bounds::HalfChunks);x<=(structureInfinite()?structureCell(from.x/16)+32:structureCell(terrain_bounds::HalfChunks-1));++x){auto cp=nativeCandidateForCell(x,z,seed);if(!structureChunkInside(cp.x,cp.z)||!structureChunkInside(cp.x+1,cp.z+1))continue;int64_t dx=int64_t(cp.x)*16+8-from.x,dz=int64_t(cp.z)*16+8-from.z;ordered.push_back({cp,dx*dx+dz*dz});}
  std::sort(ordered.begin(),ordered.end(),[](auto const&a,auto const&b){return a.distance<b.distance;});
  bool named=wanted&&(wanted->getString()=="minecraft:desert_pyramid"||wanted->getString()=="minecraft:jungle_pyramid"||wanted->getString()=="minecraft:swamp_hut"||wanted->getString()=="minecraft:igloo");
  for(auto const& c:ordered){auto& p=autoBlueprint(dim,c.cp,seed);if(!p.start||(named&&p.start->getStructureName()!=wanted->getString()))continue;auto const& b=*p.start->boundingBox;bool used=false;if(fresh)for(int z=chunk(b.min.z);z<=chunk(b.max.z);++z)for(int x=chunk(b.min.x);x<=chunk(b.max.x);++x)used|=storedChunk(dim,{x,z});if(used)continue;result={(b.min.x+b.max.x)/2,b.min.y,(b.min.z+b.max.z)/2};return true;}
  return false;
 }
};
std::set<StructureFeature*> ownedNativeFeatures;
TerrainScatteredFeature* scatteredFeatureRecord=nullptr;
TerrainScatteredFeature& registeredFeature(Dimension& dim){
 auto* registry=&*dim.getWorldGenerator()->mStructureFeatureRegistry;
 if(!registry)throw std::runtime_error("Missing native structure registry");
 if(scatteredFeatureRecord)return *scatteredFeatureRecord;
 if(registry->mStructureFeatures->size()>64)throw std::runtime_error("Invalid native registry ABI");
 auto f=std::make_unique<TerrainScatteredFeature>(structureSeed(dim));auto* p=f.get();ownedNativeFeatures.insert(p);scatteredFeatureRecord=p;registry->mStructureFeatures->push_back(std::move(f));return *p;
}
std::optional<ChunkPos> stageCenter(ChunkViewSource& view){
 // BDS 1.26.20.5: actual GridArea has an 8-byte prefix absent from SDK.
 // Only copy its bounds; validate dimensions, coordinate range and native center.
 // Never dereference SDK vector fields (the earlier observer exposed the mismatch).
 auto& area=view.getArea();std::array<int,6> bounds{};
 std::memcpy(bounds.data(),reinterpret_cast<char const*>(&area)+0xc8,24);
 if(bounds[3]-bounds[0]!=2||bounds[5]-bounds[2]!=2||bounds[1]!=0||bounds[4]!=0||std::abs(bounds[0])>1000000||std::abs(bounds[2])>1000000)throw std::runtime_error("Native neighborhood ABI mismatch");
 ChunkPos cp{bounds[0]+1,bounds[2]+1};if(!view.$getExistingChunk(cp))throw std::runtime_error("Native center is absent");return cp;
}
AutoBlueprint& autoBlueprint(Dimension& dim,ChunkPos candidate,uint seed){
 auto key=std::pair{candidate.x,candidate.z};if(auto it=automaticBlueprints.find(key);it!=automaticBlueprints.end())return it->second;
 // Blueprint cache includes rejected candidates within the configured bounds.
 auto& plan=automaticBlueprints[key];initialize();
 int x=candidate.x*16,z=candidate.z*16;if(!structureBoxInside(x,z,x+20,z+20))return plan;auto center=field->surface(x+32768,z+32768);
 if(!structureIncluded(worldgen_rules::scatteredName(center.biome))||!worldgen_rules::scatteredCenterAllowed(center))return plan;
 auto biome=dim.getBiomeRegistry().lookupByName(center.biome==11?"minecraft:desert":center.biome==13?"minecraft:jungle":center.biome==10?"minecraft:swampland":"minecraft:ice_plains");if(!biome)throw std::runtime_error("Missing native biome");
 FixedBiomeSource source(*biome);auto start=std::make_unique<Start>(source,31,candidate.x,candidate.z,provider);
 if(!start->isValid())return plan;
 auto initial=*start->boundingBox;
 std::unique_ptr<IglooLayout> igloo;
 if(start->getStructureName()=="minecraft:igloo"){
  igloo=std::make_unique<IglooLayout>(planIgloo(dim,candidate,seed,center.height));initial=igloo->bounds;
  if(!structureBoxInside(initial.min.x,initial.min.z,initial.max.x,initial.max.z)||initial.min.y<-58)return plan;
 }
 int lo=320,hi=-64,total=0;
 // Full footprint (441 columns), only after the cheap native spacing rejection.
 int columns=0;
 std::optional<worldgen_rules::FrozenIglooPlan> sharedIgloo;
 if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{}))){
  auto sample=[](int x,int z){return field->surface(x+32768,z+32768);};worldgen_rules::ScatteredLayoutPlan fit;
  if(igloo){sharedIgloo=sharedIglooGeometry(*igloo);fit=worldgen_rules::fitFrozenIglooFloor(*sharedIgloo,center.biome,sample);}else fit=worldgen_rules::planScatteredLayout({{initial.min.x,initial.min.y,initial.min.z},{initial.max.x,initial.max.y,initial.max.z}},center.biome,sample);
  if(!fit.accepted)return plan;lo=fit.low;hi=fit.high;total=fit.floor;columns=1;
 }else { for(int zz=initial.min.z;zz<=initial.max.z;++zz)for(int xx=initial.min.x;xx<=initial.max.x;++xx){auto s=field->surface(xx+32768,zz+32768);if(!worldgen_rules::scatteredFootprintAllowed(s,center.biome))return plan;lo=std::min(lo,s.height);hi=std::max(hi,s.height);total+=s.height+1;++columns;if(hi-lo>worldgen_rules::scatteredMaxSlope(center.biome))return plan;}
 }
 TerrainPointBiomes actual(dim,*biome);RandomScatteredLargeFeature feature(seed);Random rng(seed,true);
 adaptHeightMap=true;struct Reset{~Reset(){adaptHeightMap=false;}}reset;
 if(!feature.isFeatureChunk(actual,rng,candidate,seed,provider,dim))return plan;
 // Bind the whole-footprint ground level before any per-chunk writes. This avoids
 // the first processed chunk choosing a different height after restart/order changes.
 int floor=total/columns;start->moveBoundingBoxes(floor-start->boundingBox->min.y);
 for(auto& piece:*start->pieces)static_cast<ScatteredFeaturePiece*>(piece.get())->mHeightPosition=floor;
 start->calculateBoundingBox();
 if(igloo){if(sharedIgloo){for(size_t i=0;i<igloo->parts.size();++i){auto const&p=sharedIgloo->parts[i];igloo->parts[i].pos={p.origin.x,p.origin.y,p.origin.z};igloo->parts[i].bounds={{p.bounds.min.x,p.bounds.min.y,p.bounds.min.z},{p.bounds.max.x,p.bounds.max.y,p.bounds.max.z}};}igloo->bounds={{sharedIgloo->bounds.min.x,sharedIgloo->bounds.min.y,sharedIgloo->bounds.min.z},{sharedIgloo->bounds.max.x,sharedIgloo->bounds.max.y,sharedIgloo->bounds.max.z}};}else{int dy=floor-1-center.height;for(auto& p:igloo->parts){p.pos.y+=dy;p.bounds.min.y+=dy;p.bounds.max.y+=dy;}igloo->bounds.min.y+=dy;igloo->bounds.max.y+=dy;}
  *start->boundingBox=igloo->bounds;for(auto& p:*start->pieces)*p->mBoundingBox=igloo->bounds;plan.igloo=std::move(igloo);
  if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{}))){compileIglooProgram(*plan.igloo);if(plan.igloo->ladders&&!auditPlanningStarts)iglooPopulationPlans.push_back(key);}
 }
 plan.lo=lo;plan.hi=hi;plan.start=start.get();
 if((center.biome==13||center.biome==10)&&worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{})))plan.fragments=worldgen_rules::planScatteredFragments(center.biome,{candidate.x,candidate.z},seed,sharedBox(*start->boundingBox),floor);
 if(auditPlanningStarts){auditPlanningStarts->push_back(std::move(start));return plan;}
 auto& featureRecord=registeredFeature(dim);std::unique_lock lock(*featureRecord.mCacheMutex);featureRecord.mCachedStructures->emplace(candidate,std::move(start));return plan;
}
struct VegetationPair{std::unique_ptr<nativegen::Engine> normal,reserved;};
std::map<std::pair<int,int>,VegetationPair> vegetationPairs;
bool plantRepairPreview=false;J* plantRepairChanges=nullptr;
void clearPlantBlock(BlockSource& blocks,BlockPos pos,Block const& air){if(plantRepairChanges)plantRepairChanges->push_back({pos.x,pos.y,pos.z,blocks.getBlock(pos).getTypeName()});if(!plantRepairPreview)blocks.setBlockNoUpdate(pos,air);}
using ProtectedPlants=std::set<std::array<int,3>>;
void protectTemplate(ProtectedPlants& keep,LegacyStructureTemplate const& model,BlockPos origin,Rotation rotation){
 for(auto const&i:*model.mBlockInfo){auto const&name=i.mBlock.getTypeName();if(name=="minecraft:air"||name=="minecraft:structure_void")continue;auto p=LegacyStructureTemplate::transform(*i.mPos,Mirror::None,rotation);keep.insert({p.x+origin.x,p.y+origin.y,p.z+origin.z});}
}
bool removablePlant(std::string const& n){return worldgen_rules::structureReservationPlant(n);}
unsigned clearReservedPlants(BlockSource& blocks,ChunkPos cp,VegetationPair& pair,ProtectedPlants const* keep=nullptr){
 auto before=pair.normal->vegetationChunk(cp.x,cp.z),after=pair.reserved->vegetationChunk(cp.x,cp.z);auto air=Block::tryGetFromRegistry("minecraft:air");unsigned n=0;
 for(int z=0;z<16;++z)for(int x=0;x<16;++x)for(int y=-60;y<320;++y){int i=(z*16+x)*384+y+64;if(!worldgen_rules::structureReservationCandidate(before[i],after[i]))continue;BlockPos p{cp.x*16+x,y,cp.z*16+z};if(keep&&keep->contains({p.x,p.y,p.z}))continue;auto name=blocks.getBlock(p).getTypeName();if(!worldgen_rules::structureReservationRemoves(before[i],after[i],name))continue;clearPlantBlock(blocks,p,*air);++n;
  // Thin snow on the removed crown/trunk must not remain floating.
  BlockPos above{p.x,p.y+1,p.z};if(p.y<319&&blocks.getBlock(above).getTypeName()=="minecraft:snow_layer"&&(!keep||!keep->contains({above.x,above.y,above.z}))){clearPlantBlock(blocks,above,*air);++n;}
 }return n;
}
VegetationPair& structureVegetationPair(AutoBlueprint const& plan){
 auto const& b=*plan.start->boundingBox;auto key=std::pair{b.min.x,b.min.z};auto it=vegetationPairs.find(key);
 if(it==vegetationPairs.end()){if(vegetationPairs.size()>=2)vegetationPairs.erase(vegetationPairs.begin());VegetationPair pair;pair.normal=std::make_unique<nativegen::Engine>(input,std::vector<std::array<int,4>>{},true,field.get());pair.reserved=std::make_unique<nativegen::Engine>(input,std::vector<std::array<int,4>>{{b.min.x+32756,b.min.z+32756,b.max.x+32780,b.max.z+32780}},true,field.get());it=vegetationPairs.emplace(key,std::move(pair)).first;}
 return it->second;
}
unsigned reserveTrees(BlockSource& blocks,ChunkPos cp,AutoBlueprint const& plan,ProtectedPlants const* keep=nullptr){if(plan.start->getStructureName()=="minecraft:desert_pyramid")return 0;return clearReservedPlants(blocks,cp,structureVegetationPair(plan),keep);}
#include "Village.inc"
#include "ExtraStructures.inc"
#include "StructurePrefill.inc"
#include "SmallFeatures.inc"
#include "FrozenFossils.inc"
#include "VillageDecor.inc"
LL_TYPE_INSTANCE_HOOK(AutoStage,ll::memory::HookPriority::Normal,FlatWorldGenerator,&FlatWorldGenerator::$decorationPostProcessChunk,bool,ChunkViewSource& view){
 auto ok=origin(view);
 if(!automaticStructures||mLevel->getLevelData().getLevelName()!=activeWorld)return ok;
 ++stageCalls;
 StageTimer timer;
 try{
  auto cp=stageCenter(view);if(!cp)throw std::runtime_error("No native center");
  auto center=view.$getExistingChunk(*cp);if(!center||center->getState().load()!=ChunkState::DecorationPostProcessing)throw std::runtime_error("Unexpected center phase");
  if(!structureChunkInside(cp->x,cp->z))return ok;
  uint seed=mLevel->getLevelData().getSeed().to32BitRandomSeed();
  std::set<std::pair<int,int>> nearby;
  for(int z=structureCell(cp->z-3);z<=structureCell(cp->z+2);++z)for(int x=structureCell(cp->x-3);x<=structureCell(cp->x+2);++x){auto c=nativeCandidateForCell(x,z,seed);if(c.x<=cp->x+2&&c.x+3>=cp->x&&c.z<=cp->z+2&&c.z+3>=cp->z)nearby.emplace(c.x,c.z);}
  std::vector<ChunkPos> nearbyVillages;
  for(int z=structureCell(cp->z-10);z<=structureCell(cp->z+10);++z)for(int x=structureCell(cp->x-10);x<=structureCell(cp->x+10);++x){auto c=villageCandidate(x,z,seed);if(std::abs(c.x-cp->x)<=10&&std::abs(c.z-cp->z)<=10)nearbyVillages.push_back(c);}
  auto extras=nearbyExtra(*cp,seed);
  if(nearby.empty()&&nearbyVillages.empty()&&extras.empty()&&!smallPotential(*cp,seed)&&!fossilPotential(*cp,seed)){++spacingRejectCalls;spacingRejectNanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-timer.began).count();return ok;}
  std::string dbKey(8,'\0');std::memcpy(dbKey.data(),&cp->x,4);std::memcpy(dbKey.data()+4,&cp->z,4);dbKey.push_back(char(LevelChunkTag::Version));
  auto& db=mLevel->getLevelStorage();bool stored=db.hasKey(dbKey,DBHelpers::Category::Chunk);
  if(!stored){dbKey.back()=char(LevelChunkTag::LegacyVersion);stored=db.hasKey(dbKey,DBHelpers::Category::Chunk);}
  if(stored){++savedStructureSkips;if(fossilPotential(*cp,seed))++fossilSavedSkips;}
  std::lock_guard guard(stageMutex);
  bool alreadyFiltered=prefilteredChunks.erase({cp->x,cp->z})>0;
  // Bound startup diagnostics; do not open a log file for every ordinary chunk.
  if(storageLogRows++<128)std::ofstream(folder/"automatic-storage-checks.jsonl",std::ios::app)<<J{{"chunk",{cp->x,cp->z}},{"stored",stored},{"world",mLevel->getLevelData().getLevelName()}}.dump()<<'\n';
  // Restore seeded native cache records on stored chunks too; never replay
  // saved blocks, opened chests or player edits.
  if(stored)for(auto const&[type,anchor]:extras)extraPlan(center->getDimension(),type,anchor);
  if(!stored&&!extras.empty()){auto tokens=view.enableBlockEntityAccess();BlockSource blocks(view,false,true);placeExtras(center->getDimension(),blocks,*cp,extras,seed,alreadyFiltered);}
  for(auto anchor:nearbyVillages){auto& p=villagePlan(center->getDimension(),anchor,seed);if(!p.start)continue;auto b=*p.start->boundingBox;if(b.max.x+28<cp->x*16||b.min.x-28>cp->x*16+15||b.max.z+28<cp->z*16||b.min.z-28>cp->z*16+15)continue;
   if(stored){if(!(b.max.x<cp->x*16||b.min.x>cp->x*16+15||b.max.z<cp->z*16||b.min.z>cp->z*16+15))queueStructureEntities(p.start,{{cp->x*16,-64,cp->z*16},{cp->x*16+15,319,cp->z*16+15}},"village",anchor,seed);continue;}
   auto tokens=view.enableBlockEntityAccess();BlockSource blocks(view,false,true);auto t=std::chrono::steady_clock::now();auto cleared=alreadyFiltered?0:reserveVillageTrees(blocks,*cp,p);unsigned pieces=0;
   if(!(b.max.x+6<cp->x*16||b.min.x-6>cp->x*16+15||b.max.z+6<cp->z*16||b.min.z-6>cp->z*16+15))pieces=placeVillage(blocks,*cp,p,seed);
   if(pieces)queueStructureEntities(p.start,{{cp->x*16,-64,cp->z*16},{cp->x*16+15,319,cp->z*16+15}},"village",anchor,seed);
   center->setUnsaved();center->markSaveIfNeverSaved();center->markSaveNonActorDataIfDirty();std::ofstream(folder/"village-placements.jsonl",std::ios::app)<<J{{"anchor",{anchor.x*16,anchor.z*16}},{"chunk",{cp->x,cp->z}},{"pieces",pieces},{"cleared",cleared},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()}}.dump()<<'\n';
  }
  for(auto const& [x,z]:nearby){
   auto& plan=autoBlueprint(center->getDimension(),{x,z},seed);if(!plan.start)continue;
   auto const& bounds=*plan.start->boundingBox;
   bool intersects=!(bounds.max.x<cp->x*16||bounds.min.x>cp->x*16+15||bounds.max.z<cp->z*16||bounds.min.z>cp->z*16+15);
   bool forest=plan.start->getStructureName()=="minecraft:jungle_pyramid"||plan.start->getStructureName()=="minecraft:swamp_hut"||bool(plan.igloo);
   if(!intersects&&!forest)continue;
   if(bounds.max.x+28<cp->x*16||bounds.min.x-28>cp->x*16+15||bounds.max.z+28<cp->z*16||bounds.min.z-28>cp->z*16+15)continue;
   if(stored)continue; // Restore native records but never replay saved block edits.
   auto tokens=view.enableBlockEntityAccess();BlockSource blocks(view,false,true);
   if(forest&&!alreadyFiltered){auto t=std::chrono::steady_clock::now();auto cleared=reserveTrees(blocks,*cp,plan);if(cleared){center->setUnsaved();center->markSaveIfNeverSaved();center->markSaveNonActorDataIfDirty();}std::ofstream(folder/"vegetation-reservations.jsonl",std::ios::app)<<J{{"anchor",{x*16,z*16}},{"chunk",{cp->x,cp->z}},{"cleared",cleared},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()}}.dump()<<'\n';}
   if(!intersects)continue;
   BoundingBox clip{{cp->x*16,-64,cp->z*16},{cp->x*16+15,319,cp->z*16+15}};
   Random rng(seed,true);StructureFeature::setRandomSeedFor(rng,cp->x,cp->z,14357617,seed);
   auto t=std::chrono::steady_clock::now();bool placed=placeScatteredNative(blocks,plan,rng,clip);center->setUnsaved();center->markSaveIfNeverSaved();center->markSaveNonActorDataIfDirty();
   if(!plan.igloo){queueStructureEntities(plan.start,clip,std::string(plan.start->getStructureName()),{x,z},seed);for(auto const&piece:*plan.start->pieces)piece->addHardcodedSpawnAreas(*center);}
   double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   std::ofstream(folder/"automatic-placements.jsonl",std::ios::app)<<J{{"type",plan.start->getStructureName()},{"anchor",{x*16,z*16}},{"chunk",{cp->x,cp->z}},{"box",boxJson(*plan.start->boundingBox)},{"groundMin",plan.lo},{"groundMax",plan.hi},{"stage",5},{"ok",placed},{"placementMs",ms}}.dump()<<'\n';
  }
  if(!stored&&(smallPotential(*cp,seed)||fossilPotential(*cp,seed))){
   std::vector<BoundingBox> occupied;
   for(auto const&[type,anchor]:extras){auto it=extraPlans.find({type,anchor.x,anchor.z});if(it!=extraPlans.end()&&it->second.start)occupied.push_back(*it->second.start->boundingBox);}
   for(auto anchor:nearbyVillages){auto it=villages.find({anchor.x,anchor.z});if(it!=villages.end()&&it->second.start)occupied.push_back(*it->second.start->boundingBox);}
   for(auto const&key:nearby){auto it=automaticBlueprints.find(key);if(it!=automaticBlueprints.end()&&it->second.start)occupied.push_back(*it->second.start->boundingBox);}
   auto tokens=view.enableBlockEntityAccess();BlockSource blocks(view,false,true);placeFrozenFossils(center->getDimension(),blocks,*cp,seed,occupied);if(smallPotential(*cp,seed))placeSmallFeatures(center->getDimension(),blocks,*cp,seed,occupied);
  }
 }catch(std::exception const&e){automaticStructures=false;++stageErrors;std::lock_guard guard(stageMutex);std::ofstream(folder/"automatic-errors.log",std::ios::app)<<e.what()<<'\n';return ok;}
 return ok;
}
LL_TYPE_INSTANCE_HOOK(NativeLocate,ll::memory::HookPriority::Normal,FlatWorldGenerator,&FlatWorldGenerator::$findNearestStructureFeature,bool,HashedString type,BlockPos const& from,BlockPos& result,bool fresh,std::optional<HashedString> tag){
 if(!automaticStructures||mLevel->getLevelData().getLevelName()!=activeWorld)return origin(type,from,result,fresh,tag);
 std::lock_guard guard(stageMutex);initialize();auto& d=*mDimension;auto& f=registeredFeature(d);
 std::ofstream(folder/"locate-requests.jsonl",std::ios::app)<<J{{"type",type.getString()},{"registeredType",f.mStructureFeatureType->getString()},{"from",{from.x,from.y,from.z}}}.dump()<<'\n';
 for(auto const& spec:extraSpecs)if(type.getString()==std::string("minecraft:")+extraNames[spec.type]){auto b=d.getBiomeRegistry().lookupByName("minecraft:plains");FixedBiomeSource bs(*b);return registeredExtra(d,spec.type).getNearestGeneratedFeature(d,bs,from,result,provider,fresh,tag);}
 if(type.getString()=="minecraft:village")return locateVillage(d,from,result,fresh);
 if(type.getString()=="minecraft:desert_pyramid"||type.getString()=="minecraft:jungle_pyramid"||type.getString()=="minecraft:swamp_hut"||type.getString()=="minecraft:igloo"){
  auto biome=d.getBiomeRegistry().lookupByName("minecraft:plains");TerrainPointBiomes actual(d,*biome);return f.getNearestGeneratedFeature(d,actual,from,result,provider,fresh,std::optional<HashedString>{type});
 }
 if(type!=*f.mStructureFeatureType)return origin(type,from,result,fresh,tag);
 auto biome=d.getBiomeRegistry().lookupByName("minecraft:plains");TerrainPointBiomes actual(d,*biome);
 // Void's modern structure-set registry is empty; dispatch the registered
 // legacy feature directly, preserving the vanilla /locate command result.
 return f.getNearestGeneratedFeature(d,actual,from,result,provider,fresh,tag);
}
