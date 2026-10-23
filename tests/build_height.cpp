#ifdef NDEBUG
#undef NDEBUG
#endif
#include "BuildHeight.h"
#include "rules/FrozenGround.h"
#include "rules/FrozenCapture.h"
#include <cassert>
#include <iostream>
#include <limits>

namespace {
using namespace terrain_runtime;
using Json=nlohmann::json;
template<class F>void rejects(F fn){bool rejected=false;try{fn();}catch(std::exception const&){rejected=true;}assert(rejected);}
uint16_t material(int col,int y){return uint16_t(1+(col*7+y/17)%31);}
struct CaptureField {
 int height;
 int generationHeight()const{return height;}
 struct Surface{int height,biome;};
 Surface surface(int x,int z){return {height-65-((x+z)&7),((x+z)&3)};}
 void materialColumn(Surface const&s,int,int,int top,uint16_t*out){
  assert(top==height-65);for(int y=4;y<height;y++)out[y]=uint16_t(y<=s.height+64?2:0);
 }
};
std::vector<uint8_t> encoded(int height){
 std::vector<uint8_t> bytes(260,0);bytes[0]='K';bytes[1]='W';bytes[2]='2';
 auto u16=[&](int value){bytes.push_back(uint8_t(value));bytes.push_back(uint8_t(value>>8));};
 for(int col=0;col<256;col++)bytes[4+col]=uint8_t(col%7);
 for(int col=0;col<256;col++){
  u16((height+16)/17);
  for(int y=0;y<height;y+=17){u16(std::min(17,height-y));u16(material(col,y));}
 }
 return bytes;
}
void volumes(BuildHeight const& bounds,bool yContiguous){
 auto chunk=decodeChunk(encoded(bounds.generatedHeight()),32,7,bounds.generatedHeight());
 std::vector<uint16_t> guarded(bounds.blocks()+32,60000),palette(32);
 for(int i=0;i<32;i++)palette[i]=uint16_t(100+i);
 auto index=[&](int x,int y,int z)->size_t{
  return yContiguous?size_t(z*16+x)*bounds.height()+y:size_t(y)*256+z*16+x;
 };
 auto blockSpan=std::span<uint16_t>(guarded.data()+16,bounds.blocks());
 fillGeneratedBlockVolume(blockSpan,std::span<uint16_t const>{palette},chunk,bounds,index);
 for(int col=0;col<256;col++)for(int y=0;y<bounds.height();y++)
  assert(blockSpan[index(col%16,y,col/16)]==palette[y<chunk.height?material(col,y):0]);
 for(int i=0;i<16;i++)assert(guarded[i]==60000&&guarded[guarded.size()-1-i]==60000);
 assert(blockSpan[index(15,chunk.height-1,15)]==palette[material(255,chunk.height-1)]);
 if(chunk.height<bounds.height())assert(blockSpan[index(15,chunk.height,15)]==palette[0]);
 rejects([&]{fillGeneratedBlockVolume(blockSpan.first(blockSpan.size()-1),std::span<uint16_t const>{palette},chunk,bounds,index);});
 rejects([&]{fillGeneratedBlockVolume(blockSpan,std::span<uint16_t const>{palette},chunk,bounds,[](int,int,int){return size_t(0);});});
 rejects([&]{fillGeneratedBlockVolume(blockSpan,std::span<uint16_t const>{palette},chunk,bounds,[&](int x,int y,int z){return index(x,y,z)+bounds.blocks();});});
 auto truncated=chunk;truncated.runs.pop_back();
 rejects([&]{fillGeneratedBlockVolume(blockSpan,std::span<uint16_t const>{palette},truncated,bounds,index);});
 std::vector<uint16_t> biome(4096),biomePalette{7,8,9,10,11,12,13};
 fillBiomeSubchunk(std::span<uint16_t>{biome},std::span<uint16_t const>{biomePalette},chunk);
 std::vector<std::vector<uint16_t>> biomeVolume(bounds.subchunks(),biome);
 for(int sub=0;sub<bounds.subchunks();sub++)for(int col=0;col<256;col++)for(int y=0;y<16;y++)
  assert(biomeVolume[sub][((col%16)<<8)|((col/16)<<4)|y]==biomePalette[col%7]);
 assert(biomeVolume.size()*16==size_t(bounds.height()));
 auto invalidBiome=chunk;invalidBiome.biomes[255]=7;
 rejects([&]{fillBiomeSubchunk(std::span<uint16_t>{biome},std::span<uint16_t const>{biomePalette},invalidBiome);});
}
}
int main(int argc,char**argv){
 try{
  using namespace terrain_runtime;
  auto legacy=BuildHeight::read(Json::object());
  auto buildOnly=BuildHeight::read({{"buildMaxYExclusive",512}});
  auto expanded=BuildHeight::read({{"buildMaxYExclusive",512},{"generatedMaxYExclusive",512}});
  assert(legacy.height()==384&&legacy.generatedHeight()==384&&legacy.subchunks()==24);
  assert(buildOnly.height()==576&&buildOnly.generatedHeight()==384&&buildOnly.subchunks()==36);
  assert(expanded.height()==576&&expanded.generatedHeight()==576&&expanded.subchunks()==36);
  for(auto const& bounds:{legacy,buildOnly,expanded}){
   assert(bounds.contains(-64)&&bounds.contains(319)&&!bounds.contains(-65));
   assert(bounds.contains(bounds.maxYExclusive-1)&&!bounds.contains(bounds.maxYExclusive));
   bounds.requireDimension(-64,bounds.maxYExclusive);
   rejects([&]{bounds.requireDimension(-63,bounds.maxYExclusive);});
   rejects([&]{bounds.requireDimension(-64,bounds.maxYExclusive+16);});
   volumes(bounds,false);volumes(bounds,true);
  }
  assert(!legacy.contains(320)&&expanded.contains(320)&&expanded.contains(511));
  legacy.requireNativeRecipe({{"config",Json::object()}},{{"maxY",319}});
  expanded.requireNativeRecipe({{"config",{{"generatedMaxYExclusive",512}}}},{{"maxY",511}});
  rejects([&]{expanded.requireNativeRecipe({{"config",Json::object()}},{{"maxY",511}});});
  rejects([&]{legacy.requireNativeRecipe({{"config",{{"generatedMaxYExclusive",512}}}},{{"maxY",319}});});
  rejects([&]{expanded.requireNativeRecipe({{"config",{{"generatedMaxYExclusive",512.0}}}},{{"maxY",511}});});
  rejects([&]{expanded.requireNativeRecipe({{"config",{{"generatedMaxYExclusive",512}}}},{{"maxY",319}});});
  rejects([&]{expanded.requireNativeRecipe({{"config",{{"generatedMaxYExclusive",512}}}},{{"maxY",511.0}});});
  for(int height:{384,576}){
   CaptureField field{height};auto capture=worldgen_rules::buildFrozenCaptureGround(field,{-3,5});
   assert(capture.height==height&&capture.blocks.size()==256*size_t(height));
   for(int col=0;col<256;col++){
    auto floor=capture.heights[col]-1;assert(floor<=height-65);
    assert(capture.blocks[col*height]==1&&capture.blocks[col*height+floor+64]==2);
    for(int y=floor+65;y<height;y++)assert(capture.blocks[col*height+y]==0);
   }
   auto spec=worldgen_rules::planFrozenCapture(worldgen_rules::FrozenCaptureKind::Portal,{0,0},42,{{0,height-100,0},{20,height-80,20}},height-90,1,height-65);
   assert(spec.placement.max.y==height-65&&spec.scan.max.y==height-65&&spec.scan.valid());
  }
  for(auto field:{"buildMaxYExclusive","generatedMaxYExclusive"})for(auto const& bad:Json::array({nullptr,true,"512",512.0,0,-64,319,321,513,1024,18446744073709551615ull}))
   rejects([&]{BuildHeight::read({{field,bad}});});
  rejects([]{BuildHeight::read({{"generatedMaxYExclusive",512}});});
  rejects([]{BuildHeight::read({{"buildMaxYExclusive",320},{"generatedMaxYExclusive",512}});});
  auto oldBytes=encoded(384),newBytes=encoded(576);
  assert(decodeChunk(oldBytes,32,7).height==384);
  assert(decodeChunk(newBytes,32,7,576).height==576);
  rejects([&]{decodeChunk(newBytes,32,7);});
  rejects([&]{decodeChunk(oldBytes,32,7,576);});
  rejects([&]{decodeChunk(newBytes,32,7,577);});
  auto oversized=newBytes;oversized[262]=255;oversized[263]=255;
  rejects([&]{decodeChunk(oversized,32,7,576);});
  validateColumnRun(575,1,31,32,576);
  rejects([]{validateColumnRun(575,2,31,32,576);});
  if(argc==2){
   auto dir=std::filesystem::path(argv[1]);std::filesystem::create_directories(dir);
   auto oldFile=dir/"height-legacy-cache.bin",newFile=dir/"height-expanded-cache.bin";
   terrain_cache::writeDerived(oldFile,oldBytes);terrain_cache::writeDerived(newFile,newBytes);
   assert(readChunk(oldFile,32,7).state==terrain_cache::ReadState::Valid);
   assert(readChunk(newFile,32,7,576).state==terrain_cache::ReadState::Valid);
   assert(readChunk(oldFile,32,7,576).state==terrain_cache::ReadState::Invalid);
   assert(readChunk(newFile,32,7).state==terrain_cache::ReadState::Invalid);
  }
  std::cout<<"Height profiles, 384/576 codec and cache separation, both native layouts, air extension and 24/36 biome stacks verified\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}
}
