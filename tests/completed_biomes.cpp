#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
namespace nativegen {
struct CompletedBiomesTest {
 static void bands(Field&f){
  std::set<int> colors;for(int y=25;y<220;++y){Surface s;s.badlands=1;s.height=230;int b=f.badlandsMaterial(s,100,y,-100);assert(b>=170&&b<=176);colors.insert(b);}
  assert(colors.size()==7);
  Surface s;s.height=45;s.badlands=1;assert(f.badlandsMaterial(s,0,45,0)==169);assert(f.badlandsMaterial(s,0,44,0)==177);assert(f.badlandsMaterial(s,0,24,0)==-1);
  s.badlands=4;s.height=95;assert(f.badlandsMaterial(s,0,94,0)==3);int top=f.badlandsMaterial(s,0,95,0);assert(top==4||top==33);
 }
 static void variants(Field&f){
  std::set<int> variants;
  for(int z=-4000;z<=4000;z+=256)for(int x=-4000;x<=4000;x+=256){Surface s;s.biome=15;s.height=100;f.badlandsSurface(s,x,z);variants.insert(s.biome);assert(s.T==.95&&s.H==.05);
   std::array<uint16_t,384> col{};f.materialColumn(s,x,z,s.height,col.data());
   for(int y=-60;y<=s.height;++y)assert(col[y+64]==f.material(s,x,y,z));
  }assert(variants.contains(26)&&variants.contains(27)&&variants.contains(28));
 }
};
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe and report required");
 J data;std::ifstream(argv[1])>>data;Field f(data);CompletedBiomesTest::bands(f);CompletedBiomesTest::variants(f);
 J site;for(int radius=0;radius<=18&&site.is_null();++radius)for(int dz=-radius;dz<=radius&&site.is_null();++dz)for(int dx=-radius;dx<=radius;++dx){
  if(std::max(std::abs(dx),std::abs(dz))!=radius)continue;
  auto a=f.inspectMushroomIsland(21+dx,21+dz);if(a.at("valid").get<bool>()){site=a;break;}
 }
 if(site.is_null())throw std::runtime_error("No ocean island found in bounded seed survey");
 int X=site.at("x"),Z=site.at("z"),R=site.at("radius");size_t land=0,edges=0,wet=0;
 // Exhaustive one-block perimeter check of the actual final surface. Query
 // neighbours in reverse order too: water collars must survive cache order.
 for(int z=Z-R-12;z<=Z+R+12;++z)for(int x=X-R-12;x<=X+R+12;++x){
  auto s=f.surface(x+32768,z+32768);if(!s.mushroom){++wet;continue;}++land;
  assert(s.biome==25&&s.height>=31&&s.water<s.height&&!s.rock);
  assert(f.material(s,x+32768,s.height,z+32768)==168);
  for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=f.surface(x+dx+32768,z+dz+32768);
   if(!b.mushroom){++edges;assert(b.biome==0||b.biome==1||b.biome==2||b.biome==21||b.biome==22);assert(b.height<31&&b.water==31);}
  }
 }assert(land>1000&&wet>1000&&edges>100);
 Engine engine(data);int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
 auto before=engine.chunk(cx,cz);engine.chunk(cx+1,cz);assert(engine.chunk(cx,cz)==before);
 bool rejected=false;std::array<uint16_t,384> column{};Surface high;high.height=320;
 try{f.materialColumn(high,0,0,320,column.data());}catch(std::out_of_range const&){rejected=true;}assert(rejected);
 J report{{"seed",data.at("seedManifest").at("terrainSeed")},{"island",site},{"landColumns",land},{"boundaryEdgesChecked",edges},{"oceanColumns",wet},{"columnMaterialParity",true},{"height320Rejected",true},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
