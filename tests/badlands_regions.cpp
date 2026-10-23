#undef NDEBUG
#include "native/Engine.h"
#include "ChunkCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
static bool ocean(int b){return b==0||b==1||b==2||b==21||b==22;}
int main(int argc,char**argv){try{
 assert(argc==3);J data;std::ifstream(argv[1])>>data;
 data["config"].update(J{{"badlandsRegions",1},{"seaLevel",40},{"maxY",511},{"generatedMaxYExclusive",512},{"mountainPeakY",480}});
 Field field(data);size_t checked=0,red=0,wet=0,desert=0;J regions=J::array(),coasts=J::array();int chunkX=0,chunkZ=0;size_t largest=0;
 auto check=[&](int x,int z){
  auto s=field.surface(x,z);if(s.badlands){++checked;chunkX=int(std::floor((x-32768)/16.));chunkZ=int(std::floor((z-32768)/16.));
   for(auto [dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,1},{-1,1},{1,-1}}){auto n=field.surface(x+dx,z+dz);
    if(!(n.badlands||n.biome==11||n.biome==16||ocean(n.biome)))throw std::runtime_error("Invalid badlands border at "+std::to_string(x-32768)+","+std::to_string(z-32768)+" -> "+std::to_string(n.biome));
   }
  }
  desert+=s.badlandsDesert;
  if(s.redSandCoast){++red;int top=field.material(s,x,s.height,z),below=field.material(s,x,s.height-1,z);
   if(top!=169||below!=177)throw std::runtime_error("Coast material overridden at "+std::to_string(x-32768)+","+std::to_string(z-32768)+": "+std::to_string(top)+","+std::to_string(below));
   if(coasts.size()<8)coasts.push_back({{"x",x-32768},{"z",z-32768},{"biome",s.biome},{"height",s.height},{"top",top},{"support",below}});
  }
  if((s.badlands||s.redSandCoast)&&s.water>s.height){++wet;assert(field.material(s,x,s.water,z)==8||field.material(s,x,s.water,z)==9);}
  return s;
 };
 for(int ring=0;ring<=6&&(regions.size()<3||red==0);++ring)for(int gz=8-ring;gz<=8+ring&&(regions.size()<3||red==0);++gz)for(int gx=8-ring;gx<=8+ring&&(regions.size()<3||red==0);++gx){
  if(std::max(std::abs(gx-8),std::abs(gz-8))!=ring)continue;auto r=field.inspectBadlandsRegion(gx,gz);if(!r["valid"].get<bool>())continue;
  int cx=r["x"].get<int>()+32768,cz=r["z"].get<int>()+32768;double rx=r["radiusX"],rz=r["radiusZ"];size_t area=0;
  // Coarse area survey plus exact one-block neighbours, including the outline.
  for(int z=-1024;z<=1024;z+=128)for(int x=-1024;x<=1024;x+=128){auto s=check(cx+x,cz+z);area+=s.badlands>0;}
  for(int a=0;a<48;++a){double angle=a*6.283185307179586/48;
   for(int offset=-64;offset<=112;offset+=16)check(cx+roundJS((rx+offset)*std::cos(angle)),cz+roundJS((rz+offset)*std::sin(angle)));
  }
  largest=std::max(largest,area*128*128);r["estimatedArea"]=area*128*128;regions.push_back(r);std::cerr<<r.dump()<<'\n';
 }
 assert(checked>0&&desert>0&&red>0&&wet>0&&largest>=262144);
 Engine engine(data,{},false,&field);auto chunk=engine.chunk(chunkX,chunkZ);auto decoded=terrain_runtime::decodeChunk(chunk,178,29,576);assert(decoded.height==576);
 engine.chunk(chunkX+1,chunkZ);assert(engine.chunk(chunkX,chunkZ)==chunk);
 J report{{"regions",regions},{"largestEstimatedArea",largest},{"badlandsBoundarySamples",checked},{"desertSamples",desert},{"redSandSamples",red},{"wetSamples",wet},{"coasts",coasts},{"chunk",{chunkX,chunkZ}},{"revisitByteEqual",true},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
