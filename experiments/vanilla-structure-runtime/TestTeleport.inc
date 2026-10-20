// Optional BDS-only test menu. Planning never loads/saves chunks or registers
// native starts. Keep generation and the client's release configuration intact.

// A requested test catalog must remain visible with sendcommandfeedback=false.
// Send bounded batches directly to its player; console callers retain command output.
struct TestTeleportListOutput {
 Player* player;CommandOutput& command;std::string pending;int lines=0;
 void flush(){if(!pending.empty()){player->sendMessage(pending);pending.clear();lines=0;}}
 template<class... Args> void success(fmt::format_string<Args...> pattern,Args&&...args){
  auto message=fmt::format(pattern,std::forward<Args>(args)...);
  if(!player){command.success(message);return;}
  if(!pending.empty())pending+='\n';pending+=message;
  if(++lines==8)flush();
 }
 void finish(){flush();if(player)command.success();}
};

// Stable biome IDs start at 101; adding them never renumbers structures.
struct TestBiomeGroup {char const* key;char const* name;int biome;};
constexpr TestBiomeGroup testBiomes[]={
 {"deep_ocean","深海",0},{"ocean","海洋",1},{"frozen_ocean","冻洋",2},
 {"beach","沙滩",3},{"cold_beach","寒冷沙滩",4},{"ice_plains","雪原",5},
 {"taiga","针叶林",6},{"plains","平原",7},{"forest","森林",9},
 {"swampland","沼泽",10},{"desert","沙漠",11},{"savanna","热带草原",12},
 {"jungle","丛林",13},{"extreme_hills","山地",14},{"mesa","恶地",15},
 {"river","河流",18},{"frozen_river","冰河",19},{"stone_beach","石岸",20},
 {"warm_ocean","暖海",21},{"cold_ocean","冷海",22},{"birch_forest","白桦林",23},
 {"ice_plains_spikes","冰刺平原",24},{"glacier","普通冰川",-1},
 {"super_glacier","超级冰川",-2},{"volcano","火山",-3},
 {"small_stone_forest","小型石峰林",-4},{"giant_forest","巨木森林",-5}
};
template<class S> bool testBiomeMatches(int i,S const&s){
 int b=testBiomes[i].biome;
 if(b>=0)return s.biome==b||(b==7&&s.biome==8)||(b==6&&s.biome==17)||(b==11&&s.biome==16);
 switch(b){case -1:return s.glacier>.5&&s.superGlacier<.1;case -2:return s.superGlacier>.5;
 case -3:return s.volcanic>.5;case -4:return s.stone>.5;case -5:return s.giant>.5;default:return false;}
}

struct StructureTestTeleport {
 bool enabled=false,started=false,complete=false,paused=false,priorityOnly=true;
 int visited=0,turn=0,saveAt=0,searchRadius=20000;
 std::array<size_t,14> cursor{};
 std::array<std::vector<structure_test::Cell>,14> cells;
 std::array<J,20> spots;
 std::array<J,std::size(testBiomes)> biomeSpots;
 size_t biomeCursor=0;
 std::vector<structure_test::Cell> biomeCells;
 bool biomesDone()const{return std::all_of(biomeSpots.begin(),biomeSpots.end(),[](auto const&j){return j.is_object();})||biomeCursor>=biomeCells.size();}
 int biomeCount()const{return int(std::count_if(biomeSpots.begin(),biomeSpots.end(),[](auto const&j){return j.is_object();}));}
 void sampleBiomes(int x,int z){
  auto s=field->surface(x+32768,z+32768);
  for(size_t i=0;i<biomeSpots.size();++i)if(!biomeSpots[i].is_object()&&testBiomeMatches(int(i),s))
   biomeSpots[i]={{"x",x},{"y",std::clamp(std::max(s.height,s.water)+8,-58,310)},{"z",z},{"family",testBiomes[i].key}};
 }

 std::unique_ptr<nativegen::Field> terrain;
 std::unique_ptr<TerrainVillageFeature> delegate;
 std::string identity,error;
 struct Return {Vec3 position;DimensionType dimension;GameType mode;};
 std::map<std::string,Return> returns;
 ll::event::ListenerPtr leave;
 int count()const{int n=0;for(auto const&s:spots)n+=int(s.size());return n;}
 bool full(int group)const{return spots[group].size()>=size_t(structure_test::Groups[group].quota);}
 bool done(int type)const{
  if(cursor[type]>=cells[type].size())return true;
  for(size_t i=0;i<spots.size();++i)if(structure_test::Groups[i].type==type&&!full(int(i)))return false;
  return true;
 }
 bool priorityCandidate(int type,ChunkPos cp){
  // Once the common types are full, don't prepare rivers across an entire
  // continent just to reject obviously warm, non-desert sites. This only
  // orders the search: exhausted priority scans retry without this screen.
  if(!priorityOnly||!field->streamed()||!input.at("config").value("deserts",J::object()).value("largeSandOnly",false))return true;
  bool desertOnly=type==13&&full(6)&&full(7)&&full(8);
  bool coldOrDesert=type==12&&full(1)&&full(2)&&full(4);
  if(!desertOnly&&!coldOrDesert)return true;
  int x=cp.x*16+32768,z=cp.z*16+32768;
  if(field->desertRegionWeight(x,z)>.4)return true;
  return coldOrDesert&&!full(0)&&field->minimumNaturalTemperature(x,z)<.35;
 }
 void save(){
  J out{{"identity",identity},{"complete",complete},{"visited",visited},{"cursor",cursor},
   {"biomes",biomeSpots},{"biomeCursor",biomeCursor},{"biomeExpected",std::size(testBiomes)},{"biomeFound",biomeCount()},{"biomesComplete",biomesDone()},
   {"spots",spots},{"expected",structure_test::expected()},{"found",count()},{"error",error},{"searchRadius",searchRadius},{"priorityOnly",priorityOnly},
   {"status","planned from current terrain; blocks generate when visited"}};
  auto tmp=folder/"test-teleports.json.tmp";
  {std::ofstream f(tmp);f<<out.dump(2);if(!f)throw std::runtime_error("Cannot write test teleport catalog");}
  if(!MoveFileExW(tmp.c_str(),(folder/"test-teleports.json").c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot replace test teleport catalog");
  saveAt=visited;
 }
 void initialize(J const&config){
  enabled=config.value("testTeleport",false);if(!enabled)return;
  auto activation=entity_store::read(terrainFolder()/"active-world.json");
  identity=activeWorld+":"+activation.at("worldSeed").get<std::string>()+":"+activation.at("sha256").get<std::string>()+":1.0.30";
  if(field->unbounded())searchRadius=std::clamp(config.value("testSearchRadius",20000)/20000,1,5)*20000;
  for(auto&s:spots)s=J::array();
  biomeCells=structure_test::expandingCells(16,searchRadius);
  for(int t=0;t<14;++t){if(t==7)continue;int spacing=t>=12?32:extraSpec(t)->spacing;cells[t]=structure_test::expandingCells(spacing,searchRadius);}
  auto path=folder/"test-teleports.json";
  if(std::filesystem::exists(path))try{
   auto j=entity_store::read(path);
   if(j.value("identity",std::string{})==identity){
    auto saved=j.at("spots").get<decltype(spots)>();auto next=j.at("cursor").get<decltype(cursor)>();
    for(size_t g=0;g<saved.size();++g){if(!saved[g].is_array()||saved[g].size()>size_t(structure_test::Groups[g].quota))throw std::runtime_error("Invalid test group");
     for(auto const&s:saved[g]){int x=s.at("x"),y=s.at("y"),z=s.at("z");if(x< -searchRadius||x>=searchRadius||z< -searchRadius||z>=searchRadius||y< -58||y>310)throw std::runtime_error("Invalid test destination");}}
    for(int t=0;t<14;++t)if(next[t]>cells[t].size())throw std::runtime_error("Invalid test cursor");
    if(j.contains("biomes")){
     auto savedBiomes=j.at("biomes").get<decltype(biomeSpots)>();auto nextBiome=j.at("biomeCursor").get<size_t>();
     if(nextBiome>biomeCells.size())throw std::runtime_error("Invalid biome cursor");
     for(size_t i=0;i<savedBiomes.size();++i)if(!savedBiomes[i].is_null()){
      auto const&b=savedBiomes[i];int x=b.at("x"),y=b.at("y"),z=b.at("z");
      if(b.at("family")!=testBiomes[i].key||x< -searchRadius||x>=searchRadius||z< -searchRadius||z>=searchRadius||y< -58||y>310)throw std::runtime_error("Invalid biome destination");
     }
     biomeSpots=std::move(savedBiomes);biomeCursor=nextBiome;
    }
    spots=std::move(saved);cursor=next;visited=j.at("visited");priorityOnly=j.value("priorityOnly",true);complete=true;
    for(int t=0;t<14;++t)if(!done(t))complete=false;
   }
  }catch(std::exception const&e){ll::mod::NativeMod::current()->getLogger().warn("Rebuilding test teleport catalog: {}",e.what());}
 }
 void list(TestTeleportListOutput&out)const{
  out.success("测试传送 {}/{}：/karma tp 序号；/karma back 返回原位置和模式。",count(),structure_test::expected());
  if(!error.empty())out.success("坐标准备中断：{}",error);
  else if(!complete)out.success("坐标在空服时分批准备；已收录的编号保持不变。当前已检查 {} 个候选。",visited);
  for(size_t g=0;g<spots.size();++g){auto const&group=structure_test::Groups[g];
   for(size_t i=0;i<spots[g].size();++i){auto const&s=spots[g][i];out.success("[{}] {} {}：{} {} {}",structure_test::firstId(int(g))+int(i),group.name,i+1,s.at("x").get<int>(),s.at("y").get<int>(),s.at("z").get<int>());}
   if(complete&&!full(int(g)))out.success("{}：范围内只找到 {}/{} 个可用位置",group.name,spots[g].size(),group.quota);
  }
 }
 void listBiomes(TestTeleportListOutput&out)const{
  out.success("群系传送 {}/{}（101 起）；空服时继续搜索，每种保留一个位置。",biomeCount(),std::size(testBiomes));
  for(size_t i=0;i<biomeSpots.size();++i){auto const&b=biomeSpots[i];
   if(b.is_object())out.success("[{}] {}：{} {} {}",101+i,testBiomes[i].name,b.at("x").get<int>(),b.at("y").get<int>(),b.at("z").get<int>());
   else out.success("[{}] {}：{}",101+i,testBiomes[i].name,biomesDone()?"搜索范围内未找到":"搜索中");
  }
 }
 void restore(Player&p){auto it=returns.find(p.getRealName());if(it==returns.end())return;auto r=it->second;p.teleport(r.position,r.dimension);p.setPlayerGameType(r.mode);returns.erase(it);}
} structureTest;

// The catalog borrows the adapter's pure planning entry points under its usual
// mutex. Every live cache is restored even if a native planner throws.
struct StructureTestPlanScope {
 decltype(automaticBlueprints) scattered;decltype(villages) village;
 decltype(extraPlans) extras;decltype(extraDelegates) delegates;
 decltype(cityLinks) links;decltype(cityNeighbourCache) neighbours;
 decltype(vegetationPairs) vegetation;std::vector<std::unique_ptr<StructureStart>> owned;
 void swap(){field.swap(structureTest.terrain);automaticBlueprints.swap(scattered);villages.swap(village);extraPlans.swap(extras);extraDelegates.swap(delegates);cityLinks.swap(links);cityNeighbourCache.swap(neighbours);vegetationPairs.swap(vegetation);}
 StructureTestPlanScope(){swap();auditPlanningStarts=&owned;auditVillageDelegate=structureTest.delegate.get();}
 ~StructureTestPlanScope(){swap();auditPlanningStarts=nullptr;auditVillageDelegate=nullptr;adaptHeightMap=false;planningHeights=nullptr;}
};
void tickStructureTest(Level&level){
 auto&s=structureTest;if(!s.enabled||(s.complete&&s.biomesDone())||!s.error.empty()||interactionAuditActive||atlasCatalog.active)return;
 bool human=false;level.forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});s.paused=human;if(human)return;
 try{
  auto dim=level.getOrCreateDimension(0).lock();if(!dim||!villageTemplatesReady)return;
  std::unique_lock guard(stageMutex,std::try_to_lock);if(!guard.owns_lock())return;
  if(!s.started){s.terrain=std::make_unique<nativegen::Field>(input);s.delegate=std::make_unique<TerrainVillageFeature>(structureSeed(*dim),dim->getBiomeRegistry());s.started=true;}
  bool changed=false;
  int oldBiomes=s.biomeCount();
  {
   StructureTestPlanScope context;
   auto began=std::chrono::steady_clock::now();uint seed=structureSeed(*dim);
   if(!s.biomesDone()){
    auto c=s.biomeCells[s.biomeCursor++];int x=c.x*256+128,z=c.z*256+128;
    if(x>=-s.searchRadius&&x<s.searchRadius&&z>=-s.searchRadius&&z<s.searchRadius&&structureChunkInside(chunk(x),chunk(z)))s.sampleBiomes(x,z);
    changed=true;
   }
   for(int budget=0;budget<32;++budget){
    int type=-1;for(int n=0;n<14;++n){int t=s.turn++%14;if(!s.done(t)){type=t;break;}}
    if(type<0){
     bool rareMissing=false;for(int g=0;g<9;++g)rareMissing|=!s.full(g);
     if(s.priorityOnly&&rareMissing){s.priorityOnly=false;s.cursor[12]=s.cursor[13]=0;changed=true;continue;}
     s.complete=true;break;
    }
    auto cell=s.cells[type][s.cursor[type]++];++s.visited;
    ChunkPos cp=type==12?villageCandidate(cell.x,cell.z,seed):type==13?nativeCandidateForCell(cell.x,cell.z,seed):extraCandidate(*extraSpec(type),cell.x,cell.z,seed);
    if(cp.x*16>=-s.searchRadius&&cp.x*16<s.searchRadius&&cp.z*16>=-s.searchRadius&&cp.z*16<s.searchRadius&&structureChunkInside(cp.x,cp.z)&&s.priorityCandidate(type,cp)){
     auto surface=field->surface(cp.x*16+32768,cp.z*16+32768);
     s.sampleBiomes(cp.x*16,cp.z*16);
     int group=structure_test::group(type,type==12?villageStyle(surface.biome):surface.biome);
     bool duplicate=group>=0&&std::any_of(s.spots[group].begin(),s.spots[group].end(),[&](auto const&row){return row.at("anchor")==J::array({cp.x,cp.z});});
     if(group>=0&&!s.full(group)&&!duplicate)if(auto*plan=auditPlan(*dim,type,cp,seed)){
      auto box=*plan->boundingBox;bool saved=false;
      // Old chunks from before structure support must not become empty targets.
      for(int z=chunk(box.min.z);z<=chunk(box.max.z)&&!saved;++z)for(int x=chunk(box.min.x);x<=chunk(box.max.x)&&!saved;++x)saved=storedChunk(*dim,{x,z});
      if(!saved){
       auto const&g=structure_test::Groups[group];int x=(box.min.x+box.max.x)/2,z=(box.min.z+box.max.z)/2;
       int y=worldgen_rules::extraUnderground(type)?std::clamp(box.min.y+4,-58,310):std::clamp(std::max(box.max.y,field->surface(x+32768,z+32768).height)+5,-58,310);
       s.spots[group].push_back({{"x",x},{"y",y},{"z",z},{"anchor",{cp.x,cp.z}},{"box",boxJson(box)},{"family",g.key}});changed=true;
      }
     }
    }
    if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()>=10.)break;
   }
  }
  if(s.biomeCount()!=oldBiomes||s.complete&&s.biomesDone()||s.visited-s.saveAt>=128||changed&&s.biomeCursor%64==0)s.save();
  if(s.complete&&s.biomesDone()){s.terrain.reset();s.delegate.reset();ll::mod::NativeMod::current()->getLogger().info("Test teleport catalog ready: {}/{} destinations",s.count(),structure_test::expected());}
 }catch(std::exception const&e){s.error=e.what();s.terrain.reset();s.delegate.reset();ll::mod::NativeMod::current()->getLogger().error("Test teleport catalog paused: {}",s.error);try{s.save();}catch(...){};}
}
LL_TYPE_INSTANCE_HOOK(StructureTestPump,ll::memory::HookPriority::Normal,ServerInstance,&ServerInstance::_update,void){
 origin();
 // Empty BDS worlds may have no level ticks. Run from the enclosing server
 // update, on the same thread, without forcing world ticks or loading chunks.
 if(!structureTest.enabled||(structureTest.complete&&structureTest.biomesDone())||!structureTest.error.empty())return;
 static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(now<next)return;
 next=now+std::chrono::milliseconds(50);
 if(auto level=ll::service::getLevel())tickStructureTest(*level);
}
void registerStructureTest(J const&config){
 structureTest.initialize(config);if(!structureTest.enabled)return;
 auto&cmd=ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("karma","K-worldgen 测试传送",CommandPermissionLevel::Any);
 cmd.runtimeOverload().text("tp").execute([](CommandOrigin const&o,CommandOutput&out,ll::command::RuntimeCommand const&){
  auto*a=o.getEntity();TestTeleportListOutput list{a&&a->isPlayer()?static_cast<Player*>(a):nullptr,out};
  structureTest.list(list);structureTest.listBiomes(list);list.finish();
 });
 cmd.runtimeOverload().text("tp").required("index",ll::command::ParamKind::Int).execute([](CommandOrigin const&o,CommandOutput&out,ll::command::RuntimeCommand const&c){
  auto*a=o.getEntity();if(!a||!a->isPlayer()){out.error("请在游戏内使用 /karma tp 序号");return;}
  int id=std::get<int>(*c["index"].as_ptr());
  if(id>=101&&id<101+int(std::size(testBiomes))){
   auto const&s=structureTest.biomeSpots[id-101];if(!s.is_object()){out.error("该群系坐标尚未找到；/karma tp 查看进度。");return;}
   auto&p=static_cast<Player&>(*a);structureTest.returns.try_emplace(p.getRealName(),StructureTestTeleport::Return{p.getPosition(),p.getDimensionId(),p.getPlayerGameType()});
   p.setPlayerGameType(GameType::Spectator);p.teleport(Vec3{float(s.at("x").get<int>())+.5f,float(s.at("y").get<int>()),float(s.at("z").get<int>())+.5f},0);
   out.success("已前往 [{}] {}；/karma back 返回。",id,testBiomes[id-101].name);return;
  }
  for(size_t g=0;g<structureTest.spots.size();++g){int i=id-structure_test::firstId(int(g));if(i<0||i>=int(structureTest.spots[g].size()))continue;
   auto const&s=structureTest.spots[g][i];auto&p=static_cast<Player&>(*a);
   structureTest.returns.try_emplace(p.getRealName(),StructureTestTeleport::Return{p.getPosition(),p.getDimensionId(),p.getPlayerGameType()});
   p.setPlayerGameType(GameType::Spectator);p.teleport(Vec3{float(s.at("x").get<int>())+.5f,float(s.at("y").get<int>()),float(s.at("z").get<int>())+.5f},0);
   out.success("已前往 [{}] {} {}，旁观模式等待附近区块生成；/karma back 返回。",id,structure_test::Groups[g].name,i+1);return;
  }out.error("编号无效或坐标尚未准备好；/karma tp 查看当前列表。");
 });
 cmd.runtimeOverload().text("back").execute([](CommandOrigin const&o,CommandOutput&out,ll::command::RuntimeCommand const&){
  auto*a=o.getEntity();if(!a||!a->isPlayer()){out.error("请在游戏内使用");return;}auto&p=static_cast<Player&>(*a);
  if(!structureTest.returns.contains(p.getRealName())){out.error("没有待恢复的测试传送");return;}structureTest.restore(p);out.success("已返回测试前的位置和游戏模式。");
 });
 structureTest.leave=ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerDisconnectEvent>([](auto&e){structureTest.restore(e.self());});
 structureTest.save();StructureTestPump::hook();
 ll::mod::NativeMod::current()->getLogger().info("Temporary /karma tp enabled: three per village style, two per structure, one per biome");
}
void stopStructureTest(){
 structureTest.enabled=false;
 StructureTestPump::unhook();
 if(auto level=ll::service::getLevel())level->forEachPlayer([](Player&p){structureTest.restore(p);return true;});
 if(structureTest.leave)ll::event::EventBus::getInstance().removeListener(structureTest.leave);
 structureTest.enabled=false;structureTest.terrain.reset();structureTest.delegate.reset();
}
