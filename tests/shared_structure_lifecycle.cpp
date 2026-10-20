#include "rules/StructureLifecycle.h"
#include "rules/StructureStore.h"
#include "rules/FrozenTemplates.h"
#include <iostream>
#include <fstream>
using namespace worldgen_rules;
using J=nlohmann::json;
void check(bool value,char const*why){if(!value)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("test-directory report.json");namespace fs=std::filesystem;
 auto directory=fs::path(argv[1])/std::to_string(worldgen_persistence::diagnosticStamp());fs::create_directories(directory);
 auto file=directory/"claims.json";auto backup=file;backup+=".bak";
 auto loaded=structure_store::load(file,"world-A","-4015943601349081962");check(loaded.enabled,"new ledger disabled");J ledger=loaded.data;
 StructureEntityQueue<int>queue;StructureBox clip{{-16,-64,0},{-1,319,15}};ChunkPosition anchor{-1,2};
 check(!queue.enqueue(ledger,1,clip,"village",anchor,42,false),"empty village fragment scheduled entities");
 check(queue.enqueue(ledger,1,clip,"village",anchor,42,true),"village actors not scheduled");check(!queue.enqueue(ledger,1,clip,"village",anchor,42,true),"duplicate pending job");
 int flushes=0,spawned=0;
 auto persist=[&](J const&j){structure_store::persist(file,j);++flushes;};
 auto work=queue.claimReady(ledger,[](auto const&){return false;},persist);check(work.empty()&&flushes==0&&queue.pending.size()==1,"unloaded fragment consumed or wrote claims");
 bool probeThrew=false;try{queue.claimReady(ledger,[](auto const&)->bool{throw std::runtime_error("temporary native probe failure");},persist);}catch(...){probeThrew=true;}
 check(probeThrew&&queue.pending.size()==1&&queue.queued.size()==1&&flushes==0,"readiness exception lost queued population or wrote a claim");
 StructureEntityQueue<int> interruptedProbes;J probeLedger={{"structureEntries",J::object()}};
 check(interruptedProbes.enqueue(probeLedger,8,clip,"pillager_outpost",{5,5},42)&&interruptedProbes.enqueue(probeLedger,9,clip,"pillager_outpost",{6,5},42),"probe transaction setup failed");int probes=0;
 try{interruptedProbes.claimReady(probeLedger,[&](auto const&){if(++probes==2)throw std::runtime_error("second probe failure");return true;},persist);}catch(...){}
 check(probes==2&&interruptedProbes.pending.size()==2&&interruptedProbes.queued.size()==2&&probeLedger["structureEntries"].empty()&&flushes==0,"late probe failure leaked an unpersisted earlier claim");
 check(interruptedProbes.pending.front().start==8&&interruptedProbes.pending.back().start==9,"failed probe transaction reordered selected jobs");
 work=queue.claimReady(ledger,[](auto const&){return true;},persist);check(work.size()==1&&flushes==1,"ready fragment not claimed in one batch");
 auto key=work[0].key;check(structure_store::read(file).at("structureEntries").at(key).at("state")=="claimed","spawn authorized before primary claim");check(structure_store::read(backup).at("structureEntries").contains(key),"spawn authorized before mirror claim");
 ++spawned;completeStructureEntityClaim(ledger,key,J::array({{{"id",101},{"type","minecraft:villager_v2"}}}));persist(ledger);
 auto reloaded=structure_store::load(file,"world-A","-4015943601349081962");check(reloaded.enabled,"valid save did not reload");check(!queue.enqueue(reloaded.data,1,clip,"village",anchor,42,true),"saved entities scheduled twice");
 J old=reloaded.data;std::string oldkey=structureOwnerKey("village",{9,9},{-1,0});old["structureEntries"][oldkey+":native-templates-v3"]={{"actors",J::array({{{"id",5}}})}};
 check(!queue.enqueue(old,2,clip,"village",{9,9},42,true),"legacy populated village duplicated");
 for(int i=0;i<20;++i)check(queue.enqueue(ledger,i,clip,"pillager_outpost",{i,0},42),"independent outpost job dropped");
 int checked=0;work=queue.claimReady(ledger,[&](auto const&){++checked;return true;},persist);
 check(work.size()==StructureEntityBatchLimit&&checked==16&&queue.pending.size()==4,"main-thread budget or batch limit changed");
 int before=spawned;auto mirrorTemp=backup;mirrorTemp+=".tmp";fs::create_directory(mirrorTemp);bool failed=false;
 try{work=queue.claimReady(ledger,[](auto const&){return true;},persist);spawned+=int(work.size());}catch(...){failed=true;}
 check(failed&&spawned==before,"entity effects escaped failed claim flush");
 auto interrupted=structure_store::load(file,"world-A","-4015943601349081962");check(interrupted.enabled&&interrupted.data==ledger,"interrupted claims lost on restart");
 fs::remove(mirrorTemp);structure_store::persist(file,ledger);
 {std::ofstream out(file,std::ios::binary);out<<std::string(200,'\0');}
 auto recovered=structure_store::load(file,"world-A","-4015943601349081962");check(recovered.enabled&&recovered.recovered&&recovered.data==ledger,"corrupt primary lost committed claims");
 check(!structure_store::load(file,"world-B","-4015943601349081962").enabled,"cross-world claims accepted");
 {std::ofstream out(file);out<<"{";}{std::ofstream out(backup);}
 auto corrupt=structure_store::load(file,"world-A","-4015943601349081962");check(!corrupt.enabled&&fs::file_size(file)==1,"corrupt ledgers reset or erased");
 auto supported=supportedStructureEntityPosition({4,65,6},[](StructurePoint p)->std::string_view{return p.y==64?"minecraft:grass_block":"minecraft:air";});check(supported&&supported->y==65,"valid actor support rejected");
 auto unsupported=supportedStructureEntityPosition({4,65,6},[](StructurePoint)->std::string_view{return "minecraft:water";});check(!unsupported,"actor allowed without dry support and headroom");
 VillageLayoutPlan population;population.population[{-1,0}]={{{-4,65,6},"minecraft:villager_v2"},{{-3,65,6},"minecraft:cat"}};
 population.witnesses={{{16,65,0},{"minecraft:oak_door",0}},{{-5,65,6},{"minecraft:bed",0}}};
 StructureEntityQueue<int> restoredQueue;J restoredLedger={{"structureEntries",J::object()}};
 check(enqueueLoadedVillageOwners(restoredQueue,restoredLedger,33,population,{-8,4},42,[](ChunkPosition){return false;})==0&&restoredQueue.pending.empty(),"reload scheduled an unloaded owner");
 check(enqueueLoadedVillageOwners(restoredQueue,restoredLedger,33,population,{-8,4},42,[](ChunkPosition){return true;})==1&&restoredQueue.pending.front().clip.min.x==-16&&restoredQueue.pending.front().clip.max.x==-1,"reload did not canonicalize negative owning chunk");
 check(enqueueLoadedVillageOwners(restoredQueue,restoredLedger,33,population,{-8,4},42,[](ChunkPosition){return true;})==0&&restoredQueue.pending.size()==1,"reload duplicated an already queued owner");
 int reads=0,loads=0;bool supportedAll=true,witness=true;
 auto names=[&](StructurePoint p)->std::string_view{++reads;check(p.x<0,"unloaded neighbouring witness was read");if(p.x==-5)return witness?"minecraft:bed":"minecraft:air";if(!supportedAll&&p.x==-3)return "minecraft:water";return p.y==65?"minecraft:stone":"minecraft:air";};
 auto unavailable=prepareVillagePopulation(population,{-1,0},[&](ChunkPosition){++loads;return false;},names);check(!unavailable&&reads==0&&loads==1,"unloaded owner read blocks or neighbours");
 auto loadedOwner=[&](ChunkPosition cp){++loads;return cp.x==-1&&cp.z==0;};
 auto readyPopulation=prepareVillagePopulation(population,{-1,0},loadedOwner,names);check(readyPopulation&&readyPopulation->size()==2&&readyPopulation->front().position.y==66,"ready population failed to use common support adjustment");
 supportedAll=false;check(!prepareVillagePopulation(population,{-1,0},loadedOwner,names),"partly unsupported population consumed a claim");supportedAll=true;
 witness=false;check(!prepareVillagePopulation(population,{-1,0},loadedOwner,names),"missing actual witness accepted blueprint-only population");witness=true;
 auto failureJob=structureOwnerKey("village",{-8,4},{-1,0})+":native-templates-v4";ledger["structureEntries"][failureJob]={{"state","claimed"}};
 failStructureEntityClaim(ledger,failureJob,"partial native spawn",J::array({{{"id",1253456}}}));check(ledger["structureEntries"][failureJob]["state"]=="review-failed"&&ledger["structureEntries"][failureJob]["partialEffectsMayExist"]==true,"partial effects not retained for review");
 check(!queue.enqueue(ledger,3,clip,"village",{-8,4},42,true),"failed native spawn automatically replayed");bool doubleSettle=false;try{completeStructureEntityClaim(ledger,failureJob,J::array());}catch(...){doubleSettle=true;}check(doubleSettle,"failed claim silently settled twice");
 auto definition=structureEntityDefinition("minecraft:villager_v2:minecraft:spawn_cleric");check(definition.name=="villager_v2"&&definition.event=="minecraft:spawn_cleric","native spawn event parsed incorrectly");
 bool bad=false;try{structureEntityDefinition("villager");}catch(...){bad=true;}check(bad,"invalid native marker accepted");
 check(iglooEntityDefinition("Zombie Villager")->name=="zombie_villager_v2","igloo resident rule changed");
 check(shipMarkerLoot("Supply")=="loot_tables/chests/shipwreck_supply.json"&&shipMarkerLoot("Treasure")=="loot_tables/chests/shipwreck_treasure.json","ship loot depends on adapter case normalization");
 auto iglooLoot=iglooMarkerLoot("chest",{-1,63,1},42);check(iglooLoot&&iglooLoot->position.y==62&&iglooLoot->seed==42,"igloo native chest marker offset changed");
 J legacyRecipe={{"config",{{"seed",17},{"sharedRules",{{"revision","llbds-structures-v2"}}}}},{"seedManifest",{{"terrainSeed","-1"}}}};
 check(structureLedgerSeed(legacyRecipe)==17,"legacy ledger identity changed");legacyRecipe["config"]["sharedRules"]["revision"]="llbds-structures-v3";
 check(structureLedgerSeed(legacyRecipe)=="-1","new ledger uses truncated world seed");
 StructureLootAssignment loot{{-1,62,1},"loot_tables/chests/igloo_chest.json",42};
 check(structureLootMarkerOwned(loot,clip,false)&&!structureLootMarkerOwned(loot,clip,true),"saved container was reseeded");loot.position.x=0;check(!structureLootMarkerOwned(loot,clip,false),"loot escaped owner fragment");int neighbourReads=0;check(!structureLootMarkerOwned(loot,clip,[&]{++neighbourReads;return false;})&&neighbourReads==0,"foreign loot marker queried a neighbouring chunk");
 J report={{"passed",true},{"durableClaimsBeforeEffects",true},{"mirrorRecovery",true},{"corruptClaimsPreservedAndDisabled",true},{"legacyVillageClaimsHonored",true},{"saveReloadNoDuplicateEntities",true},{"interruptedMirrorFlushNoSpawn",true},{"mainThreadBatchLimit",StructureEntityBatchLimit},{"unloadedChunksDoNotConsumeClaims",true},{"entityDefinitionAndSupport",true},{"lootOwnerAndExistingContainer",true},{"foreignMarkersDoNotReadNeighbours",true},{"lootCaseAndNativeChestOffset",true},{"fullSeedForNewLedger",true},{"gameplayTestPerformed",false}};
 report["populationWitnessReadiness"]=true;report["wholeFragmentSupportBeforeClaim"]=true;report["partialNativeFailureNoReplay"]=true;report["noUnloadedWitnessReads"]=true;
 report["readinessExceptionRetainsQueue"]=true;
 report["sharedReloadOwnerScheduling"]=true;
 report["wholeBatchReadinessTransactional"]=true;
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump(2)<<'\n';return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
