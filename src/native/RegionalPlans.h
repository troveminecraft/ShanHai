#pragma once
#include "Noise.h"
#include "RegionalPlanStore.h"
#include "../RegionalAlgorithms.h"
#include "DrainageCurves.h"
#include "LakeBoundary.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <deque>
#include <future>

namespace nativegen::regional {
inline constexpr int Core=2048,Halo=512,Pitch=64,Count=(Core+Halo*2)/Pitch+1;
struct Ground{int height;double humidity,temperature,stone,volcanic;bool closedBasin=false;int water=-32768;};
template<class Sample> J drainage(int gx,int gz,uint32_t seed,Sample sample,bool curved=false,int seaLevel=31,int maxSurfaceY=319,bool meanders=false,bool confinedBanks=false,bool refresh=false,bool uplandWaterways=false,J const* resolvedLakes=nullptr,bool lakesOnly=false,bool headward=false){
 constexpr int N=Count,Total=N*N;
 int originX=gx*Core-Halo,originZ=gz*Core-Halo;
 std::array<Ground,Total> ground;
 std::map<int,std::pair<int,int>> lakeAnchors;
 auto point=[&](int i){if(auto a=lakeAnchors.find(i);a!=lakeAnchors.end())return a->second;return std::pair{originX+(i%N)*Pitch,originZ+(i/N)*Pitch};};
 auto eligible=[](Ground const&s){return s.stone<=.001&&s.volcanic<=0;};
 for(int i=0;i<Total;++i){
  if(resolvedLakes&&resolvedLakes->contains("ground")){auto const&s=resolvedLakes->at("ground").at(i);ground[i]={s[0],s[1],s[2],s[3],s[4],s[5],s[6]};}
  else {auto[x,z]=point(i);ground[i]=sample(x,z);}
 }
 J lakes=J::array(),segments=J::array();
 std::array<double,Total> sink;sink.fill(-1);
 for(int i=0;i<Total;++i)if(eligible(ground[i])){auto const&s=ground[i];if(!s.closedBasin&&s.height<seaLevel)sink[i]=seaLevel;else if(s.closedBasin&&s.water>s.height)sink[i]=s.water;}
 // Closed basins are tested against their actual surrounding terrain. A lake
 // owns a small bounded footprint wholly inside its canonical region.
 size_t lakeLimit=refresh?8:curved?4:3;
 if(!resolvedLakes)for(int z=10;z<N-10&&lakes.size()<lakeLimit;++z)for(int x=10;x<N-10&&lakes.size()<lakeLimit;++x){
  int i=z*N+x;auto const&s=ground[i];auto[X,Z]=point(i);
  if((!s.closedBasin&&s.height<seaLevel+9)||s.height>maxSurfaceY-84||s.humidity<.40||!eligible(s)||hash01(X,Z,seed^63101)>(refresh?.80:.35))continue;
  bool minimum=true;for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz)minimum&=refresh?s.height<=ground[(z+dz)*N+x+dx].height:s.height<ground[(z+dz)*N+x+dx].height;
  if(!minimum)continue;
  bool overlap=false;for(auto const&lake:lakes){double ax=lake.at("center").at(0),az=lake.at("center").at(1);overlap|=std::hypot(X-ax,Z-az)<192;}if(overlap)continue;
  constexpr int Side=33;int Step=refresh?6:4;std::array<Ground,Side*Side> fine;std::array<bool,Side*Side> sampled{};int rim=maxSurfaceY+1;
  // Refreshed lakes use a connected flood, not the unrelated outer rim. Most
  // candidates only visit the central survey and a small flooded component.
  // Evaluate exactly the requested samples once instead of all 1,089 columns.
  auto fineAt=[&](int i)->Ground const&{if(!sampled[i]){
   fine[i]=sample(X+(i%Side-16)*Step,Z+(i/Side-16)*Step);sampled[i]=true;
  }return fine[i];};
  if(!refresh)for(int zz=0;zz<Side;++zz)for(int xx=0;xx<Side;++xx){auto const& q=fineAt(zz*Side+xx);if(xx==0||zz==0||xx==Side-1||zz==Side-1)rim=std::min(rim,q.height);}
  int start=16*Side+16;
  // A 64-block coarse grid rarely lands on the bottom of a natural basin.
  // Refine the seed locally; a low point on an unrelated outer boundary must
  // not reject a basin whose own connected shoreline is fully enclosed.
  if(refresh)for(int zz=8;zz<=24;++zz)for(int xx=8;xx<=24;++xx){int j=zz*Side+xx;if(eligible(fineAt(j))&&fineAt(j).height<fineAt(start).height)start=j;}
  int low=fineAt(start).height,level=refresh?low+7:std::min(s.height+4,rim-1);
  if(level<low+2)continue;
  std::array<bool,Side*Side> wet{};std::vector<int> queue;std::array<int,Side*Side> frontier{};bool accepted=false;
  for(;level>=low+2;--level){
   wet.fill(false);queue={start};wet[start]=true;bool open=false;size_t pending=1;frontier[0]=start;
   // Depth-first rejection reaches an escape sooner in open depressions.
   // Closed components still visit the identical set and emit canonical rows.
   for(size_t k=0;k<queue.size();++k){int j=refresh?frontier[--pending]:queue[k],xx=j%Side,zz=j/Side;
    if(xx==0||zz==0||xx==Side-1||zz==Side-1||!eligible(fineAt(j))){open=true;break;}
    for(auto[dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){int a=(zz+dz)*Side+xx+dx;if(!wet[a]&&fineAt(a).height<level){wet[a]=true;queue.push_back(a);if(refresh)frontier[pending++]=a;}}
   }
   if(!open&&queue.size()>=32){accepted=true;break;}
   if(!refresh)break;
  }
  if(!accepted)continue;
  J rows=J::array();
  if(refresh){
   int side=(Side-1)*Step+1;
   auto verified=closedLake(X-16*Step,Z-16*Step,side,(start%Side)*Step,(start/Side)*Step,level,low+2,sample,eligible);
   if(verified.rows.empty())continue;
   level=verified.level;for(auto const&row:verified.rows)rows.push_back(row);
  }else for(int zz=0;zz<Side;++zz)for(int xx=0;xx<Side;){if(!wet[zz*Side+xx]){++xx;continue;}int begin=xx;while(xx+1<Side&&wet[zz*Side+xx+1])++xx;
   for(int dz=0;dz<Step;++dz)rows.push_back({Z+(zz-16)*Step-Step/2+dz,X+(begin-16)*Step-Step/2,X+(xx-16)*Step+(Step-1)/2});++xx;
  }
  lakes.push_back({{"level",level},{"center",{X+(refresh?(start%Side-16)*Step:0),Z+(refresh?(start/Side-16)*Step:0)}},{"rows",rows},{"frozen",s.temperature<.42}});sink[i]=level;
 }
 if(lakesOnly){J survey=J::array();for(auto const&s:ground)survey.push_back({s.height,s.humidity,s.temperature,s.stone,s.volcanic,s.closedBasin,s.water});return J{{"lakes",lakes},{"ground",survey}};}
 if(resolvedLakes){
  lakes=resolvedLakes->at("owned");
  // Every owner sees the same immutable neighbouring basins before routing.
  // Use the actual surveyed low point, not a coarse point outside the lake.
  for(auto const&lake:resolvedLakes->at("nearby")){
   int x=lake.at("center").at(0),z=lake.at("center").at(1);
   int ix=roundJS(double(x-originX)/Pitch),iz=roundJS(double(z-originZ)/Pitch);
   if(ix<1||iz<1||ix>=N-1||iz>=N-1)continue;
   int i=iz*N+ix;lakeAnchors[i]={x,z};ground[i]=sample(x,z);sink[i]=lake.at("level");
  }
 }
 std::vector<std::pair<int,int>> fallEdges;std::set<int> fallSources;
 if(resolvedLakes&&resolvedLakes->contains("falls"))for(auto const&f:resolvedLakes->at("falls")){
  int ax=f[0],az=f[1],bx=f[2],bz=f[3];
  int ix=roundJS(double(ax-originX)/Pitch),iz=roundJS(double(az-originZ)/Pitch),jx=roundJS(double(bx-originX)/Pitch),jz=roundJS(double(bz-originZ)/Pitch);
  if(ix<1||iz<1||jx<1||jz<1||ix>=N-1||iz>=N-1||jx>=N-1||jz>=N-1)continue;
  int a=iz*N+ix,b=jz*N+jx;if(a==b||sink[a]>=0||sink[b]>=0||lakeAnchors.contains(a)||lakeAnchors.contains(b))continue;
  auto A=sample(ax,az),B=sample(bx,bz);if(!eligible(A)||!eligible(B)||A.humidity<.4||A.height-B.height<(headward?60:30)||B.height<=seaLevel)continue;
  lakeAnchors[a]={ax,az};lakeAnchors[b]={bx,bz};ground[a]=A;ground[b]=B;fallEdges.emplace_back(a,b);
 }
 std::array<double,Total> distance;distance.fill(INFINITY);
 std::array<int,Total> parent,root;parent.fill(-1);root.fill(-1);
 using Entry=std::pair<double,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> heap;
 for(int i=0;i<Total;++i)if(sink[i]>=0){distance[i]=0;root[i]=i;heap.push({0,i});}
 std::vector<int> order;
 while(!heap.empty()){
  auto[cost,i]=heap.top();heap.pop();if(cost!=distance[i])continue;order.push_back(i);int x=i%N,z=i/N;
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){if((!dx&&!dz)||x+dx<0||x+dx>=N||z+dz<0||z+dz>=N)continue;
   int k=(z+dz)*N+x+dx;if(!eligible(ground[k]))continue;
   double edge=Pitch*std::hypot(double(dx),double(dz));
   if(resolvedLakes){auto[a,b]=point(i);auto[c,d]=point(k);edge=std::hypot(double(a-c),double(b-d));}
   double uphill=std::max(0,ground[i].height-ground[k].height),next=cost+edge+35*uphill*uphill;
   if(next<distance[k]){distance[k]=next;parent[k]=i;root[k]=root[i];heap.push({next,k});}
  }
 }
 // Mountain springs may start above a measured cliff. The toe still has to
 // follow a real drainage path to a lake, sea or established river; never
 // join back through the source or introduce a closed circulation.
 for(auto[a,b]:fallEdges)if(std::isfinite(distance[b])){
  bool cycle=false;for(int i=b,n=0;i>=0&&n<Total;i=parent[i],++n)if(i==a){cycle=true;break;}
  if(cycle)continue;parent[a]=b;auto[x,z]=point(a);auto[X,Z]=point(b);distance[a]=distance[b]+std::hypot(double(x-X),double(z-Z));fallSources.insert(a);
 }
 if(!fallSources.empty())std::sort(order.begin(),order.end(),[&](int a,int b){return std::tie(distance[a],a)<std::tie(distance[b],b);});
 std::array<int,Total> flow;flow.fill(1);
 if(headward){
  // Redirecting a cliff edge changes the tree order. Recompute a topological
  // order before accumulating catchment; sorting the former path costs can
  // otherwise count a downstream node before all of its tributaries arrive.
  std::array<int,Total> children{};std::deque<int> ready;std::vector<int> leaves;
  for(int i:order)if(parent[i]>=0)++children[parent[i]];
  for(int i:order)if(!children[i])ready.push_back(i);
  while(!ready.empty()){int i=ready.front();ready.pop_front();leaves.push_back(i);
   if(parent[i]>=0){flow[parent[i]]+=flow[i];if(!--children[parent[i]])ready.push_back(parent[i]);}
  }
  if(leaves.size()!=order.size())throw std::runtime_error("Cyclic waterfall drainage");
  order.assign(leaves.rbegin(),leaves.rend());
  for(int i:order)if(parent[i]>=0){auto[x,z]=point(i);auto[X,Z]=point(parent[i]);distance[i]=distance[parent[i]]+std::hypot(double(x-X),double(z-Z));}
  // Prefer a real upstream catchment over a spring placed directly on the
  // lip. Its accumulated flow controls river width when it reaches the fall.
  std::map<int,std::pair<double,int>> upstream;
  for(int i:order){if(ground[i].humidity<.4||ground[i].height>maxSurfaceY-30||flow[i]<3)continue;
   int end=i;double run=0;
   for(int n=0;n<80&&parent[end]>=0;++n){
    if(fallSources.contains(end)){if(run>=256){double score=run*std::sqrt(double(flow[end]));auto&best=upstream[end];if(score>best.first)best={score,i};}break;}
    auto[x,z]=point(end);auto[X,Z]=point(parent[end]);run+=std::hypot(double(x-X),double(z-Z));end=parent[end];
   }
  }
  for(auto const&[fall,best]:upstream){fallSources.erase(fall);fallSources.insert(best.second);}
 }else for(auto i=order.rbegin();i!=order.rend();++i)if(parent[*i]>=0)flow[parent[*i]]+=flow[*i];
 if(curved){
  auto result=curvedDrainage(gx,gz,seed,order,ground,parent,flow,distance,sink,point,sample,seaLevel,maxSurfaceY,meanders,confinedBanks,resolvedLakes?resolvedLakes->at("nearby"):lakes,uplandWaterways,resolvedLakes!=nullptr,&fallSources,headward);
  result["region"]={gx,gz};result["lakes"]=lakes;result["samples"]=Total;return result;
 }
 std::vector<int> sources;
 for(int i:order){auto[x,z]=point(i);auto const&s=ground[i];
  if(x<gx*Core+128||x>=(gx+1)*Core-128||z<gz*Core+128||z>=(gz+1)*Core-128||s.height<90||s.height>maxSurfaceY-74||s.humidity<.45||flow[i]<6||distance[i]<400||parent[i]<0)continue;
  if(hash01(x,z,seed^63107)>.35)continue;sources.push_back(i);
 }
 std::sort(sources.begin(),sources.end(),[&](int a,int b){double A=ground[a].height*std::pow(flow[a],.18),B=ground[b].height*std::pow(flow[b],.18);return A==B?a<b:A>B;});
 std::vector<std::pair<int,int>> acceptedSources;std::set<int> occupied;
 for(int source:sources){if(acceptedSources.size()>=5)break;auto[SX,SZ]=point(source);bool close=false;
  for(auto[x,z]:acceptedSources)close|=std::hypot(double(SX-x),double(SZ-z))<400;if(close)continue;
  std::vector<int> path;int end=source;while(parent[end]>=0&&path.size()<96&&!occupied.contains(end)){path.push_back(end);end=parent[end];}
  if(path.size()<8||path.size()>=96||occupied.contains(end)||sink[end]<0)continue;
  path.push_back(end);double receiver=sink[end],water=ground[source].height-1,age=0;J route=J::array();bool bad=false;
  bool main=distance[source]>1200&&flow[source]>=12;double width=main?8.:3.;
  std::array<double,5> previous{};bool first=true;
  for(size_t k=0;k+1<path.size()&&!bad;++k){auto[ax,az]=point(path[k]);auto[bx,bz]=point(path[k+1]);int steps=int(std::ceil(std::hypot(double(bx-ax),double(bz-az))/24));
   for(int t=0;t<=steps;++t){if(k&&t==0)continue;double u=double(t)/steps,X=mix(u,ax,bx),Z=mix(u,az,bz);auto s=sample(roundJS(X),roundJS(Z));
    double radius=.85+(width-.85)*smooth(0.,192.,age),bank=std::min(sample(roundJS(X)+roundJS(radius+3),roundJS(Z)).height,sample(roundJS(X)-roundJS(radius+3),roundJS(Z)).height);
    water=std::max(receiver,std::min({water,double(s.height-1),bank-1}));if(!eligible(s)||s.height-water>12||water-s.height>2){bad=true;break;}
    if(!first){double length=std::hypot(X-previous[0],Z-previous[1]);route.push_back({previous[0],previous[1],X,Z,previous[2],water,previous[3],radius,previous[4],age,main?1.:0.});age+=length;}
    previous={X,Z,water,radius,age};first=false;
   }
  }
  if(bad||route.empty()||previous[2]-receiver>2)continue;
  for(auto const&s:route)segments.push_back(s);for(int i:path)occupied.insert(i);acceptedSources.push_back({SX,SZ});
 }
 return {{"region",{gx,gz}},{"lakes",lakes},{"segments",segments},{"sources",acceptedSources},{"samples",Total}};
}
}
