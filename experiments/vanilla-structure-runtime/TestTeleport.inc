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
 {"small_stone_forest","小型石峰林",-4},{"giant_forest","巨木森林",-5},
 {"mushroom_island","蘑菇岛",25},{"mesa_bryce","侵蚀恶地",26},
 {"mesa_plateau","恶地高原",27},{"mesa_plateau_stone","繁茂恶地高原",28}
};
template<class S> bool testBiomeMatches(int i,S const&s){
 int b=testBiomes[i].biome;
 if(b==11)return test_teleport::desertInterior(s);
 if(b>=0)return s.biome==b||(b==15&&s.biome>=26&&s.biome<=28)||(b==7&&s.biome==8)||(b==6&&s.biome==17)||(b==11&&s.biome==16);
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
 int maxYExclusive()const{return field->generationHeight()-64;}
 void sampleBiomes(int x,int z){
  if(!biomeSpots[27].is_object()){
   auto island=field->inspectMushroomIsland(int(std::floor((x+32768)/1536.)),int(std::floor((z+32768)/1536.)));
   if(island.at("valid").get<bool>()){
    int ix=island.at("x"),iz=island.at("z");auto q=field->surface(ix+32768,iz+32768);
    if(q.mushroom&&ix>=-searchRadius&&ix<searchRadius&&iz>=-searchRadius&&iz<searchRadius)
     if(auto y=test_teleport::aboveSurface(q.height,8,maxYExclusive()))
      biomeSpots[27]={{"x",ix},{"y",*y},{"z",iz},{"family","mushroom_island"}};
   }
  }
  auto s=field->surface(x+32768,z+32768);
  auto y=test_teleport::aboveSurface(std::max({s.height+s.drip,s.water,s.lava}),8,maxYExclusive());
  if(y)for(size_t i=0;i<biomeSpots.size();++i)if(!biomeSpots[i].is_object()&&testBiomeMatches(int(i),s)){
   if(testBiomes[i].biome==11&&!test_teleport::desertDestination(x,z,[&](int X,int Z){return field->surface(X+32768,Z+32768);}))continue;
   biomeSpots[i]={{"x",x},{"y",*y},{"z",z},{"family",testBiomes[i].key}};
  }
 }

 std::unique_ptr<nativegen::Field> terrain;
 std::unique_ptr<TerrainVillageFeature> delegate;
 std::string identity,error;
 ll::event::ListenerPtr join;
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
  identity=activeWorld+":"+activation.at("worldSeed").get<std::string>()+":"+activation.at("sha256").get<std::string>()+":height-v1:desert-interior-v1";
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
     for(auto const&s:saved[g]){int x=s.at("x"),y=s.at("y"),z=s.at("z");if(x< -searchRadius||x>=searchRadius||z< -searchRadius||z>=searchRadius||!test_teleport::validY(y,maxYExclusive()))throw std::runtime_error("Invalid test destination");}}
    for(int t=0;t<14;++t)if(next[t]>cells[t].size())throw std::runtime_error("Invalid test cursor");
    if(j.contains("biomes")){
     auto savedBiomes=j.at("biomes").get<decltype(biomeSpots)>();auto nextBiome=j.at("biomeCursor").get<size_t>();
     if(nextBiome>biomeCells.size())throw std::runtime_error("Invalid biome cursor");
     for(size_t i=0;i<savedBiomes.size();++i)if(!savedBiomes[i].is_null()){
      auto const&b=savedBiomes[i];int x=b.at("x"),y=b.at("y"),z=b.at("z");
      if(b.at("family")!=testBiomes[i].key||x< -searchRadius||x>=searchRadius||z< -searchRadius||z>=searchRadius||!test_teleport::validY(y,maxYExclusive()))throw std::runtime_error("Invalid biome destination");
     }
     biomeSpots=std::move(savedBiomes);biomeCursor=nextBiome;
    }
    spots=std::move(saved);cursor=next;visited=j.at("visited");priorityOnly=j.value("priorityOnly",true);complete=true;
    for(int t=0;t<14;++t)if(!done(t))complete=false;
   }
  }catch(std::exception const&e){ll::mod::NativeMod::current()->getLogger().warn("Rebuilding test teleport catalog: {}",e.what());}
 }
 void list(TestTeleportListOutput&out)const{
  out.success("测试传送 {}/{}：/karma tp <序号> [模式]（1 创造 / 0 观察者；省略保持当前模式）。",count(),structure_test::expected());
  if(!error.empty())out.success("坐标准备中断：{}",error);
  else if(!complete)out.success("坐标在空服时分批准备；已收录的编号保持不变。当前已检查 {} 个候选。",visited);
  for(size_t g=0;g<spots.size();++g){auto const&group=structure_test::Groups[g];
   for(int i=0;i<group.quota;++i){
    int id=structure_test::firstId(int(g))+i;
    if(i<int(spots[g].size())){auto const&s=spots[g][i];out.success("[{}] {} {}：{} {} {}",id,group.name,i+1,s.at("x").get<int>(),s.at("y").get<int>(),s.at("z").get<int>());}
    else out.success("[{}] {} {}：{}",id,group.name,i+1,complete?"搜索范围内未找到":"搜索中");
   }
  }
 }
 void listBiomes(TestTeleportListOutput&out)const{
  out.success("群系传送 {}/{}（101 起）；空服时继续搜索，每种保留一个位置。",biomeCount(),std::size(testBiomes));
  for(size_t i=0;i<biomeSpots.size();++i){auto const&b=biomeSpots[i];
   if(b.is_object())out.success("[{}] {}：{} {} {}",101+i,testBiomes[i].name,b.at("x").get<int>(),b.at("y").get<int>(),b.at("z").get<int>());
   else out.success("[{}] {}：{}",101+i,testBiomes[i].name,biomesDone()?"搜索范围内未找到":"搜索中");
  }
 }
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
       std::optional<int> y;
       if(worldgen_rules::extraUnderground(type)){int inside=box.min.y+4;if(test_teleport::validY(inside,s.maxYExclusive()))y=inside;}
       else y=test_teleport::aboveSurface(std::max(box.max.y,field->surface(x+32768,z+32768).height),5,s.maxYExclusive());
       if(y){s.spots[group].push_back({{"x",x},{"y",*y},{"z",z},{"anchor",{cp.x,cp.z}},{"box",boxJson(box)},{"family",g.key}});changed=true;}
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
 auto&cmd=ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("karma","山&海 群系与结构目录",CommandPermissionLevel::Any);
 cmd.runtimeOverload().execute([](CommandOrigin const&o,CommandOutput&out,ll::command::RuntimeCommand const&){
  auto*a=o.getEntity();TestTeleportListOutput list{a&&a->isPlayer()?static_cast<Player*>(a):nullptr,out};
  structureTest.listBiomes(list);structureTest.list(list);list.finish();
 });
 cmd.runtimeOverload().text("tp").required("index",ll::command::ParamKind::Int).optional("mode",ll::command::ParamKind::Int).execute([](CommandOrigin const&o,CommandOutput&out,ll::command::RuntimeCommand const&c){
  auto*a=o.getEntity();
  // Direct player messages remain visible when sendcommandfeedback is disabled.
  auto fail=[&](std::string const&message){if(a&&a->isPlayer())static_cast<Player*>(a)->sendMessage("§c"+message+"§r");out.error(message);};
  int id=std::get<int>(*c["index"].as_ptr());
  auto requested=c["mode"].as_ptr();
  auto mode=requested?test_teleport::mode(std::get<int>(*requested)):std::nullopt;
  if(requested&&!mode){fail("模式只能为 1（创造）或 0（观察者）。用法：/karma tp <序号> [模式]");return;}
  if(!a||!a->isPlayer()){fail("请在游戏内使用 /karma tp <序号> [模式]");return;}
  auto d=test_teleport::destination(id,int(std::size(testBiomes)));
  if(!d){fail("编号无效；输入 /karma 查看全部群系和结构。");return;}
  J const*spot=nullptr;std::string name;
  if(d->biome){spot=&structureTest.biomeSpots[d->group];name=testBiomes[d->group].name;}
  else{
   auto const&points=structureTest.spots[d->group];
   if(d->point<int(points.size()))spot=&points[d->point];
   name=fmt::format("{} {}",structure_test::Groups[d->group].name,d->point+1);
  }
  if(!spot||!spot->is_object()){fail("该编号的坐标尚未找到；输入 /karma 查看目录和搜索进度。");return;}
  if(!test_teleport::validY(spot->at("y").get<int>(),structureTest.maxYExclusive())){fail("该传送点高度已失效，请等待坐标重新准备。");return;}
  // Resolve and read the entire target before changing game mode.
  Vec3 target{float(spot->at("x").get<int>())+.5f,float(spot->at("y").get<int>()),float(spot->at("z").get<int>())+.5f};
  auto&p=static_cast<Player&>(*a);
  if(mode)p.setPlayerGameType(*mode==test_teleport::Mode::Creative?GameType::Creative:GameType::Spectator);
  p.teleport(target,0);
  p.sendMessage(fmt::format("§9已前往 [{}] {}，{}。§r",id,name,mode?(*mode==test_teleport::Mode::Creative?"创造模式":"观察者模式"):"保持当前模式"));out.success();
 });
 structureTest.join=ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerJoinEvent>([](auto&e){if(!e.self().isSimulated())e.self().sendMessage(test_teleport::joinMessage());});
 structureTest.save();StructureTestPump::hook();
 ll::mod::NativeMod::current()->getLogger().info("/karma enabled: full catalog; tp <index> [mode] (1=creative, 0=spectator, omitted=unchanged); blue terrain updates on join");
}
void stopStructureTest(){
 structureTest.enabled=false;
 StructureTestPump::unhook();
 if(structureTest.join)ll::event::EventBus::getInstance().removeListener(structureTest.join);
 structureTest.enabled=false;structureTest.terrain.reset();structureTest.delegate.reset();
}
