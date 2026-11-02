#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace nativegen {
struct TerrainConnectionRegression {
 static void geometry(){
  using namespace cliff_transition;auto shape=footprint(64,256,240);
  auto ridge=[](double u){return 200.+u*.25+80.*std::exp(-u*u/400.);};
  assert(ridge(curvedSample(shape,0,0,120,0).displacement)>ridge(120)+40);
  for(int i=-45;i<=45;++i){double u=i*.01;auto s=bankSample(shape,u,0,120,0);
   assert(ridge(u+s.displacement)<=231.); // No compressed intervening summit.
  }
  for(double drop:{100.,200.,300.})for(double amplitude:{20.,80.,144.}){
   auto f=footprint(64,256,drop);
   for(double offset:{-56.,0.,56.})for(double w:{0.,f.core,(f.core+f.end)*.5,f.end}){
    double last=-INFINITY;
    for(int i=0;i<=5140;++i){double u=-257.+i*.1;auto a=bankSample(f,u,w,amplitude,offset);
     assert(u+a.displacement>last);last=u+a.displacement;
     assert(a.weight>=0&&a.weight<=1&&std::abs(a.displacement)<=amplitude);
     if(std::abs(u-offset)>.45){auto old=curvedSample(f,u,w,amplitude,offset);
      assert(a.displacement==old.displacement&&a.weight==old.weight);}
    }
   }
   assert(bankSample(f,4,f.end,amplitude,0).displacement==0);
   assert(std::abs(bankSample(f,4,f.end-.001,amplitude,0).displacement)<1.e-6);
  }
 }
 static double prominence(Field&f,int x,int z){
  double h=f.surface(x,z,false).height,neighbour=-INFINITY;
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz)
   neighbour=std::max(neighbour,double(f.surface(x+dx,z+dz,false).height));
  return h-neighbour;
 }
 struct Stats{int spikes=0;double worst=0;};
 static Stats edgeStats(Field& f,Cliff const&a){
  std::set<std::pair<int,int>> points;
  auto shape=cliff_transition::footprint(64,256,a.drop);
  for(int w=-int(shape.end);w<=int(shape.end);w+=2){
   double u=cliff_transition::traceOffset(a.trace,w);
   int x=roundJS(a.x-a.nz*w+a.nx*u),z=roundJS(a.z+a.nx*w+a.nz*u);
   for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)points.emplace(x+dx,z+dz);
  }
  std::map<std::pair<int,int>,double> cache;
  auto height=[&](int x,int z){auto [it,fresh]=cache.try_emplace({x,z});
   if(fresh){Surface s;double h=f.uncliff(x,z);f.connectedCliffRelief(s,h,x,z);it->second=std::floor(h);}return it->second;};
  Stats stats;for(auto [x,z]:points){double n=-INFINITY;
   for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz)n=std::max(n,height(x+dx,z+dz));
   double spike=height(x,z)-n;stats.spikes+=spike>4;stats.worst=std::max(stats.worst,spike);
  }return stats;
 }
 static int faceDrop(Field&f,Cliff const&a){
  int x=roundJS(a.x),z=roundJS(a.z),drop=0;
  for(int dz=-3;dz<=3;++dz)for(int dx=-3;dx<=3;++dx){auto h=f.compute(x+dx,z+dz,false).height;
   for(auto [ox,oz]:{std::pair{2,0},std::pair{0,2}})drop=std::max(drop,std::abs(h-f.compute(x+dx+ox,z+dz+oz,false).height));}
  return drop;
 }
 static J check(J data){
  geometry();data["config"]["cliffRevision"]=4;J next=data;next["config"]["cliffRevision"]=5;
  Field before(data),after(next);J feedback=J::array(),sites=J::array();
  if(data.at("seedManifest").at("terrainSeed")=="7439727727494036361"&&data["config"]["cliffs"]["chance"]==.52){
   for(auto [X,Z]:{std::pair{2915,-3487},std::pair{2949,-3469}}){
    int x=X+32768,z=Z+32768;double a=prominence(before,x,z),b=prominence(after,x,z);
    assert(a>=20&&b<=1);
    feedback.push_back({{"x",X},{"z",Z},{"beforeHeight",before.surface(x,z,false).height},
     {"afterHeight",after.surface(x,z,false).height},{"beforeProminence",a},{"afterProminence",b}});
   }
  }
  int checked=0,spikesBefore=0,spikesAfter=0,tallBefore=0,tallAfter=0;double worstBefore=0,worstAfter=0;
  for(int gz=80;gz<192&&checked<48;++gz)for(int gx=80;gx<192&&checked<48;++gx){
   auto a=before.patch(gx,gz),b=after.patch(gx,gz);assert(a.valid==b.valid);if(!a.valid)continue;
   assert(a.trace==b.trace&&a.drop==b.drop&&a.x==b.x&&a.z==b.z);
   auto old=edgeStats(before,a),fixed=edgeStats(after,b);int od=faceDrop(before,a),nd=faceDrop(after,b);
   spikesBefore+=old.spikes;spikesAfter+=fixed.spikes;worstBefore=std::max(worstBefore,old.worst);worstAfter=std::max(worstAfter,fixed.worst);
   tallBefore+=od>=100&&od<=300;tallAfter+=nd>=100&&nd<=300;++checked;
   sites.push_back({{"x",a.x-32768},{"z",a.z-32768},{"spikesBefore",old.spikes},{"spikesAfter",fixed.spikes},{"faceBefore",od},{"faceAfter",nd}});
  }
  std::cout<<J{{"checked",checked},{"spikesBefore",spikesBefore},{"spikesAfter",spikesAfter},
   {"worstAfter",worstAfter},{"tallBefore",tallBefore},{"tallAfter",tallAfter}}<<std::endl;
  for(auto const&site:sites)if(site["spikesAfter"]!=0||(site["faceBefore"]>=100&&site["faceBefore"]<=300&&(site["faceAfter"]<100||site["faceAfter"]>300)))std::cout<<site<<std::endl;
  assert(checked==48&&spikesAfter==0&&worstAfter<=4&&tallAfter>=tallBefore);
  return {{"passed",true},{"cliffsChecked",checked},{"spikesBefore",spikesBefore},{"spikesAfter",spikesAfter},
   {"worstBefore",worstBefore},{"worstAfter",worstAfter},{"tallFacesBefore",tallBefore},{"tallFacesAfter",tallAfter},
   {"feedback",feedback},{"sites",sites}};
 }
 static J describe(J const& data) {
  Field f(data);J patches=J::array(),points=J::array();
  for(int gz=111;gz<=117;++gz)for(int gx=136;gx<=142;++gx){
   auto a=f.patch(gx,gz);if(!a.valid)continue;
   J p={{"grid",{gx,gz}},{"center",{a.x-32768,a.z-32768}},
        {"h",a.h},{"normal",{a.nx,a.nz}},{"warp",a.warp},{"drop",a.drop},{"trace",a.trace}};
   J edge=J::array();
   for(int w=-160;w<=160;++w){
    double offset=cliff_transition::traceOffset(a.trace,w);
    double x=a.x-a.nz*w+a.nx*offset,z=a.z+a.nx*w+a.nz*offset;
    double high=f.uncliff(x+a.nx*a.warp,z+a.nz*a.warp),low=f.uncliff(x-a.nx*a.warp,z-a.nz*a.warp);
    edge.push_back({w,x-32768,z-32768,high,low});
   }
   p["edge"]=edge;patches.push_back(p);
  }
  for(auto [x,z]:{std::pair{2915,-3487},std::pair{2949,-3469}}){
   J grid=J::array();
   for(int dz=-5;dz<=5;++dz){J row=J::array();for(int dx=-5;dx<=5;++dx)
     row.push_back(f.surface(x+dx+32768,z+dz+32768,false).height);grid.push_back(row);}
   points.push_back({{"x",x},{"z",z},{"heights",grid}});
  }
  return {{"patches",patches},{"points",points}};
 }
};}
int main(int argc,char**argv){try{if(argc!=3&&argc!=4)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 auto result=argc==4?nativegen::TerrainConnectionRegression::describe(data):nativegen::TerrainConnectionRegression::check(data);
 std::ofstream(argv[2])<<result.dump(2);result.erase("patches");result.erase("sites");std::cout<<result<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
