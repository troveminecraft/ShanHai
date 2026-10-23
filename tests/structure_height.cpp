#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rules/FrozenTemplates.h"
#include "rules/ScatteredExecution.h"
#include "rules/StructureFinish.h"
#include "rules/FossilExecution.h"
#include "rules/CityEnvironment.h"
#include <cassert>
#include <iostream>
using namespace worldgen_rules;
namespace {
struct Surface {int height,water=40,biome=12;double lake=0,river=0,volcanic=0,stone=0,iceHabitat=0,ocean=0,coast=0;};
template<class F>void rejects(F fn){bool rejected=false;try{fn();}catch(std::exception const&){rejected=true;}assert(rejected);}
VillageBlueprint village(int y){
 VillageBlueprint out{{0,0},42,12,{}};
 StructureBlueprintPiece house;house.box={{0,y,0},{4,y+7,4}};house.origin={0,y,0};house.location="village/savanna/houses/height-test";
 for(int z=0;z<=4;z++)for(int x=0;x<=4;x++){
  house.cells.push_back({{x,0,z},{"minecraft:grass_block",0},{}});
  house.cells.push_back({{x,1,z},{"minecraft:air",0},{}});
 }
 house.cells.push_back({{2,1,1},{"minecraft:acacia_door",0},{}});
 house.cells.push_back({{2,1,2},{"minecraft:structure_block",0},{},"minecraft:villager_v2"});
 out.pieces.push_back(std::move(house));return out;
}
struct CityEnvironment {
 int maxY,reads=0,writes=0;
 std::string_view get(StructurePoint p){assert(p.y>=-64&&p.y<=maxY);++reads;return "minecraft:stone";}
 bool natural(std::string_view){return true;}
 void air(StructurePoint p){assert(p.y<=maxY);++writes;}
};
void verify(int maxY,int h){
 auto sample=[&](int,int){return Surface{h};};auto hash=[](int,int,uint32_t){return .5;};
 auto blueprint=village(h);auto plan=planVillageLayout(blueprint,sample,hash,maxY);
 assert(plan.accepted&&plan.maxY==maxY&&plan.groundMin==h&&plan.groundMax==h&&plan.verticalMoves[0]==0);
 assert(villageSiteStyle({0,0},sample)==villageStyle(12));
 auto steep=planVillageLayout(blueprint,[&](int x,int){return Surface{h+(x>0?12:0)};},hash,maxY);
 assert(!steep.accepted);
 VillageCells roof{{{0,h+4,0},"minecraft:oak_planks"},{{1,h+1,0},"minecraft:dirt"},{{1,h+2,0},"minecraft:oak_planks"}};
 assert(villageCoveredGround(roof,{0,0},h,h+8,[](auto n){return n;},maxY));
 assert(villageFinishedHeight(roof,{1,0},h,{},maxY)==h+2);
 StructureEntityQueue<int> queue;LifecycleJson ledger={{"structureEntries",LifecycleJson::object()}};
 assert(enqueueLoadedVillageOwners(queue,ledger,1,plan,{0,0},42,[](auto){return true;})==1);
 assert(queue.pending.front().clip.max.y==maxY);
 auto names=[&](StructurePoint p)->std::string_view{return p.y==h?"minecraft:stone":"minecraft:air";};
 auto actor=supportedStructureEntityPosition({1,h+1,1},names,maxY);assert(actor&&actor->y==h+1);
 assert(!supportedStructureEntityPosition({1,maxY,1},[](auto)->std::string_view{return "minecraft:air";},maxY));

 StructureBlueprintPiece extra;extra.origin={0,h,0};extra.box={{0,h,0},{4,h+10,4}};
 auto extraPlan=planExtraLayout(ExtraBlueprint{0,{0,0},extra.box,{extra}},sample,maxY);
 assert(extraPlan.accepted&&extraPlan.bounds.min.y==h+1&&extraPlan.bounds.max.y==h+11);
 extra.box.max.y=maxY+1;assert(!planExtraLayout(ExtraBlueprint{0,{0,0},extra.box,{extra}},sample,maxY).accepted);
 FrozenIglooPlan igloo;igloo.bounds={{0,h,0},{6,h+4,7}};
 igloo.parts.push_back({IglooTemplateRole::TopWithoutTrapdoor,{0,h,0},igloo.bounds});
 auto fit=fitFrozenIglooFloor(igloo,5,[&](int,int){auto s=Surface{h};s.biome=5;return s;},maxY);
 assert(fit.accepted&&fit.low==h&&fit.high==h&&igloo.bounds.max.y==h+4);

 auto recipe=planScatteredFragments(13,{0,0},42,{{0,h,0},{20,h+8,20}},h,maxY);
 int fragments=0,placed=0;
 executeScatteredFragments(recipe,{{0,-64,0},{31,maxY,31}},[](auto const&,auto const&){return 1;},
  [&](auto&,auto const&r,auto const&f){++fragments;assert(f.clip.max.y==maxY);
   for(int z=r.bounds.min.z;z<=r.bounds.max.z;z++)for(int x=r.bounds.min.x;x<=r.bounds.max.x;x++)
    if(scatteredFragmentWriteOwned(f.clip,{x,h+8,z}))++placed;
   assert(!scatteredFragmentWriteOwned(f.clip,{f.clip.min.x,maxY+1,f.clip.min.z}));return true;});
 assert(fragments==4&&placed==21*21);
 rejects([&]{planScatteredFragments(13,{0,0},42,{{0,h,0},{20,maxY+1,20}},h,maxY);});
 std::map<int,std::string_view> writes;
 writeStructureGroundColumn(0,0,maxY,maxY-2,12,[&](auto p,auto b){assert(p.y<=maxY);writes[p.y]=b.name;},maxY);
 assert(writes.size()==3&&writes.at(maxY)=="minecraft:air"&&writes.at(maxY-2)=="minecraft:grass_block");
 rejects([&]{writeStructureGroundColumn(0,0,maxY,maxY+1,12,[](auto,auto){assert(false);},maxY);});
 int cleared=0;
 clearUnsupportedStructurePlants({0,0},maxY-3,maxY+3,
  [&](auto p)->std::string_view{return p==StructurePoint{0,maxY-2,0}?"minecraft:tall_grass":"minecraft:air";},
  [](auto){return false;},[&](auto p){assert((p==StructurePoint{0,maxY-2,0}));++cleared;},maxY);
 assert(cleared==1);
 assert(smallFeatureWriteOwned({0,0},{1,maxY,1},false,maxY)&&!smallFeatureWriteOwned({0,0},{1,maxY+1,1},false,maxY));
 std::map<std::array<int,3>,std::string_view> fossil{{{1,h,1},"minecraft:bone_block"}};
 assert(fossilOutputBounds({0,0},fossil,[](auto p){return StructurePoint{p[0],p[1],p[2]};},[](auto n){return n;},maxY).min.y==h);

 CityEnvironment env{maxY};StructureBlueprintPiece city;city.box={{0,h,0},{4,h+7,4}};
 assert(carveCityEnvironment(std::span<StructureBlueprintPiece const>(&city,1),{},{},{{0,-64,0},{4,maxY,4}},42,env)>0);
 assert(env.reads==env.writes);
 // Empty city columns retain a safe sentinel, with no signed-overflow scan.
 CityEnvironment empty{maxY};city.box={{100,h,100},{104,h+7,104}};
 assert(carveCityEnvironment(std::span<StructureBlueprintPiece const>(&city,1),{},{},{{0,-64,0},{4,maxY,4}},42,empty)==0);
 if(maxY==511){
  assert(!planVillageLayout(blueprint,sample,hash).accepted);
  assert(!planExtraLayout(ExtraBlueprint{0,{0,0},{{0,h,0},{4,h+10,4}},{blueprint.pieces.front()}},sample).accepted);
  assert(!fitFrozenIglooFloor(igloo,5,[&](int,int){auto s=Surface{h};s.biome=5;return s;}).accepted);
  rejects([&]{planScatteredFragments(13,{0,0},42,recipe.bounds,h);});
  assert(!supportedStructureEntityPosition({1,h+1,1},names));
  assert(!smallFeatureWriteOwned({0,0},{1,h,1},false));
  rejects([&]{fossilOutputBounds({0,0},fossil,[](auto p){return StructurePoint{p[0],p[1],p[2]};},[](auto n){return n;});});
 }
}
}
int main(){try{verify(319,280);verify(511,480);std::cout<<"Structure height: legacy/expanded planning, template fitting, fragment writes, population headroom, ground, vegetation and fossils verified\n";}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
