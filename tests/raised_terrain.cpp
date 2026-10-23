#undef NDEBUG
#include "native/Engine.h"
#include "ChunkCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
static J point(int x,int z,Surface const&s){return {{"x",x},{"z",z},{"height",s.height},{"water",s.water},{"biome",s.biome},{"river",s.river},{"lake",s.lake}};}
int main(int argc,char**argv){std::string stage="recipe";try{
 if(argc!=3)throw std::runtime_error("recipe and report required");
 J data;std::ifstream(argv[1])>>data;
 data["config"]["seaLevel"]=40;data["config"]["generatedMaxYExclusive"]=512;data["config"]["mountainPeakY"]=480;
 if(!data.contains("regionalGeneration"))throw std::runtime_error("regional fixture required");
 Field field(data);assert(field.seaLevel()==40&&field.generationHeight()==576);
 stage="peak survey";std::vector<std::pair<int,std::pair<int,int>>> peaks;size_t coarseSamples=0;
 for(int z=-8192;z<=8192;z+=256)for(int x=-8192;x<=8192;x+=256){
  auto s=field.bootstrapSurface(x+32768,z+32768);++coarseSamples;
  if(s.stone>.01||s.volcanic>0||s.mushroom||s.badlands)continue;peaks.push_back({s.height,{x,z}});
 }
 std::sort(peaks.begin(),peaks.end(),std::greater<>());assert(!peaks.empty());int peakX=0,peakZ=0;Surface peak;
 for(size_t i=0;i<std::min(size_t(8),peaks.size());++i){auto[x,z]=peaks[i].second;
  auto s=field.surface(x+32768,z+32768);if(s.height>peak.height){peak=s;peakX=x;peakZ=z;}if(peak.height>=470)break;
 }
 assert(peak.height>=470&&peak.height<500);std::cerr<<"peak "<<peakX<<","<<peakZ<<"="<<peak.height<<'\n';
 stage="production drainage";J regions=J::array(),water=J::array(),lakeEvidence=J::array();size_t riverSamples=0,lakeSamples=0,marineSamples=0,waterFaces=0,isolatedWetSamples=0;int highestRiverWater=-32768;
 auto wetColumn=[&](int x,int z,bool river){
  auto s=field.surface(x+32768,z+32768,false);assert(s.height>=-61&&s.height<512);assert(s.water<512);if(s.water<=s.height)return false;
  int block=field.material(s,x+32768,s.water,z+32768);assert(block==8||block==9);assert(field.material(s,x+32768,s.water+1,z+32768)==0);
  int connected=0;for(auto[dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){auto q=field.surface(x+dx+32768,z+dz+32768,false);if(q.water>q.height&&std::min(s.water,q.water)>std::max(s.height,q.height))++connected;}
  waterFaces+=connected;isolatedWetSamples+=connected==0;
  if(river){if(water.size()<40)water.push_back(point(x,z,s));highestRiverWater=std::max(highestRiverWater,s.water);++riverSamples;}
  else{if(lakeEvidence.size()<20)lakeEvidence.push_back(point(x,z,s));++lakeSamples;}return true;
 };
 for(int ring=0;ring<=3&&(riverSamples<30||lakeSamples<8);++ring)for(int gz=-ring;gz<=ring&&(riverSamples<30||lakeSamples<8);++gz)for(int gx=-ring;gx<=ring&&(riverSamples<30||lakeSamples<8);++gx){
  if(std::max(std::abs(gx),std::abs(gz))!=ring)continue;
  auto plan=field.inspectRegionalDrainage(gx,gz);size_t beforeR=riverSamples,beforeL=lakeSamples;auto const&segments=plan.at("segments");
  for(size_t i=0;i<segments.size();i+=std::max(size_t(1),segments.size()/24)){auto const&s=segments[i];assert(double(s[4])>=double(s[5])&&double(s[5])>=40);wetColumn(roundJS(s[0]),roundJS(s[1]),true);}
  for(auto const&lake:plan.at("lakes"))for(auto const&row:lake.at("rows")){int z=row[0],lo=row[1],hi=row[2];if(wetColumn((lo+hi)/2,z,false)&&lakeSamples-beforeL>=12)break;}
  regions.push_back({{"region",{gx,gz}},{"segments",segments.size()},{"lakes",plan["lakes"].size()},{"wetRiverSamples",riverSamples-beforeR},{"wetLakeSamples",lakeSamples-beforeL}});
 }
 // A bounded seed survey need not contain a closed lake basin. Record its
 // actual coverage; cross-seed runs exercise the lake path where one exists.
 std::cerr<<"water survey: regions="<<regions.size()<<" river="<<riverSamples<<" lake="<<lakeSamples<<" isolated="<<isolatedWetSamples<<'\n';
 assert(riverSamples>0&&waterFaces>0&&isolatedWetSamples==0);stage="ocean";
 for(int z=-4096;z<=4096&&marineSamples<24;z+=256)for(int x=-4096;x<=4096&&marineSamples<24;x+=256){
  auto s=field.bootstrapSurface(x+32768,z+32768);if(s.height>=40||s.water!=40||s.stone||s.volcanic||s.lake)continue;
  s=field.surface(x+32768,z+32768,false);if(s.water!=40||s.height>=40||s.lake||s.river)continue;
  int b=field.material(s,x+32768,40,z+32768);assert(b==8||b==9);assert(field.material(s,x+32768,41,z+32768)==0);++marineSamples;
 }assert(marineSamples>0);
 stage="peak chunk";Engine engine(data,{},false,&field);int cx=int(std::floor(peakX/16.)),cz=int(std::floor(peakZ/16.));auto bytes=engine.chunk(cx,cz);
 auto decoded=terrain_runtime::decodeChunk(bytes,178,29,576);assert(decoded.height==576);int highBlocks=0,highestBlock=-64;size_t airAbove500=0;
 for(int column=0;column<256;++column){auto const&r=decoded.runs;size_t q=decoded.columns[column];int count=r[q]|(r[q+1]<<8);q+=2;int localY=0;
  for(int run=0;run<count;++run){int length=r[q]|(r[q+1]<<8),block=r[q+2]|(r[q+3]<<8);q+=4;
   for(int k=0;k<length;++k){int y=localY+k-64;if(y<=-61)assert(block==1);if(y>=500){assert(block==0);++airAbove500;}if(block&&y>319){++highBlocks;highestBlock=std::max(highestBlock,y);}}localY+=length;
  }assert(localY==576);
 }assert(highBlocks>256&&airAbove500==256*12&&highestBlock>=470&&highestBlock<500);
 auto other=engine.chunk(cx+1,cz);assert(engine.chunk(cx,cz)==bytes);
 J report{{"seed",data["seedManifest"]["terrainSeed"]},{"seaLevel",40},{"generatedMaxYExclusive",512},{"coarsePeakSamples",coarseSamples},{"peak",point(peakX,peakZ,peak)},{"riverSamples",riverSamples},{"highestRiverWater",highestRiverWater},{"lakeSamples",lakeSamples},{"lakeFoundInBoundedSurvey",lakeSamples>0},{"maximumSurveyRegionRing",3},{"connectedWaterFaces",waterFaces},{"isolatedWetSamples",isolatedWetSamples},{"oceanSamples",marineSamples},{"regions",regions},{"waterEvidence",water},{"lakeEvidence",lakeEvidence},{"chunk",{{"x",cx},{"z",cz},{"decodedHeight",decoded.height},{"bytes",bytes.size()},{"blocksAbove319",highBlocks},{"highestGeneratedBlock",highestBlock},{"airCellsAtOrAbove500",airAbove500},{"bedrockVerified",true},{"revisitByteEqual",true}}},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<stage<<": "<<e.what()<<'\n';return 1;}}
