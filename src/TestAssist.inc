// Temporary test-world facilities; main thread only. Never edits permissions.json.
struct TestSpot { std::string name; int x,y,z; bool spectator=false; };
struct TestObservation {GameType mode;Vec3 position;DimensionType dimension;};
struct TestVisitor { std::optional<TestObservation> observation; unsigned ticks=0; bool granted=false,operatorCommands=false,teleport=false; CommandPermissionLevel command; PlayerPermissionLevel player; };
Json testEquipment=Json::object();
std::vector<TestSpot> testSpots;
std::unordered_map<std::string,TestVisitor> testVisitors;
ll::event::ListenerPtr testJoin,testLeave;
bool testEnabled=false;
void testList(Player& p){
 p.sendMessage("§e[地形测试] 以下为当前版本观察点。/karma tp 序号 直接前往，/karma tp 重看列表。");
 for(size_t i=0;i<testSpots.size();++i){auto const&s=testSpots[i];p.sendMessage(fmt::format("§b[{}] {}：{} {} {}",i+1,s.name,s.x,s.y,s.z));}
}
void testRestore(Player& p){
 auto it=testVisitors.find(p.getRealName());if(it==testVisitors.end())return;
 if(it->second.observation){auto saved=*it->second.observation;p.teleport(saved.position,saved.dimension);p.setPlayerGameType(saved.mode);it->second.observation.reset();}
 if(!it->second.granted)return;
 auto abilities=p.getAbilities();abilities.setPlayerPermissions(it->second.player);
 abilities.setAbility(AbilitiesIndex::OperatorCommands,it->second.operatorCommands);abilities.setAbility(AbilitiesIndex::Teleport,it->second.teleport);
 p.setAbilities(abilities);p.setCommandPermissions(it->second.command);
 p.getLevel().sendAllPlayerAbilities(p);
 it->second.granted=false;
}
void testAssistTick(Level& level){
 if(!testEnabled)return;
 level.forEachPlayer([&](Player& p){
  if(p.getRealName()!="xuixianDL")return true;
  auto it=testVisitors.try_emplace(p.getRealName()).first;
  if(it!=testVisitors.end()&&++it->second.ticks==40){
   auto& v=it->second;v.command=p.getCommandPermissionLevel();v.player=p.getPlayerPermissionLevel();v.granted=true;
   auto abilities=p.getAbilities();v.operatorCommands=abilities.getBool(AbilitiesIndex::OperatorCommands);v.teleport=abilities.getBool(AbilitiesIndex::Teleport);
   abilities.setPlayerPermissions(PlayerPermissionLevel::Operator);abilities.setAbility(AbilitiesIndex::OperatorCommands,true);abilities.setAbility(AbilitiesIndex::Teleport,true);p.setAbilities(abilities);
   if(v.command<CommandPermissionLevel::Owner)p.setCommandPermissions(CommandPermissionLevel::Owner);
   level.sendAllPlayerAbilities(p);
   // Apply on each login independently of the once-per-world equipment ledger.
   p.addEffect(MobEffectInstance(MobEffect::NIGHT_VISION()->getId(),EffectDuration::INFINITE_DURATION(),1));
   p.sendMessage("§a[地形测试] 无限夜视已启用（等级参数 1）。");
   bool dropped=false;
   bool received=testEquipment.contains(worldName)&&testEquipment.at(worldName).contains(p.getRealName());
   auto give=[&](ItemStack const&item){
    auto& inv=p.getInventory();int slot=-1;
    for(int i=0;i<inv.getContainerSize();++i)if(inv.getItem(i).isNull()){slot=i;break;}
    if(slot>=0)inv.setItem(slot,item);else{p.spawnAtLocation(item,0);dropped=true;}
   };
   if(!received){
    ItemStack elytra("minecraft:elytra",1,0,nullptr),rocket("minecraft:firework_rocket",64,0,nullptr);
    if(elytra.isNull()||rocket.isNull())throw std::runtime_error("Test item unavailable");
    ItemStack previous(p.getArmor(SharedTypes::Legacy::ArmorSlot::Torso));
    p.setArmor(SharedTypes::Legacy::ArmorSlot::Torso,elytra);if(!previous.isNull())give(previous);give(rocket);
    testEquipment[worldName][p.getRealName()]=true;
    auto temp=Base/"test-equipment.json.tmp",path=Base/"test-equipment.json";
    std::ofstream file(temp);file<<testEquipment.dump(2);file.close();
    if(!file||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))log().error("测试装备记录保存失败，本次运行仍不重复发放");
   }
   p.sendInventory(true);p.sendMessage(received?"§a[地形测试] 临时 OP 已同步；本档装备已领取，不重复发放。":dropped?"§e[地形测试] 鞘翅已穿戴；放不下的物品掉落在脚下。临时 OP 已同步。":"§a[地形测试] 鞘翅已穿戴、烟花 ×64；本档只发一次，临时 OP 已同步。");testList(p);
  }

  return true;
 });
}
void registerTestAssist(){
 try{
  if(std::filesystem::exists(Base/"test-equipment.json")){testEquipment=jsonFile(Base/"test-equipment.json");if(!testEquipment.is_object())throw std::runtime_error("Invalid equipment ledger");}
  auto j=jsonFile(Base/"test-assist.json");
  if(!j.value("enabled",false)||j.value("worldName","")!=worldName||j.value("profileSha256","")!=metadata.at("sha256").get<std::string>())return;
  for(auto const& a:j.at("spots")){
   TestSpot s{a.at("name"),a.at("x"),a.at("y"),a.at("z"),a.value("spectator",false)};
   auto kind=a.value("kind",std::string{});
   bool caveSpot=(a.value("verifiedAir",false)&&(kind=="pool"||kind=="dry"))||(s.spectator&&a.value("verifiedSealedFluid",false));
   if(s.x<-Half*16+144||s.x>Half*16-145||s.z<-Half*16+144||s.z>Half*16-145||s.y<(caveSpot?-59:-48)||s.y>319||(s.y<32&&!caveSpot&&!a.value("verifiedWater",false)))throw std::runtime_error("Unsafe test destination");
   testSpots.push_back(s);
  }
  if(testSpots.empty()||testSpots.size()>64)throw std::runtime_error("Expected 1..64 test destinations");
 }catch(std::exception const& e){log().warn("测试辅助未启用：{}",e.what());return;}
 testEnabled=true;
 auto& bus=ll::event::EventBus::getInstance();
 testJoin=bus.emplaceListener<ll::event::PlayerJoinEvent>([](auto&e){if(testEnabled)testVisitors.try_emplace(e.self().getRealName());});
 testLeave=bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](auto&e){testRestore(e.self());testVisitors.erase(e.self().getRealName());});
 auto& cmd=ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("karma","karma-mod 修仙模组",CommandPermissionLevel::Any);
 cmd.runtimeOverload().text("tp").execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const&){
  if(!testEnabled){out.error("地形测试辅助已关闭");return;}
  auto a=origin.getEntity();if(!a||!a->isPlayer()){for(size_t i=0;i<testSpots.size();++i){auto const&s=testSpots[i];out.success("[{}] {}: {} {} {}",i+1,s.name,s.x,s.y,s.z);}return;}
  testList(static_cast<Player&>(*a));
 });
 cmd.runtimeOverload().text("tp").required("index",ll::command::ParamKind::Int).execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const& command){
  if(!testEnabled){out.error("地形测试辅助已关闭");return;}
  auto a=origin.getEntity();if(!a||!a->isPlayer()){out.error("请在游戏内执行 /karma tp 序号");return;}
  int i=std::get<int>(*command["index"].as_ptr());if(i<1||i>int(testSpots.size())){out.error("序号应为 1..{}",testSpots.size());return;}
  auto const&s=testSpots[i-1];auto& player=static_cast<Player&>(*a);
  auto&visitor=testVisitors[player.getRealName()];
  if(s.spectator){
   if(!visitor.observation)visitor.observation=TestObservation{player.getPlayerGameType(),player.getPosition(),player.getDimensionId()};
   player.setPlayerGameType(GameType::Spectator);
  }
  player.teleport(Vec3{float(s.x)+.5f,float(s.y),float(s.z)+.5f},0);
  if(!s.spectator&&visitor.observation){player.setPlayerGameType(visitor.observation->mode);visitor.observation.reset();}
  if(s.spectator)player.sendMessage("§e[地形测试] 封闭液体旁观观察；传送到普通测试点会恢复原模式，退出则返回进入前位置。");
  // Request loading only on the subsequent streaming tick, after teleporting.
  auto& stream=playerStreams[player.getRealName()];stream.here.reset();stream.ahead.reset();
  stream.cx=stream.cz=100000;stream.planner.sampled=false;
  out.success("已传送至 [{}] {}，落点地形随后加载",i,s.name);
 });
 auto& terrainCmd=ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("terrain","Fixed runtime terrain",CommandPermissionLevel::Owner);
 terrainCmd.runtimeOverload().text("teststatus").execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const&){
  ItemStack elytra("minecraft:elytra",1,0,nullptr),rocket("minecraft:firework_rocket",64,0,nullptr);
  out.success("testEnabled={} world={} spots={} elytraValid={} rocketsValid={} immediateTeleport=true nightVisionOnJoin=infinite amplifier=1",testEnabled,worldName,testSpots.size(),!elytra.isNull(),!rocket.isNull());
  if(auto level=origin.getLevel())level->forEachPlayer([&](Player&p){out.success("player={} commandLevel={} playerLevel={} operatorCommands={} teleport={} equipmentOnceRecorded={}",p.getRealName(),int(p.getCommandPermissionLevel()),int(p.getPlayerPermissionLevel()),p.getAbilities().getBool(AbilitiesIndex::OperatorCommands),p.getAbilities().getBool(AbilitiesIndex::Teleport),testEquipment.contains(worldName)&&testEquipment.at(worldName).contains(p.getRealName()));return true;});
 });
 terrainCmd.runtimeOverload().text("testoff").execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const&){
  if(origin.getOriginType()!=CommandOriginType::DedicatedServer){out.error("仅服务器后台可关闭");return;}
  auto j=jsonFile(Base/"test-assist.json");j["enabled"]=false;std::ofstream file(Base/"test-assist.json");file<<j.dump(2);file.close();
  if(!file){out.error("配置保存失败，未关闭");return;}
  if(auto level=origin.getLevel())level->forEachPlayer([](Player&p){testRestore(p);return true;});
  testEnabled=false;testVisitors.clear();out.success("测试辅助已关闭，临时权限已恢复；原有 OP 不变。");
 });
 log().info("[地形测试] 临时进服装备/OP 已启用，{} 个实测观察点；后台 terrain testoff 可关闭",testSpots.size());
}
