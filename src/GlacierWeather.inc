// Client precipitation override. Never changes global weather, snow blocks,
// fluid ticks, or terrain generation. Biomes still choose snow versus rain.
#include "GlacierWeatherPolicy.h"
struct GlacierWeatherState {glacier_weather::Policy policy;int x=0,z=0;unsigned samples=0;bool sampled=false,enter=false,stay=false;};
std::unordered_map<std::string,GlacierWeatherState> glacierWeatherPlayers;
uint64_t glacierWeatherPackets=0,glacierWeatherRestores=0;

void glacierWeatherSend(Player& player,SharedTypes::Legacy::LevelEvent event,float strength){
 LevelEventPacketPayload payload{};payload.mEventId=int(event);payload.mPos=Vec3{};
 payload.mData=int(std::clamp(strength,0.f,1.f)*65535.f);
#ifdef SHANHAI_MC_26_51
 LevelEventPacket packet{};
 static_cast<LevelEventPacketPayload&>(packet)=payload;
 packet.mSerializationMode=SerializationMode::ManualOnly;
#else
 LevelEventPacket packet(payload);
#endif
 player.sendNetworkPacket(packet);++glacierWeatherPackets;
}
void glacierWeatherTick(Level& level){

 std::unordered_set<std::string> present;
 level.forEachPlayer([&](Player& player){
  auto name=player.getRealName();present.insert(name);
  auto& state=glacierWeatherPlayers[name];
  auto pos=bedrock_compat::position(player);int x=int(std::floor(pos.x)),z=int(std::floor(pos.z));
  if(player.getDimensionId()!=0){state.enter=state.stay=false;state.sampled=false;}
  else if(!state.sampled||state.x!=x||state.z!=z||++state.samples>=4){
   state.x=x;state.z=z;state.sampled=true;state.samples=0;
   auto& region=player.getDimensionBlockSource();
   auto superAt=[&](int X,int Z){
    int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
    if(!glacier_weather::sampleChunkAllowed(unboundedWorld,cx,cz)||biomes.size()<=24)return false;
    auto chunk=region.getChunk(cx,cz);if(!chunk||bedrock_compat::loadState(*chunk).load()!=ChunkState::Loaded)return false;
    auto& biome=chunk->getBiome(ChunkBlockPos{BlockPos{X-cx*16,64,Z-cz*16},-64});
    return &biome==biomes[24];
   };
   // Stop within cold terrain, before the client sees nearby warm rain.
   // Missing chunks fail closed; no generation or disk reads are requested.
   auto interior=[&](int radius){
    for(int dz:{-radius,0,radius})for(int dx:{-radius,0,radius})
     if(!superAt(x+dx,z+dz))return false;
    return true;
   };
   state.stay=superAt(x,z)&&interior(24);
   state.enter=state.stay&&interior(40);
  }
  int action=state.policy.step(state.enter,state.stay);
  if(action>0){
   glacierWeatherSend(player,SharedTypes::Legacy::LevelEvent::StartRaining,glacier_weather::Strength);
   glacierWeatherSend(player,SharedTypes::Legacy::LevelEvent::StopThunderstorm,0.f);
  }else if(action<0){
   auto& weather=bedrock_compat::weather(bedrock_compat::dimension(player));
   // The old accessor interpolated old/current values; alpha=1 is current.
   float rain=weather.mRainLevel,thunder=weather.mLightningLevel;
   glacierWeatherSend(player,rain>0?SharedTypes::Legacy::LevelEvent::StartRaining:SharedTypes::Legacy::LevelEvent::StopRaining,rain);
   glacierWeatherSend(player,thunder>0?SharedTypes::Legacy::LevelEvent::StartThunderstorm:SharedTypes::Legacy::LevelEvent::StopThunderstorm,thunder);
   ++glacierWeatherRestores;
  }
  return true;
 });
 for(auto it=glacierWeatherPlayers.begin();it!=glacierWeatherPlayers.end();)
  if(!present.contains(it->first))it=glacierWeatherPlayers.erase(it);else ++it;
}
