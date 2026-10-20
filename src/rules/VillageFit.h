#pragma once
#include "StructureRules.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <map>
#include <cmath>
#include <optional>
#include <limits>
#include <set>
#include <vector>
namespace worldgen_rules {
using VillageXZ=std::pair<int,int>;
using VillageCells=std::map<std::array<int,3>,std::string_view>;
// Jigsaw is a template instruction, not the block left in the finished house.
// Entrance stairs commonly exist only as its final_state. Omitting them makes
// the solver align the floor with soil and bury the bottom step in the road.
inline std::string_view villagePlanningBlock(std::string_view name,std::string_view finalState={}){
 if(name!="minecraft:jigsaw")return name;
 auto block=finalState.substr(0,finalState.find('['));
 return block.starts_with("minecraft:")?block:std::string_view{};
}
inline std::pair<int,int> rotateVillageXZ(int x,int z,int rotation){
 switch(rotation){case 0:return {x,z};case 1:return {-z,x};case 2:return {-x,-z};case 3:return {z,-x};default:throw std::runtime_error("Invalid village rotation");}
}
inline bool villageOpen(std::string_view n);
inline bool villageFoundationBlock(std::string_view n){
 return n!="minecraft:air"&&n!="minecraft:structure_void"&&n!="minecraft:jigsaw"&&
  !villageOpen(n)&&
  !n.ends_with("_leaves")&&!n.ends_with("_fence")&&!n.ends_with("_fence_gate")&&n!="minecraft:fence_gate"&&!n.ends_with("_door")&&
  n!="minecraft:grass"&&n!="minecraft:short_grass"&&n!="minecraft:tall_grass"&&n!="minecraft:snow_layer"&&
  n!="minecraft:torch"&&n!="minecraft:structure_block"&&n!="minecraft:water"&&n!="minecraft:lava";
}
inline bool villageSoil(std::string_view n){return n=="minecraft:dirt"||n=="minecraft:grass_block"||n=="minecraft:grass_path"||n=="minecraft:farmland"||n=="minecraft:sand"||n=="minecraft:sandstone"||n=="minecraft:smooth_sandstone"||n=="minecraft:snow";}
inline bool villageOpen(std::string_view n){return n=="minecraft:grass"||n.ends_with("_sapling")||n=="minecraft:poppy"||n=="minecraft:dandelion"||n=="minecraft:fern"||n=="minecraft:large_fern"||n=="minecraft:blue_orchid"||n=="minecraft:allium"||n=="minecraft:azure_bluet"||n.ends_with("_tulip")||n=="minecraft:oxeye_daisy"||n=="minecraft:cornflower"||n=="minecraft:lily_of_the_valley"||n=="minecraft:air"||n=="minecraft:short_grass"||n=="minecraft:tall_grass"||n=="minecraft:deadbush"||n=="minecraft:snow_layer"||n=="minecraft:wheat"||n=="minecraft:carrots"||n=="minecraft:potatoes"||n=="minecraft:beetroot"||n=="minecraft:melon_stem"||n=="minecraft:pumpkin_stem";}
// Farm templates contain a grass/dirt skirt below the cultivated layer. That
// skirt is padding, not a second floor that should lift the entire field.
inline void trimVillageFarmFooting(VillageCells const&cells,std::map<VillageXZ,int>&footing){
 bool farm=false;std::set<VillageXZ> core;
 for(auto const&[p,n]:cells){farm|=n=="minecraft:farmland";if(n=="minecraft:farmland"||n=="minecraft:water"||(!villageSoil(n)&&!villageOpen(n)&&villageFoundationBlock(n)))core.emplace(p[0],p[2]);}
 if(farm)std::erase_if(footing,[&](auto const&e){return !core.contains(e.first);});
}
template<class Cells,class Name> bool villageCoveredGround(Cells const&cells,VillageXZ xz,int ground,int ceiling,Name name){
 for(int y=ground+1;y<=std::min(319,ceiling);++y){auto c=cells.find({xz.first,y,xz.second});if(c!=cells.end()){auto n=name(c->second);if(villageFoundationBlock(n)&&!villageSoil(n)&&!villageOpen(n))return true;}}
 return false;
}
// Actual-footing and narrow apron rules extracted from the LLBDS adapter.
// Template padding and structure_void never define a rectangular foundation.
struct VillageGroundPlan {std::map<VillageXZ,int> fitted,finished;};
inline int villageFinishedHeight(VillageCells const&cells,VillageXZ xz,int h,std::map<VillageXZ,int> const&){
  auto at=[&](int y)->std::string_view{auto it=cells.find({xz.first,y,xz.second});return it==cells.end()?"":it->second;};
  // Read this column's buried soil and first structural surface. A building
  // may have several stair levels and low planter borders: its lowest door
  // floor is not a valid height for every column in the footprint.
  for(int y=h+1;y<=std::min(h+8,319);++y){
   auto n=at(y);
   if(n=="minecraft:farmland")return y;
   if(n.empty()||n=="minecraft:air"||n=="minecraft:structure_void")return y==h+1?h+1:y-1;
   if(villageSoil(n)&&n!="minecraft:sandstone"&&n!="minecraft:smooth_sandstone"){
    auto above=at(y+1);
    if(above.empty()||villageOpen(above)||above.ends_with("_door"))return y;
    continue;
   }
   if(!villageFoundationBlock(n)||villageOpen(n))return y-1;
   return y-(n.ends_with("_stairs")||n.ends_with("_slab")||n=="minecraft:campfire"||n=="minecraft:soul_campfire"?1:0);
  }
  return h+1;
}
// Visit open ground approaches of every actual walking edge, regardless of
// template name or presence of a door. At most two cells per horizontal side.
template<class Visit> void villageApproaches(VillageCells const&cells,std::map<VillageXZ,int> const&foundations,VillageXZ xz,int finished,Visit visit){
  auto at=[&](int y)->std::string_view{auto it=cells.find({xz.first,y,xz.second});return it==cells.end()?std::string_view{}:it->second;};
  auto open=[](std::string_view n){return n.empty()||villageOpen(n)||n.ends_with("_door");};
  auto step=at(finished+1);
  int clearance=finished+1;
  if(step.ends_with("_stairs")||step.ends_with("_slab"))++clearance;
  if(!open(at(clearance))||!open(at(clearance+1)))return;
  for(auto [dx,dz]:std::array<VillageXZ,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
   if(foundations.contains({xz.first+dx,xz.second+dz}))continue;
   for(int distance=1;distance<=2;++distance){
    VillageXZ outside{xz.first+dx*distance,xz.second+dz*distance};
    if(foundations.contains(outside))break;
    // Do not sample across an adjacent wall or other template obstruction.
    bool blocked=false;for(int y=clearance;y<=clearance+1;++y){auto c=cells.find({outside.first,y,outside.second});if(c!=cells.end()&&!open(c->second)){blocked=true;break;}}
    if(blocked)break;
    visit(outside,distance);
   }
  }
}
// Match each rigid piece to the immutable soil under its actual footing.
// Roads are projected later; no whole-village or bounding-box levelling.
template<class Sample> std::optional<int> villageOriginOffset(
 VillageCells const&cells,std::map<VillageXZ,int> const&foundations,
 std::map<VillageXZ,int> const&doors,Sample sample){
 if(foundations.empty())return 0;
 std::vector<int> offsets,edges;int approachFloor=std::numeric_limits<int>::min();offsets.reserve(foundations.size());
 for(auto const&[xz,h]:foundations){
  int finished=villageFinishedHeight(cells,xz,h,doors);
  offsets.push_back(sample(xz.first,xz.second)-finished);
  villageApproaches(cells,foundations,xz,finished,[&](VillageXZ outside,int distance){
   approachFloor=std::max(approachFloor,sample(outside.first,outside.second)-finished-distance);
   edges.push_back(sample(xz.first,xz.second)-finished);
  });
 }
 // Interior basins and buried template soil are not entrances. Align the
 // accessible perimeter first; otherwise a low decorative water trough lifts
 // every floor and stair in the building. Enclosed pieces retain footing fit.
 if(!edges.empty())offsets=std::move(edges);
 // Neighbouring slopes may rise one block per horizontal step. Treating a
 // point two cells away as another floor lifts the whole building to that
 // remote height. Only the walkable footprint has zero-distance authority.
 // Reject lots requiring a tall downhill podium.
 auto [low,high]=std::minmax_element(offsets.begin(),offsets.end());int shift=std::max(*high,approachFloor);
 if(std::abs(shift)>VillagePieceMaxOffset)return {};
 if(shift-*low>2)return {};
 return shift;
}
template<class Sample> VillageGroundPlan planVillageGround(
 VillageCells const&cells,std::map<VillageXZ,int> const&foundations,
 std::map<VillageXZ,int> const&doors,std::map<VillageXZ,int> const&roadOffsets,Sample sample){
 std::map<VillageXZ,int> finished,result,approachMinimum;
 std::map<VillageXZ,std::pair<double,double>> blends;
 std::map<VillageXZ,double> strength;
 for(auto const&[xz,h]:foundations){
  auto at=[&](int y)->std::string_view{auto it=cells.find({xz.first,y,xz.second});return it==cells.end()?"":it->second;};
  int top=villageFinishedHeight(cells,xz,h,doors);bool exposed=false;
  for(int y=h+1;y<=h+2;++y)exposed|=villageSoil(at(y))&&villageOpen(at(y+1));
  finished[xz]=top;
  villageApproaches(cells,foundations,xz,top,[&](VillageXZ outside,int distance){auto [it,added]=approachMinimum.emplace(outside,top-distance);if(!added)it->second=std::max(it->second,top-distance);});
  int apron=exposed?top:std::min(top,sample(xz.first,xz.second));
  for(int dz=-VillageApronRadius;dz<=VillageApronRadius;++dz)for(int dx=-VillageApronRadius;dx<=VillageApronRadius;++dx){double distance=std::hypot(dx,dz);if(distance>=VillageApronRadius)continue;double w=1-distance/VillageApronRadius;w*=w;VillageXZ p{xz.first+dx,xz.second+dz};auto&b=blends[p];b.first+=w*apron;b.second+=w;strength[p]=std::max(strength[p],w);}
 }
 for(auto const&[xz,b]:blends){int natural=sample(xz.first,xz.second);result[xz]=natural+std::clamp(int(std::round((b.first/b.second-natural)*strength[xz])),-VillageApronMaxShift,VillageApronMaxShift);}
 // Keep a walkable descent from an elevated threshold. Bound earthwork to
 // the same one-block apron budget, rather than raising an entire village.
 for(auto const&[xz,minimum]:approachMinimum){int natural=sample(xz.first,xz.second);auto [it,added]=result.emplace(xz,natural);it->second=std::max(it->second,std::min(minimum,natural+VillageApronMaxShift));}
 for(auto const&[xz,h]:finished)result[xz]=h;
 std::map<VillageXZ,int> tops;
 for(auto const&[xz,offset]:roadOffsets){auto it=result.find(xz);tops[xz]=(it==result.end()?sample(xz.first,xz.second):it->second)+offset;}
 for(int pass=0;pass<RoadRelaxationPasses;++pass){bool changed=false;for(auto&[xz,top]:tops){if(foundations.contains(xz))continue;int target=top;
  for(auto [dx,dz]:std::array<VillageXZ,4>{{{1,0},{-1,0},{0,1},{0,-1}}})if(auto it=tops.find({xz.first+dx,xz.second+dz});it!=tops.end())target=std::min(target,it->second+1);
  if(auto it=approachMinimum.find(xz);it!=approachMinimum.end())target=std::max(target,std::min(it->second,sample(xz.first,xz.second)+VillageApronMaxShift)+roadOffsets.at(xz));
  if(target!=top){top=target;changed=true;}
 }if(!changed)break;}
 for(auto const&[xz,top]:tops)if(!foundations.contains(xz))result[xz]=top-roadOffsets.at(xz);
 return {std::move(result),std::move(finished)};
}
}
