// Console-only integration verification. No work runs when no audit is active.
bool interactionAuditActive=false;
unsigned interactionAuditTicks=0;
std::set<int64_t> interactionAuditBaseline;
std::set<std::array<int,3>> interactionAuditBlocks;
J interactionAuditEvents=J::array();
std::weak_ptr<Village> interactionAuditRaidVillage;
char const* auditName="KarmaNativeAudit";
SimulatedPlayer* auditPlayer(Level&level){SimulatedPlayer*found=nullptr;level.forEachPlayer([&](Player&p){if(p.getRealName()==auditName)found=SimulatedPlayer::tryGetFromEntity(p.getEntityContext(),false);return true;});return found;}
bool auditMob(std::string const&n){static std::set<std::string> const names={"minecraft:zombie","minecraft:husk","minecraft:skeleton","minecraft:stray","minecraft:bogged","minecraft:spider","minecraft:cave_spider","minecraft:silverfish","minecraft:slime","minecraft:breeze","minecraft:pillager","minecraft:vindicator","minecraft:evocation_illager","minecraft:ravager","minecraft:witch","minecraft:warden","minecraft:item"};return names.contains(n);}
void collectAuditActors(Level&level){
 auto*p=auditPlayer(level);if(!p)return;auto pos=p->getPosition();for(auto*a:level.getRuntimeActorList()){
  auto id=a->getOrCreateUniqueID().rawID;auto village=interactionAuditRaidVillage.lock();auto*raid=village?village->getRaid():nullptr;if(raid&&raid->isRaider(a->getOrCreateUniqueID())){a->addTag("karma_interaction_audit");continue;}if(interactionAuditBaseline.contains(id)||a==p||!a->isAlive()||!auditMob(a->getTypeName())||a->hasTag("karma_native_structure"))continue;
  auto q=a->getPosition();if(std::hypot(q.x-pos.x,q.z-pos.z)>64||std::abs(q.y-pos.y)>32)continue;a->addTag("karma_interaction_audit");
 }
}
void stopInteractionAudit(Level&level){
 if(!interactionAuditActive)return;collectAuditActors(level);std::vector<Actor*>remove;for(auto*a:level.getRuntimeActorList())if(a->hasTag("karma_interaction_audit"))remove.push_back(a);for(auto*a:remove)a->remove();
 if(auto*p=auditPlayer(level))p->simulateDisconnect();interactionAuditActive=false;interactionAuditBaseline.clear();interactionAuditBlocks.clear();interactionAuditRaidVillage.reset();
}
void tickInteractionAudit(Level&level){if(!interactionAuditActive)return;bool human=false;level.forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human||++interactionAuditTicks>=20*1800){stopInteractionAudit(level);return;}if(interactionAuditTicks%20==0)collectAuditActors(level);}
void snapshotInteractionAudit(Dimension&d){
 auto&level=d.getLevel();auto*p=auditPlayer(level);if(!p)throw std::runtime_error("Audit simulated player missing");collectAuditActors(level);J actors=J::array(),targets=J::array();
 for(auto*a:level.getRuntimeActorList())if(a->hasTag("karma_interaction_audit")){auto q=a->getPosition();J row{{"id",a->getOrCreateUniqueID().rawID},{"type",a->getTypeName()},{"alive",a->isAlive()},{"pos",{q.x,q.y,q.z}}};if(a->getTypeName()=="minecraft:item"){auto&stack=static_cast<ItemActor*>(a)->item();row["item"]=stack.getTypeName();row["count"]=stack.mCount;}actors.push_back(row);}
 auto&blocks=d.getBlockSourceFromMainChunkSource();for(auto const&a:interactionAuditBlocks){BlockPos pos{a[0],a[1],a[2]};auto const&block=blocks.getBlock(pos);J row{{"pos",a},{"block",block.getTypeName()}};row["state"]=J::parse(block.mSerializationId->toSnbt(SnbtFormat::Jsonify),nullptr,false,true);if(auto*actor=blocks.getBlockEntity(pos)){CompoundTag tag;SaveContext context;row["saved"]=actor->save(tag,context);row["data"]=J::parse(tag.toSnbt(SnbtFormat::Jsonify),nullptr,false,true);}targets.push_back(row);}
 auto q=p->getPosition();auto const&selected=p->getSelectedItem();J result{{"world",activeWorld},{"ticks",interactionAuditTicks},{"player",{{"id",p->getOrCreateUniqueID().rawID},{"name",p->getRealName()},{"simulated",p->isSimulated()},{"gameType",int(p->getPlayerGameType())},{"health",p->getHealth()},{"pos",{q.x,q.y,q.z}},{"selected",selected.getTypeName()},{"selectedCount",selected.mCount}}},{"actors",actors},{"blocks",targets},{"events",interactionAuditEvents}};
 if(auto&manager=d.getVillageManager();manager){auto v=manager->fetchClosestVillage({int(q.x),int(q.y),int(q.z)},160,256).lock();if(v){auto c=v->getCenter();result["village"]={{"center",{c.x,c.y,c.z}},{"hasRaid",v->hasRaid()}};if(auto*raid=v->getRaid()){CompoundTag tag;raid->addAdditionalSaveData(tag);result["raid"]=J::parse(tag.toSnbt(SnbtFormat::Jsonify),nullptr,false,true);}}}
 std::ofstream(folder/"interaction-audit.json")<<result.dump(2);
}
void interactionAudit(Dimension&d,int action,BlockPos pos,int value){
 auto&level=d.getLevel();if(action==7){if(auditPlayer(level))snapshotInteractionAudit(d);stopInteractionAudit(level);return;}bool human=false;level.forEachPlayer([&](Player&p){human|=!p.isSimulated();return true;});if(human)throw std::runtime_error("Interaction audit requires an empty human-player session");
 if(action==0){if(interactionAuditActive||auditPlayer(level))throw std::runtime_error("Audit already active");ensureLoaded(d,{pos,pos});interactionAuditBaseline.clear();for(auto*a:level.getRuntimeActorList())interactionAuditBaseline.insert(a->getOrCreateUniqueID().rawID);
  auto p=SimulatedPlayer::create(auditName,{pos.x+.5f,float(pos.y),pos.z+.5f});if(!p)throw std::runtime_error("Native simulated player creation failed");p->setPlayerGameType(GameType::Survival);if(value>0&&value<=32)p->setChunkRadius(uint(value));p->addTag("karma_interaction_audit_player");interactionAuditActive=true;interactionAuditTicks=0;interactionAuditBlocks.clear();interactionAuditEvents=J::array();
 }else{
  auto*p=auditPlayer(level);if(!interactionAuditActive||!p)throw std::runtime_error("Start audit first");
  if(action==1){interactionAuditBlocks.insert({pos.x,pos.y,pos.z});if(interactionAuditBlocks.size()>16)throw std::runtime_error("Audit watch limit exceeded");}
  else if(action==2){ensureLoaded(d,{pos,pos});p->teleportTo({pos.x+.5f,float(pos.y),pos.z+.5f},true,0,0,false);}
  else if(action==3){ItemStack key(value?"minecraft:ominous_trial_key":"minecraft:trial_key",1,0,nullptr);if(!p->simulateSetItem(key,true,0))throw std::runtime_error("Audit key could not be equipped");p->simulateLookAt(pos);bool ok=p->simulateInteract(pos,ScriptModuleMinecraft::ScriptFacing::Up);interactionAuditEvents.push_back({{"action","use-key"},{"pos",{pos.x,pos.y,pos.z}},{"ominous",bool(value)},{"result",ok}});}
  else if(action==4){p->simulateMoveToLocation({pos.x+.5f,float(pos.y),pos.z+.5f},.5f,true);}
  else if(action==5){p->simulateStopMoving();}
  else if(action==6){collectAuditActors(level);for(auto*a:level.getRuntimeActorList())if(a->hasTag("karma_interaction_audit")&&a->getTypeName()!="minecraft:item"&&a->isAlive())a->kill();}
  else if(action==7){snapshotInteractionAudit(d);stopInteractionAudit(level);return;}
  else if(action==8){auto&blocks=d.getBlockSourceFromMainChunkSource();auto*s=SculkShriekerBlockActor::tryGet(blocks,pos);if(!s)throw std::runtime_error("No original shrieker");bool ok=s->tryShriek(blocks,pos,*p);interactionAuditEvents.push_back({{"action","native-player-shriek-response"},{"pos",{pos.x,pos.y,pos.z}},{"result",ok}});}
  else if(action==9){auto&manager=d.getVillageManager();auto v=manager?manager->fetchClosestVillage(pos,160,256).lock():nullptr;if(!v||!v->getRaid())throw std::runtime_error("No native village raid");interactionAuditRaidVillage=v;collectAuditActors(level);interactionAuditEvents.push_back({{"action","track-native-raid-members"}});}
  else throw std::runtime_error("Unknown audit action");
 }
 snapshotInteractionAudit(d);
}
