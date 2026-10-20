#include "rules/StructurePlan.h"
#include <cassert>
using namespace worldgen_rules;
struct Surface{int height=53,water=31,biome=7;double lake=0,river=0,volcanic=0,stone=0,iceHabitat=0;};
int main(){
 // The reported house at 349,54,2840 has an east-facing final-state stair,
 // rotated 180 degrees. Its front road is one block above the house's soil.
 for(int rotation=0;rotation<4;++rotation){
  VillageBlueprint b{{23,176},42,7,{}};StructureBlueprintPiece p;
  p.origin={349,54,2846};p.rotation=rotation;p.location="village/plains/houses/entrance";
  p.box={{342,54,2833},{356,59,2859}};
  p.cells={{{0,0,6},{"minecraft:jigsaw",1},"minecraft:cobblestone_stairs[facing=east,half=bottom]"},
   {{1,0,6},{"minecraft:cobblestone",2}},{{2,0,6},{"minecraft:cobblestone",2}},
   {{1,1,6},{"minecraft:wooden_door",3}},{{1,2,6},{"minecraft:wooden_door",3}},
   {{0,1,6},{"minecraft:air",4}}};
  b.pieces.push_back(p);auto step=villageWorldPoint(p,{0,0,6});
  auto sample=[&](int x,int z){Surface s;if(x==step.x&&z==step.z)s.height=54;return s;};
  auto plan=planVillageLayout(b,sample,[](int,int,uint32_t){return .5;});
  assert(plan.accepted&&plan.verticalMoves[0]==1);
  // Stair bottom remains at the road's height, with its block one above soil.
  assert(step.y+plan.verticalMoves[0]-1==54);
  assert(plan.ground.finished.at({step.x,step.z})==54);
  auto moved=b;moved.pieces[0].origin.y+=1;moved.pieces[0].box=moved.pieces[0].box.moved(1);
  auto again=planVillageLayout(moved,sample,[](int,int,uint32_t){return .5;});
  assert(again.accepted&&again.verticalMoves[0]==0);
  // Flat terrain also needs the bottom half of the entry step kept clear.
  auto flat=planVillageLayout(b,[](int,int){return Surface{};},[](int,int,uint32_t){return .5;});
  assert(flat.accepted&&flat.verticalMoves[0]==0);
 }
 assert(villagePlanningBlock("minecraft:jigsaw","minecraft:air")=="minecraft:air");
 assert(villagePlanningBlock("minecraft:jigsaw").empty());
}
