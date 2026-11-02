#pragma once
#include "OrderedVillage.h"
#include <queue>

namespace worldgen_rules {
// Dense, irregular plots grown outwards from the village centre. Geometry is
// packed before surveying terrain; the road graph is built once on a bounded
// two-block grid. Native template rotations and population budgets survive.
template<class Sample> OrderedVillage clusterVillage(VillageBlueprint const&source,Sample sample){
 OrderedVillage out;std::vector<size_t> copies;
 for(size_t i=0;i<source.pieces.size();++i){auto const&p=source.pieces[i];
  if(p.kind!=StructurePieceKind::Template||!p.rigid||!p.box.valid())continue;
  bool house=villageBuilding(p.location);out.baseline+=house;
  for(int n=0;n<(house?6:1);++n)copies.push_back(i);
 }
 out.target=out.baseline*6;if(!out.baseline||copies.size()>768){out.rejection="building-budget";return out;}
 StructureRandom random(placementSeed(source.anchor.x,source.anchor.z,VillageSalt+401,source.seed));
 for(size_t i=copies.size();i>1;--i)std::swap(copies[i-1],copies[random.nextInt(uint32_t(i))]);
 constexpr int reach=250,side=251;
 int X=source.anchor.x*16,Z=source.anchor.z*16,style=villageStyle(source.biome);
 std::vector<int> occupancy(side*side,-1);std::vector<bool> populated(source.pieces.size());
 using Surface=decltype(sample(0,0));std::map<VillageXZ,Surface> samples;
 auto at=[&](int x,int z)->Surface const&{auto k=VillageXZ{x,z};auto it=samples.find(k);
  if(it==samples.end()){++out.surfaceQueries;it=samples.emplace(k,sample(x,z)).first;}return it->second;};
 auto node=[&](int x,int z){return ((z+reach)/2)*side+(x+reach)/2;};
 for(auto id:copies){auto const&p=source.pieces[id];auto original=p.box;
  int w=original.max.x-original.min.x+1,d=original.max.z-original.min.z+1;
  double phase=random.nextInt(10000)*.0006283185307;bool placed=false;int surveys=0;
  // A rotated spiral is a candidate ordering, not the plot layout. Full
  // template footprints and varied clearances determine accepted positions.
  for(int k=0;k<10000;++k){double radius=3.25*std::sqrt(double(k)),angle=phase+k*2.3999632297;
   int x=int(std::round(radius*std::cos(angle)))-w/2,z=int(std::round(radius*std::sin(angle)))-d/2;
   if(x< -reach+6||z< -reach+6||x+w>reach-6||z+d>reach-6)continue;
   int gap=3+int((id+out.lots.size())%3);bool free=true;
   for(int zz=z-gap;zz<=z+d+gap&&free;zz+=2)for(int xx=x-gap;xx<=x+w+gap;++xx)
    if(occupancy[node(xx,zz)]>=0){free=false;break;}
   if(!free)continue;
   int low=INT_MAX,high=INT_MIN;
   for(int zz=z;zz<z+d&&free;zz+=3)for(int xx=x;xx<x+w;xx+=3){auto const&s=at(X+xx,Z+zz);
    if(!villageSurfaceAllowed(s,style)){free=false;break;}low=std::min(low,s.height);high=std::max(high,s.height);}
   if(!free||high-low>12){if(++surveys>=24)break;continue;}
   int dx=X+x-original.min.x,dz=Z+z-original.min.z,dy=at(X+x+w/2,Z+z+d/2).height-p.origin.y;
   auto box=original;box.min.x+=dx;box.max.x+=dx;box.min.z+=dz;box.max.z+=dz;box=box.moved(dy);
   out.lots.push_back({id,{dx,dy,dz},box,!populated[id]});populated[id]=true;
   for(int zz=z-2;zz<=z+d+1;++zz)for(int xx=x-2;xx<=x+w+1;++xx)occupancy[node(xx,zz)]=int(out.lots.size())-1;
   placed=true;break;
  }
  if(!placed){out.rejection="cluster-lot-capacity-or-terrain";return out;}
 }
 // A single flood tree joins every plot. Walking around occupied footprints
 // and rotating ties removes the former full rectangular street lattice.
 int root=-1;double nearest=1e20;
 for(int z=1;z<side-1;++z)for(int x=1;x<side-1;++x){int i=z*side+x;if(occupancy[i]>=0)continue;
  double r=(x-side/2)*(x-side/2)+(z-side/2)*(z-side/2);if(r<nearest){nearest=r;root=i;}}
 if(root<0){out.rejection="cluster-road-root";return out;}
 std::vector<int> parent(side*side,-1),queue{root};parent[root]=root;
 constexpr int dx[4]={1,0,-1,0},dz[4]={0,1,0,-1};
 for(size_t n=0;n<queue.size();++n){int i=queue[n],x=i%side,z=i/side;
  int rotation=int((uint32_t(x)*73856093u^uint32_t(z)*19349663u^source.seed)>>10)&3;
  for(int d=0;d<4;++d){int k=(d+rotation)%4,xx=x+dx[k],zz=z+dz[k];if(xx<1||xx>=side-1||zz<1||zz>=side-1)continue;
   int j=zz*side+xx;if(parent[j]>=0||occupancy[j]>=0)continue;parent[j]=i;queue.push_back(j);}
 }
 auto road=[&](int i){int x=X+i%side*2-reach,z=Z+i/side*2-reach;
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)out.roads[{x+dx,z+dz}]=0;};
 std::vector<bool> connected(side*side);connected[root]=true;road(root);
 for(auto const&lot:out.lots){auto const&b=lot.box;int best=-1;double score=1e30;
  for(int z=b.min.z-Z-6;z<=b.max.z-Z+6;z+=2)for(int x=b.min.x-X-6;x<=b.max.x-X+6;x+=2){
   int i=node(x,z);if(parent[i]<0)continue;
   double r=double(x)*x+double(z)*z;if(r<score){score=r;best=i;}}
  if(best<0){out.rejection="cluster-road-disconnected";return out;}
  for(int i=best;!connected[i];i=parent[i]){connected[i]=true;road(i);}
  // Short perimeter aprons make doors on any native rotation accessible.
  for(int i=best-1;i<=best+1;++i)if(i>=0&&i<side*side&&parent[i]>=0)road(i);
 }
 for(auto const&[xz,unused]:out.roads){auto const&s=at(xz.first,xz.second);
  if(!villageSurfaceAllowed(s,style)){out.rejection="road-biome-or-water";return out;}
  for(auto [dx,dz]:{VillageXZ{1,0},{0,1}})if(out.roads.contains({xz.first+dx,xz.second+dz})&&std::abs(s.height-at(xz.first+dx,xz.second+dz).height)>3){out.rejection="road-grade";return out;}
 }
 out.accepted=true;return out;
}
}
