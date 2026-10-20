#include "rules/StructureReservations.h"
#include "rules/SmallFeaturePlan.h"
#include "rules/FossilExecution.h"
#include "rules/VillageDecorExecution.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
using namespace worldgen_rules;
using J=nlohmann::json;
void check(bool ok,char const*why){if(!ok)throw std::runtime_error(why);}
struct Surface {int height=64,water=31,biome=11;double river{},lake{},volcanic{},iceHabitat{},ocean{};};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("report.json");
 J selectors=J::array();size_t wells=0,fossils=0,potential=0;
 for(int z=-128;z<128;++z)for(int x=-128;x<128;++x){ChunkPosition cp{x,z};uint32_t seed=uint32_t(uint64_t(-4015943601349081962LL));
  auto n=smallFeatureSelector(cp,seed);wells+=wellCandidate(cp,seed);fossils+=fossilCandidate(cp,seed);potential+=smallFeaturePotential(cp,seed);
  if(x%31==0&&z%29==0)selectors.push_back({x,z,n});
  if(fossilCandidate(cp,seed)){check(fossilNeighbourPotential(cp,seed),"fossil anchor not discovered");for(int dz:{-1,1})for(int dx:{-1,1})check(fossilNeighbourPotential({x+dx,z+dz},seed),"diagonal owner cannot discover fossil");}
 }
 check(wells>70&&wells<200&&fossils>800&&fossils<1250&&potential>15000&&potential<18000,"upstream selector frequencies changed");
 for(ChunkPosition cp: {ChunkPosition{0,0},{-1,-1},{-33,42}}){
  auto p=wellPosition(cp,[](int,int){return Surface{};});check(p&&p->x==cp.x*16+8&&p->z==cp.z*16+8&&p->y==65,"well position changed");
  for(int mode=0;mode<5;++mode){auto bad=wellPosition(cp,[&](int x,int z){auto s=Surface{};if(x==cp.x*16+11&&z==cp.z*16+11){if(mode==0)++s.height;if(mode==1)s.biome=12;if(mode==2)s.river=1;if(mode==3)s.water=64;if(mode==4)s.lake=1;}return s;});check(!bad,"well crosses slope/water/biome boundary");}
  check(smallFeatureWriteOwned(cp,{cp.x*16,-58,cp.z*16},false),"owned negative chunk edge rejected");check(!smallFeatureWriteOwned(cp,{cp.x*16-1,64,cp.z*16},false),"feature writes into western neighbour");check(!smallFeatureWriteOwned(cp,{cp.x*16,64,cp.z*16+16},false),"feature writes into southern neighbour");check(!smallFeatureWriteOwned(cp,{cp.x*16,-59,cp.z*16},false),"feature overwrites bedrock shell");check(!smallFeatureWriteOwned(cp,{cp.x*16,0,cp.z*16},true),"feature overwrites bedrock block");
 }
 auto water=Surface{};water.ocean=1;check(!smallFeatureHabitat(water),"room in deep ocean");auto cold=Surface{};cold.iceHabitat=.1;check(!smallFeatureHabitat(cold),"room inside glacier");
 auto forest=Surface{};forest.biome=12;check(!fossilHabitat(forest),"fossil outside swamp/desert");forest.biome=10;check(fossilHabitat(forest),"swamp fossil missing");
 // A real chamber profile: two dry floors at unequal depths, a flooded floor,
 // and one missing roof. No excavating to make a candidate fit is permitted.
 using XYZ=std::array<int,3>;std::map<XYZ,std::string> cells;ChunkPosition owner{-2,3};int cx=owner.x*16+8,cz=owner.z*16+8;
 auto put=[&](int x,int y,int z,std::string s){cells[{x,y,z}]=std::move(s);};
 for(int y:{-30,18,44}){put(cx,y,cz,"minecraft:air");put(cx+3,y,cz,"minecraft:air");put(cx+3,y+1,cz,"minecraft:air");}
 put(cx,44+4,cz,"minecraft:air");put(cx,7,cz,"minecraft:water");
 size_t reads=0;auto name=[&](StructurePoint p)->std::string_view{++reads;auto it=cells.find({p.x,p.y,p.z});if(it==cells.end())return "minecraft:stone";return it->second;};
 auto solid=[&](StructurePoint p){auto n=name(p);return n!="minecraft:air"&&n!="minecraft:water";};
 auto levels=dungeonProbeLevels(owner,80,name,solid);check(levels==std::vector<int>({-30,18,44,48}),"floor discovery missing or duplicate levels");
 check(reads<=5*115*2,"probe performed unbounded scan");check(dungeonProbeSupported({cx,-30,cz},name,solid)&&dungeonProbeSupported({cx,18,cz},name,solid),"natural dry chamber rejected");check(!dungeonProbeSupported({cx,44,cz},name,solid),"open roof accepted");
 check(dungeonProbeLevels(owner,-30,name,solid).empty(),"shallow cover limit ignored");
 std::vector<StructureBox> occupied{{{cx-2,10,cz-2},{cx+2,20,cz+2}}};check(!smallFeatureFits(dungeonBounds({cx,18,cz}),occupied),"dungeon intersects larger structure");check(smallFeatureFits(dungeonBounds({cx,-30,cz}),occupied),"vertically distant structures collide");
 struct Random {std::mt19937 engine;int nextInt(int n){return int(engine()%uint32_t(n));}};
 // Native callbacks consume this very same RNG. Compare against the previous
 // upstream execution sequence, including failed attempts and occupied rooms.
 for(uint32_t seed=0;seed<128;++seed){
  Random old{std::mt19937(placementSeed(owner.x,owner.z,SmallFeatureSalt,seed))},shared=old;
  using Call=std::tuple<std::string,int,int>;std::vector<Call>expected,actual;
  auto surface=[](int,int){Surface s;s.height=80;s.biome=12;return s;};
  auto candidates=dungeonProbeLevels(owner,80,name,solid);if(!candidates.empty())std::rotate(candidates.begin(),candidates.begin()+old.nextInt(int(candidates.size())),candidates.end());
  unsigned checked=0;for(int y:candidates){if(checked++>=16)break;StructurePoint p{cx,y,cz};if(!dungeonProbeSupported(p,name,solid)||!smallFeatureFits(dungeonBounds(p),occupied))continue;expected.emplace_back("monster_room",y,old.nextInt(100));break;}
  executeSmallFeatures(owner,seed,shared,surface,name,solid,occupied,[&](std::string_view kind,StructurePoint p,auto&r){actual.emplace_back(kind,p.y,r.nextInt(100));return true;});
  check(expected==actual&&old.nextInt(10000)==shared.nextInt(10000),"shared small-feature execution changed RNG/order");
 }
 J origins=J::array();for(bool deep:{false,true})for(int height:{8,40,100})for(int i=0;i<128;++i){Random rng{std::mt19937(placementSeed(-2,3,fossilPlacementSalt(deep),uint32_t(i)))};auto p=fossilOrigin(owner,height,deep,rng);check(p.x==cx&&p.z==cz,"fossil origin moved horizontally");check(deep?(p.y>=-32&&p.y<=-17):(p.y>=8&&p.y<=48),"fossil depth outside original range");if(i<4)origins.push_back({deep,height,i,p.x,p.y,p.z});}
 check(fossilWriteInReach({-1,-1},{-32,-58,-32}),"negative fossil neighbour rejected");check(!fossilWriteInReach({-1,-1},{16,-20,0}),"fossil writes farther than one chunk");check(!fossilBlockAllowed("minecraft:diamond_block"),"foreign fossil output admitted");
 {
  std::map<XYZ,int>writes{{{cx,-20,cz},7},{{cx+1,-20,cz},8},{{cx+16,-20,cz},9}};
  auto point=[](XYZ p){return StructurePoint{p[0],p[1],p[2]};};
  auto bounds=fossilOutputBounds(owner,writes,point,[](int){return "minecraft:bone_block";});check(bounds.min.x==cx&&bounds.max.x==cx+16,"fossil whole bounds clipped too early");
  std::map<XYZ,int>world{{{cx,-20,cz},1},{{cx+1,-20,cz},2},{{cx+16,-20,cz},3}};auto original=world;
  auto key=[](StructurePoint p){return XYZ{p.x,p.y,p.z};};int updates=0;
  bool rejected=false;try{publishFossilOwner(owner,writes,point,[&](auto p){return world.at(key(p));},[&](auto p,int value){world[key(p)]=value;return ++updates!=2;},[&](auto p,int value){world[key(p)]=value;return true;});}catch(std::runtime_error const&){rejected=true;}
  check(rejected&&world==original,"fossil partial write was not rolled back");
  updates=0;rejected=false;try{publishFossilOwner(owner,writes,point,[&](auto p){if(p.x==cx+1)throw std::runtime_error("protected block");return world.at(key(p));},[&](auto,int){++updates;return true;},[&](auto,int){return true;});}catch(std::runtime_error const&){rejected=true;}
  check(rejected&&updates==0&&world==original,"fossil wrote before preflight completed");
  auto count=publishFossilOwner(owner,writes,point,[&](auto p){return world.at(key(p));},[&](auto p,int value){world[key(p)]=value;return true;},[&](auto p,int value){world[key(p)]=value;return true;});
  check(count==2&&world.at({cx+16,-20,cz})==3,"fossil fragment modified foreign owner");
 }
 {
  std::map<XYZ,int>pending{{{-1,65,-1},4}};
  auto read=[&](StructurePoint p){return villageDecorRead(p,pending,[](auto q){return q==StructurePoint{-1,64,-1}?3:0;},[](int,int){return 63;},[](int,int){return 4;},[](std::string_view n){return n=="minecraft:air"?0:n=="minecraft:dirt"?1:n=="minecraft:grass_block"?2:9;});};
  check(read({-1,65,-1})==4&&read({-1,64,-1})==3&&read({-1,63,-1})==2&&read({-1,62,-1})==1&&read({-1,66,-1})==0,"village decor lost pending/template/final ground precedence");
  pending[{-2,65,-1}]=5;pending[{-3,65,-1}]=6;pending[{0,65,-1}]=7;std::vector<int>published;
  check(publishVillageDecor({-1,-1},pending,[](auto p){return p.x!=-2;},[](auto p){return p.x==-3;},[&](auto,int b){published.push_back(b);},[](auto,int){return true;})==1&&published==std::vector<int>{4},"village scatter overwrites occupied/entity/foreign owner");
  std::map<XYZ,std::string> stacks{{{-1344,63,4410},"minecraft:hay_block"},{{-1344,64,4410},"minecraft:hay_block"},{{-1346,63,4411},"minecraft:melon_block"}};
  auto grounded=groundVillageDecor(stacks,[](auto const&n){return n;},[](int,int){return 61;});
  check(grounded.contains({-1344,62,4410})&&grounded.contains({-1344,63,4410})&&grounded.contains({-1346,62,4411}),"decor stacks must rest on soil, preserving stack height");
  check(publishVillageDecor({-84,275},grounded,[](auto){return true;},[](auto){return false;},[](auto,auto){},[](auto,auto){return false;})==0,"unsupported scatter must not publish");
  check(villageSurfaceConflict({{-1024,55,6768},{-1008,80,6784}},{{-1050,56,6740},{-1000,70,6790}}),"portal must not overlap village");
  check(!villageSurfaceConflict({{0,50,0},{10,70,10}},{{100,50,100},{120,70,120}}),"distant structures must remain eligible");
  std::map<XYZ,std::string>plants{{{-1,62,-1},"minecraft:grass_path"},{{-1,63,-1},"minecraft:tall_grass"},{{-1,64,-1},"minecraft:tall_grass"},{{-2,63,-1},"minecraft:fern"}};
  auto name=[&](StructurePoint p)->std::string_view{auto i=plants.find({p.x,p.y,p.z});if(i==plants.end())return "minecraft:air";return i->second;};
  auto count=clearUnsupportedStructurePlants({-1,-1},63,64,name,[&](auto p){return p.x==-2||name({p.x,p.y-1,p.z})=="minecraft:tall_grass";},[&](auto p){plants[{p.x,p.y,p.z}]="minecraft:air";});
  check(count==2&&name({-2,63,-1})=="minecraft:fern","plant cleanup lost lower-to-upper survival/order");
 }
 check(structureReservationRemoves(2,0,"minecraft:stone"),"reserved surface boulder survives prefill");
 check(!structureReservationRemoves(2,2,"minecraft:stone"),"unchanged natural stone must survive");
 check(!structureReservationRemoves(2,3,"minecraft:stone"),"substrate replacement is not a removable boulder");
 check(!structureReservationRemoves(2,0,"minecraft:cobblestone"),"structure materials must survive boulder filtering");
 check(!structureReservationRemoves(10,0,"minecraft:stone"),"tree diff must not clear geological stone");
 check(structureReservationRemoves(10,0,"minecraft:oak_log"),"tree reservation regressed");
 check(structurePlantReservation({{-20,60,-30},{-10,70,-20}})==std::optional<std::array<int,4>>{{32736,32726,32770,32760}},"upstream vegetation halo changed");
 check(!structurePlantReservation({{1,0,1},{0,0,0}}),"empty feature bounds clear vegetation");
 J result={{"passed",true},{"selectors",selectors},{"counts",{{"wells",wells},{"fossils",fossils},{"potential",potential}}},{"origins",origins},{"negativeOwnerClipping",true},{"immutableHabitatAdmission",true},{"boundedNaturalDungeonProbes",true},{"nativeFeatureExecutionVerified",false},{"gameplayTestPerformed",false}};
 std::ofstream out(argv[1]);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("cannot write report");std::cout<<"Shared small-feature policy passed; native execution remains adapter work\n";return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
