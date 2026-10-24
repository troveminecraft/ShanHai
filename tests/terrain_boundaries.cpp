#undef NDEBUG
#include "native/Engine.h"
#include "TestTeleportPolicy.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
struct Survey {int wet=0,leaks=0,maxStep=0;};
Survey survey(Field&f,int X,int Z,int radius,bool mesa=false){
 int n=radius*2+1;std::vector<Surface> rows(n*n);Survey result;
 for(int z=0;z<n;++z)for(int x=0;x<n;++x){auto&s=rows[z*n+x];s=f.surface(X-radius+x+32768,Z-radius+z+32768);result.wet+=s.river>0;
  if(mesa){assert(s.iceHabitat==0);assert(s.biome==11||s.badlands||s.water>=s.height);}
 }
 for(int z=1;z<n-1;++z)for(int x=1;x<n-1;++x){auto const&s=rows[z*n+x];
  for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto const&t=rows[(z+dz)*n+x+dx];
   result.maxStep=std::max(result.maxStep,std::abs(s.height-t.height));
   if(s.river>0&&!t.river&&t.height<s.water&&t.water<s.water)++result.leaks;
   if(mesa&&s.badlands)assert(t.badlands||t.biome==11||t.biome==16||(t.height<t.seaLevel&&t.water==t.seaLevel));
  }
 }
 return result;
}
std::vector<uint16_t> decode(std::vector<uint8_t>const&b,int height){
 assert(b.size()>260&&b[0]=='K'&&b[1]=='W'&&b[2]=='2'&&b[3]==0);size_t pos=260;std::vector<uint16_t> out;
 auto word=[&]{assert(pos+2<=b.size());int v=b[pos]|(b[pos+1]<<8);pos+=2;return v;};
 for(int col=0;col<256;++col){int runs=word(),total=0;for(int r=0;r<runs;++r){int n=word(),id=word();assert(n>0);total+=n;out.insert(out.end(),n,uint16_t(id));}assert(total==height);}
 assert(pos==b.size());return out;
}
int main(int argc,char**argv){try{
 assert(argc==3);J data;std::ifstream(argv[1])>>data;
 assert(data.at("seedManifest").at("terrainSeed")=="11");
 auto old=data;old["config"]["badlandsRegions"]=2;old["config"].erase("riverBankSurvey");
 data["config"]["badlandsRegions"]=3;data["config"]["riverBankSurvey"]=1;
 Field before(old);Engine engine(data);auto&after=engine.terrain();
 auto a=survey(before,-918,410,96),b=survey(after,-918,410,96);
 assert(a.leaks>0&&b.leaks==0&&b.wet>=a.wet/2&&b.wet>0);
 auto point=after.surface(32768-918,32768+410);assert(point.river>0&&point.water<=point.preRiverHeight);
 J report{{"oldWaterFaces",a.leaks},{"newWaterFaces",b.leaks},{"oldWetColumns",a.wet},{"newWetColumns",b.wet},{"riverPoint",{{"height",point.height},{"water",point.water}}}};
 J mesas=J::array();
 for(auto [x,z]:{std::pair{-1220,-6868},{-1901,-6219},{-1504,-5984}}){auto s=survey(after,x,z,64,true);assert(s.maxStep<=12);mesas.push_back({{"x",x},{"z",z},{"maxAdjacentStep",s.maxStep}});}
 report["mesas"]=mesas;
 assert(!test_teleport::desertInterior(before.surface(32768-1504,32768-5984)));
 assert(!test_teleport::desertInterior(after.surface(32768-1504,32768-5984)));
 // Decode actual KW2 chunks after vegetation/underground passes, including
 // negative chunk boundaries. Surface-only assertions miss later overwrites.
 std::map<std::pair<int,int>,std::vector<uint16_t>> chunks;int height=after.generationHeight();
 for(int cz=24;cz<=27;++cz)for(int cx=-59;cx<=-56;++cx)chunks[{cx,cz}]=decode(engine.chunk(cx,cz),height);
 auto block=[&](int x,int y,int z){int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));return chunks.at({cx,cz})[((z-cz*16)*16+x-cx*16)*height+y+64];};
 int water=0,airFaces=0;
 for(int z=385;z<431;++z)for(int x=-943;x<-897;++x){auto s=after.surface(x+32768,z+32768);if(!s.river)continue;
  if(block(x,s.water,z)!=8)continue;++water;
  for(auto[dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto t=after.surface(x+dx+32768,z+dz+32768);
   if(!t.river&&t.water<s.water&&block(x+dx,s.water,z+dz)==0)++airFaces;
  }
 }
 assert(water>20&&airFaces==0);report["encodedChunks"]=chunks.size();report["encodedWaterColumns"]=water;report["encodedAirFaces"]=airFaces;report["gameplayTestPerformed"]=false;
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
