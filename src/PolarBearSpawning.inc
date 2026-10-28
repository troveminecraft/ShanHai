// Read loaded blocks only. No terrain calculation, chunk requests or disk work
// in a natural-spawn callback, on either host.
ll::event::ListenerPtr polarBearListener;
std::atomic_uint64_t polarBearAllowed=0,polarBearRejected=0;
void stopPolarBearSpawning(){
 if(polarBearListener){ll::event::EventBus::getInstance().removeListener(polarBearListener);polarBearListener.reset();}
}
void startPolarBearSpawning(){
 if(polarBearListener)return;
 polarBearAllowed=0;polarBearRejected=0;
 polarBearListener=ll::event::EventBus::getInstance().emplaceListener<ll::event::SpawningMobEvent>([](auto&e){
  if(!active||*e.identifier().mFullName!="minecraft:polar_bear")return;
  auto session=publishedSession.load();auto work=session->tasks.tryEnter();if(!work)return;
  auto& region=e.blockSource();auto& dim=bedrock_compat::dimension(region);
  if(!glacier_ecology::restrictPolarBear(session->belongsTo(&bedrock_compat::level(dim)),static_cast<int>(dim.getDimensionId())==0,e.naturalSpawn(),e.fromSpawner()))return;
  auto p=e.pos();int x=int(std::floor(p.x)),z=int(std::floor(p.z)),y=int(std::floor(p.y))-1;
  int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
  auto chunk=region.getChunk(cx,cz);bool allow=false;
  if(chunk&&bedrock_compat::loadState(*chunk).load()==ChunkState::Loaded&&biomes.size()>24&&palette.size()>148){
   auto& biome=chunk->getBiome(ChunkBlockPos{BlockPos{x-cx*16,64,z-cz*16},-64});
   bool dense=false;
   // Skip a snow layer or the air above it, but never look through a lake's
   // ordinary ice sheet into water to invent a glacier substrate.
   for(int i=0;i<3&&y-i>=-64&&y-i<buildHeight.maxYExclusive;++i){
    auto const* block=&chunk->getBlock(ChunkBlockPos{BlockPos{x-cx*16,y-i,z-cz*16},-64});
    if(block==palette[143]||block==palette[144]){dense=true;break;}
    if(block!=palette[0]&&block!=palette[9]&&block!=palette[146]&&block!=palette[147]&&block!=palette[148])break;
   }
   allow=glacier_ecology::polarBearHabitat(&biome==biomes[2],&biome==biomes[19],&biome==biomes[5]||&biome==biomes[24],dense);
  }
  if(allow)++polarBearAllowed;else{e.cancel();++polarBearRejected;}
 });
}
