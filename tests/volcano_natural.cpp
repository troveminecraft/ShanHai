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
 assert(data.at("config").at("volcanoRevision")==4);Engine engine(data);auto&f=engine.terrain();
 auto volcanoes=f.inspectVolcanoes(-16384,16384);assert(volcanoes.size()>15);J sites=J::array();size_t checked=0;int sizes[3]{};
 for(auto const&v:volcanoes){int cx=v.at("x"),cz=v.at("z");double outer=v.at("outer"),crater=v.at("crater");int top=v.at("lavaTop"),size=v.at("sizeClass");++sizes[size];
  assert(crater>=18&&crater<=58);assert(size==2||crater<=42);
  auto center=f.surface(cx+32768,cz+32768,false);
  for(int y=-56;y<=top;++y)assert(f.material(center,cx+32768,y,cz+32768)==43);
  int minLip=512,maxLip=-64;std::vector<int> widths;
  for(int dir=0;dir<16;++dir){double a=dir*6.283185307179586/16;int lip=-64;
   for(int radius=0;radius<=int(outer*.8);radius+=2){
    int x=cx+int(std::round(radius*std::cos(a)))+32768,z=cz+int(std::round(radius*std::sin(a)))+32768;auto s=f.surface(x,z,false);
    if(radius<crater*1.5)lip=std::max(lip,s.height);
    for(int y=-64;y<-60;++y)assert(f.material(s,x,y,z)==1);
    for(int y=-60;y<=-57;++y){int m=f.material(s,x,y,z);assert(m!=0&&m!=43&&m!=8&&m!=9);}
    if(radius>crater*1.4){for(int y=std::max(-56,s.height-6);y<=s.height;++y)assert(f.material(s,x,y,z)!=43);}
    for(int y=-56;y<=top;y+=5)if(magma::contains(s.volcanoMargins,y,s.volcanoLava,s.volcanoMagmaTop)){assert(f.material(s,x,y,z)==43);++checked;}
   }
   minLip=std::min(minLip,lip);maxLip=std::max(maxLip,lip);
  }
  assert(minLip-top>=3&&maxLip-top<=28);
  sites.push_back({{"x",cx},{"z",cz},{"nominalDiameter",crater*2},{"sizeClass",size},{"lava",top},{"minLip",minLip},{"maxLip",maxLip}});
 }
 assert(sizes[0]>0&&sizes[1]>sizes[2]);
 auto v=volcanoes.at(0);for(auto const&row:volcanoes)if(row.at("x")==1642&&row.at("z")==-2265)v=row;
 int cx=v.at("x"),cz=v.at("z"),height=f.generationHeight();J cuts=J::array();
 std::map<std::pair<int,int>,std::vector<uint16_t>> chunks;int actualMagma=0,irregularRows=0;
 for(int axis=0;axis<2;++axis){J columns=J::array();
  for(int offset=-420;offset<=420;++offset){int x=cx+(axis==0?offset:0),z=cz+(axis==1?offset:0);
   int chunkX=int(std::floor(x/16.)),chunkZ=int(std::floor(z/16.));auto key=std::pair{chunkX,chunkZ};
   if(!chunks.contains(key))chunks[key]=unpack(engine.chunk(chunkX,chunkZ),height);
   auto const&raw=chunks.at(key);auto col=raw.data()+((z-chunkZ*16)*16+x-chunkX*16)*height;auto s=f.surface(x+32768,z+32768,false);
   for(int y=-64;y<=511;++y){
    if(y<-60)assert(col[y+64]==1);
    if(magma::contains(s.volcanoMargins,y,s.volcanoLava,s.volcanoMagmaTop)){assert(col[y+64]==43);++actualMagma;}
   }
   columns.push_back(std::vector<uint16_t>(col,col+height));
  }
  // A rotational funnel has equal left/right edges at every depth. Require
  // persistent measured asymmetry in the completed voxel result.
  for(int y=-50;y<int(v.at("lavaTop"))-10;y+=5){int left=0,right=840;while(left<840&&columns[left][y+64]!=43)++left;while(right>0&&columns[right][y+64]!=43)--right;if(std::abs(left+right-840)>=4)++irregularRows;}
  cuts.push_back({{"axis",axis==0?"x":"z"},{"start",-420},{"columns",columns}});
 }
 assert(actualMagma>20000&&irregularRows>=10);
 J report{{"passed",true},{"volcano",v},{"sites",sites},{"sizes",{sizes[0],sizes[1],sizes[2]}},{"sampledLava",checked},{"fullChunkLava",actualMagma},{"asymmetricDepthRows",irregularRows},{"decodedChunks",chunks.size()},{"bottomSeal",true},{"sections",cuts},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump();report.erase("sections");report.erase("sites");std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
