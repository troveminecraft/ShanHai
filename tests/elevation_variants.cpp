#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct ElevationVariantsTest {
 static void verify(J data){
  data["config"].update(J{{"elevationRevision",1},{"landscapeRevision",3},{"seaLevel",100},{"generatedMaxYExclusive",512},{"mountainPeakY",480}});
  Engine engine(data);auto& f=engine.field;
  f.iceAtlas.clear();f.iceAtlas.push_back({true,0,0,1600,1600,0,0});
  for(auto [x,expected]:{std::pair{2240,1.},std::pair{2496,.5},std::pair{2752,0.}}){
   Surface s;s.height=180;s.biome=13;s.H=.8;f.applySnowVariant(s,x+32768,32768);
   assert(std::abs(s.glacierSnow-expected)<1e-9);assert(s.parentBiome==13&&s.biome==13);assert(s.altitudeBand==1);
  }
  Surface hot;hot.height=180;hot.biome=20;hot.volcanoHeat=.9;f.applySnowVariant(hot,32768,32768);assert(hot.snowCover==0);
  int terrain[]={2,4,5,80,168,169,170,171,172,173,174,175,176,177,178};
  for(int biome=0;biome<29;++biome){
   std::array<Surface,256> surfaces{};std::vector<uint16_t> raw(256*576);
   for(int i=0;i<256;++i){surfaces[i].height=180;surfaces[i].biome=biome;surfaces[i].snowCover=1;raw[i*576+244]=terrain[i%15];}
   engine.variantSnowCover(32768,32768,raw,surfaces);
   for(int i=0;i<256;++i){assert(raw[i*576+244]==terrain[i%15]);assert(raw[i*576+245]==146);}
  }
  // Lava, water and exposed bedrock do not acquire floating snow caps.
  std::array<Surface,256> wet{};std::vector<uint16_t> raw(256*576);
  for(int i=0;i<256;++i){wet[i].height=90;wet[i].water=100;wet[i].snowCover=1;raw[i*576+164]=8;}
  engine.variantSnowCover(32768,32768,raw,wet);
  for(int i=0;i<256;++i)assert(raw[i*576+164]==9&&raw[i*576+165]==0);
  std::cout<<"PASS 29 parent substrates, frozen water, hot core exclusion, glacier edge 512/768/1024 and altitude type\n";
 }
};
}
int main(int argc,char**argv){if(argc!=2)return 2;nativegen::J data;std::ifstream(argv[1])>>data;nativegen::ElevationVariantsTest::verify(data);}
