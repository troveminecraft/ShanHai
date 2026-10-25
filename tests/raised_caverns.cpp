#include "native/CavernGroups.h"
#include <iostream>
#include <limits>

namespace {
using Caves=nativegen::CavernGroups;
void require(bool ok,char const*message){if(!ok)throw std::runtime_error(message);}
template<class Height> uint64_t legacyFingerprint(Caves&caves,Height height){
 uint64_t hash=14695981039346656037ULL;
 for(int z=-704;z<704;z+=11)for(int x=-704;x<704;x+=11){
  auto const&c=caves.column(x,z,height);
  require((c[0]&255ULL)==0,"protected layers below-56 were carved");
  for(int w=0;w<6;++w){hash^=c[w];hash*=1099511628211ULL;}
  require(c[6]==0&&c[7]==0&&c[8]==0,"legacy profile carved above319");
 }
 return hash;
}
template<class Height> std::array<int,3> crossing(Caves&caves,nativegen::J const&plan,int y,Height height,bool both){
 for(auto const&group:plan)for(auto const&s:group.at("passages")){
  auto const&a=s.at("a");auto const&b=s.at("b");double ay=a[1],by=b[1];
  if(std::min(ay,by)>y||std::max(ay,by)<y||std::abs(ay-by)<.001)continue;
  double t=(y-ay)/(by-ay);
  int x=nativegen::roundJS(nativegen::mix(t,a[0].get<double>(),b[0].get<double>()));
  int z=nativegen::roundJS(nativegen::mix(t,a[2].get<double>(),b[2].get<double>()));
  for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx)
   if(caves.contains(x+dx,y,z+dz,height)&&(!both||caves.contains(x+dx,y-1,z+dz,height)))return {x+dx,y,z+dz};
 }
 throw std::runtime_error("expected passage did not cross height boundary");
}
}

int main(){try{
 require(Caves(13).maxYExclusive()==320,"default changed legacy ceiling");
 bool rejected=false;try{Caves invalid(13,false,false,false,false,513);}catch(std::invalid_argument const&){rejected=true;}
 require(rejected,"unsupported height accepted");

 // Recorded from commit 513b7d6's six-word mask, with the same precise-FP
 // compiler settings. Each fixture samples 16384 columns over four groups,
 // cycling far beyond the 64-entry mask cache. The raised terrain fixtures
 // also preserve the old recipe's historical clipping intentionally.
 struct Fixture{double ground;uint64_t hash;};
 for(auto f:{Fixture{140.,6922487944571162906ULL},Fixture{480.,9124389268700542034ULL},Fixture{640.,13188267371156953599ULL}}){
  Caves legacy(13,true,true,true,true);auto height=[&](int,int){return f.ground;};
  require(legacyFingerprint(legacy,height)==f.hash,"legacy six-word geometry changed");
 }

 auto low=[](int,int){return 140.;};
 Caves lowOld(13,true,true,true,true),lowNew(13,true,true,true,true,512);
 for(int z=-704;z<704;z+=17)for(int x=-704;x<704;x+=17)
  require(lowOld.column(x,z,low)==lowNew.column(x,z,low),"raising ceiling changed low terrain caves");

 auto mountain=[](int,int){return 480.;};
 Caves old(13,true,true,true,true),raised(13,true,true,true,true,512);
 auto plan=raised.inventory(-704,703,mountain,true);
 require(plan==old.inventory(-704,703,mountain,true),"raising ceiling changed cave routes or random choices");
 auto seam=crossing(raised,plan,320,mountain,true);
 require(!old.contains(seam[0],320,seam[2],mountain),"legacy seam fixture unexpectedly open");
 require(raised.contains(seam[0],319,seam[2],mountain),"raised seam bottom closed");

 // An exaggerated terrain envelope intentionally drives a planned passage
 // through the final representable bit. It catches 511/512 and word 8 errors
 // even though production mountain peaks leave a protective roof below 511.
 auto overTop=[](int,int){return 640.;};
 Caves ceiling(13,true,true,true,true,512);
 auto top=crossing(ceiling,ceiling.inventory(-704,703,overTop,true),511,overTop,false);
 require((ceiling.column(top[0],top[2],overTop)[8]&(uint64_t(1)<<63))!=0,"last mask bit not carved");
 for(int y:{-65,-64,-57,512,513,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
  require(!ceiling.contains(top[0],y,top[2],overTop),"out-of-height query escaped guard");

 // Revisit a nonempty upper cave after eviction of both its active column
 // and its chunk mask; the bounded cache must not change generated geometry.
 Caves::Column expected=raised.column(seam[0],seam[2],mountain);
 for(int z=-352;z<=352;z+=32)for(int x=-352;x<=352;x+=32)raised.column(x,z,mountain);
 require(expected==raised.column(seam[0],seam[2],mountain),"upper mask changed after cache eviction");

 std::cout<<nativegen::J{{"legacyFixtures",3},{"legacyColumnsPerFixture",16384},{"legacyGoldenMatches",true},
  {"lowTerrainUnchanged",true},{"routesUnchanged",true},{"crossing319to320",seam},{"lastBit511",top},
  {"outOfRangeRejected",true},{"cacheEvictionStable",true}}.dump()<<std::endl;
 return 0;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
