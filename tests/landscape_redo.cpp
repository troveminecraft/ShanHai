#undef NDEBUG
#include "native/Engine.h"
#include "rules/StructureRandom.h"
#include "rules/StructureReservations.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
namespace nativegen {
struct LandscapeRedoTest {
 static J flora(J const&data){
  Engine e(data);std::array<Surface,256>s;for(auto&a:s){a.height=120;a.H=.7;}
  int moss=0,carpet=0,azalea=0,falseAzalea=0;
  for(int c=0;c<12;++c){std::vector<uint16_t> a(256*576),b;
   for(int i=0;i<256;++i){a[i*576+184]=4;a[i*576+124]=80;}b=a;
   e.surfaceFlora(32768+c*16,32768,a,s);for(int i=0;i<256;++i)b[i*576+124]=2;
   e.surfaceFlora(32768+c*16,32768,b,s);
   for(int i=0;i<256;++i){moss+=a[i*576+184]==80;int v=a[i*576+185];carpet+=v==120;azalea+=v==121||v==122;v=b[i*576+185];falseAzalea+=v==121||v==122;}
   Engine reserved(data,{{32768+c*16,32768,32768+c*16+15,32783}});auto blocked=b;
   for(int i=0;i<256;++i){blocked[i*576+184]=4;blocked[i*576+185]=0;}
   auto prior=blocked;reserved.surfaceFlora(32768+c*16,32768,blocked,s);assert(blocked==prior);
  }
  assert(moss>0&&carpet>0&&azalea>0&&falseAzalea==0);
  assert(worldgen_rules::structureReservationRemoves(122,0,"minecraft:flowering_azalea"));
  assert(!worldgen_rules::structureReservationRemoves(80,4,"minecraft:moss_block"));
  return {{"moss",moss},{"carpet",carpet},{"azaleaAboveLush",azalea},{"azaleaWithoutLush",falseAzalea}};
 }
 static J ores(J const&data){
  Engine e(data);int normal=0,bonus=0;
  std::array<Surface,256>s;for(auto&a:s){a.height=80;a.volcanic=1;}
  for(int n=0;n<64;++n){std::vector<uint16_t> a(256*576),b;
   for(int i=0;i<256;++i)for(int y=-59;y<=80;++y)a[i*576+y+64]=y<0?95:2;b=a;
   for(auto&v:s)v.volcanoHeat=0;e.vanillaOreProfiles(32768+n*16,32768,a,s);
   for(auto&v:s)v.volcanoHeat=1;e.vanillaOreProfiles(32768+n*16,32768,b,s);
   for(size_t i=0;i<a.size();++i){normal+=a[i]==106||a[i]==114;bonus+=b[i]==106||b[i]==114;if(a[i]==106||a[i]==114)assert(a[i]==b[i]);}
  }
  assert(normal>0&&bonus>normal*1.65&&bonus<normal*2.35);
  return {{"normalDiamondBlocks",normal},{"centreDiamondBlocks",bonus},{"ratio",double(bonus)/normal}};
 }
};
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("0.1.13 recipe and report required");
 J old;std::ifstream(argv[1])>>old;J data=old;data["version"]="karma-terrain-2.205.0-native-agent-a";
 data["config"].update(J{{"seaLevel",64},{"landscapeRevision",2},{"volcanoRevision",2},{"surfaceMoss",2},{"villageSpacing",20}});
 Field before(old),after(data);assert(after.seaLevel()==64);J report;
 std::vector<double> aSlope,bSlope;std::array<int,6> tiers{};double gain=0;int count=0,peak=0;
 for(int z=-8192;z<=8192;z+=256)for(int x=-8192;x<=8192;x+=256){
  auto a=before.bootstrapSurface(x+32768,z+32768),b=after.bootstrapSurface(x+32768,z+32768);
  if(a.volcanic||b.volcanic||a.stone||b.stone||a.badlands||b.badlands)continue;
  if(a.height>=40){gain+=b.height-a.height;++count;}
  peak=std::max(peak,b.height);if(b.height>=100)++tiers[b.height<180?0:b.height<260?1:b.height<350?2:b.height<430?3:b.height<480?4:5];
  auto slope=[&](Field&f){return std::hypot(f.bootstrapSurface(x+32784,z+32768).height-f.bootstrapSurface(x+32752,z+32768).height,f.bootstrapSurface(x+32768,z+32784).height-f.bootstrapSurface(x+32768,z+32752).height)/32.;};
  if(a.height>=80&&b.height>=80){aSlope.push_back(slope(before));bSlope.push_back(slope(after));}
 }
 std::sort(aSlope.begin(),aSlope.end());std::sort(bSlope.begin(),bSlope.end());
 auto p95=[](auto const&v){return v[size_t(v.size()*.95)];};
 report["relief"]={{"landMeanRise",gain/count},{"peak",peak},{"heightBands",tiers},{"oldSlopeP95",p95(aSlope)},{"newSlopeP95",p95(bSlope)}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<std::endl;
 assert(gain/count>0&&peak>=430&&peak<512&&p95(bSlope)<p95(aSlope));for(int i=0;i<5;++i)assert(tiers[i]>0);
 int deep=0,air=0,lava=0,edgeSoil=0;J examples=J::array();
 auto volcanoes=after.inspectVolcanoes(-8192,8192);assert(!volcanoes.empty());
 for(auto const&v:volcanoes){int X=v.at("x"),Z=v.at("z");
  for(int dz=-90;dz<=90;dz+=6)for(int dx=-90;dx<=90;dx+=6){auto s=after.surface(X+dx+32768,Z+dz+32768,false);
   for(int y=-56;y<50;++y){int b=after.material(s,X+dx+32768,y,Z+dz+32768);deep+=b==95;air+=b==0;lava+=b==43;}
  }
  auto centre=after.surface(X+32768,Z+32768);assert(after.material(centre,X+32768,-56,Z+32768)!=43);
  double outer=v.at("outer");for(int i=0;i<32;++i){double a=i*6.283185307179586/32;int x=X+int((outer+190)*std::cos(a))+32768,z=Z+int((outer+190)*std::sin(a))+32768;auto s=after.surface(x,z);assert(s.barren==0);++edgeSoil;}
  examples.push_back(v);if(examples.size()==2)break;
 }
 assert(deep>0&&air>0&&lava>0);report["volcano"]={{"deepslateSamples",deep},{"airSamples",air},{"lavaSamples",lava},{"haloRestored",edgeSoil},{"examples",examples}};
 report["flora"]=LandscapeRedoTest::flora(data);report["diamond"]=LandscapeRedoTest::ores(data);
 int oldN=0,newN=0;for(int z=-100;z<=100;++z)for(int x=-100;x<=100;++x){for(int spacing:{32,20}){auto p=worldgen_rules::candidateForCell(x,z,15,spacing,spacing==20?16:8,worldgen_rules::VillageSalt);if(p.x>=-640&&p.x<640&&p.z>=-640&&p.z<640)(spacing==20?newN:oldN)++;}}
 assert(double(newN)/oldN==2.56);report["village"]={{"oldCandidates",oldN},{"newCandidates",newN},{"ratio",double(newN)/oldN},{"minimumAnchorDistanceBlocks",272}};
 report["gameplayTested"]=false;std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
