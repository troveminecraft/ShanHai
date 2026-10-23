#undef NDEBUG
#include "native/Field.h"
#include "rules/StructureRules.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;

static void checkDrainage(J const&plan,int sea){
 auto const&segments=plan.at("segments");
 for(auto const&s:segments){
  assert(double(s[4])>=double(s[5])&&double(s[5])>=sea);
  assert(std::hypot(double(s[0])-double(s[2]),double(s[1])-double(s[3]))<=6.00001);
 }
 for(auto const&r:plan.at("routes")){
  size_t begin=r.at("firstSegment"),count=r.at("segmentCount");assert(count>0);
  for(size_t i=begin+1;i<begin+count;++i){auto const&a=segments[i-1];auto const&b=segments[i];assert(a[2]==b[0]&&a[3]==b[1]&&a[5]==b[4]);}
  auto const&end=segments[begin+count-1];
  if(r.at("receiver")=="ocean")assert(double(end[5])==sea);
  if(r.at("receiver")=="river"){
   bool joined=false;for(size_t i=0;i<begin;++i){auto const&s=segments[i];joined|=(s[0]==end[2]&&s[1]==end[3]&&s[4]==end[5])||(s[2]==end[2]&&s[3]==end[3]&&s[5]==end[5]);}assert(joined);
  }
 }
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe and report required");
 Surface shore;shore.seaLevel=40;shore.height=48;shore.biome=3;
 assert(worldgen_rules::extraAnchorAllowed(4,shore,false));shore.seaLevel=31;assert(!worldgen_rules::extraAnchorAllowed(4,shore,false));
 Surface marine;marine.seaLevel=40;marine.height=30;marine.water=40;marine.ocean=1;
 assert(worldgen_rules::extraAnchorAllowed(2,marine,false));marine.seaLevel=31;assert(!worldgen_rules::extraAnchorAllowed(2,marine,false));
 J data;std::ifstream(argv[1])>>data;
 auto explicitOld=data;explicitOld["config"]["seaLevel"]=31;auto implicitOld=explicitOld;implicitOld["config"].erase("seaLevel");
 // Compare the public surface/material paths, including absent-key legacy
 // recipes. Separate Fields ensure neither comparison reuses the other's cache.
 explicitOld.erase("regionalGeneration");implicitOld.erase("regionalGeneration");
 Field old(explicitOld),missing(implicitOld);assert(old.seaLevel()==31&&missing.seaLevel()==31);
 int legacyColumns=0;
 for(int z=-2048;z<=2048;z+=256)for(int x=-2048;x<=2048;x+=256){
  int X=x+32768,Z=z+32768;auto a=old.surface(X,Z),b=missing.surface(X,Z);
  assert(a.height==b.height&&a.water==b.water&&a.biome==b.biome&&a.surf==b.surf&&a.treeD==b.treeD);
  for(int y=-64;y<=std::max(a.height,a.water)+1;++y)assert(old.material(a,X,y,Z)==missing.material(b,X,y,Z));++legacyColumns;
 }
 auto raised=explicitOld;raised["config"]["seaLevel"]=40;Field sea(raised);assert(sea.seaLevel()==40);
 int oceans=0;
 for(int z=-4096;z<=4096;z+=256)for(int x=-4096;x<=4096;x+=256){
  int X=x+32768,Z=z+32768;auto s=sea.surface(X,Z,false);
  if(s.water!=40||s.height>=40||s.lake||s.river||s.stone||s.volcanic)continue;
  assert(s.biome==0||s.biome==1||s.biome==2||s.biome==21||s.biome==22);
  int top=sea.material(s,X,40,Z);assert(top==8||top==9);assert(sea.material(s,X,41,Z)==0);++oceans;
 }assert(oceans>10);
 J island;
 for(int radius=0;radius<=18&&island.is_null();++radius)for(int dz=-radius;dz<=radius&&island.is_null();++dz)for(int dx=-radius;dx<=radius;++dx){
  if(std::max(std::abs(dx),std::abs(dz))!=radius)continue;auto a=sea.inspectMushroomIsland(21+dx,21+dz);if(a.at("valid").get<bool>()){island=a;break;}
 }
 assert(!island.is_null());int islandX=island.at("x"),islandZ=island.at("z"),radius=island.at("radius"),islandLand=0,islandEdges=0;
 for(int z=islandZ-radius-12;z<=islandZ+radius+12;++z)for(int x=islandX-radius-12;x<=islandX+radius+12;++x){
  auto s=sea.surface(x+32768,z+32768,false);if(!s.mushroom)continue;++islandLand;assert(s.height>=40&&s.water<s.height);
  for(auto[dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){auto q=sea.surface(x+dx+32768,z+dz+32768,false);if(q.mushroom)continue;++islandEdges;assert(q.water==40&&q.height<40);assert(q.biome==0||q.biome==1||q.biome==2||q.biome==21||q.biome==22);}
 }assert(islandLand>1000&&islandEdges>100);
 using namespace nativegen::regional;
 auto slope=[](int x,int z){return Ground{int(std::floor(28.+(z+512)*.08+std::abs(x-900)*.005)),.65,.55,0,0};};
 auto defaultPlan=drainage(0,0,7,slope,true),explicitPlan=drainage(0,0,7,slope,true,31,319);
 assert(defaultPlan==explicitPlan);auto newPlan=drainage(0,0,7,slope,true,40,511);checkDrainage(newPlan,40);
 assert(!newPlan.at("routes").empty());
 auto highSlope=[](int x,int z){return Ground{int(std::floor(28.+(z+512)*.16+std::abs(x-900)*.005)),.65,.55,0,0};};
 auto highPlan=drainage(0,0,7,highSlope,true,40,511);checkDrainage(highPlan,40);int highestSource=0;
 for(auto const&r:highPlan.at("routes")){int x=r["source"][0],z=r["source"][1];highestSource=std::max(highestSource,highSlope(x,z).height);}assert(highestSource>245);
 auto oldIdentity=data,newIdentity=data;oldIdentity["config"]["seaLevel"]=31;newIdentity["config"]["seaLevel"]=40;
 if(data.contains("regionalGeneration"))assert(identity(oldIdentity)!=identity(newIdentity));
 J report{{"seaLevel",40},{"legacyColumnsCompared",legacyColumns},{"oceanColumnsChecked",oceans},{"island",island},{"islandLandColumns",islandLand},{"islandOceanEdges",islandEdges},{"riverRoutes",newPlan["routes"].size()},{"mountainRoutes",highPlan["routes"].size()},{"highestSyntheticSource",highestSource},{"legacyDrainageExact",true},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
