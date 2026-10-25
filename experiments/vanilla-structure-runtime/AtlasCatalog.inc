// Explicit, read-only map export. No world chunks, entities, or native locate
// records are created. Candidate work is paced, with private bounded caches.
struct AtlasCatalog {
 bool active=false;int type=0,index=0,visited=0,total=0,lastSaved=0;double maxBatchMs=0;
 std::unique_ptr<nativegen::Field> terrain;
 std::unique_ptr<TerrainVillageFeature> delegate;
 J rows=J::array();std::string error;
 void save(){J meta=entity_store::read((terrainFolder()/"terrain.json"));J out{{"world",activeWorld},{"profileSha256",meta.at("sha256")},{"bounds",{-20000,-20000,19999,19999}},{"complete",!active&&error.empty()},{"visited",visited},{"total",total},{"maxBatchMs",maxBatchMs},{"error",error},{"status","terrain-qualified blueprint candidates, not generated blocks"},{"rows",rows}};
  auto temp=folder/"atlas-catalog.tmp";{std::ofstream f(temp);f<<out.dump();if(!f)throw std::runtime_error("Atlas export write failed");}if(!MoveFileExW(temp.c_str(),(folder/"atlas-catalog.json").c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Atlas export replace failed");
 }
 void start(Dimension&d){if(active)throw std::runtime_error("Atlas export already running");type=0;index=visited=total=0;rows=J::array();error.clear();maxBatchMs=0;terrain=std::make_unique<nativegen::Field>(input);delegate=std::make_unique<TerrainVillageFeature>(structureSeed(d),d.getBiomeRegistry());for(int t=0;t<14;++t){if(t==7&&!mansionHabitatAvailable)continue;int s=t==12?villageSpacing():t==13?32:extraSpec(t)->spacing;int n=spacingCell(terrain_bounds::HalfChunks-1,s)-spacingCell(-terrain_bounds::HalfChunks,s)+1;total+=n*n;}active=true;save();}
} atlasCatalog;
void tickAtlasCatalog(Level&level){
 if(!atlasCatalog.active||interactionAuditActive)return;
 // A single original template plan is not preemptible and may exceed the
 // batch target. Never run this optional whole-world export while playing.
 bool human=false;level.forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human)return;
 auto dim=level.getDimension(0).lock();if(!dim)return;
 auto began=std::chrono::steady_clock::now();
 try{
  std::unique_lock guard(stageMutex,std::try_to_lock);if(!guard.owns_lock())return;
  // Swap only while holding the same mutex used by world generation. Input is
  // the current frozen seed. Destruction restores every live cache on errors.
  struct Context {
   decltype(automaticBlueprints) scattered;decltype(villages) village;
   decltype(extraPlans) extras;decltype(extraDelegates) delegates;
   decltype(cityLinks) links;decltype(cityNeighbourCache) neighbours;
   decltype(vegetationPairs) vegetation;std::vector<std::unique_ptr<StructureStart>> owned;
   void swap(){field.swap(atlasCatalog.terrain);automaticBlueprints.swap(scattered);villages.swap(village);extraPlans.swap(extras);extraDelegates.swap(delegates);cityLinks.swap(links);cityNeighbourCache.swap(neighbours);vegetationPairs.swap(vegetation);}
   Context(){swap();auditPlanningStarts=&owned;auditVillageDelegate=atlasCatalog.delegate.get();}
   ~Context(){swap();auditPlanningStarts=nullptr;auditVillageDelegate=nullptr;adaptHeightMap=false;planningHeights=nullptr;}
  } context;
  uint seed=structureSeed(*dim);for(int budget=0;budget<64&&atlasCatalog.type<14;++budget){
   int t=atlasCatalog.type;if(t==7&&!mansionHabitatAvailable){++atlasCatalog.type;atlasCatalog.index=0;continue;}
   int spacing=t==12?villageSpacing():t==13?32:extraSpec(t)->spacing,lo=spacingCell(-terrain_bounds::HalfChunks,spacing),hi=spacingCell(terrain_bounds::HalfChunks-1,spacing),side=hi-lo+1;
   if(atlasCatalog.index>=side*side){++atlasCatalog.type;atlasCatalog.index=0;continue;}
   int i=atlasCatalog.index++,x=lo+i%side,z=lo+i/side;++atlasCatalog.visited;
   ChunkPos cp=t==12?villageCandidate(x,z,seed):t==13?nativeCandidateForCell(x,z,seed):extraCandidate(*extraSpec(t),x,z,seed);
   if(terrain_bounds::containsChunk(cp.x,cp.z))if(auto*p=auditPlan(*dim,t,cp,seed)){
    auto s=field->surface(cp.x*16+32768,cp.z*16+32768);atlasCatalog.rows.push_back({{"family",p->getStructureName()},{"anchor",{cp.x*16,cp.z*16}},{"box",boxJson(*p->boundingBox)},{"biome",s.biome},{"pieces",p->pieces->size()},{"status","planned"}});
   }
   if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()>=20.)break;
  }
 }catch(std::exception const&e){atlasCatalog.error=e.what();atlasCatalog.active=false;}
 atlasCatalog.maxBatchMs=std::max(atlasCatalog.maxBatchMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
 if(atlasCatalog.type>=14)atlasCatalog.active=false;
 if(atlasCatalog.visited-atlasCatalog.lastSaved>=2048||!atlasCatalog.active){try{atlasCatalog.save();atlasCatalog.lastSaved=atlasCatalog.visited;}catch(std::exception const&e){std::ofstream(folder/"atlas-error.log",std::ios::app)<<e.what()<<'\n';}if(!atlasCatalog.active){atlasCatalog.terrain.reset();atlasCatalog.delegate.reset();}}
}
