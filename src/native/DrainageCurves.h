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
                                              RiverPoint outletDirection={}){
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
  int steps=std::max(1,int(std::ceil(3*std::max({length(b-a),length(c-b),length(d-c)})/6.)));
  for(int k=1;k<=steps;++k)result.push_back({bezier(a,b,c,d,double(k)/steps),k==steps?int(i+1):-1});
 }
 return result;
}

struct RiverAnchor {double water=0,radius=0;RiverPoint downstream{};};
template<class Point,class Sample,class GroundArray,class ParentArray,class FlowArray,class DistanceArray,class SinkArray>
J curvedDrainage(int gx,int gz,uint32_t seed,std::vector<int> const&order,GroundArray const&ground,
                 ParentArray const&parent,FlowArray const&flow,DistanceArray const&distance,
                 SinkArray const&sink,Point point,Sample sample,int seaLevel=31,int maxSurfaceY=319){
 auto eligible=[](auto const&s){return s.stone<=.001&&s.volcanic<=0;};
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
 for(int source:sources){
  if(accepted.size()>=14)break;if(network.contains(source))continue;
  auto[SX,SZ]=point(source);bool close=false;
  for(auto[x,z]:accepted)close|=std::hypot(double(SX-x),double(SZ-z))<192;if(close)continue;
  std::vector<int> path;int end=source;
  while(parent[end]>=0&&path.size()<96&&!network.contains(end)){path.push_back(end);end=parent[end];}
  bool tributary=network.contains(end);
  if(path.size()<3||path.size()>=96||(!tributary&&(sink[end]<seaLevel||independent>=6)))continue;
  path.push_back(end);double receiver=tributary?network.at(end).water:sink[end];
  bool main=!tributary&&distance[source]>1200&&flow[source]>=12;
  double width=main?8.:ground[source].height>=105?1.8:2.6;
  std::vector<RiverPoint> nodes;for(int i:path){auto[x,z]=point(i);nodes.push_back({double(x),double(z)});}
  auto curve=riverCurve(nodes,tributary?network.at(end).downstream:RiverPoint{});
  std::vector<double> run(curve.size()),water(curve.size()),radius(curve.size());bool bad=false;
  for(size_t i=0;i<curve.size();++i){
   if(i)run[i]=run[i-1]+length(curve[i].p-curve[i-1].p);
   radius[i]=.85+(width-.85)*smooth(0.,192.,run[i]);
   if(tributary)radius[i]=std::min(radius[i],network.at(end).radius);
   auto p=curve[i].p;
   auto tangent=direction(curve[std::min(i+1,curve.size()-1)].p-curve[i?i-1:i].p);
   auto normal=RiverPoint{-tangent.z,tangent.x}*(radius[i]+2);
   auto s=sample(roundJS(p.x),roundJS(p.z));
   auto left=sample(roundJS(p.x+normal.x),roundJS(p.z+normal.z));
   auto right=sample(roundJS(p.x-normal.x),roundJS(p.z-normal.z));
   if(!eligible(s)||!eligible(left)||!eligible(right)){bad=true;break;}
   water[i]=std::max(receiver,double(std::min({s.height,left.height,right.height})-1));
  }
  if(bad)continue;
  water.back()=receiver;
  // Limit the final descent as well as interior falls; do not manufacture a
  // large last-segment waterfall just to reach a lake or existing tributary.
  for(size_t i=water.size()-1;i>0;--i)water[i-1]=std::min(water[i-1],water[i]+.75*(run[i]-run[i-1]));
  for(size_t i=1;i<water.size();++i)water[i]=std::min(water[i],water[i-1]);
  for(size_t i=0;i<curve.size();++i){auto p=curve[i].p;auto s=sample(roundJS(p.x),roundJS(p.z));
   if(s.height-water[i]>12||water[i]-s.height>2){bad=true;break;}
  }
  if(bad)continue;
  size_t begin=segments.size();
  for(size_t i=1;i<curve.size();++i){auto a=curve[i-1].p,b=curve[i].p;
   segments.push_back({a.x,a.z,b.x,b.z,water[i-1],water[i],radius[i-1],radius[i],run[i-1],run[i],main?1.:0.});
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
 }
 return {{"segments",segments},{"sources",accepted},{"routes",routes}};
}
}
