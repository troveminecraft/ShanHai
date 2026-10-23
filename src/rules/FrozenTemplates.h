#pragma once
#include "StructurePlan.h"
#include "StructureLifecycle.h"
namespace worldgen_rules {
enum class IglooTemplateRole {TopWithTrapdoor,TopWithoutTrapdoor,Ladder,Laboratory};
inline std::string_view iglooTemplateName(IglooTemplateRole role){switch(role){case IglooTemplateRole::TopWithTrapdoor:return "igloo/igloo_top_trapdoor";case IglooTemplateRole::TopWithoutTrapdoor:return "igloo/igloo_top_no_trapdoor";case IglooTemplateRole::Ladder:return "igloo/igloo_middle";case IglooTemplateRole::Laboratory:return "igloo/igloo_bottom";}throw std::runtime_error("Unknown igloo template role");}
struct FrozenTemplatePart {IglooTemplateRole role;StructurePoint origin;StructureBox bounds;};
struct FrozenIglooPlan {int rotation{},ladders{};uint32_t seed{};StructureBox bounds;std::vector<FrozenTemplatePart>parts;};
inline StructureBox rotatedTemplateBounds(StructurePoint size,StructurePoint origin,int rotation){
 if(size.x<=0||size.y<=0||size.z<=0)throw std::runtime_error("Invalid native template size");
 StructureBox out;
 for(int z:{0,size.z-1})for(int y:{0,size.y-1})for(int x:{0,size.x-1}){auto[rx,rz]=rotateVillageXZ(x,z,rotation);StructurePoint p{origin.x+rx,origin.y+y,origin.z+rz};out.include({p,p});}return out;
}
// The RNG service must implement the pinned game's nextInt/nextDouble calls;
// the layout, call order and selection rules remain in this common function.
template<class Random,class Size> FrozenIglooPlan planFrozenIgloo(ChunkPosition cp,int floor,Random&rng,Size size){
 FrozenIglooPlan out;out.rotation=rng.nextInt(4);bool basement=rng.nextDouble()>=.5;
 out.ladders=basement?rng.nextInt(8)+4:0;out.seed=uint32_t(rng.nextInt());StructurePoint origin{cp.x*16,floor,cp.z*16};
 auto add=[&](IglooTemplateRole role,StructurePoint pos){auto bounds=rotatedTemplateBounds(size(role),pos,out.rotation);out.parts.push_back({role,pos,bounds});out.bounds.include(bounds);};
 add(basement?IglooTemplateRole::TopWithTrapdoor:IglooTemplateRole::TopWithoutTrapdoor,origin);
 auto connected=[&](StructurePoint a,StructurePoint b){auto[ax,az]=rotateVillageXZ(a.x,a.z,out.rotation);auto[bx,bz]=rotateVillageXZ(b.x,b.z,out.rotation);return StructurePoint{origin.x+ax-bx,origin.y+a.y-b.y,origin.z+az-bz};};
 if(basement){for(int i=0;i<out.ladders;++i)add(IglooTemplateRole::Ladder,connected({3,-1-i*3,5},{1,2,1}));add(IglooTemplateRole::Laboratory,connected({3,-1-out.ladders*3,5},{3,5,7}));}return out;
}
struct FrozenShipSelection {int variant,rotation;};
template<class Random> FrozenShipSelection chooseFrozenShip(Random&rng){return {rng.nextInt(20),rng.nextInt(4)};}
// Preserve the LLBDS native catalog's twenty variants and ordering, including
// degraded halves. The Android native static initializer is independently
// replayed against this table; adapters must not substitute another catalog.
inline constexpr std::array<std::string_view,20>FrozenShipTemplates{{
 "shipwreck/swwithmast","shipwreck/swwithmastdegraded",
 "shipwreck/swsidewaysfull","shipwreck/swsidewaysfronthalf","shipwreck/swsidewaysbackhalf",
 "shipwreck/swsidewaysfulldegraded","shipwreck/swsidewaysfronthalfdegraded","shipwreck/swsidewaysbackhalfdegraded",
 "shipwreck/swrightsideupfull","shipwreck/swrightsideupfronthalf","shipwreck/swrightsideupbackhalf",
 "shipwreck/swrightsideupfulldegraded","shipwreck/swrightsideupfronthalfdegraded","shipwreck/swrightsideupbackhalfdegraded",
 "shipwreck/swupsidedownfull","shipwreck/swupsidedownfronthalf","shipwreck/swupsidedownbackhalf",
 "shipwreck/swupsidedownfulldegraded","shipwreck/swupsidedownfronthalfdegraded","shipwreck/swupsidedownbackhalfdegraded"
}};
inline std::string_view shipMarkerLoot(std::string_view marker){
 std::string normalized(marker);for(auto&c:normalized)if(c>='A'&&c<='Z')c=char(c-'A'+'a');marker=normalized;
 if(marker.find("map")!=std::string_view::npos)return "loot_tables/chests/shipwreck_map.json";
 if(marker.find("supply")!=std::string_view::npos)return "loot_tables/chests/shipwreck_supply.json";
 if(marker.find("treasure")!=std::string_view::npos)return "loot_tables/chests/shipwreck_treasure.json";
 return {};
}
inline std::optional<StructureLootAssignment> iglooMarkerLoot(std::string_view marker,StructurePoint p,uint32_t seed){
 if(marker!="chest")return {};--p.y;return StructureLootAssignment{p,"loot_tables/chests/igloo_chest.json",seed};
}
inline uint32_t structureMarkerLootSeed(int type,StructurePoint p,uint32_t seed){
 auto spec=extraSpec(type);if(!spec)throw std::runtime_error("Unknown loot structure family");return placementSeed(p.x,p.z,spec->salt,seed);
}
struct LegacyDataMarker {StructurePoint local;std::string label;};
struct FrozenRuinPart {std::string templateName;StructurePoint origin;StructureBox bounds;int rotation{};std::vector<LegacyDataMarker>markers;};
struct FrozenRuinMarker {size_t part;StructurePoint position;std::string label;};
struct FrozenRuinProgram {
 ChunkPosition anchor;uint32_t worldSeed{};std::vector<FrozenRuinPart>parts;
 std::map<VillageXZ,std::vector<size_t>>owners;
 std::map<VillageXZ,std::vector<FrozenRuinMarker>>chests;
 StructureBox bounds;
};
inline FrozenRuinProgram compileFrozenRuins(ChunkPosition anchor,uint32_t seed,std::vector<FrozenRuinPart>parts){
 if(parts.empty()||parts.size()>4096)throw std::runtime_error("Invalid frozen ruin piece count");
 FrozenRuinProgram out{anchor,seed,std::move(parts),{}, {}};std::set<std::array<int,3>>chests;
 for(size_t i=0;i<out.parts.size();++i){auto const&p=out.parts[i];if(!p.bounds.valid()||p.rotation<0||p.rotation>3||int64_t(p.bounds.max.x)-p.bounds.min.x>256||int64_t(p.bounds.max.z)-p.bounds.min.z>256)throw std::runtime_error("Invalid frozen ruin descriptor");
  for(int z=spacingCell(p.bounds.min.z,16);z<=spacingCell(p.bounds.max.z,16);++z)for(int x=spacingCell(p.bounds.min.x,16);x<=spacingCell(p.bounds.max.x,16);++x)out.owners[{x,z}].push_back(i);
  out.bounds.include(p.bounds);
  for(auto const&m:p.markers){if(m.label!="chest")continue;auto[x,z]=rotateVillageXZ(m.local.x,m.local.z,p.rotation);StructurePoint q{p.origin.x+x,p.origin.y+m.local.y,p.origin.z+z};
   if(q.x<p.bounds.min.x||q.x>p.bounds.max.x||q.y<p.bounds.min.y||q.y>p.bounds.max.y||q.z<p.bounds.min.z||q.z>p.bounds.max.z||!chests.insert({q.x,q.y,q.z}).second)continue;
   out.chests[{spacingCell(q.x,16),spacingCell(q.z,16)}].push_back({i,q,m.label});
  }
 }return out;
}
template<class Place,class ExistingChest,class Marker> bool executeFrozenRuins(FrozenRuinProgram const&program,StructureBox const&clip,Place place,ExistingChest existing,Marker marker){
 if(!program.bounds.intersectsXZ(clip))return false;bool any=false;
 int minX=std::max(spacingCell(clip.min.x,16),spacingCell(program.bounds.min.x,16)),maxX=std::min(spacingCell(clip.max.x,16),spacingCell(program.bounds.max.x,16));
 int minZ=std::max(spacingCell(clip.min.z,16),spacingCell(program.bounds.min.z,16)),maxZ=std::min(spacingCell(clip.max.z,16),spacingCell(program.bounds.max.z,16));
 for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x){auto found=program.owners.find({x,z});if(found==program.owners.end())continue;
  StructureBox owner{{std::max(x*16,clip.min.x),clip.min.y,std::max(z*16,clip.min.z)},{std::min(x*16+15,clip.max.x),clip.max.y,std::min(z*16+15,clip.max.z)}};
  for(auto i:found->second){auto const&p=program.parts[i];if(!p.bounds.intersectsXZ(owner))continue;place(i,p,extraPieceSeed(3,i,program.anchor,{x,z},program.worldSeed),owner);any=true;}
  // Cold brick/cracked/mossy layers may cover an earlier chest. Finalize only
  // after ALL this owner's layers, retaining existing containers and using the
  // upstream's first marker for duplicate positions. No set allocation here.
  if(auto it=program.chests.find({x,z});it!=program.chests.end())for(auto const&m:it->second)if(inStructureOwner(m.position,owner)&&!existing(m.position))marker(m,structureMarkerLootSeed(3,m.position,program.worldSeed),owner);
 }return any;
}
struct FrozenShipPlan {
 ChunkPosition anchor;uint32_t worldSeed{},settingsSeed{};FrozenShipSelection selection;
 StructurePoint origin;StructureBox bounds;
};
struct FrozenShipProgram {FrozenShipPlan geometry;std::vector<StructureEntityMarker>markers;};
template<class Random,class Size> FrozenShipPlan planFrozenShip(ChunkPosition cp,uint32_t worldSeed,Random&rng,Size size){
 auto selected=chooseFrozenShip(rng);if(selected.variant<0||selected.variant>=int(FrozenShipTemplates.size())||selected.rotation<0||selected.rotation>3)throw std::runtime_error("Invalid frozen ship selection");
 FrozenShipPlan out{cp,worldSeed,uint32_t(rng.nextInt()),selected,{cp.x*16,0,cp.z*16},{}};
 out.bounds=rotatedTemplateBounds(size(FrozenShipTemplates[selected.variant]),out.origin,selected.rotation);return out;
}
template<class Size> FrozenShipPlan planFrozenShip(ChunkPosition cp,uint32_t worldSeed,Size size){StructureRandom rng(placementSeed(cp.x,cp.z,165745298,worldSeed));return planFrozenShip(cp,worldSeed,rng,size);}
inline void moveFrozenShip(FrozenShipPlan&plan,int dy){plan.origin.y+=dy;plan.bounds=plan.bounds.moved(dy);}
template<class Sample> ExtraLayoutPlan fitFrozenShipFloor(FrozenShipPlan&plan,Sample sample,int maxY=319){
 StructureBlueprintPiece piece;piece.box=plan.bounds;piece.origin=plan.origin;
 auto fit=planExtraLayout(ExtraBlueprint{2,plan.anchor,plan.bounds,{piece}},sample,maxY);if(fit.accepted)moveFrozenShip(plan,fit.verticalMoves.at(0));return fit;
}
template<class Markers> FrozenShipProgram compileFrozenShip(FrozenShipPlan plan,Markers const&markers){
 FrozenShipProgram out{std::move(plan),{}};auto const&g=out.geometry;
 for(auto const&m:markers){auto[x,z]=rotateVillageXZ(m.local.x,m.local.z,g.selection.rotation);StructurePoint q{g.origin.x+x,g.origin.y+m.local.y,g.origin.z+z};
  if(q.x<g.bounds.min.x||q.x>g.bounds.max.x||q.y<g.bounds.min.y||q.y>g.bounds.max.y||q.z<g.bounds.min.z||q.z>g.bounds.max.z)continue;
  if(!shipMarkerLoot(m.label).empty())out.markers.push_back({q,m.label});
 }return out;
}
inline void moveFrozenShipProgram(FrozenShipProgram&program,int dy){moveFrozenShip(program.geometry,dy);for(auto&m:program.markers)m.position.y+=dy;}
template<class Place,class Name,class Clear,class CreateChest,class Loot> bool executeFrozenShip(FrozenShipProgram const&program,StructureBox const&clip,Place place,Name name,Clear clear,CreateChest create,Loot loot){
 auto const&g=program.geometry;if(!g.bounds.intersectsXZ(clip))return false;
 auto randomSeed=extraPieceSeed(2,0,g.anchor,g.anchor,g.worldSeed);place(g,randomSeed,clip);
 for(auto const&m:program.markers){if(!inStructureOwner(m.position,clip))continue;auto chest=m.position;
  if(name(StructurePoint{chest.x,chest.y-1,chest.z})=="minecraft:chest")--chest.y;
  clear(m.position);
  if(name(chest)!="minecraft:chest")create(chest,2);
  loot(StructureLootAssignment{chest,std::string(shipMarkerLoot(m.definition)),structureMarkerLootSeed(2,m.position,g.worldSeed)});
 }return true;
}
struct FrozenPartProgram {uint32_t seed;std::vector<StructureEntityMarker> markers;};
struct FrozenIglooProgram {
 FrozenIglooPlan geometry;
 std::vector<FrozenPartProgram> parts;
 std::map<VillageXZ,std::vector<size_t>> owners;
 std::map<VillageXZ,std::vector<StructureEntityMarker>> population;
 std::vector<StructurePoint> lootWitnesses;
};
inline void moveFrozenIgloo(FrozenIglooPlan&plan,int dy){plan.bounds=plan.bounds.moved(dy);for(auto&p:plan.parts){p.origin.y+=dy;p.bounds=p.bounds.moved(dy);}}
template<class Sample> ScatteredLayoutPlan fitFrozenIglooFloor(FrozenIglooPlan&plan,int biome,Sample sample,int maxY=319){
 if(plan.parts.empty()||plan.bounds.min.y< -58)return {};
 auto fit=planScatteredLayout(plan.bounds,biome,sample);if(!fit.accepted)return fit;
 int dy=fit.floor-1-plan.parts.front().origin.y;
 if(plan.bounds.min.y+dy< -58||plan.bounds.max.y+dy>maxY){fit.accepted=false;return fit;}
 moveFrozenIgloo(plan,dy);return fit;
}
template<class Markers> FrozenIglooProgram compileFrozenIgloo(FrozenIglooPlan plan,Markers markers){
 FrozenIglooProgram out;out.geometry=std::move(plan);out.parts.reserve(out.geometry.parts.size());
 for(size_t i=0;i<out.geometry.parts.size();++i){auto const&p=out.geometry.parts[i];FrozenPartProgram part{out.geometry.seed+uint32_t(i)*0x9e3779b9u,{}};
  for(auto const&m:markers(p.role)){auto[x,z]=rotateVillageXZ(m.local.x,m.local.z,out.geometry.rotation);StructurePoint q{p.origin.x+x,p.origin.y+m.local.y,p.origin.z+z};if(q.x<p.bounds.min.x||q.x>p.bounds.max.x||q.y<p.bounds.min.y||q.y>p.bounds.max.y||q.z<p.bounds.min.z||q.z>p.bounds.max.z)continue;
   part.markers.push_back({q,m.label});
   if(auto definition=iglooEntityDefinition(m.label))out.population[{spacingCell(q.x,16),spacingCell(q.z,16)}].push_back({q,definition->nameSpace+":"+definition->name+":"+definition->event});
   if(auto loot=iglooMarkerLoot(m.label,q,out.geometry.seed))out.lootWitnesses.push_back(loot->position);
  }
  out.parts.push_back(std::move(part));for(int z=spacingCell(p.bounds.min.z,16);z<=spacingCell(p.bounds.max.z,16);++z)for(int x=spacingCell(p.bounds.min.x,16);x<=spacingCell(p.bounds.max.x,16);++x)out.owners[{x,z}].push_back(i);
 }
 if(out.geometry.ladders){unsigned villagers=0,zombies=0;for(auto const&p:out.parts)for(auto const&m:p.markers){villagers+=m.definition=="Villager";zombies+=m.definition=="Zombie Villager";}if(villagers!=1||zombies!=1||out.lootWitnesses.size()!=1)throw std::runtime_error("Native igloo laboratory markers incomplete");}
 return out;
}
template<class Place,class Clear,class Loot> bool executeFrozenIgloo(FrozenIglooProgram const&program,StructureBox const&clip,Place place,Clear clear,Loot loot){
 // The native root callback can cover several owners. Part selection and order
 // are fixed before placement; each owning fragment gets the same part seed.
 if(program.parts.size()>16)throw std::runtime_error("Igloo part budget exceeded");std::array<bool,16> indices{};bool any=false;
 for(int z=spacingCell(clip.min.z,16);z<=spacingCell(clip.max.z,16);++z)for(int x=spacingCell(clip.min.x,16);x<=spacingCell(clip.max.x,16);++x)if(auto it=program.owners.find({x,z});it!=program.owners.end())for(auto i:it->second){indices[i]=true;any=true;}
 for(size_t i=0;i<program.parts.size();++i){if(!indices[i])continue;auto const&p=program.geometry.parts[i];auto const&op=program.parts[i];place(i,p,program.geometry.rotation,program.geometry.seed,op.seed,clip);
  for(auto const&m:op.markers){if(!inStructureOwner(m.position,clip))continue;clear(m.position);if(auto assignment=iglooMarkerLoot(m.definition,m.position,program.geometry.seed))loot(*assignment);}
 }return any;
}
template<class Loaded,class Name> std::optional<std::vector<StructureEntityMarker>> prepareIglooPopulation(FrozenIglooProgram const&program,ChunkPosition owner,Loaded loaded,Name name){
 if(!program.geometry.ladders)return {};auto found=program.population.find({owner.x,owner.z});if(found==program.population.end()||found->second.empty())return {};
 auto const&lab=program.geometry.parts.back().bounds;
 for(int z=spacingCell(lab.min.z,16);z<=spacingCell(lab.max.z,16);++z)for(int x=spacingCell(lab.min.x,16);x<=spacingCell(lab.max.x,16);++x)if(!loaded(ChunkPosition{x,z}))return {};
 for(auto const&p:program.lootWitnesses)if(name(p)!="minecraft:chest")return {};
 for(auto const&[key,markers]:program.population)for(auto const&m:markers){auto p=m.position;if(name(p)!="minecraft:air"||name(StructurePoint{p.x,p.y-1,p.z})=="minecraft:air")return {};}
 return found->second;
}
}
