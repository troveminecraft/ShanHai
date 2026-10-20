#include "rules/StructurePlan.h"
#include <cassert>
using namespace worldgen_rules;
struct Surface{int height=64,water=31,biome=7;double lake=0,river=0,volcanic=0,stone=0,iceHabitat=0;};
int main(){
 VillageCells cells;std::map<VillageXZ,int> foot,doors;
 for(int x=0;x<5;++x){cells[{x,64,0}]="minecraft:oak_planks";foot[{x,0}]=63;}
 auto terrain=[](int x,int){return x==4?66:64;};
 auto shift=villageOriginOffset(cells,foot,doors,terrain);assert(shift==2);
 for(auto [xz,h]:foot)assert(h+1+*shift>=terrain(xz.first,xz.second));
 assert(!villageOriginOffset(cells,foot,doors,[](int x,int){return x==4?67:64;}));
 VillageBlueprint b{{0,0},42,7,{}};StructureBlueprintPiece p;
 p.origin={0,64,0};p.box={{0,64,0},{4,70,4}};p.location="village/plains/houses/porch";
 p.cells={{{0,0,0},{"minecraft:oak_planks",1}},{{1,0,0},{"minecraft:air",2}},{{1,4,0},{"minecraft:oak_planks",1}}};b.pieces.push_back(p);
 auto plan=planVillageLayout(b,[](int,int){return Surface{};},[](int,int,uint32_t){return .5;});
 assert(plan.accepted);
 for(auto const&[cp,writes]:plan.paddingWrites)for(auto const&w:writes)assert(!(w.position.x==1&&w.position.z==0));
 // A savanna farm has a Y=0 soil skirt and cultivated cells at Y=1.
 // The skirt must not hold the farmland one block above natural ground.
 p.location="village/savanna/houses/savanna_small_farm";
 p.cells={{{0,0,0},{"minecraft:grass_block",1}},{{0,1,0},{"minecraft:air",2}},{{0,2,0},{"minecraft:air",2}},
          {{1,0,0},{"minecraft:dirt",3}},{{1,1,0},{"minecraft:farmland",4}},{{1,2,0},{"minecraft:melon_stem",5}}};
 b.pieces={p};plan=planVillageLayout(b,[](int,int){return Surface{};},[](int,int,uint32_t){return .5;});
 assert(plan.accepted&&plan.verticalMoves[0]==-1);
 assert(plan.ground.finished.at({1,0})==64);
 assert(!plan.foundations.contains({0,0}));
 bool repaired=false;for(auto const&[cp,writes]:plan.paddingWrites)for(auto const&w:writes)if(w.position==StructurePoint{0,64,0})repaired=w.block.name=="minecraft:grass_block";
 assert(repaired);
 // plains_small_house_8 at 361,56,2844: two soil layers below the
 // entrance. Align the actual stair/floor rather than lifting its dirt base.
 VillageCells raised{{{361,55,2844},"minecraft:dirt"},{{361,56,2844},"minecraft:dirt"},{{361,57,2844},"minecraft:stone_stairs"},
                     {{360,55,2844},"minecraft:dirt"},{{360,56,2844},"minecraft:dirt"},{{360,57,2844},"minecraft:cobblestone"},{{360,58,2844},"minecraft:wooden_door"}};
 std::map<VillageXZ,int> base{{{361,2844},54},{{360,2844},54}},entry{{{361,2844},57},{{360,2844},57}};
 auto dy=villageOriginOffset(raised,base,entry,[](int,int){return 55;});
 assert(dy==-1);
 assert(villageFinishedHeight(raised,{361,2844},54,entry)+*dy==55);
 assert(villageFinishedHeight(raised,{360,2844},54,entry)+*dy==56);
 // Real tannery entrance: soil under the step is 39, but its approach rises
 // to 40 two blocks outward. That slope must not raise the entrance itself.
 VillageCells tannery{{{-3052,40,-4266},"minecraft:stone_stairs"},{{-3053,40,-4266},"minecraft:cobblestone"},{{-3053,41,-4266},"minecraft:wooden_door"}};
 std::map<VillageXZ,int> tanneryFoot{{{-3052,-4266},39},{{-3053,-4266},39}},tanneryDoor{{{-3052,-4266},40},{{-3053,-4266},40}};
 auto tanneryShift=villageOriginOffset(tannery,tanneryFoot,tanneryDoor,[](int x,int){return x>=-3050?40:39;});
 assert(tanneryShift==0);
 assert(villageFinishedHeight(tannery,{-3052,-4266},39,tanneryDoor)+*tanneryShift==39);
 assert(!villageOriginOffset(tannery,tanneryFoot,tanneryDoor,[](int x,int){return x>=-3050?44:39;}));
 // The same edge fit applies without stairs, doors, or a house template name.
 for(auto floor:{"minecraft:oak_planks","minecraft:stone_slab","minecraft:farmland"}){
  VillageCells edge{{{0,64,0},floor},{{0,65,0},"minecraft:air"},{{0,66,0},"minecraft:air"}};
  std::map<VillageXZ,int> f{{{0,0},63}},d;
  int top=villageFinishedHeight(edge,{0,0},63,d);
  auto shift=villageOriginOffset(edge,f,d,[&](int x,int){return top+(x==2?1:0);});
  assert(shift==0);
  edge[{0,65,0}]="minecraft:cobblestone";
  edge[{0,66,0}]="minecraft:cobblestone";
  // A wall is not an entrance: unrelated ground outside must not lift it.
  assert(villageOriginOffset(edge,f,d,[&](int x,int){return top+(x==2?1:0);})==0);
 }

 // Ground/road planning must preserve access after the piece has been fitted.
 // A raised doorway may descend by one block, but road smoothing cannot erase
 // that first step and leave a two-block ledge along an otherwise low road.
 VillageCells access{{{0,64,0},"minecraft:stone_stairs"},{{0,65,0},"minecraft:air"},{{0,66,0},"minecraft:air"}};
 std::map<VillageXZ,int> accessFoot{{{0,0},63}},accessDoors,roads{{{1,0},0},{{2,0},0},{{3,0},0}};
 auto ground=planVillageGround(access,accessFoot,accessDoors,roads,[](int,int){return 61;});
 assert(ground.finished.at({0,0})==63);
 assert(ground.fitted.at({1,0})==62);
 assert(ground.fitted.at({2,0})==61);

 // A two-step entry and low log planter cannot inherit the higher door floor.
 VillageCells library{{{707,54,4215},"minecraft:grass_block"},{{707,55,4215},"minecraft:acacia_stairs"},{{707,56,4215},"minecraft:air"},
  {{706,54,4215},"minecraft:dirt"},{{706,55,4215},"minecraft:acacia_planks"},{{706,56,4215},"minecraft:acacia_stairs"},
  {{707,54,4214},"minecraft:dirt"},{{707,55,4214},"minecraft:acacia_log"},{{707,56,4214},"minecraft:air"}};
 std::map<VillageXZ,int> libraryFoot{{{707,4215},53},{{706,4215},53},{{707,4214},53}},libraryDoor{{{707,4215},56},{{706,4215},56},{{707,4214},56}};
 assert(villageFinishedHeight(library,{707,4215},53,libraryDoor)==54);
 assert(villageFinishedHeight(library,{706,4215},53,libraryDoor)==55);
 assert(villageFinishedHeight(library,{707,4214},53,libraryDoor)==55);
 auto libraryGround=planVillageGround(library,libraryFoot,libraryDoor,{{{708,4215},0},{{709,4215},0}},[](int,int){return 54;});
 assert(libraryGround.fitted.at({708,4215})==54);

 VillageCells sandstone;for(int y=64;y<=70;++y)sandstone[{0,y,0}]="minecraft:sandstone";
 assert(villageFinishedHeight(sandstone,{0,0},63,{})==64);

}
