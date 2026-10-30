#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct UplandTerrainRegression {
 static J rivers(){
  using namespace regional;
  auto valley=[](int x,int z){return Ground{int(96+(z+512)*.05+60*smooth(884.,888.,double(z))+std::abs(x-900)*.002),.7,.5,0,0};};
  auto old=drainage(0,0,7,valley,true,100,511,true,true,true);
  auto next=drainage(0,0,7,valley,true,100,511,true,true,true,true);
  auto upland=[&](J const&p){int count=0;for(auto const&s:p.at("sources"))count+=valley(s[0],s[1]).height>=200;return count;};
  int falls=0;double maxFall=0;
  for(auto const&s:next.at("segments")){
   double drop=double(s[4])-double(s[5]);assert(drop>=0&&double(s[5])>=100);maxFall=std::max(maxFall,drop);
   assert(std::hypot(double(s[0])-double(s[2]),double(s[1])-double(s[3]))<=6.00001);
   falls+=double(s[11])>0&&drop>=6;
   assert(valley(roundJS(s[0]),roundJS(s[1])).height-double(s[4])<=12);
  }
  for(auto const&r:next.at("routes")){
   size_t begin=r.at("firstSegment"),n=r.at("segmentCount");auto const&ss=next.at("segments");
   for(size_t i=begin+1;i<begin+n;++i){assert(ss[i-1][2]==ss[i][0]&&ss[i-1][3]==ss[i][1]&&ss[i-1][5]==ss[i][4]);}
   auto const&e=ss[begin+n-1];
   if(r.at("receiver")=="ocean")assert(valley(roundJS(e[2]),roundJS(e[3])).height<100&&e[5]==100);
   if(r.at("receiver")=="river"){
    bool match=false;for(size_t i=0;i<begin;++i)match|=(e[2]==ss[i][0]&&e[3]==ss[i][1]&&e[5]==ss[i][4])||(e[2]==ss[i][2]&&e[3]==ss[i][3]&&e[5]==ss[i][5]);assert(match);
   }
  }
  J report={{"oldUplandSources",upland(old)},{"newUplandSources",upland(next)},{"falls",falls},{"maximumDrop",maxFall}};
  std::cout<<report.dump()<<std::endl;
  assert(upland(next)>upland(old)&&falls>0);
  for(bool lava:{false,true}){
   auto dry=drainage(0,0,7,[&](int,int){return Ground{240,lava?.7:.1,.5,0,lava?1.:0.};},true,100,511,true,true,true,true);
   assert(dry.at("segments").empty());
  }
  return report;
 }
 static J climate(J old,J next){
  Field before(old),after(next);
  for(auto*f:{&before,&after}){f->iceAtlas={{true,0,0,1600,1600,0,0}};f->iceAtlasReady=true;}
  J levels=J::array(),halo=J::array();
  for(int y:{120,150,180,240,300,360,420,470}){
   Surface a;a.height=y;a.T=.1;auto b=a;before.glacierSkin(a,32768,32768);after.glacierSkin(b,32768,32768);
   if(y>=180&&y<=360)assert(a.superGlacier==0&&b.superGlacier>.99);
   levels.push_back({y,a.superGlacier,b.superGlacier});
  }
  double previous=0;
  for(int distance:{-256,0,128,256,384,511,512,768,1024}){
   Surface a;a.height=240;a.T=.7;auto b=a;before.applySnowVariant(a,32768+1728+distance,32768);after.applySnowVariant(b,32768+1728+distance,32768);
   assert(b.glacierVegetation>=previous);previous=b.glacierVegetation;
   if(distance>=512)assert(b.glacierSnow==0&&b.glacierVegetation==1);
   if(distance<=256)assert(b.glacierSnow==1);
   if(distance==768)assert(a.glacierSnow>0);
   halo.push_back({distance,a.glacierSnow,b.glacierSnow,b.glacierVegetation});
  }
  return {{"altitudes",levels},{"halo",halo}};
 }
 static J cliffs(J old,J next){
  Field before(old),after(next);int a=0,b=0,high=0;J sites=J::array();
  for(int z=224;z<288;++z)for(int x=224;x<288;++x){auto A=before.patch(x,z),B=after.patch(x,z);a+=A.valid;b+=B.valid;
   if(B.valid&&B.h>260){++high;if(sites.size()<5)sites.push_back({B.x-32768,B.z-32768,B.h});}
  }
  assert(b>a&&high>0);return {{"oldCandidates",a},{"newCandidates",b},{"newAbove260",high},{"examples",sites}};
 }
 static J ice(J data){
  Field field(data);Engine engine(data,{},false,&field);J sites=J::array();
  for(auto const&r:field.inspectIceRegions()){
   int cx=int(r.at("x").get<double>()/96),cz=int(r.at("z").get<double>()/96);
   for(int gz=cz-9;gz<=cz+9&&sites.size()<4;++gz)for(int gx=cx-9;gx<=cx+9&&sites.size()<4;++gx){
    auto random=[&](int salt){return hash01(gx,gz,engine.terrainSeed^salt);};
    int x=int(std::floor((gx+.18+.64*random(51301))*96)),z=int(std::floor((gz+.18+.64*random(51303))*96));
    auto s=field.surface(x+32768,z+32768,false);if(s.height<180||s.height>320||s.superGlacier<=.65)continue;
    auto d=engine.iceSpireDescriptor(gx,gz,x,z);if(!d.valid)continue;
    double lean=std::hypot(d.ax,d.az);assert(d.height>=88&&d.height<=159&&lean>=d.height*.4&&lean<=d.height*.7&&d.base+d.height<=502);
    double radius=*std::max_element(d.distance.begin(),d.distance.end());assert(radius<=20);
    sites.push_back({{"x",x},{"z",z},{"base",d.base},{"height",d.height},{"lean",lean},{"radius",radius}});
   }if(sites.size()==4)break;
  }
  std::cout<<J{{"uplandIceSpires",sites}}.dump()<<std::endl;assert(sites.size()==4);return sites;
 }
 static J waterWorld(J data){
  Field field(data);J regions=J::array(),falls=J::array();int streams=0;
  for(int z=-2;z<=1;++z)for(int x=-2;x<=1;++x){
   auto p=field.inspectRegionalDrainage(x,z);int high=0,count=0;
   for(auto const&s:p.at("segments")){
    high+=double(s[4])>=180;
    if(s.size()<=11||double(s[11])==0||double(s[4])-double(s[5])<6)continue;
    ++count;if(falls.size()>=12)continue;
    int X=roundJS((double(s[0])+double(s[2]))*.5),Z=roundJS((double(s[1])+double(s[3]))*.5);
    int wet=0,connected=0;double maxDrop=0;
    for(int dz=-8;dz<=8;++dz)for(int dx=-8;dx<=8;++dx){
     auto a=field.surface(X+dx+32768,Z+dz+32768,false);if(a.river<=0)continue;
     auto natural=field.compute(X+dx+32768,Z+dz+32768,false);
     assert(a.water<=natural.height+2);++wet;
     for(auto [nx,nz]:{std::pair{1,0},std::pair{0,1}}){
      auto b=field.surface(X+dx+nx+32768,Z+dz+nz+32768,false);if(b.river<=0)continue;
      int level=std::min(a.water,b.water);
      assert(a.height<level&&b.height<level);++connected;
      maxDrop=std::max(maxDrop,double(std::abs(a.water-b.water)));
     }
    }
    falls.push_back({{"x",X},{"z",Z},{"profileDrop",double(s[4])-double(s[5])},{"wetColumns",wet},{"waterConnections",connected},{"maxVoxelStep",maxDrop}});
   }
   streams+=high;regions.push_back({{"region",{x,z}},{"uplandSegments",high},{"falls",count}});
   std::cerr<<"water owner "<<x<<","<<z<<" high="<<high<<" falls="<<count<<" routes="<<p.at("sources").size()<<" "<<p.at("diagnostics").dump()<<'\n';
  }
  assert(streams>0&&!falls.empty());return {{"regions",regions},{"waterfalls",falls}};
 }
};}
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=5)return 2;nativegen::J old,next;std::ifstream(argv[1])>>old;std::ifstream(argv[2])>>next;
 nativegen::regional::Store cache;nativegen::regional::StoreScope scope(cache);
 nativegen::regional::storage(next,std::filesystem::path(argv[3]).parent_path()/(std::filesystem::path(argv[3]).stem().string()+"-cache"));
 using T=nativegen::UplandTerrainRegression;nativegen::J out;
 if(argc==5){out=T::waterWorld(next);std::ofstream(argv[3])<<out.dump(2);return 0;}
 out["rivers"]=T::rivers();out["climate"]=T::climate(old,next);out["cliffs"]=T::cliffs(old,next);
 std::cout<<out.dump()<<std::endl;out["iceSpires"]=T::ice(next);
 std::ofstream(argv[3])<<out.dump(2);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
