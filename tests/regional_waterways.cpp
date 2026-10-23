#undef NDEBUG
#include "native/Field.h"
#include "WorldGenerationMode.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
using namespace nativegen::regional;
int main(int argc,char**argv){std::string stage="geometry";try{
 // A grid right angle must have continuous tangents and interpolate the join.
 auto curve=riverCurve({{0,0},{64,0},{64,64},{128,64}});
 int joins=0;double maxTurn=0;
 for(size_t i=1;i<curve.size();++i){
  assert(length(curve[i].p-curve[i-1].p)<=6.00001);
  if(i+1<curve.size()){
   auto a=direction(curve[i].p-curve[i-1].p),b=direction(curve[i+1].p-curve[i].p);
   maxTurn=std::max(maxTurn,std::acos(clamp(a.x*b.x+a.z*b.z,-1.,1.)));
  }
  if(curve[i].node==1){assert(curve[i].p.x==64&&curve[i].p.z==0);++joins;}
 }
 assert(joins==1&&maxTurn<.35);
 assert(riverCurve({{0,0}}).empty());
 auto line=riverCurve({{0,0},{64,0}});for(auto const&p:line)assert(p.p.z==0&&p.p.x>=0&&p.p.x<=64);
 // Synthetic sloping catchment exercises actual branch acceptance without
 // relying on one lucky world seed. Dry/volcanic terrain must not acquire water.
 auto slope=[](int x,int z){return Ground{int(std::floor(28.+(z+512)*.08+std::abs(x-900)*.005)),.65,.55,0,0};};
 auto graph=drainage(0,0,7,slope,true);int tributaries=0;
 for(auto const&r:graph.at("routes"))tributaries+=r.at("receiver")=="river";
 assert(tributaries>0);
 auto dry=drainage(0,0,7,[](int,int){return Ground{100,.1,.6,0,0};},true);assert(dry.at("segments").empty());
 auto lava=drainage(0,0,7,[](int,int z){return Ground{20+z/20,.7,.6,0,1};},true);assert(lava.at("segments").empty());
 auto check=[](J const&plan){
  auto const&segments=plan.at("segments");
  for(auto const&s:segments){
   for(auto const&v:s)assert(std::isfinite(v.get<double>()));
   assert(double(s[4])>=double(s[5])&&double(s[5])>=31);
   assert(double(s[6])>=.65&&double(s[7])>=.65&&double(s[6])<=8&&double(s[7])<=8);
   assert(std::hypot(double(s[0])-double(s[2]),double(s[1])-double(s[3]))<=6.00001);
   assert(double(s[9])>=double(s[8]));
  }
  for(auto const&r:plan.at("routes")){
   size_t begin=r.at("firstSegment"),count=r.at("segmentCount");assert(count>0&&begin+count<=segments.size());
   for(size_t i=begin+1;i<begin+count;++i){auto const&a=segments[i-1];auto const&b=segments[i];assert(a[2]==b[0]&&a[3]==b[1]&&a[5]==b[4]&&a[7]==b[6]&&a[9]==b[8]);}
   if(r.at("receiver")=="river"){
    auto const&end=segments[begin+count-1];bool matched=false;
    for(size_t i=0;i<begin;++i){auto const&s=segments[i];matched|=(end[2]==s[0]&&end[3]==s[1]&&end[5]==s[4])||(end[2]==s[2]&&end[3]==s[3]&&end[5]==s[5]);}
    assert(matched);
   }
  }
 };
 check(graph);
 if(argc!=3)throw std::runtime_error("expected recipe and report path");
 J data;std::ifstream(argv[1])>>data;auto old=data;old["regionalGeneration"]["algorithm"]="regional-drainage-v1";
 assert(identity(data)!=identity(old));
 auto profile=J{{"boundsMode","infinite"}};assert(terrain_runtime::WorldGenerationMode::read(profile,data).unbounded);
 assert(terrain_runtime::WorldGenerationMode::read(profile,old).unbounded);
 Field current(data),previous(old);J reports=J::array();size_t newSources=0,oldSources=0,wetSamples=0;
 for(auto[gx,gz]:{std::pair{0,0},{-1,0},{0,-1},{-1,-1}}){
  stage="region "+std::to_string(gx)+","+std::to_string(gz);
  auto plan=current.inspectRegionalDrainage(gx,gz),before=previous.inspectRegionalDrainage(gx,gz);check(plan);
  assert(current.inspectRegionalDrainage(gx,gz)==plan);
  newSources+=plan.at("sources").size();oldSources+=before.at("sources").size();
  for(auto const&r:plan.at("routes")){
   size_t begin=r.at("firstSegment"),count=r.at("segmentCount");
   for(size_t i=begin;i<begin+count;i+=8){auto const&s=plan["segments"][i];int x=roundJS(s[0])+32768,z=roundJS(s[1])+32768;
    stage="surface "+std::to_string(x-32768)+","+std::to_string(z-32768);
    auto actual=current.surface(x,z,false);assert(actual.river>0||actual.lake||actual.water>=actual.height);++wetSamples;
   }
  }
  reports.push_back({{"region",{gx,gz}},{"before",before},{"after",plan}});
 }
 // Measure the same seeded stone-forest site with old and new dimensions.
 // Vertical growth stays unchanged; widening must retain separated pillars.
 stage="stone forest";auto stoneData=data;stoneData.erase("regionalGeneration");auto smallData=stoneData;
 auto&q=smallData["config"]["stoneForest"];q["smallRegionInner"]=72;q["smallRegionOuter"]=112;
 q["smallRadiusMin"]=16;q["smallRadiusMax"]=22;q.erase("smallPillarPitch");
 Field larger(stoneData),smaller(smallData);auto sites=larger.inspectSmallStoneForests(-40000,40000);
 assert(!sites.empty());int sx=sites[0]["x"],sz=sites[0]["z"],oldArea=0,newArea=0,oldPeaks=0,newPeaks=0;
 J stoneRows=J::array();
 for(int z=sz-176;z<=sz+176;z+=4)for(int x=sx-176;x<=sx+176;x+=4){
  auto a=smaller.base(x+32768,z+32768),b=larger.base(x+32768,z+32768);
  oldArea+=a.stone>.01;newArea+=b.stone>.01;oldPeaks+=a.pillar>.5;newPeaks+=b.pillar>.5;
  assert(b.height>=-61&&b.height<=319);stoneRows.push_back({x,z,a.height,b.height,a.stone,b.stone});
 }
 assert(newArea>oldArea*1.7&&newPeaks>oldPeaks*1.3);
 J stone={{"center",{sx,sz}},{"oldArea",oldArea*16},{"newArea",newArea*16},{"oldPeakArea",oldPeaks*16},{"newPeakArea",newPeaks*16},{"rows",stoneRows}};
 J report={{"seed",data["seedManifest"]["terrainSeed"]},{"oldSources",oldSources},{"newSources",newSources},{"wetSamples",wetSamples},{"stoneForest",stone},
           {"curveMaximumTurnDegrees",maxTurn*180/3.141592653589793},{"syntheticTributaries",tributaries},{"regions",reports}};
 std::ofstream(argv[2])<<report.dump();report.erase("regions");report["stoneForest"].erase("rows");std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<stage<<": "<<e.what()<<'\n';return 1;}}
