#undef NDEBUG
#include "native/Engine.h"
#include "ChunkCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace nativegen;
std::vector<uint16_t> unpack(std::vector<uint8_t>const&bytes,int height){
 auto c=terrain_runtime::decodeChunk(bytes,65536,256,height);std::vector<uint16_t> raw(256*height);
 for(int i=0;i<256;++i){size_t q=c.columns[i];int n=c.runs[q]|c.runs[q+1]<<8;q+=2;int y=0;
  for(int j=0;j<n;++j){int len=c.runs[q]|c.runs[q+1]<<8,block=c.runs[q+2]|c.runs[q+3]<<8;q+=4;std::fill_n(raw.data()+i*height+y,len,uint16_t(block));y+=len;}}
 return raw;
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe/report required");J data;std::ifstream(argv[1])>>data;
 assert(data.at("config").at("volcanoRevision")==3);Engine engine(data);auto&f=engine.terrain();
 auto volcanoes=f.inspectVolcanoes(-8192,8192);assert(volcanoes.size()>2);J sites=J::array();size_t checked=0;
 for(auto const&v:volcanoes){int cx=v.at("x"),cz=v.at("z");double outer=v.at("outer");int rootLava=0;
  for(int dir=0;dir<8;++dir)for(int radius=0;radius<=int(outer);radius+=8){double a=dir*6.283185307179586/8;
   int x=cx+int(std::round(radius*std::cos(a)))+32768,z=cz+int(std::round(radius*std::sin(a)))+32768;auto s=f.surface(x,z,false);
   for(int y=-64;y<-60;++y)assert(f.material(s,x,y,z)==1);
   for(int y=-60;y<=-57;++y){int m=f.material(s,x,y,z);assert(m!=0&&m!=43&&m!=8&&m!=9);}
   if(s.volcanoMagmaTop>-32768){
    for(int y=-56;y<=int(std::floor(s.volcanoMagmaTop));++y){assert(f.material(s,x,y,z)==43);++checked;}
    rootLava+=f.material(s,x,-56,z)==43;
    if(radius>int(double(v.at("crater"))*1.2))assert(s.volcanoMagmaTop<=s.height-7);
   }
  }
  assert(rootLava>150);sites.push_back({{"x",cx},{"z",cz},{"base",v.at("base")},{"rootSamples",rootLava}});
 }
 // Both cross-sections are decoded from the complete chunk pipeline, including
 // ecology, sealed fluids and ores. This catches later passes carving the body.
 auto v=volcanoes.at(0);int cx=v.at("x"),cz=v.at("z"),height=f.generationHeight();J cuts=J::array();
 std::map<std::pair<int,int>,std::vector<uint16_t>> chunks;int actualMagma=0;
 for(int axis=0;axis<2;++axis){J columns=J::array();
  for(int offset=-560;offset<=560;++offset){int x=cx+(axis==0?offset:0),z=cz+(axis==1?offset:0);
   int chunkX=int(std::floor(x/16.)),chunkZ=int(std::floor(z/16.));auto key=std::pair{chunkX,chunkZ};
   if(!chunks.contains(key))chunks[key]=unpack(engine.chunk(chunkX,chunkZ),height);
   auto const&raw=chunks.at(key);auto col=raw.data()+((z-chunkZ*16)*16+x-chunkX*16)*height;auto s=f.surface(x+32768,z+32768,false);
   for(int y=-64;y<=511;++y){
    if(y<-60)assert(col[y+64]==1);
    if(y>=-56&&y<=s.volcanoMagmaTop){assert(col[y+64]==43);++actualMagma;}
   }
   columns.push_back(std::vector<uint16_t>(col,col+height));
  }cuts.push_back({{"axis",axis==0?"x":"z"},{"start",-560},{"columns",columns}});
 }
 assert(actualMagma>50000);
 J report{{"volcano",v},{"sites",sites},{"continuousLavaChecks",checked},{"fullChunkLavaChecks",actualMagma},{"decodedChunks",chunks.size()},{"bottomSeal",true},{"sections",cuts},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump();report.erase("sections");std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
