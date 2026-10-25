#undef NDEBUG
#include "native/Engine.h"
#include "rules/SmallFeaturePlan.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
using namespace nativegen;
namespace hm=high_mountain_underground;

Surface mountain(int h=480){Surface s;s.height=h;s.mount=1;s.biome=5;s.H=.8;return s;}
std::vector<uint16_t> stoneVolume(int height=576){return std::vector<uint16_t>(256*height,2);}
void chamber(std::vector<uint16_t>&raw,int lo,int hi){for(int i=0;i<256;++i)for(int y=lo;y<=hi;++y)raw.at(i*576+y+64)=0;}
int countUpper(std::vector<uint16_t>const&raw,int first,int last){int n=0;for(int i=0;i<256;++i)for(int y=321;y<=480;++y){int b=raw.at(i*576+y+64);n+=b>=first&&b<=last;}return n;}
std::vector<uint16_t> decode(std::vector<uint8_t>const&bytes){
 size_t pos=260;std::vector<uint16_t> raw;
 auto word=[&]{assert(pos+2<=bytes.size());int n=bytes[pos]|(bytes[pos+1]<<8);pos+=2;return n;};
 for(int i=0;i<256;++i){int runs=word(),height=0;for(int r=0;r<runs;++r){int n=word(),b=word();assert(n>0);height+=n;raw.insert(raw.end(),n,uint16_t(b));}assert(height==576);}
 assert(pos==bytes.size());return raw;
}

namespace nativegen {
struct EngineBoundaryTest {
 static J run(Engine&old,Engine&now){
  J out;std::array<Surface,256> surfaces;surfaces.fill(mountain());
  int geology=0,ecology=0;
  for(int n=0;n<32&&(geology==0||ecology==0);++n){
   int x0=32768+n*32,z0=32768+n*16;
   auto a=stoneVolume(),b=a;chamber(a,350,364);chamber(b,350,364);
   old.undergroundEcology(x0,z0,a,surfaces);now.undergroundEcology(x0,z0,b,surfaces);
   for(int i=0;i<256;++i)for(int y=-64;y<=120;++y)assert(a.at(i*576+y+64)==b.at(i*576+y+64));
   assert(countUpper(a,95,99)==0&&countUpper(a,116,125)==0);
   geology+=countUpper(b,97,99);ecology+=countUpper(b,116,125)+countUpper(b,80,80)+countUpper(b,27,27);
   assert(countUpper(b,95,96)==0); // Deep strata must not be copied upwards.
  }
  assert(geology>0&&ecology>0);out["upperRockBlocks"]=geology;out["upperCaveDecorationBlocks"]=ecology;
  // A mountain chamber crossing the former Y120 scan cap was skipped in its
  // entirety. Completing that chamber intentionally also decorates its lower
  // floor; preservation applies to lowland columns, not a sliced-off chamber.
  int crossingChanges=0;
  for(int n=0;n<32&&!crossingChanges;++n){auto a=stoneVolume(),b=a;chamber(a,100,140);chamber(b,100,140);
   old.undergroundEcology(32768+n*32,32768+n*16,a,surfaces);now.undergroundEcology(32768+n*32,32768+n*16,b,surfaces);
   for(int i=0;i<256;++i)for(int y=90;y<=120;++y)crossingChanges+=a.at(i*576+y+64)!=b.at(i*576+y+64);
  }
  assert(crossingChanges>0);out["cross120ChamberCompletedLowerDecorationChanges"]=crossingChanges;
  // Lowland output is byte-identical with this new flag alone enabled.
  surfaces.fill(mountain(100));surfaces[0].mount=0;
  auto a=stoneVolume(),b=a;chamber(a,50,62);chamber(b,50,62);
  old.undergroundEcology(32768,32768,a,surfaces);now.undergroundEcology(32768,32768,b,surfaces);assert(a==b);
  out["lowlandEcologyUnchanged"]=true;
  surfaces.fill(mountain());
  // Find a deterministic ownership cell, then exercise the production pool
  // pass against actual raw blocks, including full output equality when off.
  bool pooled=false;
  for(int gz=-4;gz<=4&&!pooled;++gz)for(int gx=-4;gx<=4&&!pooled;++gx){
   auto r=[&](int k){return hash01(gx,gz,now.terrainSeed^(19800+k));};
   if(r(0)>.28||r(6)>.36)continue;
   int x=gx*96+int(r(1)*6)*16+5+int(r(2)*6),z=gz*96+int(r(3)*6)*16+5+int(r(4)*6);
   int x0=int(std::floor(x/16.))*16,z0=int(std::floor(z/16.))*16;
   auto dry=stoneVolume();chamber(dry,350,362);a=dry;b=dry;
   old.smallUndergroundPockets(x0,z0,a,surfaces);now.smallUndergroundPockets(x0,z0,b,surfaces);
   assert(a==dry);int wet=countUpper(b,8,8);assert(wet>=9&&wet<=25);
   for(int i=0;i<256;++i)for(int y=321;y<480;++y)if(b.at(i*576+y+64)==8){
    assert(y==349&&b.at(i*576+y+63)==2&&b.at(i*576+y+62)==2);
   }
   out["productionUpperPoolBlocks"]=wet;out["upperPoolInternalAnchor"]={x,350,z};pooled=true;
  }
  assert(pooled);
  auto snow=mountain();assert(!old.vanillaOreCondition(1,snow,0)&&now.vanillaOreCondition(1,snow,0));
  for(int mode=0;mode<4;++mode){auto excluded=snow;if(mode==0)excluded.glacier=.1;if(mode==1)excluded.iceHabitat=.1;if(mode==2)excluded.volcanic=.1;if(mode==3)excluded.height=100;assert(!now.vanillaOreCondition(1,excluded,0));}
  int emerald=0;
  for(int n=0;n<4&&!emerald;++n){b=stoneVolume();now.vanillaOreProfiles(32768+n*16,32768,b,surfaces);emerald+=countUpper(b,107,107);}
  assert(emerald>0);out["productionUpperSnowMountainEmeraldBlocks"]=emerald;
  // Exercise production publication for a validated, cross-chunk high cave.
  // The injected plan isolates shell/ownership correctness from terrain-site
  // frequency; this is not a claim that a natural game site was visited.
  bool published=false;
  for(int gx=0;gx<64&&!published;++gx){int gz=0;
   auto r=[&](int k){return hash01(gx,gz,seedHash(now.terrainSeed,56100+k));};
   if(r(0)>.14)continue;
   int x=gx*128+32+int(r(1)*64),z=32+int(r(2)*64);
   if(x%16>3&&x%16<13)continue;
   auto p=std::make_shared<Engine::SealedFluidCave>();p->x=x;p->z=z;p->y=360;p->ground=390;p->fluid=8;p->reach=4;
   for(int dx=-4;dx<=4;++dx)for(int dz=-2;dz<=2;++dz)p->columns.push_back({x+dx,z+dz,358,362});
   auto sample=[](int,int){return mountain(390);};auto suitable=[](Surface const&s){return hm::weight(s)>0;};
   assert(hm::sealedRockBoxFits(x-4,x+4,z-2,z+2,358,362,511,false,sample,suitable,[](auto const&,int,int,int){return 2;}));
   now.sealedFluidPlans.get(key2(gx*2,gz),[&]{return p;});
   int writes=0,owners=0;
   for(int cz=int(std::floor((z-2)/16.));cz<=int(std::floor((z+2)/16.));++cz)
    for(int cx=int(std::floor((x-4)/16.));cx<=int(std::floor((x+4)/16.));++cx){
     b=stoneVolume();now.sealedFluidCaves(cx*16,cz*16,b);int local=0;
     for(int i=0;i<256;++i)for(int y=321;y<480;++y)if(b.at(i*576+y+64)==8){
      int X=cx*16+i%16,Z=cz*16+i/16;assert(X>=x-4&&X<=x+4&&Z>=z-2&&Z<=z+2&&y>=358&&y<=362);++local;
     }
     writes+=local;owners+=local>0;
    }
   assert(writes==9*5*5&&owners>=2);out["productionSealedWaterBlocks"]=writes;out["productionSealedWaterOwners"]=owners;published=true;
  }
  assert(published);return out;
 }
};
}

int main(int argc,char**argv){try{
 assert(argc==3);J data;std::ifstream(argv[1])>>data;
 data["config"]["generatedMaxYExclusive"]=512;data["config"]["cave"]["highMountainUnderground"]=1;
 data["config"]["cave"]["raisedCaveHeight"]=1;
 auto legacy=data;legacy["config"]["cave"].erase("highMountainUnderground");
 Engine old(legacy),now(data);assert(!old.terrain().highMountainUndergroundEnabled()&&now.terrain().highMountainUndergroundEnabled());
 auto s=mountain();assert(hm::scanTop(s,true,511,120,14)==466);assert(hm::scanTop(s,false,511,120,14)==120);
 assert(hm::scanTop(s,true,319,120,14)==120);assert(hm::scanTop(mountain(180),true,511,120,14)==120);
 int prev=120;for(int h=181;h<=480;++h){int top=hm::scanTop(mountain(h),true,511,120,14);assert(top>=prev&&top<=h-14&&top<=509);prev=top;}
 for(int type=0;type<6;++type){auto bad=s;if(type==0)bad.mount=.2;if(type==1)bad.glacier=.1;if(type==2)bad.iceHabitat=.1;if(type==3)bad.volcanic=.1;if(type==4)bad.stone=.1;if(type==5)bad.cliffLow=300;assert(hm::weight(bad)==0);}
 // Use checked reads to prove footprint validation precedes all array access.
 auto raw=stoneVolume();chamber(raw,350,362);int reads=0;
 auto read=[&](int x,int y,int z){assert(x>=0&&x<16&&z>=0&&z<16&&y>=-64&&y<=511);++reads;return raw.at((z*16+x)*576+y+64);};
 auto rock=[](int b){return b==2;};auto surface=[](int,int){return mountain();};
 assert(hm::poolFits(8,350,8,3,511,read,rock,surface));
 for(auto [x,z]:{std::pair{1,8},{14,8},{8,1},{8,14}}){reads=0;assert(!hm::poolFits(x,350,z,3,511,read,rock,surface)&&reads==0);}
 for(int mode=0;mode<4;++mode){auto saved=raw;if(mode==0)raw.at((8*16+12)*576+349+64)=0;
  if(mode==1)raw.at((8*16+8)*576+347+64)=0;
  if(mode==2)for(int y=363;y<=390;++y)raw.at((8*16+8)*576+y+64)=0;
  if(mode==3)raw.at((8*16+8)*576+363+64)=8;
  assert(!hm::poolFits(8,350,8,3,511,read,rock,surface));raw=std::move(saved);
 }
 auto shellSample=[](int,int){return mountain(390);};auto suitable=[](auto const&s){return hm::weight(s)>0;};
 for(int defect=0;defect<4;++defect){auto material=[&](auto const&,int x,int y,int z){return (x==2&&z==2&&y==(defect==0?356:defect==1?364:360))?(defect==2?8:0):2;};
  assert(!hm::sealedRockBoxFits(0,4,0,4,358,362,511,false,shellSample,suitable,material));
 }
 assert(!hm::sealedRockBoxFits(0,4,0,4,505,510,511,false,shellSample,suitable,[](auto const&,int,int,int){return 2;}));
 assert(!hm::sealedRockBoxFits(0,4,0,4,358,362,511,false,[](int,int){return mountain(366);},suitable,[](auto const&,int,int,int){return 2;}));
 // Upper natural rooms become candidates, while the legacy call contract
 // remains capped and all probes stay below the player's build ceiling.
 auto name=[](worldgen_rules::StructurePoint p)->std::string_view{return p.y==350?"minecraft:air":"minecraft:stone";};
 auto solid=[&](worldgen_rules::StructurePoint p){return name(p)!="minecraft:air";};
 assert(worldgen_rules::dungeonProbeLevels({0,0},480,name,solid).empty());
 assert(worldgen_rules::dungeonProbeLevels({0,0},480,name,solid,true,511)==std::vector<int>{350});
 J report=EngineBoundaryTest::run(old,now);report["shellRejectionsVerified"]=true;report["upperDungeonProbeVerified"]=true;
 if(data.at("seedManifest").at("terrainSeed")=="13"){
  J natural=J::array();int rocks=0;
  for(auto [x,z]:{std::pair{-2291,-1927},std::pair{181,-7929}}){
   auto s=now.terrain().surface(x+32768,z+32768);int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
   auto blocks=decode(now.chunk(cx,cz));int count=countUpper(blocks,97,99);rocks+=count;
   natural.push_back({{"point",{x,s.height,z}},{"mount",s.mount},{"biome",s.biome},{"undergroundWeight",hm::weight(s)},
    {"upperRocks",count},{"upperCaveDecoration",countUpper(blocks,116,125)+countUpper(blocks,80,80)+countUpper(blocks,27,27)},
    {"upperEmerald",countUpper(blocks,107,107)}});
  }
  assert(rocks>0);report["naturalSeed13Chunks"]=natural;
 }
 report["naturalSiteFrequencyTested"]=false;report["gameplayTestPerformed"]=false;
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
