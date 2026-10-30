#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct HeadwardWaterfallRegression {
 static J ice(J old){
  J next=old;next["config"]["glacierBodyRevision"]=1;Field a(old),b(next);
  auto before=a.surface(30289,30512,false),after=b.surface(30289,30512,false);
  assert(before.superGlacier==0&&before.iceGround==143);
  assert(after.superGlacier==0&&after.glacier==0&&after.iceGround==-1);
  int checked=0;
  for(int z=30448;z<=30576;z+=16)for(int x=30225;x<=30353;x+=16){
   auto s=b.surface(x,z,false);assert(s.superGlacier>0||s.iceGround!=143);++checked;
  }
  b.iceAtlas={{true,0,0,1700,1700,0,0}};b.iceAtlasReady=true;
  Surface core;core.height=220;core.T=.1;core.H=.6;b.glacierSkin(core,32768,32768);
  assert(core.superGlacier>.99&&core.glacier>.99&&core.iceGround==143);
  Surface outside;outside.height=220;outside.T=.1;outside.H=.6;b.glacierSkin(outside,32768+4096,32768);
  assert(outside.glacier==0&&outside.superGlacier==0&&outside.iceGround==-1);
  return {{"beforeBiome",before.biome},{"afterBiome",after.biome},{"height",after.height},{"nonGlacierSamples",checked},{"superGlacierIceRetained",true}};
 }
 static J falls(){
  using namespace regional;J results=J::array();
  for(int drop:{60,100,180}){
   constexpr int count=30;std::array<Ground,count> ground;std::array<int,count> parent,flow;std::array<double,count> distance,sink;
   std::vector<int> order;std::set<int> sources{29};
   auto point=[](int i){return std::pair{900,64*i};};
   auto sample=[&](int x,int z){return Ground{int(96.+z*.024+(z>=850?drop:0)+std::abs(x-900)*.4),.75,.6,0,0};};
   for(int i=0;i<count;++i){auto[x,z]=point(i);ground[i]=sample(x,z);parent[i]=i?i-1:-1;flow[i]=32;distance[i]=i*64;sink[i]=i==0?100:-1;order.push_back(i);}
   auto result=curvedDrainage(0,0,31,order,ground,parent,flow,distance,sink,point,sample,100,511,true,true,J::array(),true,true,&sources,true);
   std::cout<<"drop="<<drop<<" forms="<<result["waterfalls"].size()<<" diagnostics="<<result["diagnostics"]<<std::endl;
   assert(!result["waterfalls"].empty());
   for(auto const&r:result["waterfalls"]){
    double actual=double(r["upper"])-double(r["lower"]);assert(actual>=60&&actual<=180);assert(double(r["radius"])>=6&&double(r["catchment"])>=24);
    WaterfallLandform f{r["lip"][0],r["lip"][1],r["direction"][0],r["direction"][1],r["upper"],r["lower"],r["radius"],r["retreat"],r["runout"]};
    auto under=waterfallSection(f,f.x-f.nx*2,f.z-f.nz*2),pool=waterfallSection(f,f.x+f.nx*3,f.z+f.nz*3);
    assert(under.cutLow>f.lower&&under.cutHigh<=f.upper-7&&under.cutHigh>under.cutLow+20);
    assert(pool.poolDepth>=6&&waterfallSection(f,f.x+f.nz*40,f.z-f.nx*40).poolDepth==0);
   }
   results.push_back({{"requested",drop},{"forms",result["waterfalls"]}});
  }return results;
 }
 static J cliffs(J const&data){
  Field f(data);int grid=data["config"]["cliffs"]["grid"];J sites=J::array();
  for(int gz=(32768-4096)/grid;gz<(32768+4096)/grid;++gz)for(int gx=(32768-4096)/grid;gx<(32768+4096)/grid;++gx){
   auto c=f.patch(gx,gz);if(!c.valid)continue;int X=roundJS(c.x),Z=roundJS(c.z),drop=0;
   for(int z=Z-3;z<=Z+3;++z)for(int x=X-3;x<=X+3;++x)for(auto[dx,dz]:{std::pair{2,0},std::pair{0,2}})
    drop=std::max(drop,std::abs(f.compute(x,z,false).height-f.compute(x+dx,z+dz,false).height));
   if(drop>=100&&drop<=300)sites.push_back({X-32768,Z-32768,drop});
  }assert(sites.size()>=4);return sites;
 }
};}
int main(int argc,char**argv){try{if(argc!=4)return 2;nativegen::J old,next;std::ifstream(argv[1])>>old;std::ifstream(argv[2])>>next;
 nativegen::regional::Store cache;nativegen::regional::StoreScope scope(cache);using T=nativegen::HeadwardWaterfallRegression;
 nativegen::J report;report["ice"]=T::ice(old);std::cout<<report["ice"]<<std::endl;report["waterfalls"]=T::falls();report["cliffs"]=T::cliffs(next);
 std::ofstream(argv[3])<<report.dump(2);std::cout<<report<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
