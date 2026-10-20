#include "rules/FrozenTemplates.h"
#include <fstream>
#include <iostream>
using namespace worldgen_rules;
using J=nlohmann::json;
void check(bool value,char const*why){if(!value)throw std::runtime_error(why);}
struct FixedShipRandom{int variant,rotation;int nextInt(int n){return n==20?variant:rotation;}int nextInt(){return 1234567;}};
struct Sea{int height=18,water=31,biome=1;double ocean=1,volcanic=0,iceHabitat=0,lake=0,river=0,coast=0;};
using Key=std::array<int,3>;
Key key(StructurePoint p){return {p.x,p.y,p.z};}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("report.json asset-catalog.json");J fixture;{std::ifstream input(argv[2]);input>>fixture;}check(fixture.at("models").size()==20,"native fixture catalog incomplete");
 std::map<std::string,J const*,std::less<>>models;for(auto const&m:fixture.at("models"))models.emplace(m.at("name").get<std::string>(),&m);
 auto size=[&](std::string_view name){auto s=models.at(std::string(name))->at("size");return StructurePoint{s[0],s[1],s[2]};};
 J geometries=J::array(),rng=J::array();size_t fragments=0,lootCount=0;
 for(int variant=0;variant<20;++variant)for(int rotation=0;rotation<4;++rotation){
  ChunkPosition anchor{-3,-2};uint32_t worldSeed=0xfedcba98u;FixedShipRandom random{variant,rotation};auto plan=planFrozenShip(anchor,worldSeed,random,size);auto fit=fitFrozenShipFloor(plan,[](int,int){return Sea{};});check(fit.accepted&&fit.floor==19&&plan.origin.y==19&&plan.bounds.min.y==19,"shared frozen ship floor fit differs");
  auto again=plan;auto same=fitFrozenShipFloor(again,[](int,int){return Sea{};});check(same.accepted&&again.origin==plan.origin&&structureBoxJson(again.bounds)==structureBoxJson(plan.bounds),"ship floor fit double-shifted a saved layout");
  auto const&model=*models.at(std::string(FrozenShipTemplates[variant]));std::vector<LegacyDataMarker>markers;
  for(auto const&m:model.at("rawMarkers")){auto p=m.at("local");markers.push_back({{p[0],p[1],p[2]},m.at("label")});}
  auto program=compileFrozenShip(plan,markers);check(!program.markers.empty(),"raw native ship has no loot markers");
  auto worldPoint=[&](J const&p){auto[x,z]=rotateVillageXZ(int(p[0]),int(p[2]),rotation);return StructurePoint{plan.origin.x+x,plan.origin.y+int(p[1]),plan.origin.z+z};};
  std::vector<VillageXZ>owners;for(int z=spacingCell(plan.bounds.min.z,16);z<=spacingCell(plan.bounds.max.z,16);++z)for(int x=spacingCell(plan.bounds.min.x,16);x<=spacingCell(plan.bounds.max.x,16);++x)owners.emplace_back(x,z);
  using World=std::map<Key,std::string>;using Loots=std::map<Key,std::pair<std::string,uint32_t>>;World forward,reverse;Loots forwardLoot,reverseLoot;
  auto generate=[&](auto const&order,World&world,Loots&loot){for(auto owner:order){++fragments;StructureBox clip{{owner.first*16,-64,owner.second*16},{owner.first*16+15,319,owner.second*16+15}};
   bool placed=executeFrozenShip(program,clip,[&](auto const&g,uint32_t seed,auto const&){check(g.selection.variant==variant&&g.selection.rotation==rotation&&g.settingsSeed==1234567&&seed==extraPieceSeed(2,0,anchor,anchor,worldSeed),"placement random state depends on fragment order");for(auto const&p:model.at("rawChests")){auto q=worldPoint(p);if(inStructureOwner(q,clip))world[key(q)]="minecraft:chest";}for(auto const&m:program.markers)if(inStructureOwner(m.position,clip))world[key(m.position)]="minecraft:structure_block";},[&](StructurePoint p)->std::string_view{check(inStructureOwner(p,clip),"ship marker read foreign chunk");auto it=world.find(key(p));return it==world.end()?"minecraft:air":std::string_view(it->second);},[&](StructurePoint p){check(inStructureOwner(p,clip),"ship cleared a foreign marker");world[key(p)]="minecraft:air";},[&](StructurePoint p,int facing){check(facing==2&&inStructureOwner(p,clip),"fallback chest orientation/owner differs");world[key(p)]="minecraft:chest";},[&](StructureLootAssignment const&a){check(inStructureOwner(a.position,clip)&&world.at(key(a.position))=="minecraft:chest","loot assigned outside its owning chest");check(loot.emplace(key(a.position),std::pair{a.table,a.seed}).second,"marker assigned loot twice");});check(placed,"ship fragment missed selected template");
  }};
  generate(owners,forward,forwardLoot);std::reverse(owners.begin(),owners.end());generate(owners,reverse,reverseLoot);check(forward==reverse&&forwardLoot==reverseLoot,"ship outputs depend on fragment order");check(forwardLoot.size()==program.markers.size(),"ship lost a native chest marker");lootCount+=forwardLoot.size();
  int outsideReads=0;bool outside=executeFrozenShip(program,{{1000,-64,1000},{1015,319,1015}},[&](auto const&,auto,auto const&){++outsideReads;},[&](auto)->std::string_view{++outsideReads;return "minecraft:air";},[&](auto){++outsideReads;},[&](auto,int){++outsideReads;},[&](auto const&){++outsideReads;});check(!outside&&outsideReads==0,"non-overlap ship callback touched game state");
  J lootRows=J::array();for(auto const&[p,assignment]:forwardLoot)lootRows.push_back({p[0],p[1],p[2],assignment.first,assignment.second});geometries.push_back({{"variant",variant},{"template",std::string(FrozenShipTemplates[variant])},{"rotation",rotation},{"origin",{plan.origin.x,plan.origin.y,plan.origin.z}},{"bounds",structureBoxJson(plan.bounds)},{"settingsSeed",plan.settingsSeed},{"loot",lootRows}});
 }
 for(uint32_t seed:{0u,1u,42u,0xffffffffu,0x8ab341abu}){auto plan=planFrozenShip({-3,4},seed,size);rng.push_back({seed,plan.selection.variant,plan.selection.rotation,plan.settingsSeed,structureBoxJson(plan.bounds)});}
 // Exercise an absent native chest, independent of actual asset variants.
 FrozenShipProgram missing;missing.geometry={ {0,0},42,1,{0,0},{0,19,0},{{0,19,0},{15,30,15}}};missing.markers.push_back({{2,23,2},"SupplyChest"});int creates=0,loots=0;bool exists=false;
 executeFrozenShip(missing,{{0,-64,0},{15,319,15}},[](auto const&,auto,auto const&){},[&](StructurePoint p)->std::string_view{return exists&&p==StructurePoint{2,23,2}?"minecraft:chest":"minecraft:air";},[](auto){},[&](StructurePoint p,int facing){check(p==StructurePoint{2,23,2}&&facing==2,"missing chest fallback moved incorrectly");exists=true;++creates;},[&](auto const&a){check(a.position==StructurePoint{2,23,2}&&a.table=="loot_tables/chests/shipwreck_supply.json","fallback chest loot differs");++loots;});check(creates==1&&loots==1,"fallback chest lifecycle incomplete");
 FixedShipRandom random{0,0};auto land=planFrozenShip({0,0},42,random,size);auto rejected=fitFrozenShipFloor(land,[](int,int){auto s=Sea{};s.ocean=0;return s;});check(!rejected.accepted&&land.origin.y==0,"rejected ship mutated its frozen geometry");
 J report={{"passed",true},{"planCases",geometries.size()},{"fragments",fragments},{"lootAssignments",lootCount},{"geometry",geometries},{"randomPlans",rng},{"rawModels",fixture.at("models")},{"orderIndependentMarkersAndLoot",true},{"nativeAssetChestsUsed",true},{"savedLayoutFitIdempotent",true},{"missingChestFallback",true},{"nonOverlapZeroGameCalls",true},{"gameplayTestPerformed",false},{"nativeTemplatePlacementExecuted",false},{"nativeTemplateParityVerified",false}};std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)throw std::runtime_error("report write failed");std::cout<<"Shared frozen ship plan/fragment contract passed\n";return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
