#pragma once
#include "StructurePlan.h"
#include "VillageGrowth.h"
#include <numeric>

namespace worldgen_rules {
struct VillageLot {size_t prototype;StructurePoint delta;StructureBox box;bool populate=true;};
struct OrderedVillage {
 bool accepted=false;int baseline=0,target=0;std::string_view rejection;
 std::vector<VillageLot> lots;std::map<VillageXZ,int> roads;
 size_t surfaceQueries=0;
};
// Compact rows of lots, with three-block streets around every row and house.
// Large towns use a bounded 256-block footprint. Native chunk dependency
// requests remain unchanged; plans and placements are clipped to owner chunks.
template<class Sample> OrderedVillage orderVillage(VillageBlueprint const&source,Sample sample){
 OrderedVillage out;std::vector<size_t> prototypes;
 for(size_t i=0;i<source.pieces.size();++i){auto const&p=source.pieces[i];
  if(p.kind!=StructurePieceKind::Template||!p.rigid||!p.box.valid())continue;
  bool house=villageBuilding(p.location);out.baseline+=house;
  // Preserve town centres, wells, farms and animal pens once; replace the
  // old winding road templates with the connected street plan below.
  for(int copy=0;copy<(house?6:1);++copy)prototypes.push_back(i);
 }
 out.target=out.baseline*6;if(!out.baseline||prototypes.size()>768){out.rejection="building-budget";return out;}
 std::stable_sort(prototypes.begin(),prototypes.end(),[&](size_t a,size_t b){auto const&A=source.pieces[a].box,&B=source.pieces[b].box;
  return std::tuple{A.max.z-A.min.z,A.max.x-A.min.x}>std::tuple{B.max.z-B.min.z,B.max.x-B.min.x};});
 struct Row {int z=0,depth=0,width=0;std::vector<std::pair<size_t,int>> pieces;};std::vector<std::pair<double,std::vector<Row>>> choices;
 // Six bounded packing attempts. Padding includes the complete template
 // box (including overhanging roofs), not only the wall footprint.
 for(int width:{192,256,320,384,448,500}){
  std::vector<Row> rows(1);bool bad=false;
  for(auto id:prototypes){auto const&b=source.pieces[id].box;int w=b.max.x-b.min.x+7,d=b.max.z-b.min.z+7;
   if(w>width||d>80){bad=true;break;}if(rows.back().width+w>width){int z=rows.back().z+rows.back().depth;rows.push_back({z});}
   auto&r=rows.back();r.pieces.emplace_back(id,r.width);r.width+=w;r.depth=std::max(r.depth,d);
  }
  int depth=rows.back().z+rows.back().depth;if(bad||depth>500)continue;
  int actual=0;for(auto const&r:rows)actual=std::max(actual,r.width);
  double cost=std::max(actual,depth)+.15*std::abs(actual-depth);
  choices.emplace_back(cost,std::move(rows));
 }
 if(choices.empty()){out.rejection="lot-capacity";return out;}
 std::stable_sort(choices.begin(),choices.end(),[](auto const&a,auto const&b){return a.first<b.first;});
 using Surface=decltype(sample(0,0));std::map<VillageXZ,Surface> cache;
 auto at=[&](int x,int z)->Surface const&{auto key=VillageXZ{x,z};auto it=cache.find(key);if(it==cache.end()){++out.surfaceQueries;it=cache.emplace(key,sample(x,z)).first;}return it->second;};
 for(auto const&choice:choices){auto const&best=choice.second;out.lots.clear();out.roads.clear();
 auto attempt=[&](){
 std::vector<bool> populated(source.pieces.size());
 int width=0,depth=best.back().z+best.back().depth;for(auto const&r:best)width=std::max(width,r.width);
 int X=source.anchor.x*16-width/2,Z=source.anchor.z*16-depth/2,style=villageStyle(source.biome);
 auto road=[&](int x,int z){out.roads[{x,z}]=0;};
 for(auto const&r:best){
  for(int x=X;x<X+width;++x)for(int d=0;d<3;++d){road(x,Z+r.z+d);road(x,Z+r.z+r.depth-1-d);}
  for(auto [id,offset]:r.pieces){auto const&p=source.pieces[id];auto b=p.box;int x=X+offset+3,z=Z+r.z+3;
   int dx=x-b.min.x,dz=z-b.min.z;int lo=INT_MAX,hi=INT_MIN;
   for(int zz=b.min.z;zz<=b.max.z;zz+=3)for(int xx=b.min.x;xx<=b.max.x;xx+=3){auto const&s=at(xx+dx,zz+dz);
    if(!villageSurfaceAllowed(s,style)){out.rejection="lot-biome-or-water";return false;}lo=std::min(lo,s.height);hi=std::max(hi,s.height);
   }
   if(hi-lo>12){out.rejection="lot-slope";return false;}
   int y=at((b.min.x+b.max.x)/2+dx,(b.min.z+b.max.z)/2+dz).height,dy=y-p.origin.y;
   b.min.x+=dx;b.max.x+=dx;b.min.z+=dz;b.max.z+=dz;b=b.moved(dy);
   out.lots.push_back({id,{dx,dy,dz},b,!populated[id]});populated[id]=true;
   int right=offset+(p.box.max.x-p.box.min.x+7);
   for(int zz=Z+r.z;zz<Z+r.z+r.depth;++zz)for(int d=0;d<3;++d){road(X+offset+d,zz);road(X+right-1-d,zz);}
  }
 }
 // Streets cannot span a lake, cliff or incompatible biome. Their natural
 // slope is retained, then the common doorway/apron solver fits entrances.
 for(auto const&[xz,unused]:out.roads){auto const&s=at(xz.first,xz.second);if(!villageSurfaceAllowed(s,style)){out.rejection="road-biome-or-water";return false;}
  for(auto [dx,dz]:{VillageXZ{1,0},{0,1}})if(out.roads.contains({xz.first+dx,xz.second+dz})&&std::abs(s.height-at(xz.first+dx,xz.second+dz).height)>3){out.rejection="road-grade";return false;}
 }
 return true;};
 if(attempt()){out.accepted=true;out.rejection={};return out;}
 }
 return out;
}
// Thin only competing suitable sites. A deterministic local priority makes
// neighbouring towns non-overlapping without dependence on exploration order.
template<class Candidate,class Sample> bool villageTownOwner(ChunkPosition anchor,uint32_t seed,int spacing,Candidate candidate,Sample sample){
 constexpr int separation=OrderedVillageReach*2+8,reach=(separation+15)/16;
 auto fitness=[&](ChunkPosition p){int lo=INT_MAX,hi=INT_MIN,style=-1;
  for(int dz=-96;dz<=96;dz+=48)for(int dx=-96;dx<=96;dx+=48){auto s=sample(p.x*16+dx,p.z*16+dz);int kind=villageStyle(s.biome);
   if(style<0)style=kind;if(!villageSurfaceAllowed(s,style))return -1;lo=std::min(lo,s.height);hi=std::max(hi,s.height);if(hi-lo>32)return -1;
  }return hi-lo;
 };
 auto rank=[&](ChunkPosition p,int score){return std::tuple{score,placementSeed(p.x,p.z,VillageSalt+391,seed),p.z,p.x};};
 int own=fitness(anchor);if(own<0)return false;auto priority=rank(anchor,own);
 for(int z=spacingCell(anchor.z-reach,spacing);z<=spacingCell(anchor.z+reach,spacing);++z)
 for(int x=spacingCell(anchor.x-reach,spacing);x<=spacingCell(anchor.x+reach,spacing);++x){auto c=candidate(x,z);
  if((c.x==anchor.x&&c.z==anchor.z)||std::abs(c.x-anchor.x)*16>=separation||std::abs(c.z-anchor.z)*16>=separation)continue;
  int score=fitness(c);if(score>=0&&rank(c,score)<priority)return false;
 }
 return true;
}
}
