#include "ChunkCache.h"
#include <future>
#include <iostream>
#include <vector>
#include <stdexcept>
struct OldShard{size_t operator()(uint32_t k)const{return k;}};
void require(bool b){if(!b)throw std::runtime_error("Cache regression");}
template<class Selector>int route(int direction){
 terrain_cache::ChunkCache<int,1024,16,Selector> c;std::vector<uint32_t> keys;
 for(int i=0;i<192;i++){uint32_t key=direction==0?(i+32)*256+128:direction==1?128*256+i+32:(i+32)*256+i+32;keys.push_back(key);c.put(key,std::make_shared<int const>(key));}
 int hits=0;for(auto k:keys)if(auto p=c.get(k)){require(*p==int(k));hits++;}return hits;
}
int main(){try{
 int old=route<OldShard>(0),north=route<terrain_cache::MixedShard>(0),east=route<terrain_cache::MixedShard>(1),diagonal=route<terrain_cache::MixedShard>(2);
 require(old==64&&north==192&&east==192&&diagonal==192);
 terrain_cache::ChunkCache<int> c;c.put(0,std::make_shared<int const>(42));auto held=c.get(0);
 for(uint32_t i=1;i<=65536;i++)c.put(i,std::make_shared<int const>(i));require(c.size()==1024&&*held==42&&c.evictions>0);
 std::vector<std::future<void>>jobs;
 for(unsigned worker=0;worker<4;worker++)jobs.push_back(std::async(std::launch::async,[&,worker]{for(uint32_t i=0;i<8192;i++){auto k=(i*37+worker*113)%4096;c.put(k,std::make_shared<int const>(k));if(auto p=c.get(k))require(*p==int(k));}}));
 for(auto&job:jobs)job.get();require(c.size()<=1024&&*held==42);
 // A second world may request the same coordinate with a different seed.
 c.put(17,std::make_shared<int const>(101));auto previous=c.get(17);
 c.clear();require(c.size()==0&&!c.get(17)&&c.evictions==0&&*previous==101);
 c.put(17,std::make_shared<int const>(202));require(*c.get(17)==202&&*previous==101);
 terrain_cache::ChunkCache<int,2,1> lru;lru.put(1,std::make_shared<int const>(1));lru.put(2,std::make_shared<int const>(2));require(bool(lru.get(1)));lru.put(3,std::make_shared<int const>(3));require(!lru.get(2)&&bool(lru.get(1)));
 std::cout<<"{\"oldNorthRouteHits\":"<<old<<",\"northRouteHits\":"<<north<<",\"eastRouteHits\":"<<east<<",\"diagonalRouteHits\":"<<diagonal<<",\"routeLength\":192,\"concurrentOperations\":32768,\"cacheBound\":1024,\"retainedPointerAfterEviction\":true,\"lruVerified\":true}"<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
