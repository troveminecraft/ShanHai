#include "rules/StructurePlan.h"
#include <cassert>
using namespace worldgen_rules;
int main(){
 // A fence gate stands on soil; its top is not the yard's walking surface.
 VillageCells pen{{{0,79,0},"minecraft:grass_block"},{{0,80,0},"minecraft:oak_fence_gate"},
                  {{1,79,0},"minecraft:grass_block"},{{1,80,0},"minecraft:oak_fence"}};
 assert(!villageFoundationBlock("minecraft:oak_fence_gate"));
 assert(!villageFoundationBlock("minecraft:fence_gate"));
 assert(villageFinishedHeight(pen,{0,0},78,{})==79);
 assert(villageOriginOffset(pen,{{{0,0},78},{{1,0},78}},{},[](int,int){return 80;})==1);
 // A hillside beside the stair can be one block higher without lifting it.
 VillageCells stair{{{0,88,0},"minecraft:oak_stairs"}};
 assert(villageOriginOffset(stair,{{{0,0},87}},{},[](int x,int z){return x==2||z==2?87:86;})==-1);

 for(auto n:{"minecraft:fern","minecraft:large_fern","minecraft:poppy","minecraft:oak_sapling"})assert(!villageFoundationBlock(n));
 VillageCells fire{{{0,64,0},"minecraft:campfire"}};
 assert(villageOriginOffset(fire,{{{0,0},63}},{},[](int,int){return 64;})==1);
 // Interior water basin floor is below the entry; it must not raise the house.
 VillageCells basin{{{0,64,0},"minecraft:dirt"},{{0,65,0},"minecraft:water"},
 {{1,64,0},"minecraft:dirt"},{{1,65,0},"minecraft:dirt"},{{1,66,0},"minecraft:stone_stairs"},
 {{1,67,0},"minecraft:air"},{{1,68,0},"minecraft:air"}};
 assert(villageOriginOffset(basin,{{{0,0},63},{{1,0},63}},{},[](int,int){return 64;})==-1);
 // Native template air cleared a road. Publish the planned surface, not an
 // inferred y-1 surface, and do not touch a neighbouring owner's column.
 VillageLayoutPlan plan;plan.roadSurfaceWrites[{0,0}]={{{2,64,2},{"minecraft:grass_path",0}},{{3,64,2},{"minecraft:grass_path",0}}};
 plan.roadSurfaceWrites[{1,0}]={{{16,64,2},{"minecraft:grass_path",0}}};
 std::map<std::array<int,3>,std::string_view> world{{{2,63,2},"minecraft:grass_path"},{{3,64,2},"minecraft:stone_stairs"}};
 auto get=[&](StructurePoint p){auto it=world.find({p.x,p.y,p.z});return it==world.end()?std::string_view("minecraft:air"):it->second;};
 auto set=[&](StructurePoint p,StructureBlock b){assert(p.x>=0&&p.x<16);world[{p.x,p.y,p.z}]=b.name;};
 applyVillageRepairs(plan,{0,0},get,set);
 assert(get({2,64,2})=="minecraft:grass_path");assert(get({3,64,2})=="minecraft:stone_stairs");assert(get({16,64,2})=="minecraft:air");
 auto once=world;applyVillageRepairs(plan,{0,0},get,set);assert(once==world);
}
