#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <queue>
using namespace nativegen;
namespace nativegen {
struct TerrainRevisionTest {
 static J pools(J data){
  data.erase("regionalGeneration");data["config"]["badlandsRegions"]=1;
  auto old=data;old["config"].erase("smallPoolsVersion");data["config"]["smallPoolsVersion"]=2;
  Field before(old),after(data);int water=0,clusters=0,triples=0,lava=0;J examples=J::array();
  for(int ring=0;ring<=35 && (water<12||triples<1);++ring)
   for(int gz=204-ring;gz<=204+ring;++gz)for(int gx=204-ring;gx<=204+ring;++gx){
    if(std::max(std::abs(gx-204),std::abs(gz-204))!=ring)continue;
    auto p=after.smallPool(gx,gz);if(!p.valid)continue;auto b=before.smallPool(gx,gz);assert(b.valid);
    assert(p.count>=1&&p.count<=3);
    if(p.lava){assert(p.level==b.level&&p.count==1);++lava;continue;}
    assert(p.level==b.level+1);++water;clusters+=p.count>1;triples+=p.count==3;
    std::set<std::pair<int,int>> wet;
    for(int z=p.z-28;z<=p.z+28;++z)for(int x=p.x-28;x<=p.x+28;++x){
     auto s=after.surface(x,z);
     if(Field::poolDistance(p,x,z)<1){assert(s.water==p.level&&s.height<p.level);wet.insert({x,z});}
    }
    assert(!wet.empty());auto remaining=wet;std::queue<std::pair<int,int>> queue;queue.push(*remaining.begin());remaining.erase(queue.front());
    while(!queue.empty()){auto [x,z]=queue.front();queue.pop();for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){
     auto q=std::pair{x+dx,z+dz};if(remaining.erase(q))queue.push(q);
     if(!wet.contains(q)){auto bank=after.surface(q.first,q.second);assert(bank.height>=p.level);}
    }}
    assert(remaining.empty()); // All lobes form one contained four-connected water body.
    if(examples.size()<16||p.count==3)examples.push_back({{"x",p.x-32768},{"z",p.z-32768},{"level",p.level},{"count",p.count},{"wetColumns",wet.size()}});
   }
  assert(water>=12&&clusters>0&&triples>0);
  return {{"water",water},{"clusters",clusters},{"triples",triples},{"lava",lava},{"examples",examples}};
 }
 static J mesas(J data){
  data["config"].update(J{{"badlandsRegions",2},{"seaLevel",40},{"maxY",511},{"generatedMaxYExclusive",512},{"mountainPeakY",480}});
  data["regionalGeneration"]["algorithm"]="regional-drainage-v3";
  Field field(data);auto old=data;old["config"]["badlandsRegions"]=1;Field before(old);
  int count=0,cliffs=0,dry=0,borders=0;double gain=0;J regions=J::array();
  for(int ring=0;ring<7&&regions.size()<2;++ring)for(int gz=8-ring;gz<=8+ring&&regions.size()<2;++gz)for(int gx=8-ring;gx<=8+ring&&regions.size()<2;++gx){
   if(std::max(std::abs(gx-8),std::abs(gz-8))!=ring)continue;auto r=field.inspectBadlandsRegion(gx,gz);if(!r["valid"].get<bool>())continue;
   int x0=r["x"].get<int>()+32768,z0=r["z"].get<int>()+32768;
   for(int dz=-1024;dz<=1024;dz+=64)for(int dx=-1024;dx<=1024;dx+=64){int x=x0+dx,z=z0+dz;
    auto s=field.surface(x,z);assert(s.height<512);
    if(field.dryHabitatWeight(x,z)>=.25){assert(s.biome!=9&&s.biome!=10&&s.biome!=13&&s.biome!=23);++dry;}
    if(s.badlands){++count;gain+=s.height-before.surface(x,z).height;
     auto e=field.surface(x+1,z),n=field.surface(x,z+1);cliffs+=std::max(std::abs(s.height-e.height),std::abs(s.height-n.height))>=2;
     for(auto t:{e,n,field.surface(x-1,z),field.surface(x,z-1)}){assert(t.badlands||t.biome==11||t.biome==16||Field::badlandsOcean(t));++borders;}
    }
   }
   regions.push_back(r);
  }
  assert(count>0&&cliffs>0&&dry>0&&gain/count>15);
  return {{"badlandsSamples",count},{"meanHeightGain",gain/count},{"steepSamples",cliffs},{"drySamples",dry},{"boundaryNeighbours",borders},{"regions",regions}};
 }
};
}
int main(int argc,char**argv){try{
 assert(argc==3);J data;std::ifstream(argv[1])>>data;J report;
 report["pools"]=TerrainRevisionTest::pools(data);std::cout<<report.dump()<<std::endl;
 report["mesas"]=TerrainRevisionTest::mesas(data);report["gameplayTestPerformed"]=false;
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
