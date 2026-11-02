// Only a mountains&seas world's biome enum is replaced. Structure locating
// and ordinary worlds retain the game's command handling.
constexpr int ModBiomeEnumBase=0x4000;
bool modBiomeWorld(Level const* level){
 return active&&level&&bedrock_compat::levelName(level->getLevelData())==worldName;
}
bool locateChinese(CommandOrigin const& origin){
 if(auto* entity=origin.getEntity();entity&&entity->isPlayer()){
  if(auto connection=static_cast<Player*>(entity)->getConnectionRequest())
   return terrain_runtime::biome_catalog::chinese(connection->getLanguageCode());
#ifdef KARMA_CLIENT
  return terrain_runtime::biome_catalog::chinese(*getI18n().getCurrentLanguage()->mCode);
#endif
 }
 return false;
}
struct BiomeLocateRequest {
 std::shared_ptr<Session> owner;
 mce::UUID player;
 bool toPlayer=false,chinese=false;
 int catalogId=-1;
 std::unique_ptr<terrain_runtime::BiomeSearchJob> job;
};
std::unique_ptr<BiomeLocateRequest> biomeLocateRequest;
void clearBiomeLocate(){biomeLocateRequest.reset();}

LL_STATIC_HOOK(LocateBiomeSetup,ll::memory::HookPriority::Normal,&LocateCommand::setup,
 void,CommandRegistry& registry,Level const& level){
 origin(registry,level);
 try{
  auto signature=registry.mSignatures.find("locate");
  if(signature==registry.mSignatures.end())throw std::runtime_error("locate signature missing");
  std::string enumName;
  for(auto const& overload:signature->second.overloads)for(auto const& param:overload.params)
   if(param.mName=="biome"&&param.mEnumNameOrPostfix)enumName=param.mEnumNameOrPostfix;
  auto found=registry.mEnumLookup.find(enumName);
  if(enumName.empty()||found==registry.mEnumLookup.end())throw std::runtime_error("locate biome enum missing");
  auto index=found->second;
  auto& enumeration=registry.mEnums.at(index);
  // A reused registry must not retain a previous mod world's candidates.
  std::erase_if(enumeration.values,[](auto const& value){
   return value.second>=ModBiomeEnumBase&&value.second<ModBiomeEnumBase+terrain_runtime::biome_catalog::entries().size();
  });
  if(!modBiomeWorld(&level))return;
  auto session=publishedSession.load();
  std::vector<std::pair<std::string,uint64>> values;
  auto const& all=terrain_runtime::biome_catalog::entries();
  bool elevation=session->nativeData.at("config").value("elevationRevision",0)>=1;
  for(size_t i=0;i<all.size();++i)
   if(terrain_runtime::biome_catalog::available(all[i],metadata.at("biomeNames").size(),elevation))
    values.emplace_back(all[i].id(),ModBiomeEnumBase+i);
  auto originalValues=enumeration.values;
  auto type=enumeration.type;auto parse=enumeration.parse;
  enumeration.values.clear();
  try{registry._addEnumValuesInternal(enumName,values,type,parse);}
  catch(...){registry.mEnums.at(index).values=std::move(originalValues);throw;}
  session->bind(&level);
  Json registered=Json::array();for(auto const& value:registry.mEnums.at(index).values)
   registered.push_back({{"name",registry.mEnumValues.at(value.first)},{"value",value.second}});
  std::ofstream(Base/"biome-command-catalog.json")<<Json{{"world",worldName},{"enum",enumName},{"entries",registered}}.dump(2);
  log().info("Registered {} peakarma biome candidates for {}",values.size(),worldName);
 }catch(std::exception const&e){log().error("Biome command catalog registration failed: {}",e.what());}
}

LL_TYPE_INSTANCE_HOOK(LocateModBiome,ll::memory::HookPriority::Normal,LocateCommand,&LocateCommand::$execute,
 void,CommandOrigin const& commandOrigin,CommandOutput& output){
 auto session=publishedSession.load();auto* dimension=commandOrigin.getDimension();
 if(mSubcommand!=LocateSubcommand::Biome){origin(commandOrigin,output);return;}
 bool zh=locateChinese(commandOrigin);
 auto message=[zh](char const* en,char const* cn){return zh?cn:en;};
 int id=int(mBiome)-ModBiomeEnumBase;
 auto const& all=terrain_runtime::biome_catalog::entries();
 if(!modBiomeWorld(commandOrigin.getLevel())||!dimension||int(dimension->getDimensionId())!=0){
  if(id>=0&&size_t(id)<all.size())
   output.error(message("mountains&seas: these biomes can only be located in a mountains&seas overworld.",
                        "山&海：这些群系只能在山海存档的主世界中检索。"));
  else origin(commandOrigin,output);
  return;
 }
 if(id<0||size_t(id)>=all.size()){
  output.error(message("mountains&seas: choose a peakarma: biome from the command suggestions.",
                       "山&海：请从命令候选中选择 peakarma: 群系。"));return;
 }
 if(biomeLocateRequest){output.error(message("mountains&seas: another biome search is running.","山&海：已有群系检索正在进行，请等待结果。"));return;}
 try{
  auto lease=session->tasks.tryEnter();if(!lease){output.error(message("mountains&seas: this world is closing.","山&海：当前世界正在关闭。"));return;}
  if(!terrain_runtime::biome_catalog::available(all[id],metadata.at("biomeNames").size(),
      session->nativeData.at("config").value("elevationRevision",0)>=1)){
   output.error(message("mountains&seas: this biome is unavailable in this world's terrain recipe.","山&海：当前存档的地形配方不包含这个群系。"));return;
  }
  auto request=std::make_unique<BiomeLocateRequest>();request->owner=session;request->catalogId=id;request->chinese=zh;
  if(auto* entity=commandOrigin.getEntity();entity&&entity->isPlayer()){
   request->player=static_cast<Player*>(entity)->getUuid();request->toPlayer=true;
  }
  auto pos=commandOrigin.getBlockPosition();
  request->job=std::make_unique<terrain_runtime::BiomeSearchJob>(session->nativeData,pos.x,pos.z,std::vector{id},generationMode.unbounded,
      []{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);},true);
  biomeLocateRequest=std::move(request);
  output.success("{} — {}: {}",all[id].id(),all[id].title(zh),all[id].description(zh));
  output.success(message("mountains&seas: searching this world in the background; coordinates will follow.",
                         "山&海：正在后台检索当前存档，完成后显示坐标。"));
 }catch(std::exception const&e){output.error(message("mountains&seas: biome search could not start.","山&海：群系检索启动失败。"));log().error("Biome locate start failed: {}",e.what());}
}
void tickBiomeLocate(Level& level){
 if(!biomeLocateRequest)return;
 auto& request=*biomeLocateRequest;
 if(!request.owner->belongsTo(&level)||request.owner!=publishedSession.load()){clearBiomeLocate();return;}
 if(!request.job->done())return;
 auto const&r=request.job->get();std::string message;bool zh=request.chinese;
 if(!r.error.empty()){message=zh?"§c[山&海] 群系检索失败，请查看日志。":"§c[mountains&seas] Biome search failed; see the log.";log().error("Biome locate failed: {}",r.error);}
 else if(r.found){
  auto p=*r.found;auto const& biome=terrain_runtime::biome_catalog::entries().at(request.catalogId);
  message=fmt::format("§b[{}] {} — {}: {} {} {}\n{}",zh?"山&海":"mountains&seas",biome.id(),biome.title(zh),p[0],p[1],p[2],biome.description(zh));
 }
 else if(r.interrupted)message=zh?"§e[山&海] 本次检索达到时间或采样上限，尚未找到匹配群系。可换个位置再试。":"§e[mountains&seas] Search time or sample limit reached. No match yet; try another position.";
 else message=zh?fmt::format("§e[山&海] 周围 {} 格的采样范围内未找到匹配群系。",r.radius)
                :fmt::format("§e[mountains&seas] No matching biome in the sampled area within {} blocks.",r.radius);
 if(request.toPlayer)level.forEachPlayer([&](Player&player){if(player.getUuid()==request.player)player.sendMessage(message);return true;});
 else log().warn("Biome locate result: {}",message);
 clearBiomeLocate();
}

#ifdef KARMA_CLIENT
LL_TYPE_INSTANCE_HOOK(LocateBiomeDescriptions,ll::memory::HookPriority::Normal,CommandRegistry,
 &CommandRegistry::getAutoCompleteOptions,std::unique_ptr<AutoCompleteInformation>,
 CommandOrigin const& commandOrigin,std::string const& commandLine,uint cursor) {
 auto result=origin(commandOrigin,commandLine,cursor);
 if(!result)return result;
 bool zh=terrain_runtime::biome_catalog::chinese(*getI18n().getCurrentLanguage()->mCode);
 for(auto& option:*result->possibilities){
  int id=terrain_runtime::biome_catalog::find(*option.tabCompleteText);
  if(id<0)id=terrain_runtime::biome_catalog::find(*option.visualText);
  if(id<0)continue;
  auto const& biome=terrain_runtime::biome_catalog::entries()[id];
  option.description=biome.title(zh)+" — "+biome.description(zh);
 }
 return result;
}
#endif
void biomeLocateHooks(terrain_runtime::HookTransaction& hooks){
 hooks.add<LocateBiomeSetup>("terrain::LocateBiomeSetup");
 hooks.add<LocateModBiome>("terrain::LocateModBiome");
#ifdef KARMA_CLIENT
 hooks.add<LocateBiomeDescriptions>("terrain::LocateBiomeDescriptions");
#endif
}
void biomeLocateUnhook(){
 clearBiomeLocate();LocateModBiome::unhook();LocateBiomeSetup::unhook();
#ifdef KARMA_CLIENT
 LocateBiomeDescriptions::unhook();
#endif
}
