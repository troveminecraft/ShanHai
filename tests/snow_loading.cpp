#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
int main(int argc,char**argv){
 assert(argc==3);J data;std::ifstream(argv[1])>>data;
 data["config"]["seaLevel"]=40;data["config"]["generatedMaxYExclusive"]=512;data["config"]["mountainPeakY"]=480;
 data["config"]["badlandsRegions"]=3;data["config"]["riverBankSurvey"]=1;
 data["regionalGeneration"]["algorithm"]="regional-drainage-v3";data["rivers"]["algorithm"]="regional-drainage-v3";
 data["config"]["mountainSnowline"]=0;J newer=data;newer["config"]["mountainSnowline"]=1;
 Field oldSnow(data),newSnow(newer);int samples=0,low=0,oldCount=0,newCount=0,raised=0,peaks=0;double oldY=0,newY=0;
 // Natural surface survey isolates the snow belt from rivers/structures; the
 // chunk checks below separately exercise the complete streamed generator.
 for(int z=-4096;z<=4096;z+=128)for(int x=-4096;x<=4096;x+=128){
  auto a=oldSnow.bootstrapSurface(x+32768,z+32768),b=newSnow.bootstrapSurface(x+32768,z+32768);++samples;
  assert(a.height==b.height&&a.water==b.water);
  if(a.height<=80){assert(a.surf==b.surf&&a.biome==b.biome);++low;}
  if(a.stone||a.volcanic||a.giant||a.badlands||a.kind||a.lake||a.river||a.height<100)continue;
  if(a.surf==2){++oldCount;oldY+=a.height;}
  if(b.surf==2){++newCount;newY+=b.height;}
  if(a.surf==2&&b.surf!=2)++raised;
  if(b.height>=470&&b.surf==2)++peaks;
 }
 assert(low>0&&raised>0&&newCount>0&&peaks>0&&oldCount>newCount);
 assert(newY/newCount>oldY/oldCount);
 // Compare the conservative owner bounds against the full legacy 3x3 planner,
 // including negative/world-region borders and the reported river coordinate.
 std::vector<std::pair<int,int>> chunks{{0,0},{-58,25},{127,127},{128,-1},{-129,-129},{64,64}};
 std::vector<std::vector<uint8_t>> expected;J timings=J::array();
 auto clear=[](){std::lock_guard lock(regional::mutex);regional::plans.clear();regional::residentOrder.clear();};
 clear();
 {Field field(data,false);Engine engine(data,{},false,&field);for(auto[x,z]:chunks){auto t=Clock::now();expected.push_back(engine.chunk(x,z));timings.push_back({{"chunk",{x,z}},{"fullOwnersMs",ms(t)}});}}
 clear();
 {Field field(data,true);Engine engine(data,{},false,&field);for(size_t i=0;i<chunks.size();++i){auto[x,z]=chunks[i];auto t=Clock::now();auto bytes=engine.chunk(x,z);timings[i]["culledOwnersMs"]=ms(t);assert(bytes==expected[i]);}
  for(size_t i=chunks.size();i-->0;){auto[x,z]=chunks[i];assert(engine.chunk(x,z)==expected[i]);}}
 clear();
 {Field field(data,true);Engine engine(data,{},false,&field);for(size_t i=chunks.size();i-->0;){auto[x,z]=chunks[i];assert(engine.chunk(x,z)==expected[i]);}}
 J report{{"snow",{{"samples",samples},{"lowlandUnchanged",low},{"oldSnowColumns",oldCount},{"newSnowColumns",newCount},{"meanOldSnowY",oldY/oldCount},{"meanNewSnowY",newY/newCount},{"retainedSnowySummits",peaks},{"formerSnowNowExposed",raised}}},{"chunks",timings},{"encodedChunksIdentical",true},{"reverseExplorationIdentical",true},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}
