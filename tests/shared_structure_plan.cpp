#include "rules/StructurePlan.h"
#include "rules/CityPlan.h"
#include "rules/FrozenTemplates.h"
#include "native/Noise.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace worldgen_rules;
using J=nlohmann::json;
struct Surface {int height=64,water=31,biome=12;double lake=0,river=0,volcanic=0,stone=0,iceHabitat=0,ocean=0,coast=0;};
void require(bool v,char const*why){if(!v)throw std::runtime_error(why);}
J read(std::filesystem::path const&p){J j;std::ifstream(p)>>j;return j;}
auto hash=[](int x,int z,uint32_t seed){return nativegen::hash01(x,z,seed);};
VillageBlueprint synthetic(){
 VillageBlueprint b{{0,0},42,12,{}};
 StructureBlueprintPiece house;house.box={{0,60,0},{4,67,4}};house.origin={0,60,0};house.location="village/savanna/houses/test";
 for(int z=0;z<=4;++z)for(int x=0;x<=4;++x){house.cells.push_back({{x,0,z},{"minecraft:grass_block",5},{}});house.cells.push_back({{x,1,z},{"minecraft:air",1},{}});}
 house.cells.push_back({{2,1,1},{"minecraft:acacia_door",7},{}});b.pieces.push_back(house);
 StructureBlueprintPiece road;road.box={{3,60,0},{12,63,4}};road.origin={3,60,0};road.rigid=false;road.location="village/savanna/streets/test";
 for(int x=0;x<=9;++x)road.cells.push_back({{x,0,2},{"minecraft:jigsaw",3},"minecraft:grass_path[foo=1]"});b.pieces.push_back(road);
 StructureBlueprintPiece tree;tree.kind=StructurePieceKind::Feature;tree.location="minecraft:savanna_tree";tree.origin={1,64,1};tree.box={{1,64,1},{1,64,1}};b.pieces.push_back(tree);
 return b;
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("fixture-dir report.json");std::filesystem::path dir=argv[1];auto fixtures=read(dir/"template-footing-fixture.json"),real=read(dir/"native-village.json"),terrain=read(dir/"planned-vs-natural.json");
 std::map<VillageXZ,int> natural;for(auto const&r:terrain)natural[{r.at("x"),r.at("z")}]=r.at("natural");
 VillageBlueprint reported{{10,-58},uint32_t(uint64_t(-4015943601349081962LL)),12,{}};
 for(auto const&f:fixtures){StructureBlueprintPiece p;p.location=f.at("location").get_ref<std::string const&>();p.origin={f.at("pos")[0],f.at("pos")[1],f.at("pos")[2]};p.rotation=f.at("rotation");
  for(auto const&r:real.at("pieces"))if(r.at("location")==f.at("location")&&r.at("pos")==f.at("pos")){auto a=r.at("box");p.box={{a[0],a[1],a[2]},{a[3],a[4],a[5]}};break;}
  for(auto const&c:f.at("blocks"))p.cells.push_back({{c[0],c[1],c[2]},{c[3].get_ref<std::string const&>(),99},{}});reported.pieces.push_back(std::move(p));
 }
 // The captured surface map covers actual footing and a narrow apron. Sampling
 // other bare columns is outside that fixture; exercise origin replay there,
 // then test full-plan determinism with complete synthetic surfaces below.
 size_t realAccepted=0;for(auto const&p:reported.pieces){VillageCells c;std::map<VillageXZ,int>foot,doors;int entry=320;
  for(auto const&b:p.cells)if(b.block.name.ends_with("_door"))entry=std::min(entry,p.origin.y+b.local.y-1);
  for(auto const&b:p.cells){auto q=villageWorldPoint(p,b.local);auto n=b.block.name;if(n=="minecraft:structure_void"||n=="minecraft:jigsaw")continue;c[{q.x,q.y,q.z}]=n;if(q.y<p.origin.y-1||q.y>p.origin.y+1||!villageFoundationBlock(n))continue;mergeMinimum(foot,{q.x,q.z},q.y-1);if(entry<320)doors[{q.x,q.z}]=entry;}
  auto shift=villageOriginOffset(c,foot,doors,[&](int x,int z){return natural.at({x,z});});require(shift.has_value(),"real native footing lost");++realAccepted;
 }
 auto b=synthetic();size_t calls=0;std::set<VillageXZ>queried;
 auto sample=[&](int x,int z){++calls;require(queried.insert({x,z}).second,"same terrain column sampled repeatedly");return Surface{};};
 auto plan=planVillageLayout(b,sample,hash);require(plan.accepted,"synthetic village rejected");require(plan.verticalMoves[0]==4,"house should fit soil by its actual footprint");require(plan.blockedFeatures[2],"tree crown conflict escaped common plan");require(calls==plan.surfaceQueries,"query accounting wrong");
 auto order=villagePlacementOrder(b,plan,{0,0});require(order.size()==2&&order[0].index==1&&order[1].index==0,"roads must precede rigid buildings");
 auto repeated=planVillageLayout(b,[](int,int){return Surface{};},hash);require(plan.ground.fitted==repeated.ground.fitted&&plan.verticalMoves==repeated.verticalMoves,"plan depends on prior execution");
 auto moved=b;for(size_t i=0;i<b.pieces.size();++i){moved.pieces[i].origin.y+=plan.verticalMoves[i];moved.pieces[i].box=moved.pieces[i].box.moved(plan.verticalMoves[i]);}
 auto reload=planVillageLayout(moved,[](int,int){return Surface{};},hash);require(reload.accepted,"saved layout rejected");for(int dy:reload.verticalMoves)require(dy==0,"saved layout shifted twice");
 auto steep=planVillageLayout(b,[](int x,int){auto s=Surface{};s.height=x<2?60:70;return s;},hash);require(!steep.accepted,"steep lot should reject without partial mutation");
 auto desert=b;desert.biome=11;auto oasis=planVillageLayout(desert,[](int,int){auto s=Surface{};s.biome=11;return s;},hash);require(oasis.accepted&&!oasis.oasis.empty(),"upstream oasis not included");
 require(legacyDataMarker("minecraft:structure_block","DaTA","minecraft:villager_v2")=="minecraft:villager_v2","legacy DATA mode not normalized");
 require(!legacyDataMarker("minecraft:structure_block","SAVE","minecraft:villager_v2")&&!legacyDataMarker("minecraft:stone","DATA","minecraft:villager_v2"),"non-DATA block interpreted as actor marker");
 J populationGeometry=J::array();
 for(int rotation=0;rotation<4;++rotation){auto actors=b;actors.pieces.resize(1);auto&p=actors.pieces.front();p.rotation=rotation;p.origin={-16,60,48};p.box=rotatedTemplateBounds({5,8,5},p.origin,rotation);
  p.cells.push_back({{2,2,2},{"minecraft:structure_block",77},{},"minecraft:villager_v2:minecraft:spawn_farmer"});
  p.cells.push_back({{2,30,2},{"minecraft:structure_block",77},{},"minecraft:iron_golem"});
  p.cells.push_back({{1,2,1},{"minecraft:structure_block",77},{},"Villager"});
  p.cells.push_back({{0,2,0},{"minecraft:cobblestone_wall",78},{},{}});
  auto pop=planVillageLayout(actors,[](int,int){return Surface{};},hash);require(pop.accepted,"actor template rotation rejected");
  auto q=villageWorldPoint(p,{2,2,2},pop.verticalMoves[0]);auto owner=ChunkPosition{spacingCell(q.x,16),spacingCell(q.z,16)};auto markers=villagePopulation(pop,owner);
  require(markers.size()==1&&markers[0].position==q&&markers[0].definition=="minecraft:villager_v2:minecraft:spawn_farmer","actor marker rotation/owner/clipping differs");
  require(villagePopulation(pop,{owner.x+100,owner.z}).empty(),"actor marker leaked to foreign owner");
  require(pop.witnesses.size()==1&&pop.witnesses[0].position==villageWorldPoint(p,{2,1,1},pop.verticalMoves[0]),"reload witness lost template origin/rotation");
  require(pop.wallPositions.size()==1&&pop.wallPositions[0]==villageWorldPoint(p,{0,2,0},pop.verticalMoves[0]),"wall repair positions differ from template");
  populationGeometry.push_back({rotation,q.x,q.y,q.z,owner.x,owner.z,markers[0].definition});
 }
 for(int type:{0,1,2,3,4,5,6,8,9,10,11}){ExtraBlueprint e{type,{0,0},{{0,0,0},{4,6,4}},{{}}};e.pieces[0].box=e.bounds;e.pieces[0].origin={0,0,0};
  auto s=Surface{};if(extraMarine(type)){s.ocean=1;s.height=0;s.biome=0;}if(type==4){s.biome=3;s.height=32;}if(type==8)s.height=110;if(type==10)s.biome=6;
  auto p=planExtraLayout(e,[&](int,int){return s;});require(p.accepted,"upstream extra family rejected valid fixture");
  if(type==0)require(p.verticalMoves[0]==65&&p.pads[0].height==64,"outpost ground not frozen");if(type==2)require(p.bounds.min.y==1,"ship not on immutable seabed");if(type==3)require(p.bounds.min.y==0,"ruin not on lowest immutable seabed");
 }
 require(!planExtraLayout(ExtraBlueprint{0,{0,0},{{0,0,0},{4,6,4}},{}},[](int,int){return Surface{};}).accepted,"empty native start admitted");
 StructureBox footprint{{-16,32,48},{4,45,68}};size_t footprintReads=0;
 auto uneven=[&](int x,int z){++footprintReads;auto s=Surface{};s.biome=13;s.height=x>=3?66:64;return s;};
 auto scattered=planScatteredLayout(footprint,13,uneven);require(scattered.accepted&&scattered.floor==65&&footprintReads==441,"scattered whole-footprint floor incorrect");
 auto repeatedScattered=planScatteredLayout(footprint.moved(scattered.floor-footprint.min.y),13,uneven);require(repeatedScattered.floor==scattered.floor,"scattered reload changed floor");
 auto edgeMismatch=planScatteredLayout(footprint,13,[](int x,int z){auto s=Surface{};s.biome=x==4&&z==68?12:13;return s;});require(!edgeMismatch.accepted,"far scattered footprint edge crosses biome boundary");
 auto a=extraPieceSeed(2,0,{10,10},{0,0},42),c=extraPieceSeed(2,0,{10,10},{1,0},42);require(a==c,"ship random changed between fragments");
 // A connected basement must remain connected through every rotation. These
 // are geometric requirements, independent of the template loader's ABI.
 struct FixedRandom {int rotation;int draws=0;int nextInt(int n){return draws++?2%n:rotation;}int nextInt(){return 123;}double nextDouble(){return .75;}};
 for(int rotation=0;rotation<4;++rotation){FixedRandom rng{rotation};auto igloo=planFrozenIgloo({-2,3},80,rng,[](IglooTemplateRole role){return role==IglooTemplateRole::Ladder?StructurePoint{3,3,3}:StructurePoint{7,6,8};});
  require(igloo.ladders==6&&igloo.parts.size()==8,"basement layout was not frozen");
  for(size_t i=2;i+1<igloo.parts.size();++i)require(igloo.parts[i-1].origin.y-igloo.parts[i].origin.y==3,"basement shaft has a gap");
  require(igloo.parts.back().bounds.min.y==56,"basement laboratory height wrong");
 }
 std::vector<StructureBlueprintPiece>rooms(1);rooms[0].box={{-12,-45,-12},{12,-32,12}};
 auto shapes=cityShapes(rooms,{{-48,-64,-48},{48,80,48}});auto column=cityColumn(shapes,{}, {},0,0,41);
 require(column.floor==-46&&column.ceiling>=-27,"city room undercuts its foundation or roof");
 require(cityNatural("minecraft:diamond_ore")&&!cityNatural("minecraft:deepslate_bricks")&&!cityNatural("minecraft:chest"),"city carving destroys structure masonry");
 auto links=cityConnections(rooms[0].box,{0,0},42,{},[](int,int){return Surface{};},[](auto const&,int,int y,int){return y<=-44?2:0;});
 require(!links.empty(),"city did not connect to reachable dry caves");
 for(auto const&link:links)require(link.a.y>=-52&&link.b.y>=-52,"city links breach protected bottom");
 auto started=std::chrono::steady_clock::now();size_t operations=0;for(int i=0;i<10000;++i)operations+=villagePlacementOrder(b,plan,{0,0}).size();auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 J out={{"passed",true},{"actualNativeTemplates",realAccepted},{"fullSyntheticPlan",true},{"terrainColumnsQueriedOnce",true},{"savedLayoutIdempotent",true},{"roadsBeforeBuildings",true},{"treeConflicts",true},{"desertOasis",true},{"extraFamilies",11},{"iglooFourRotations",true},{"cityClearanceAndConnections",true},{"cachedPlacementOperations",operations},{"cachedPlacement10000Ms",ms},{"nativeTemplateParityVerified",false},{"gameplayAcceptancePerformed",false}};
 J geometry;geometry["moves"]=plan.verticalMoves;geometry["population"]=std::move(populationGeometry);
 for(auto const&[p,h]:plan.ground.fitted)geometry["ground"].push_back({p.first,p.second,h});
 for(auto const&[p,b]:plan.templateBlocks)geometry["blocks"].push_back({p[0],p[1],p[2],b.name});
 for(auto const&[cp,ops]:plan.placements)for(auto const&op:ops)geometry["placements"].push_back({cp.first,cp.second,op.index,op.seed});
 for(auto const&[p,c]:oasis.oasis)geometry["oasis"].push_back({p.first,p.second,c.floor,c.water,c.green});
 for(int z=-48;z<=48;z+=3)for(int x=-48;x<=48;x+=3){auto c=cityColumn(shapes,links,{},x,z,41);geometry["city"].push_back({x,z,c.floor,c.ceiling,c.requiredCeiling});}
 auto saved=out;saved["geometry"]=std::move(geometry);std::ofstream(argv[2])<<saved.dump(2);std::cout<<out.dump(2)<<'\n';return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
