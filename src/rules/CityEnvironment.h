#pragma once
#include "CityPlan.h"
#include <optional>
#include <type_traits>

namespace worldgen_rules {
// LLBDS city environment, in its original execution order. Environment is a
// native API service (immutable state handles, solidity, survival and writes).
// No native object layouts, registries or game-owned pointers live here.
inline constexpr std::array<StructurePoint,6> CityFaces{{
 {0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}
}};
inline bool cityEcologyWriteAllowed(StructureBox clip,StructurePoint p,bool bedrock){return p.x>=clip.min.x&&p.x<=clip.max.x&&p.z>=clip.min.z&&p.z<=clip.max.z&&p.y>=-58&&p.y<=clip.max.y&&!bedrock;}
template<class Get,class Predicate,class Emit>void fetchCityEcologyBlocks(StructureBox box,Get get,Predicate predicate,Emit emit){
 int64_t dx=int64_t(box.max.x)-box.min.x+1,dy=int64_t(box.max.y)-box.min.y+1,dz=int64_t(box.max.z)-box.min.z+1;
 if(dx<=0||dy<=0||dz<=0||dx>262144||dy>262144||dz>262144||dx*dy>262144||dx*dy*dz>262144)throw std::runtime_error("City ecology query exceeds bounded patch");
 StructurePoint c{int((int64_t(box.min.x)+box.max.x)/2),int((int64_t(box.min.y)+box.max.y)/2),int((int64_t(box.min.z)+box.max.z)/2)};
 for(int64_t z=box.min.z;z<=box.max.z;++z)for(int64_t x=box.min.x;x<=box.max.x;++x)for(int64_t y=box.min.y;y<=box.max.y;++y){StructurePoint p{int(x),int(y),int(z)};auto block=get(p);if(predicate(block)){int64_t xx=x-c.x,yy=y-c.y,zz=z-c.z;emit(block,p,uint32_t(xx*xx+yy*yy+zz*zz));}}
}
inline bool cityAttachment(std::string_view n){
 return n=="minecraft:moss_carpet"||n=="minecraft:red_mushroom"||n=="minecraft:brown_mushroom"||n=="minecraft:spore_blossom"||n=="minecraft:azalea"||n=="minecraft:flowering_azalea"||n=="minecraft:fern"||n=="minecraft:large_fern"||n=="minecraft:short_grass"||n=="minecraft:tall_grass"||n=="minecraft:pointed_dripstone"||n.starts_with("minecraft:big_dripleaf")||n.starts_with("minecraft:small_dripleaf");
}
struct CityNeighbourCandidate {int type;ChunkPosition anchor;};
inline std::vector<CityNeighbourCandidate> cityNeighbourCandidates(StructureBox city,uint32_t seed){
 std::vector<CityNeighbourCandidate>out;
 // An exposed mineshaft keeps native bridge supports. Reserving its entire
 // spread as a hanging slab would close the city's surveyed cave exits.
 for(int type:{6,11}){auto const&s=*extraSpec(type);int reach=128+s.radius*16;
  for(int z=spacingCell(spacingCell(city.min.z-reach,16),s.spacing);z<=spacingCell(spacingCell(city.max.z+reach,16),s.spacing);++z)
   for(int x=spacingCell(spacingCell(city.min.x-reach,16),s.spacing);x<=spacingCell(spacingCell(city.max.x+reach,16),s.spacing);++x)out.push_back({type,candidateForCell(x,z,seed,s.spacing,s.separation,s.salt)});
 }return out;
}
inline bool cityNeighbourIncluded(StructureBox city,StructureBox neighbour){return neighbour.intersectsXZ(city,128);}
struct CityEcologyNode {size_t piece;StructurePoint position;std::string_view feature;uint32_t seed;};
inline std::vector<CityEcologyNode> cityEcologyNodes(std::span<StructureBlueprintPiece const>pieces,ChunkPosition anchor,StructureBox owner,uint32_t seed){
 std::vector<CityEcologyNode>out;
 for(size_t i=0;i<pieces.size();++i){auto const&p=pieces[i];if(p.kind!=StructurePieceKind::Feature)continue;auto q=p.origin;
  if(q.x<owner.min.x||q.x>owner.max.x||q.z<owner.min.z||q.z>owner.max.z)continue;
  out.push_back({i,q,p.location,placementSeed(anchor.x,anchor.z,extraSpec(8)->salt+int(i),seed)});
 }return out;
}
template<class Air,class Solid> std::optional<StructurePoint> cityEcologySupport(StructurePoint p,Air air,Solid solid){
 for(int dy:{0,-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,1,2}){
  StructurePoint q{p.x,p.y+dy,p.z};if(air(q)&&solid(StructurePoint{q.x,q.y-1,q.z}))return q;
 }return {};
}
template<class Environment>
unsigned carveCityEnvironment(std::span<StructureBlueprintPiece const>pieces,std::vector<CityLink>const&allLinks,std::vector<StructureBox>const&neighbours,StructureBox clip,uint32_t seed,Environment&env){
 auto shapes=cityShapes(pieces,clip);std::vector<CityLink>links;
 for(auto const&l:allLinks)if(std::max(l.a.x,l.b.x)+4>=clip.min.x&&std::min(l.a.x,l.b.x)-4<=clip.max.x&&std::max(l.a.z,l.b.z)+4>=clip.min.z&&std::min(l.a.z,l.b.z)-4<=clip.max.z)links.push_back(l);
 if(shapes.empty()&&links.empty())return 0;unsigned carved=0;
 for(int z=clip.min.z;z<=clip.max.z;++z)for(int x=clip.min.x;x<=clip.max.x;++x){auto c=cityColumn(shapes,links,neighbours,x,z,seed);
  for(int y=c.floor+1;y<=std::min(c.ceiling,clip.max.y-9);++y){StructurePoint p{x,y,z};if(env.natural(env.get(p))){env.air(p);++carved;}}
 }return carved;
}
template<class Sample,class Material,class Environment>
void finishCityEnvironment(StructureBox city,std::span<StructureBlueprintPiece const>pieces,std::vector<CityLink>const&links,std::vector<StructureBox>const&neighbours,StructureBox clip,uint32_t seed,Sample sample,Material material,Environment&env){
 auto halo=clip;--halo.min.x;--halo.min.z;++halo.max.x;++halo.max.z;
 auto shapes=cityShapes(pieces,halo);
 using Surface=std::decay_t<decltype(sample(0,0))>;
 std::map<std::pair<int,int>,std::pair<CityColumn,Surface>>edge;
 auto support=[&](StructurePoint q){
  if(q.x>=clip.min.x&&q.x<=clip.max.x&&q.z>=clip.min.z&&q.z<=clip.max.z)return env.solid(env.get(q));
  auto key=std::pair{q.x,q.z};auto it=edge.find(key);if(it==edge.end())it=edge.emplace(key,std::pair{cityColumn(shapes,links,neighbours,q.x,q.z,seed),sample(q.x,q.z)}).first;
  auto const&[c,surface]=it->second;if(q.y>c.floor&&q.y<=c.ceiling)return false;
  int m=material(surface,q.x,q.y,q.z);return m!=0&&m!=8&&m!=43;
 };
 for(int z=clip.min.z;z<=clip.max.z;++z)for(int x=clip.min.x;x<=clip.max.x;++x){
  double dx=std::max({city.min.x-x,0,x-city.max.x}),dz=std::max({city.min.z-z,0,z-city.max.z});double fade=1-std::clamp(std::hypot(dx,dz)/36.,0.,1.);
  double cover=cityNoise(x,z,seed^982731,23)*.72+cityNoise(x,z,seed^982737,8)*.28;double strength=fade*std::clamp((cover-.25)/.40,0.,1.);
  for(int y=std::max(-57,city.min.y-3);y<=std::min(110,city.max.y+48);++y){StructurePoint p{x,y,z};auto b=env.get(p);auto n=env.name(b);
   if(cityAttachment(n)&&!env.survives(b,p)){env.air(p);continue;}
   if(n.starts_with("minecraft:cave_vines")){int top=y;while(top<y+16&&env.name(env.get({x,top+1,z})).starts_with("minecraft:cave_vines"))++top;if(!env.solid(env.get({x,top+1,z}))){env.air(p);continue;}}
   if(n=="minecraft:sculk"||n=="minecraft:moss_block"){
    bool anchored=false;for(auto v:CityFaces)if(support({x+v.x,y+v.y,z+v.z})){anchored=true;break;}
    if(!anchored){env.air(p);continue;}
   }
   if(n=="minecraft:sculk_vein"){
    int bits=env.veinBits(b),valid=0;
    for(size_t i=0;i<CityFaces.size();++i){auto v=CityFaces[i];int mask=env.faceMask(i);if((bits&mask)&&support({x+v.x,y+v.y,z+v.z}))valid|=mask;}
    if(valid!=bits){if(valid)env.vein(p,b,valid);else env.air(p);}continue;
   }
   if(strength<=0||!cityRock(n))continue;
   double pick=nativegen::hash01(x,z,seed^982733^uint32_t(y*137));if(pick>strength)continue;
   bool exposed=false;for(auto v:CityFaces){StructurePoint q{x+v.x,y+v.y,z+v.z};if(q.x<clip.min.x||q.x>clip.max.x||q.z<clip.min.z||q.z>clip.max.z)continue;if(env.isAir(env.get(q))){exposed=true;break;}}
   if(exposed){
    bool anchored=false;for(auto v:CityFaces)if(support({x+v.x,y+v.y,z+v.z})){anchored=true;break;}
    if(!anchored){env.air(p);continue;}
    env.sculk(p);
    StructurePoint above{x,y+1,z};if(env.isAir(env.get(above))&&nativegen::hash01(x,z,seed^982739^uint32_t((y+1)*173))<.14)env.fringe(above,env.faceMask(0));
   }
  }
 }
}
}
