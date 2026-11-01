// Preserve the game's own argument/alias resolution. The native biome enum is
// opaque on 26.51, so capture the predicate that /locate actually constructs.
// This scope exists only during this mod's command interception, on this thread.
struct BiomeLocateCapture {Dimension* dimension;bool called=false;std::vector<int> ids;};
thread_local BiomeLocateCapture* biomeLocateCapture=nullptr;
struct BiomeLocateRequest {
 std::shared_ptr<Session> owner;
 mce::UUID player;
 bool toPlayer=false;
 std::vector<std::string> names;
 std::unique_ptr<terrain_runtime::BiomeSearchJob> job;
};
std::unique_ptr<BiomeLocateRequest> biomeLocateRequest;
void clearBiomeLocate(){biomeLocateRequest.reset();}
template<class Predicate> void captureBiomeIds(Predicate predicate){
 auto& c=*biomeLocateCapture;c.called=true;
 for(size_t i=0;i<biomes.size();++i)if(biomes[i]&&predicate(*biomes[i]))c.ids.push_back(int(i));
 std::sort(c.ids.begin(),c.ids.end());c.ids.erase(std::unique(c.ids.begin(),c.ids.end()),c.ids.end());
}
LL_STATIC_HOOK(LocateBiomePredicate,ll::memory::HookPriority::Normal,
 &LocateCommandUtil::locateNearbyBiomeByPredicate,std::optional<BlockPos>,
 BlockPos pos,Dimension* dimension,std::function<bool(Biome const&)> const& predicate){
 if(!biomeLocateCapture||biomeLocateCapture->dimension!=dimension)return origin(pos,dimension,predicate);
 captureBiomeIds(predicate);return std::nullopt;
}
#ifndef SHANHAI_MC_26_51
LL_STATIC_HOOK(LocateBiomeSet,ll::memory::HookPriority::Normal,
 &LocateCommandUtil::locateNearbyBiomeInSet,std::optional<BlockPos>,
 BlockPos pos,Dimension* dimension,std::set<BiomeIdType> const& ids){
 if(!biomeLocateCapture||biomeLocateCapture->dimension!=dimension)return origin(pos,dimension,ids);
 captureBiomeIds([&](Biome const&b){return std::any_of(ids.begin(),ids.end(),[&](auto const&id){return id.mValue==b.mId->mValue;});});return std::nullopt;
}
#endif
LL_TYPE_INSTANCE_HOOK(LocateModBiome,ll::memory::HookPriority::Normal,LocateCommand,&LocateCommand::$execute,
 void,CommandOrigin const& commandOrigin,CommandOutput& output){
 auto session=publishedSession.load();auto* dimension=commandOrigin.getDimension();
 if(mSubcommand!=LocateSubcommand::Biome||!active||!session->belongsTo(commandOrigin.getLevel())
    ||!dimension||int(dimension->getDimensionId())!=0){origin(commandOrigin,output);return;}
 if(biomeLocateRequest){output.error("山&海：已有群系检索正在进行，请等待结果。");return;}
 try{
  auto lease=session->tasks.tryEnter();if(!lease){output.error("山&海：当前世界正在关闭。");return;}
  resolveRegistry(*dimension);
  BiomeLocateCapture capture{dimension};
  struct CaptureScope {BiomeLocateCapture* old;CaptureScope(BiomeLocateCapture& c):old(biomeLocateCapture){biomeLocateCapture=&c;}~CaptureScope(){biomeLocateCapture=old;}};
  CommandOutput ignored(CommandOutputType::AllOutput);
  {CaptureScope scope(capture);origin(commandOrigin,ignored);}
  if(!capture.called){output.error("山&海：当前游戏版本的群系检索入口未匹配，未返回原版坐标。");log().error("Biome locate command did not invoke the expected native query");return;}
  if(capture.ids.empty()){output.error("山&海：这个群系不在当前存档的生成配方中。");return;}
  auto request=std::make_unique<BiomeLocateRequest>();request->owner=session;
  if(auto* entity=commandOrigin.getEntity();entity&&entity->isPlayer()){
   request->player=static_cast<Player*>(entity)->getUuid();request->toPlayer=true;
  }
  for(auto const&name:metadata.at("biomeNames"))request->names.push_back(name.get<std::string>());
  auto pos=commandOrigin.getBlockPosition();
  request->job=std::make_unique<terrain_runtime::BiomeSearchJob>(session->nativeData,pos.x,pos.z,std::move(capture.ids),generationMode.unbounded,
      []{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);});
  biomeLocateRequest=std::move(request);
  output.success("山&海：正在后台检索当前存档的群系，完成后显示坐标。");
 }catch(std::exception const&e){output.error("山&海：群系检索启动失败。");log().error("Biome locate start failed: {}",e.what());}
}
void tickBiomeLocate(Level& level){
 if(!biomeLocateRequest)return;
 auto& request=*biomeLocateRequest;
 if(!request.owner->belongsTo(&level)||request.owner!=publishedSession.load()){clearBiomeLocate();return;}
 if(!request.job->done())return;
 auto const&r=request.job->get();std::string message;
 if(!r.error.empty()){message="§c[山&海] 群系检索失败，请查看日志。";log().error("Biome locate failed: {}",r.error);}
 else if(r.found){auto p=*r.found;message=fmt::format("§b[山&海] {}：{} {} {}",request.names.at(p[3]),p[0],p[1],p[2]);}
 else if(r.interrupted)message="§e[山&海] 本次检索达到时间或采样上限，尚未找到匹配群系。可换个位置再试。";
 else message=fmt::format("§e[山&海] 周围 {} 格的采样范围内未找到匹配群系。",r.radius);
 if(request.toPlayer)level.forEachPlayer([&](Player&player){if(player.getUuid()==request.player)player.sendMessage(message);return true;});
 else log().warn("{}",message);
 clearBiomeLocate();
}
void biomeLocateHooks(terrain_runtime::HookTransaction& hooks){
 hooks.add<LocateBiomePredicate>("terrain::LocateBiomePredicate");
#ifndef SHANHAI_MC_26_51
 hooks.add<LocateBiomeSet>("terrain::LocateBiomeSet");
#endif
 hooks.add<LocateModBiome>("terrain::LocateModBiome");
}
void biomeLocateUnhook(){
 clearBiomeLocate();LocateModBiome::unhook();LocateBiomePredicate::unhook();
#ifndef SHANHAI_MC_26_51
 LocateBiomeSet::unhook();
#endif
}
