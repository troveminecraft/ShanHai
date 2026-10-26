#undef NDEBUG
#include "native/Engine.h"
#include "rules/StructureRules.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct ElevationVariantsTest {
 static void run(J data){
  data["config"]["elevationRevision"]=2;data["config"]["smallPoolsVersion"]=3;
  data["config"]["seaLevel"]=100;data["config"]["generatedMaxYExclusive"]=512;
  Engine engine(data);auto&f=engine.field;
  Field::BasinSite basin{true,1000,2000,450,390,75,135};
  double lo=10,hi=0;
  for(int i=0;i<64;++i){double a=i*6.283185307179586/64;
   double q=f.basinRadius(basin,1000+450*std::cos(a),2000+390*std::sin(a));lo=std::min(lo,q);hi=std::max(hi,q);
  }
  assert(hi-lo>.2); // Angular contour must not reduce to the old ellipse.
  for(int size=1;size<=3;++size){
   Field canyon(data);Field::LandRift r;r.valid=true;r.size=size;r.x=1100;r.z=1100;r.dx=1;r.phase=.4;
   r.length=size==1?110:size==2?270:570;r.width=size==1?3.5:size==2?5.5:9;r.depth=size==1?38:size==2?66:104;
   for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){
    canyon.basinSites.get(key2(x,z),[]{return Field::BasinSite{};});
    canyon.landRifts.get(key2(x,z),[&]{return x==0&&z==0?r:Field::LandRift{};});
   }
   int cutColumns=0;double deepest=0;
   for(int dz=-64;dz<=64;++dz){Surface s;s.height=200;double height=200;canyon.elevationRelief(s,height,1100+32768,1100+dz+32768);
    if(s.riftWeight>.1){++cutColumns;deepest=std::max(deepest,200-height);assert(s.closedBasin);}
   }
   assert(cutColumns>=2&&cutColumns<24&&deepest>r.depth*.7);
  }
  // Exercise the production pool pass on a known safe, three-lobed plan.
  // Water and lava share precisely the same bed and fluid level.
  Field::SmallPool pool;pool.x=32880;pool.z=32880;pool.level=150;pool.radius=5;
  pool.phase=.8;pool.valid=true;pool.count=3;pool.lobes={{{32886,32883,4},{32892,32880,4}}};
  int gx=pool.x/160,gz=pool.z/160;
  // Match the cheap candidate rejection before installing the ownership plan.
  pool.x=gx*160+24+int(hash01(gx,gz,seedHash(f.terrainSeed,9101))*112);
  pool.z=gz*160+24+int(hash01(gx,gz,seedHash(f.terrainSeed,9102))*112);
  pool.lobes={{{double(pool.x+6),double(pool.z+3),4},{double(pool.x+12),double(pool.z),4}}};
  auto legacy=data;legacy["config"]["smallPoolsVersion"]=3;Field water(legacy);
  water.smallPools.get(key2(gx,gz),[&]{return pool;});pool.lava=true;f.smallPools.get(key2(gx,gz),[&]{return pool;});
  int wet=0,rim=0;
  for(int z=pool.z-12;z<=pool.z+12;++z)for(int x=pool.x-12;x<=pool.x+24;++x){
   Surface a;a.height=151;a.water=-32768;a.lava=-32768;a.biome=1;a.treeD=1;
   auto b=a;water.smallSurfacePool(a,x,z);f.smallSurfacePool(b,x,z);
   double q=f.poolDistance(pool,x,z);
   if(q<1){assert(a.water==150&&b.lava==150&&a.height==b.height);++wet;}
   else{bool edge=false;for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}})edge|=f.poolDistance(pool,x+dx,z+dz)<1;
    assert(b.poolRim==edge);if(edge){assert(f.material(b,x,b.height,z)==2&&b.treeD==0);++rim;}
   }
  }
  assert(wet>100&&rim>30);
  // Actual tree-site generation, in a separate nonstreaming seed fixture.
  // Every surviving identical anchor receives exactly the requested 20..30.
  auto oldData=data;oldData["config"]["elevationRevision"]=1;Engine old(oldData);
  int trees=0;
  for(int gz=4670;gz<4740&&!trees;++gz)for(int gx=4670;gx<4740;++gx){
   auto a=old.jungleSite(gx,gz),b=engine.jungleSite(gx,gz);
   if(a.h&&b.h){assert(b.h-a.h>=20&&b.h-a.h<=30);assert(b.ground+b.h<=511);++trees;}
  }
  std::cout<<"pool beds="<<wet<<", stone rim="<<rim<<", matched natural jungle sites="<<trees<<"\n";
 }
};
}
int main(int argc,char**argv){try{
 assert(argc==2);nativegen::J data;std::ifstream(argv[1])>>data;
 nativegen::ElevationVariantsTest::run(data);
 auto low=[](int,int){return 140.;};auto high=[](int,int){return 380.;};
 uint64_t added=0;
 for(unsigned seed:{5,13,22,23}){
  nativegen::CavernGroups old(seed,true,true,true,true,512),now(seed,true,true,true,true,512,true);
  for(int z=-704;z<704;z+=23)for(int x=-704;x<704;x+=23)assert(old.column(x,z,low)==now.column(x,z,low));
  nativegen::CavernGroups a(seed,true,true,true,true,512),b(seed,true,true,true,true,512,true);
  for(int z=-704;z<704;z+=11)for(int x=-704;x<704;x+=11){
   auto ca=a.column(x,z,high),cb=b.column(x,z,high);
   for(int y=101;y<350;++y){int word=(y+64)/64,bit=(y+64)%64;auto mask=uint64_t(1)<<bit;
    assert(!(ca[word]&mask)||(cb[word]&mask));added+=!(ca[word]&mask)&&bool(cb[word]&mask);
   }
   assert((cb[0]&255)==0);
  }
 }
 assert(added>10000);
 auto ground=[](int x,int z){double nearest=1e9;for(int a=256;a<=1792;a+=384)for(int b=256;b<=1792;b+=384)nearest=std::min(nearest,std::hypot(double(x-a),double(z-b)));
  return nativegen::regional::Ground{int(150+std::min(20.,nearest*.12)),.8,.6,0,0,false,-32768};};
 int lakes=0;for(int seed=0;seed<8;++seed){auto plan=nativegen::regional::drainage(0,0,seed,ground,true,100,511,true,true,true);lakes+=int(plan.at("lakes").size());}
 assert(lakes>=16);
 assert(worldgen_rules::configuredVillageSpacing(16)==16);
 std::cout<<"PASS upper cave added voxels="<<added<<", closed lakes="<<lakes<<"; lowland cave masks unchanged\n";
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
