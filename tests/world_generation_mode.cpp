#include "WorldGenerationMode.h"
#include "ChunkCache.h"
#include <cassert>
#include <iostream>
#include <set>
int main(){
 using Mode=terrain_runtime::WorldGenerationMode;
 using J=nlohmann::json;
 auto frozen=Mode::read(J{{"sizeChunks",2500}},J::object());
 assert(frozen.contains(-1250,-1250)&&frozen.contains(1249,1249));
 assert(!frozen.contains(1250,0)&&!frozen.contains(-1251,0));
 assert(frozen.cacheKey(-1250,-1250)==0&&frozen.cacheKey(1249,1249)==6249999);
 J recipe={{"regionalGeneration",{{"algorithm","regional-drainage-v1"},{"boundsMode","infinite"}}},{"seedManifest",{{"generationMode","regional-on-demand"}}}};
 auto regional=Mode::read(J{{"boundsMode","infinite"},{"sizeChunks",nullptr}},recipe);
 assert(regional.regional&&regional.unbounded&&regional.contains(1300,-1300));
 assert(regional.contains(-1875000,-1875000)&&!regional.contains(1875000,0));
 terrain_cache::ChunkCache<int,128,1,terrain_cache::MixedShard,uint64_t> cache;
 std::set<uint64_t> keys;int count=0;
 for(int x:{-1300,-1,0,1,1300})for(int z:{-1300,-1,0,1,1300}){
  auto k=regional.cacheKey(x,z);assert(keys.insert(k).second);cache.put(k,std::make_shared<int const>(++count));
 }
 count=0;for(int x:{-1300,-1,0,1,1300})for(int z:{-1300,-1,0,1,1300})assert(*cache.get(regional.cacheKey(x,z))==++count);
 bool rejected=false;try{Mode::read(J{{"sizeChunks",2500}},recipe);}catch(std::exception const&){rejected=true;}assert(rejected);
 rejected=false;try{regional.cacheKey(1875000,0);}catch(std::out_of_range const&){rejected=true;}assert(rejected);
 std::cout<<"finite compatibility, regional bounds, signed 64-bit cache keys and mode mismatch: PASS\n";
}
