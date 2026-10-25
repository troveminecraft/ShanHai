#pragma once
#include "StructureRandom.h"
#include "VillageFit.h"
#include <limits>
#include <climits>
#include <set>
#include <span>
#include <tuple>
#include <optional>
#include <string>

// LLBDS-owned, game-independent blueprints. Adapters only decode game objects,
// invoke native template builders and execute the returned operations. Opaque
// block handles carry native states through the plan; they NEVER affect a rule.
namespace worldgen_rules {
struct StructurePoint {int x{},y{},z{};bool operator==(StructurePoint const&)const=default;};
struct StructureBox {
 StructurePoint min{INT_MAX,INT_MAX,INT_MAX},max{INT_MIN,INT_MIN,INT_MIN};
 bool valid()const{return min.x<=max.x&&min.y<=max.y&&min.z<=max.z;}
 bool intersectsXZ(StructureBox const&b,int margin=0)const{return valid()&&b.valid()&&max.x+margin>=b.min.x&&min.x-margin<=b.max.x&&max.z+margin>=b.min.z&&min.z-margin<=b.max.z;}
 void include(StructureBox const&b){if(!b.valid())return;min.x=std::min(min.x,b.min.x);min.y=std::min(min.y,b.min.y);min.z=std::min(min.z,b.min.z);max.x=std::max(max.x,b.max.x);max.y=std::max(max.y,b.max.y);max.z=std::max(max.z,b.max.z);}
 StructureBox moved(int dy)const{if(!valid())return *this;auto b=*this;b.min.y+=dy;b.max.y+=dy;return b;}
};
struct StructureBlock {std::string_view name;uintptr_t handle{};};
// Adapters read the template's real tag representation. DATA mode/metadata is
// normalized without taking ownership of a game-allocated unordered_map.
inline std::optional<std::string_view> legacyDataMarker(std::string_view block,std::string_view mode,std::string_view metadata){
 if(block!="minecraft:structure_block"||mode.size()!=4)return {};
 constexpr std::string_view data="data";for(size_t i=0;i<4;++i){char c=mode[i];if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(c!=data[i])return {};}
 return metadata;
}
struct StructureTemplateCell {StructurePoint local;StructureBlock block;std::string_view finalState;std::string_view marker;};
enum class StructurePieceKind {Template,Feature,Other};
struct StructureBlueprintPiece {
 StructureBox box;StructurePoint origin;int rotation{};bool rigid=true;
 StructurePieceKind kind=StructurePieceKind::Template;std::string_view location;
 std::vector<StructureTemplateCell> cells;
};
struct VillageBlueprint {ChunkPosition anchor;uint32_t seed{};int biome{};std::vector<StructureBlueprintPiece> pieces;};
struct StructureWrite {StructurePoint position;StructureBlock block;};
struct VillageOasisColumn {int floor,water;bool green;};
struct StructurePiecePlacement {size_t index;uint32_t seed;};
struct StructureEntityMarker {StructurePoint position;std::string definition;};
struct VillageLayoutPlan {
 bool accepted=false;std::string_view rejection;
 StructureBox bounds;std::vector<int> verticalMoves;std::vector<bool> blockedFeatures;
 VillageGroundPlan ground;std::map<VillageXZ,int> foundations,doors,roadOffsets;
 std::map<std::array<int,3>,StructureBlock> templateBlocks;
 std::map<VillageXZ,std::pair<int,int>> roadSurfaceFix;
 std::map<VillageXZ,std::vector<StructureWrite>> roadSurfaceWrites;
 std::map<ChunkPosition,std::vector<StructureWrite>,bool(*)(ChunkPosition const&,ChunkPosition const&)> paddingWrites{
  [](ChunkPosition const&a,ChunkPosition const&b){return std::tie(a.x,a.z)<std::tie(b.x,b.z);}};
 std::map<VillageXZ,VillageOasisColumn> oasis;
 std::map<VillageXZ,std::vector<StructurePiecePlacement>> placements;
 std::map<VillageXZ,std::vector<StructureEntityMarker>> population;
 std::vector<StructureWrite> witnesses;
 std::vector<StructurePoint> wallPositions;
 size_t surfaceQueries{};int groundMin=INT_MAX,groundMax=-64;int maxY=319;
};
inline bool sharedStructurePlan(std::string_view revision){return revision==StructurePlanRevision;}
// Villages own their streets/aprons as well as individual buildings. Surface
// structures must reserve their complete footprint before any chunk is written.
inline bool villageSurfaceConflict(StructureBox const&structure,StructureBox const&village){return structure.intersectsXZ(village,28);}
inline bool sharedCandidateRules(std::string_view revision){return revision=="llbds-structures-v1"||revision=="llbds-structures-v2"||sharedStructurePlan(revision);}
inline std::string_view villagePavement(StructureTemplateCell const&cell){
 auto n=cell.block.name;if(n=="minecraft:jigsaw"){n=cell.finalState;n=n.substr(0,n.find('['));}
 return n=="minecraft:grass_path"||n=="minecraft:smooth_sandstone"||n=="minecraft:packed_ice"||n=="minecraft:blue_ice"||n=="minecraft:snow"?n:std::string_view{};
}
inline void mergeMinimum(std::map<VillageXZ,int>&out,VillageXZ p,int h){auto[it,added]=out.emplace(p,h);if(!added)it->second=std::min(it->second,h);}
inline StructurePoint villageWorldPoint(StructureBlueprintPiece const&p,StructurePoint q,int dy=0){auto[x,z]=rotateVillageXZ(q.x,q.z,p.rotation);return {p.origin.x+x,p.origin.y+q.y+dy,p.origin.z+z};}
inline bool villageDecorBlocked(StructureBlueprintPiece const&p,std::span<StructureBlueprintPiece const>pieces){
 // Match the upstream: scatter flowers are captured separately, only tree
 // crowns require a radius exclusion against buildings/roads.
 if(p.kind!=StructurePieceKind::Feature||p.location.find("tree")==std::string_view::npos)return false;
 for(auto const&q:pieces){if(q.kind!=StructurePieceKind::Template||q.location.find("animals/")!=std::string_view::npos||q.location.find("terminators/")!=std::string_view::npos||!q.box.valid())continue;
  int dx=std::max({q.box.min.x-p.origin.x,0,p.origin.x-q.box.max.x}),dz=std::max({q.box.min.z-p.origin.z,0,p.origin.z-q.box.max.z});
  if(dx*dx+dz*dz<=VillageTreeMargin*VillageTreeMargin)return true;
 }return false;
}
template<class Sample> int villageSiteStyle(ChunkPosition anchor,Sample sample){
 auto center=sample(anchor.x*16,anchor.z*16);int style=villageStyle(center.biome),lo=INT_MAX,hi=-64;
 for(int z=-VillageSampleRadius;z<=VillageSampleRadius;z+=VillageSampleStep)for(int x=-VillageSampleRadius;x<=VillageSampleRadius;x+=VillageSampleStep){auto s=sample(anchor.x*16+x,anchor.z*16+z);if(!villageSurfaceAllowed(s,style))return -1;lo=std::min(lo,s.height);hi=std::max(hi,s.height);if(hi-lo>VillageMaxSlope)return -1;}
 return style;
}
// The sampler is immutable terrain, never the live world's half-built chunks.
// One cache covers validation, origin fitting, road fitting and oasis planning.
template<class Sample,class Hash> VillageLayoutPlan planVillageLayout(VillageBlueprint const&input,Sample sample,Hash hash,int maxY=319){
 VillageLayoutPlan out;out.maxY=maxY;out.verticalMoves.resize(input.pieces.size());out.blockedFeatures.resize(input.pieces.size());
 if(input.pieces.size()>4096){out.rejection="piece-limit";return out;}
 using Surface=decltype(sample(0,0));std::map<VillageXZ,Surface> terrain;
 auto at=[&](int x,int z)->Surface const&{auto key=VillageXZ{x,z};auto it=terrain.find(key);if(it==terrain.end()){++out.surfaceQueries;it=terrain.emplace(key,sample(x,z)).first;}return it->second;};
 auto height=[&](int x,int z){return at(x,z).height;};
 int style=villageSiteStyle(input.anchor,at);if(style<0||style!=villageStyle(input.biome)){out.rejection="village-site";return out;}
 VillageCells cells;std::map<VillageXZ,std::string_view> roadMaterials;
 // Stage everything before applying any move to a native object.
 for(size_t i=0;i<input.pieces.size();++i){auto const&p=input.pieces[i];
  if(p.kind!=StructurePieceKind::Template)continue;
  int wx=input.anchor.x*16,wz=input.anchor.z*16;
  if(p.box.valid()&&(p.box.min.x<wx-VillageReach||p.box.max.x>wx+VillageReach||p.box.min.z<wz-VillageReach||p.box.max.z>wz+VillageReach)){out.rejection="piece-reach";return out;}
  if(p.cells.size()>131072){out.rejection="template-limit";return out;}
  VillageCells local;std::map<VillageXZ,int> footing,doors;int entry=INT_MAX;
  for(auto const&c:p.cells)if(c.block.name.ends_with("_door"))entry=std::min(entry,p.origin.y+c.local.y-1);
  for(auto const&c:p.cells){auto q=villageWorldPoint(p,c.local);auto n=villagePlanningBlock(c.block.name,c.finalState);auto key=VillageXZ{q.x,q.z};
   if(p.rigid){if(n.empty()||n=="minecraft:structure_void")continue;local[{q.x,q.y,q.z}]=n;
    if(q.y<p.origin.y-1||q.y>p.origin.y+1||!villageFoundationBlock(n))continue;
    mergeMinimum(footing,key,q.y-1);if(entry!=INT_MAX)mergeMinimum(doors,key,entry);
   }else if(p.location.find("/streets/")!=std::string_view::npos&&!villagePavement(c).empty()){
    auto[it,added]=out.roadOffsets.emplace(key,c.local.y);if(added||c.local.y>=it->second){it->second=c.local.y;roadMaterials[key]=villagePavement(c);}
   }
  }
  trimVillageFarmFooting(local,footing);
  int dy=0;if(p.rigid){auto shift=villageOriginOffset(local,footing,doors,height,maxY);if(!shift){out.rejection="steep-footing";return out;}dy=*shift;}out.verticalMoves[i]=dy;
  auto box=p.box.moved(dy);out.bounds.include(box);
  if(box.valid())for(int z=box.min.z;z<=box.max.z;z+=VillagePieceStep)for(int x=box.min.x;x<=box.max.x;x+=VillagePieceStep){auto const&s=at(x,z);
   if(!villageSurfaceAllowed(s,style)||(p.rigid&&std::abs(s.height-(p.origin.y+dy))>VillagePieceMaxOffset)){out.rejection="piece-terrain";return out;}
   out.groundMin=std::min(out.groundMin,s.height);out.groundMax=std::max(out.groundMax,s.height);
  }
  if(!p.rigid)continue;
  for(auto const&[q,n]:local)cells[{q[0],q[1]+dy,q[2]}]=n;
  for(auto const&[xz,h]:footing)mergeMinimum(out.foundations,xz,h+dy);
  for(auto const&[xz,h]:doors)mergeMinimum(out.doors,xz,h+dy);
  for(auto const&c:p.cells){if(c.block.name=="minecraft:structure_void"||c.block.name=="minecraft:jigsaw")continue;auto q=villageWorldPoint(p,c.local,dy);out.templateBlocks[{q.x,q.y,q.z}]=c.block;}
 }
 for(auto const&p:input.pieces)if(p.kind!=StructurePieceKind::Template)out.bounds.include(p.box);
 auto b=out.bounds;int wx=input.anchor.x*16,wz=input.anchor.z*16;
 if(!b.valid()||b.min.y< -58||b.max.y>maxY||b.min.x<wx-VillageReach||b.max.x>wx+VillageReach||b.min.z<wz-VillageReach||b.max.z>wz+VillageReach){out.rejection="village-reach";return out;}
 for(size_t i=0;i<input.pieces.size();++i)out.blockedFeatures[i]=villageDecorBlocked(input.pieces[i],input.pieces);
 out.ground=planVillageGround(cells,out.foundations,out.doors,out.roadOffsets,height,maxY);
 // Publish road surfaces from the same immutable plan used by projection.
 // Do not lower an already placed path based on incidental dirt underneath.
 // Rigid template columns own their blocks, including resolved jigsaw cells.
 for(auto const&[xz,offset]:out.roadOffsets){
  if(out.foundations.contains(xz))continue;
  auto it=out.ground.fitted.find(xz);int y=(it==out.ground.fitted.end()?height(xz.first,xz.second):it->second)+offset;
  auto c=cells.find({xz.first,y,xz.second});if(c!=cells.end()&&!villageOpen(c->second)&&!villageSoil(c->second))continue;
  // A floor/roof above this point can be an interior: template air must survive.
  if(villageCoveredGround(cells,xz,y,out.bounds.max.y,[](auto n){return n;},maxY))continue;
  std::string_view material=roadMaterials.at(xz);
  out.roadSurfaceWrites[{spacingCell(xz.first,16),spacingCell(xz.second,16)}].push_back({{xz.first,y,xz.second},{material,0}});
 }
 for(auto const&p:input.pieces){if(p.kind!=StructurePieceKind::Template||p.rigid)continue;
  for(auto const&c:p.cells){auto n=c.block.name,pavement=villagePavement(c);if(n=="minecraft:air"||n=="minecraft:structure_void"||(n=="minecraft:jigsaw"&&pavement.empty()))continue;
   auto q=villageWorldPoint(p,c.local);auto it=out.ground.fitted.find({q.x,q.z});q.y=(it==out.ground.fitted.end()?height(q.x,q.z):it->second)+c.local.y;
   auto block=n=="minecraft:jigsaw"?StructureBlock{pavement,0}:c.block;out.templateBlocks.try_emplace({q.x,q.y,q.z},block);
  }
 }
 for(auto const&[xz,h]:out.ground.fitted){if(out.foundations.contains(xz))continue;auto it=out.templateBlocks.find({xz.first,h,xz.second});if(it==out.templateBlocks.end())continue;
  // Padding repairs belong outside buildings. Filling air under a roof or
  // elevated floor turns an interior/porch into a dirt-filled room.
  if(villageCoveredGround(out.templateBlocks,xz,h,out.bounds.max.y,[](auto const&b){return b.name;},maxY))continue;
  auto original=it->second;if(original.name!="minecraft:air"&&original.name!="minecraft:snow_layer")continue;
  auto& writes=out.paddingWrites[{spacingCell(xz.first,16),spacingCell(xz.second,16)}];
  for(int y=h;y>=h-8;--y){auto c=out.templateBlocks.find({xz.first,y,xz.second});if(c==out.templateBlocks.end()||(c->second.name!="minecraft:air"&&c->second.name!="minecraft:snow_layer"))break;
   StructureBlock replacement{input.biome==11?(y==h?"minecraft:sand":"minecraft:sandstone"):(y==h?"minecraft:grass_block":"minecraft:dirt"),0};c->second=replacement;writes.push_back({{xz.first,y,xz.second},replacement});
  }
  if(original.name=="minecraft:snow_layer"){auto above=out.templateBlocks.find({xz.first,h+1,xz.second});if(above==out.templateBlocks.end()||above->second.name=="minecraft:air"){out.templateBlocks[{xz.first,h+1,xz.second}]=original;writes.push_back({{xz.first,h+1,xz.second},original});}}
 }
 if(input.biome==11)for(int attempt=0;attempt<12&&out.oasis.empty();++attempt){double angle=attempt*.5235987756;int x=(b.min.x+b.max.x)/2+int(std::cos(angle)*((b.max.x-b.min.x)/2+20)),z=(b.min.z+b.max.z)/2+int(std::sin(angle)*((b.max.z-b.min.z)/2+20));
  int radius=6+int(hash(x,z,input.seed^77123)*3),lo=INT_MAX,hi=-64;bool valid=true;
  for(int dz=-radius-3;dz<=radius+3&&valid;++dz)for(int dx=-radius-3;dx<=radius+3&&valid;++dx){auto const&t=at(x+dx,z+dz);if(t.biome!=11||t.water>=t.height||t.river||t.lake||t.volcanic>.01||t.stone>.01)valid=false;lo=std::min(lo,t.height);hi=std::max(hi,t.height);
   for(auto const&p:input.pieces){auto bb=p.box;if(!bb.valid())continue;if(x+dx>=bb.min.x-3&&x+dx<=bb.max.x+3&&z+dz>=bb.min.z-3&&z+dz<=bb.max.z+3){valid=false;break;}}
  }
  if(!valid||hi-lo>2)continue;
  for(int dz=-radius-2;dz<=radius+2;++dz)for(int dx=-radius-2;dx<=radius+2;++dx){double a=std::atan2(double(dz),double(dx)),q=std::hypot(dx,dz)/(radius*(1+.12*std::sin(a*3+angle)));if(q>1.25)continue;
   out.oasis[{x+dx,z+dz}]={q<1?lo-(q<.5?2:1):height(x+dx,z+dz),q<1?lo:-32768,q>=1};
  }
 }
 // Index actual piece intersections once. Ordinary chunk execution performs
 // one map lookup, without reparsing templates or scanning every village piece.
 for(int pass=0;pass<2;++pass)for(size_t i=0;i<input.pieces.size();++i){auto const&p=input.pieces[i];if(out.blockedFeatures[i]||p.rigid!=bool(pass)||(p.kind==StructurePieceKind::Feature&&p.location.find("tree")==std::string_view::npos))continue;
  auto b=p.box.moved(out.verticalMoves[i]);if(!b.valid())continue;
  for(int cz=spacingCell(b.min.z,16);cz<=spacingCell(b.max.z,16);++cz)for(int cx=spacingCell(b.min.x,16);cx<=spacingCell(b.max.x,16);++cx)out.placements[{cx,cz}].push_back({i,placementSeed(cx,cz,VillageSalt+int(i),input.seed)});
 }
 for(size_t i=0;i<input.pieces.size();++i){auto const&p=input.pieces[i];if(p.kind!=StructurePieceKind::Template)continue;bool witnessed=false;auto box=p.box.moved(out.verticalMoves[i]);
  // Native getMarkers has one value per transformed position. Sorting gives
  // both architectures the same finite spawn order rather than STL hash order.
  std::map<std::array<int,3>,std::string_view> markers;
  for(auto const&c:p.cells){auto q=villageWorldPoint(p,c.local,out.verticalMoves[i]);auto n=c.block.name;
   if(n.ends_with("_wall"))out.wallPositions.push_back(q);
   if(!witnessed&&(n.find("_door")!=std::string_view::npos||n=="minecraft:bed")){out.witnesses.push_back({q,c.block});witnessed=true;}
   if(c.marker.empty()||(box.valid()&&(q.x<box.min.x||q.x>box.max.x||q.y<box.min.y||q.y>box.max.y||q.z<box.min.z||q.z>box.max.z)))continue;
   markers[{q.x,q.y,q.z}]=c.marker;
  }
  for(auto const&[xyz,label]:markers)if(label.starts_with("minecraft:")){StructurePoint q{xyz[0],xyz[1],xyz[2]};out.population[{spacingCell(q.x,16),spacingCell(q.z,16)}].push_back({q,std::string(label)});}
 }
 out.accepted=true;return out;
}
inline std::span<StructureEntityMarker const> villagePopulation(VillageLayoutPlan const&plan,ChunkPosition owner){auto it=plan.population.find({owner.x,owner.z});return it==plan.population.end()?std::span<StructureEntityMarker const>{}:std::span<StructureEntityMarker const>(it->second);}
inline std::span<StructurePiecePlacement const> villagePlacementOrder(VillageBlueprint const&,VillageLayoutPlan const&plan,ChunkPosition cp){
 auto it=plan.placements.find({cp.x,cp.z});return it==plan.placements.end()?std::span<StructurePiecePlacement const>{}:std::span<StructurePiecePlacement const>(it->second);
}
struct VillageDecorPlacement {size_t index;StructurePoint position;uint32_t seed;};
template<class Ground> std::vector<VillageDecorPlacement> villageDecorOrder(VillageBlueprint const&blueprint,VillageLayoutPlan const&plan,Ground ground){
 std::vector<VillageDecorPlacement> out;
 for(size_t i=0;i<blueprint.pieces.size();++i){auto const&p=blueprint.pieces[i];if(p.kind!=StructurePieceKind::Feature||p.location.find("tree")!=std::string_view::npos||plan.blockedFeatures[i])continue;
  auto pos=p.origin;pos.y=ground(pos.x,pos.z)+1;out.push_back({i,pos,placementSeed(blueprint.anchor.x,blueprint.anchor.z,10387380+int(i),blueprint.seed)});
 }return out;
}
template<class Get,class Set> void applyVillageRepairs(VillageLayoutPlan const&plan,ChunkPosition cp,Get get,Set set){
 if(auto it=plan.paddingWrites.find(cp);it!=plan.paddingWrites.end())for(auto const&w:it->second)set(w.position,w.block);
 // Owner-chunk operations only; never query or generate neighbouring chunks.
 if(auto it=plan.roadSurfaceWrites.find({cp.x,cp.z});it!=plan.roadSurfaceWrites.end())for(auto const&w:it->second){
  auto p=w.position;auto n=get(p);
  auto replaceable=[](std::string_view n){return n=="minecraft:air"||n=="minecraft:grass_path"||n=="minecraft:grass_block"||n=="minecraft:dirt"||n=="minecraft:podzol"||n=="minecraft:sand"||n=="minecraft:snow_layer";};
  if(!replaceable(n))continue;
  // Fill only a bounded gap down to soil, without touching underground rooms.
  for(int dy=1;dy<=2;++dy){auto below=p;below.y-=dy;if(get(below)!="minecraft:air")break;set(below,{"minecraft:dirt",0});}
  set(p,w.block);
  auto above=p;++above.y;if(get(above)=="minecraft:grass_path")set(above,{"minecraft:air",0});
 }

}
template<class Set> void writeStructureGroundColumn(int x,int z,int natural,int target,int biome,Set set,int maxY=319){
 if(target==natural)return;
 if(natural< -64||natural>maxY||target< -58||target>maxY)throw std::runtime_error("Invalid fitted structure ground column");
 auto fill=biome==11?"minecraft:sandstone":"minecraft:dirt",top=biome==11?"minecraft:sand":"minecraft:grass_block";
 if(target>natural)for(int y=std::max(-58,natural);y<target;++y)set(StructurePoint{x,y,z},StructureBlock{fill,0});
 else for(int y=target+1;y<=std::min(maxY,natural+2);++y)set(StructurePoint{x,y,z},StructureBlock{"minecraft:air",0});
 set(StructurePoint{x,target,z},StructureBlock{top,0});
}
template<class Set> void writeVillageOasisColumn(VillageOasisColumn c,int x,int z,int natural,Set set,int maxY=319){
 for(int y=c.floor-1;y<=std::min(maxY,natural+2);++y){std::string_view n=y<c.floor?"minecraft:dirt":y==c.floor?(c.green?"minecraft:grass_block":"minecraft:sand"):y<=c.water?"minecraft:water":"minecraft:air";set(StructurePoint{x,y,z},StructureBlock{n,0});}
}
inline int extraPlacementSeedSalt(int type,size_t piece){return extraSpec(type)->salt+int(piece);}
inline uint32_t extraPieceSeed(int type,size_t piece,ChunkPosition anchor,ChunkPosition owner,uint32_t seed){auto cp=(type==1||type==2||type==4||type==5||type==9)?anchor:owner;return placementSeed(cp.x,cp.z,extraPlacementSeedSalt(type,piece),seed);}
struct StructurePad {StructureBox box;int height;};
template<class Pads> int structureGround(std::map<VillageXZ,int> const&fitted,Pads const&pads,int x,int z,int natural){
 if(auto it=fitted.find({x,z});it!=fitted.end())return it->second;
 double weighted=0,weights=0,strongest=0;
 for(auto const&p:pads){auto b=p.box;int dx=std::max({b.min.x-x,0,x-b.max.x}),dz=std::max({b.min.z-z,0,z-b.max.z});double distance=std::hypot(dx,dz);if(distance>=6)continue;if(!dx&&!dz)return p.height;double w=1-distance/6;w=w*w*(3-2*w);weighted+=w*p.height;weights+=w;strongest=std::max(strongest,w);}
 return weights>0?int(std::round(natural+(weighted/weights-natural)*strongest)):natural;
}
struct ExtraBlueprint {int type;ChunkPosition anchor;StructureBox bounds;std::vector<StructureBlueprintPiece> pieces;};
struct ExtraLayoutPlan {
 bool accepted=false;std::string_view rejection;StructureBox bounds;
 int floor=-32768;std::vector<int> verticalMoves;std::vector<StructureBox> pieceBounds;
 std::vector<StructurePad> pads;std::map<VillageXZ,int> fittedGround;
 size_t surfaceQueries{};
};
// All families use the upstream's frozen floor and eligibility pipeline. No
// platform is allowed to invent a second per-piece or live-heightmap filter.
template<class Sample> ExtraLayoutPlan planExtraLayout(ExtraBlueprint const&input,Sample sample,int maxY=319){
 ExtraLayoutPlan out;int type=input.type;out.bounds=input.bounds;
 auto spec=extraSpec(type);if(!spec||input.pieces.empty()||input.pieces.size()>4096||!input.bounds.valid()){out.rejection="invalid-blueprint";return out;}
 using Surface=decltype(sample(0,0));std::map<VillageXZ,Surface> cache;
 auto at=[&](int x,int z)->Surface const&{auto key=VillageXZ{x,z};auto it=cache.find(key);if(it==cache.end()){++out.surfaceQueries;it=cache.emplace(key,sample(x,z)).first;}return it->second;};
 int wx=input.anchor.x*16,wz=input.anchor.z*16;auto center=at(wx,wz);
 if(!extraAnchorAllowed(type,center,MansionHabitatAvailable)){out.rejection="extra-site";return out;}
 for(auto const&p:input.pieces){out.verticalMoves.push_back(0);out.pieceBounds.push_back(p.box);}
 auto scan=[&](StructureBox const&b,int step,int mode,int&lo,int&hi){
  if(!b.valid()||int64_t(b.max.x)-b.min.x>spec->radius*32||int64_t(b.max.z)-b.min.z>spec->radius*32)return false;
  for(int z=b.min.z;z<=b.max.z;z+=step)for(int x=b.min.x;x<=b.max.x;x+=step){auto const&s=at(x,z);bool allowed;
   if(mode==0)allowed=s.water<s.height&&!s.river&&!s.lake&&s.volcanic<=.01&&s.iceHabitat<=.01&&villageStyle(s.biome)>=0;
   else if(mode==3)allowed=s.ocean>=.5&&s.water>=surfaceSeaLevel(s)-1&&s.height<=surfaceSeaLevel(s)-6&&s.height>=-40&&!s.lake&&!s.river&&s.iceHabitat<=.01;
   else allowed=extraFootprintAllowed(type,s);
   if(!allowed)return false;lo=std::min(lo,s.height);hi=std::max(hi,s.height);
   if(hi-lo>(mode==0?4:mode==3?3:extraMaxSlope(type)))return false;
  }return true;
 };
 if(type==1||type==2||type==4||type==5||type==7||type==10){int lo=INT_MAX,hi=-64;if(!scan(out.bounds,3,-1,lo,hi)||(type==5&&hi>surfaceSeaLevel(center)-25)){out.rejection="extra-footprint";return out;}
  out.floor=(lo+hi)/2+1;
  if(type==1){out.bounds.min.y=out.floor-4;out.bounds.max.y=out.floor+35;}
  else if(type==4){out.bounds.min.y=lo-4;out.bounds.max.y=hi;for(auto&b:out.pieceBounds)b=out.bounds;}
  else if(type==2||type==5||type==7){int dy=(type==5?hi+1:out.floor)-out.bounds.min.y;out.bounds=out.bounds.moved(dy);for(size_t i=0;i<input.pieces.size();++i){out.verticalMoves[i]=dy;out.pieceBounds[i]=out.pieceBounds[i].moved(dy);}}
  if(type==7)for(size_t i=0;i<input.pieces.size();++i)for(auto const&c:input.pieces[i].cells){auto n=c.block.name;if(n=="minecraft:air"||n=="minecraft:structure_void"||n=="minecraft:structure_block")continue;auto q=villageWorldPoint(input.pieces[i],c.local,out.verticalMoves[i]);if(q.y<out.floor-1||q.y>out.floor+1)continue;mergeMinimum(out.fittedGround,{q.x,q.z},q.y-1);}
 }
 if(type==0||type==3){out.bounds={};for(size_t i=0;i<input.pieces.size();++i){auto const&p=input.pieces[i];int lo=INT_MAX,hi=-64;
   if(!scan(p.box,1,type,lo,hi)){out.rejection="piece-footprint";return out;}
   int desired=type==0?(lo+hi)/2+1:lo,dy=desired-p.origin.y;out.verticalMoves[i]=dy;out.pieceBounds[i]=p.box.moved(dy);out.bounds.include(out.pieceBounds[i]);
   if(type==0)out.pads.push_back({out.pieceBounds[i],desired-1});
  }
 }
 auto b=out.bounds;bool underground=extraUnderground(type);
 if(!b.valid()||b.min.y< -58||b.max.y>maxY-9||(underground&&b.max.y>center.height-10)||b.min.x<wx-spec->radius*16||b.max.x>wx+spec->radius*16||b.min.z<wz-spec->radius*16||b.max.z>wz+spec->radius*16){out.rejection="extra-bounds";return out;}
 for(int z=b.min.z;z<=b.max.z;z+=16)for(int x=b.min.x;x<=b.max.x;x+=16){auto const&t=at(x,z);if((underground&&t.height<b.max.y+8)||(!extraMarine(type)&&type!=4&&t.ocean>.1)||t.volcanic>.01){out.rejection="extra-cover";return out;}}
 out.accepted=true;return out;
}
struct ScatteredLayoutPlan {bool accepted=false;int floor{},low=INT_MAX,high=-64;};
template<class Sample> ScatteredLayoutPlan planScatteredLayout(StructureBox box,int biome,Sample sample){
 ScatteredLayoutPlan out;int64_t sum=0,count=0;if(!box.valid()||int64_t(box.max.x)-box.min.x>128||int64_t(box.max.z)-box.min.z>128)return out;
 for(int z=box.min.z;z<=box.max.z;++z)for(int x=box.min.x;x<=box.max.x;++x){auto s=sample(x,z);if(!scatteredFootprintAllowed(s,biome))return out;out.low=std::min(out.low,s.height);out.high=std::max(out.high,s.height);sum+=s.height+1;++count;if(out.high-out.low>scatteredMaxSlope(biome))return out;}
 out.accepted=count>0;if(count)out.floor=int(sum/count);return out;
}
}
