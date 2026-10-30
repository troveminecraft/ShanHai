#pragma once
#include "Noise.h"
#include <set>

namespace nativegen::regional {
struct RiverPoint {double x,z;};
inline RiverPoint operator+(RiverPoint a,RiverPoint b){return {a.x+b.x,a.z+b.z};}
inline RiverPoint operator-(RiverPoint a,RiverPoint b){return {a.x-b.x,a.z-b.z};}
inline RiverPoint operator*(RiverPoint a,double b){return {a.x*b,a.z*b};}
inline double length(RiverPoint a){return std::hypot(a.x,a.z);}
inline RiverPoint direction(RiverPoint a){double n=length(a);return n>1e-9?a*(1/n):RiverPoint{};}
inline RiverPoint bezier(RiverPoint a,RiverPoint b,RiverPoint c,RiverPoint d,double t){
 double u=1-t;return a*(u*u*u)+b*(3*u*u*t)+c*(3*u*t*t)+d*(t*t*t);
}
struct RiverCurvePoint {RiverPoint p;int node=-1;};
// Interpolate drainage nodes exactly, so a tributary can reuse a receiving
// node's position and water level. Shared node tangents remove grid corners;
// short handles limit sideways departure from the surveyed valley corridor.
inline std::vector<RiverCurvePoint> riverCurve(std::vector<RiverPoint> const&nodes,
                                              RiverPoint outletDirection={},bool meanders=false,uint32_t seed=0){
 std::vector<RiverCurvePoint> result;if(nodes.size()<2)return result;
 std::vector<RiverPoint> tangent(nodes.size());
 for(size_t i=0;i<nodes.size();++i){
  auto before=nodes[i]-nodes[i?i-1:i],after=nodes[i+1<nodes.size()?i+1:i]-nodes[i];
  double span=i==0?length(after):i+1==nodes.size()?length(before):std::min(length(before),length(after));
  auto heading=i+1==nodes.size()&&length(outletDirection)>0?direction(outletDirection):direction(before+after);
  tangent[i]=heading*(span*.28);
 }
 result.push_back({nodes.front(),0});
 for(size_t i=0;i+1<nodes.size();++i){
  auto a=nodes[i],b=a+tangent[i],d=nodes[i+1],c=d-tangent[i+1];
  // The derivative lies in the convex hull of the three control edges * 3.
  // Bound each chord (not just total arc length) to six blocks, including
  // the faster middle of a curve, without per-column spline evaluation.
  auto append=[&](RiverPoint A,RiverPoint B,RiverPoint C,RiverPoint D,int node){
   int steps=std::max(1,int(std::ceil(3*std::max({length(B-A),length(C-B),length(D-C)})/6.)));
   for(int k=1;k<=steps;++k)result.push_back({bezier(A,B,C,D,double(k)/steps),k==steps?node:-1});
  };
  auto heading=direction(d-a);
  double aligned=std::min(heading.x*direction(tangent[i]).x+heading.z*direction(tangent[i]).z,
                          heading.x*direction(tangent[i+1]).x+heading.z*direction(tangent[i+1]).z);
  if(meanders&&aligned>.92){
   // Coherent world-space sign and bounded lateral excursion; no new random
   // source order or altered confluence anchors.
   double phase=double(seed%65521)*.013;
   double wave=std::sin((a.x+d.x)/710.+(a.z+d.z)/930.+phase);
   double amplitude=std::min(10.,length(d-a)*.14)*(.65+.35*std::abs(wave))*(wave<0?-1:1);
   auto mid=(a+d)*.5+RiverPoint{-heading.z,heading.x}*amplitude;
   auto midTangent=heading*(length(d-a)*.14);
   append(a,a+tangent[i]*.5,mid-midTangent,mid,-1);
   append(mid,mid+midTangent,d-tangent[i+1]*.5,d,int(i+1));
   continue;
  }
  int steps=std::max(1,int(std::ceil(3*std::max({length(b-a),length(c-b),length(d-c)})/6.)));
  for(int k=1;k<=steps;++k)result.push_back({bezier(a,b,c,d,double(k)/steps),k==steps?int(i+1):-1});
 }
 return result;
}

struct RiverAnchor {double water=0,radius=0;RiverPoint downstream{};};
template<class Point,class Sample,class GroundArray,class ParentArray,class FlowArray,class DistanceArray,class SinkArray>
J curvedDrainage(int gx,int gz,uint32_t seed,std::vector<int> const&order,GroundArray const&ground,
                 ParentArray const&parent,FlowArray const&flow,DistanceArray const&distance,
                 SinkArray const&sink,Point point,Sample sample,int seaLevel=31,int maxSurfaceY=319,bool meanders=false,
                 bool confinedBanks=false,J const&lakes=J::array(),bool uplandWaterways=false){
 auto eligible=[](auto const&s){return s.stone<=.001&&s.volcanic<=0;};
 // Survey the complete raster corridor, including the one-block dry rim.
 // Three samples at spline vertices miss shallow hollows between vertices.
 // Cache only this owner's survey; no exploration-order-dependent state.
 using Ground=std::decay_t<decltype(sample(0,0))>;
 std::unordered_map<uint64_t,Ground> corridor;
 std::unordered_map<int,std::vector<std::array<int,3>>> lakeRows;
 if(confinedBanks)for(auto const&lake:lakes)for(auto const&r:lake.at("rows"))
  lakeRows[r.at(0).get<int>()].push_back({r.at(1),r.at(2),lake.at("level")});
 auto groundAt=[&](int x,int z){auto key=key2(x,z);auto it=corridor.find(key);
  if(it!=corridor.end())return it->second;auto s=sample(x,z);corridor.emplace(key,s);return s;};
 auto receivingWater=[&](int x,int z,int height,double receiver){
  auto s=groundAt(x,z);
  if(s.closedBasin){if(s.water>=receiver&&height<s.water)return true;}else if(receiver==seaLevel&&height<seaLevel)return true;
  auto it=lakeRows.find(z);if(it==lakeRows.end())return false;
  for(auto const&r:it->second)if(x>=r[0]&&x<=r[1]&&r[2]>=receiver)return true;
  return false;
 };
 std::vector<int> sources;
 for(int i:order){auto[x,z]=point(i);auto const&s=ground[i];
  if(x<gx*2048+128||x>=(gx+1)*2048-128||z<gz*2048+128||z>=(gz+1)*2048-128||
     s.height<65||s.height>maxSurfaceY-74||s.humidity<.40||flow[i]<3||distance[i]<200||parent[i]<0)continue;
  if(hash01(x,z,seed^63107)<.55)sources.push_back(i);
 }
 std::sort(sources.begin(),sources.end(),[&](int a,int b){
  double A=ground[a].height*std::pow(flow[a],.18),B=ground[b].height*std::pow(flow[b],.18);return A==B?a<b:A>B;
 });
 std::map<int,RiverAnchor> network;
 std::vector<std::pair<int,int>> accepted;
 J segments=J::array(),routes=J::array();int independent=0;
 int noOutlet=0,invalidGround=0,lowOutlet=0,excessiveCut=0;
 for(int source:sources){
  if(accepted.size()>=14)break;if(network.contains(source))continue;
  auto[SX,SZ]=point(source);bool close=false;
  for(auto[x,z]:accepted)close|=std::hypot(double(SX-x),double(SZ-z))<192;if(close)continue;
  double maximumReceiver=INFINITY;
  for(int attempt=0;attempt<(confinedBanks?4:1);++attempt){
  std::vector<int> path;int end=source;
  while(parent[end]>=0&&path.size()<96&&(!network.contains(end)||network.at(end).water>maximumReceiver)){path.push_back(end);end=parent[end];}
  bool tributary=network.contains(end);
  if(path.size()<3||path.size()>=96||(!tributary&&(sink[end]<0||independent>=6))){++noOutlet;break;}
  path.push_back(end);double receiver=tributary?network.at(end).water:sink[end];
  bool main=!tributary&&distance[source]>1200&&flow[source]>=12;
  double width=main?8.:ground[source].height>=105?1.8:2.6;
  std::vector<RiverPoint> nodes;for(int i:path){auto[x,z]=point(i);nodes.push_back({double(x),double(z)});}
  auto curve=riverCurve(nodes,tributary?network.at(end).downstream:RiverPoint{},meanders,seed);
  std::vector<double> run(curve.size()),water(curve.size()),radius(curve.size()),terrain(curve.size());
  std::vector<bool> steep(curve.size(),false);bool bad=false;
  for(size_t i=0;i<curve.size();++i){
   if(i)run[i]=run[i-1]+length(curve[i].p-curve[i-1].p);
   radius[i]=.85+(width-.85)*smooth(0.,192.,run[i]);
   if(tributary)radius[i]=std::min(radius[i],network.at(end).radius);
   auto p=curve[i].p;
   auto tangent=direction(curve[std::min(i+1,curve.size()-1)].p-curve[i?i-1:i].p);
   auto normal=RiverPoint{-tangent.z,tangent.x}*(radius[i]+2);
   auto s=groundAt(roundJS(p.x),roundJS(p.z));
   auto left=groundAt(roundJS(p.x+normal.x),roundJS(p.z+normal.z));
   auto right=groundAt(roundJS(p.x-normal.x),roundJS(p.z-normal.z));
   if(!eligible(s)||!eligible(left)||!eligible(right)){bad=true;break;}
   terrain[i]=s.height;
   water[i]=std::max(receiver,double(std::min({s.height,left.height,right.height})-1));
  }
  if(bad){++invalidGround;break;}
  if(uplandWaterways){
   // A measured ledge may carry a cascade. Do not invent a fall at an
   // arbitrary owner boundary or a higher receiving river/lake.
   for(size_t i=1;i<curve.size();++i)if(terrain[i-1]-terrain[i]>std::max(3.,.75*(run[i]-run[i-1])))
    for(size_t k=i>2?i-2:1;k<std::min(curve.size(),i+3);++k)steep[k]=true;
   for(size_t i=0;i<curve.size();++i)if(steep[i]||(i+1<curve.size()&&steep[i+1]))
    water[i]=std::max(receiver,terrain[i]-1.);
  }
  double lowerReceiver=INFINITY;
  if(confinedBanks)for(size_t i=1;i<curve.size()&&!bad;++i){
   auto a=curve[i-1].p,b=curve[i].p,v=b-a;double len2=v.x*v.x+v.z*v.z;
   double reach=1.12*std::max(radius[i-1],radius[i])+1.5;
   double ceiling=maxSurfaceY;
   for(int z=int(std::floor(std::min(a.z,b.z)-reach));z<=int(std::ceil(std::max(a.z,b.z)+reach))&&!bad;++z)
    for(int x=int(std::floor(std::min(a.x,b.x)-reach));x<=int(std::ceil(std::max(a.x,b.x)+reach));++x){
     double t=clamp(((x-a.x)*v.x+(z-a.z)*v.z)/std::max(1e-9,len2),0.,1.);
     if(length(RiverPoint{double(x),double(z)}-(a+v*t))>reach)continue;
     auto s=groundAt(x,z);
     if(!eligible(s)){bad=true;break;}
     if(receivingWater(x,z,s.height,receiver))continue;
     ceiling=std::min(ceiling,double(s.height));
     // A river cannot climb out of a low hollow to its receiving lake.
     // Reject this route instead of suspending a ribbon of water above land.
     if(ceiling<receiver){lowerReceiver=ceiling;bad=true;break;}
    }
   // A single corridor-wide ceiling would lower the upper pool to the foot
   // of the cliff. Falling reaches instead follow the local ground during
   // rasterisation; the survey still rejects heat, stone forests and uphill outlets.
   if(!steep[i]){water[i-1]=std::min(water[i-1],ceiling);water[i]=std::min(water[i],ceiling);}
  }
  if(bad){
   ++lowOutlet;
   // A tributary may have met the receiving river too far upstream. Follow
   // its existing drainage path to a lower anchor before giving up on it.
   if(tributary&&lowerReceiver<receiver){maximumReceiver=lowerReceiver;continue;}
   break;
  }
  water.back()=receiver;
  // Limit the final descent as well as interior falls; do not manufacture a
  // large last-segment waterfall just to reach a lake or existing tributary.
  for(size_t i=water.size()-1;i>0;--i){
   double fall=.75*(run[i]-run[i-1]);
   if(steep[i])fall=std::max(fall,clamp(terrain[i-1]-terrain[i]+2.,0.,64.));
   water[i-1]=std::min(water[i-1],water[i]+fall);
  }
  for(size_t i=1;i<water.size();++i)water[i]=std::min(water[i],water[i-1]);
  for(size_t i=0;i<curve.size();++i){auto p=curve[i].p;auto s=groundAt(roundJS(p.x),roundJS(p.z));
   if(s.height-water[i]>12||water[i]-s.height>2){bad=true;break;}
  }
  if(bad){++excessiveCut;break;}
  size_t begin=segments.size();
  for(size_t i=1;i<curve.size();++i){auto a=curve[i-1].p,b=curve[i].p;
   J segment={a.x,a.z,b.x,b.z,water[i-1],water[i],radius[i-1],radius[i],run[i-1],run[i],main?1.:0.};
   if(uplandWaterways)segment.push_back(steep[i]?1.:0.);
   segments.push_back(std::move(segment));
  }
  for(size_t i=0;i<curve.size();++i)if(curve[i].node>=0){
   int node=path[curve[i].node];if(network.contains(node))continue;
   auto tangent=curve[std::min(i+1,curve.size()-1)].p-curve[i?i-1:i].p;
   network.emplace(node,RiverAnchor{water[i],radius[i],direction(tangent)});
  }
  auto[EX,EZ]=point(end);
  routes.push_back({{"source",{SX,SZ}},{"join",{EX,EZ}},{"receiver",tributary?"river":receiver==seaLevel?"ocean":"lake"},
                    {"firstSegment",begin},{"segmentCount",segments.size()-begin},{"length",run.back()},{"main",main}});
  accepted.push_back({SX,SZ});if(!tributary)++independent;
  break;
  }
 }
 J result={{"segments",segments},{"sources",accepted},{"routes",routes}};
 if(uplandWaterways)result["diagnostics"]={{"candidates",sources.size()},{"noOutlet",noOutlet},{"invalidGround",invalidGround},{"lowOutlet",lowOutlet},{"excessiveCut",excessiveCut},{"surveyColumns",corridor.size()}};
 return result;
}
}
