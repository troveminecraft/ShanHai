#include "TerrainKernel.h"
#include <fstream>
#include <iostream>
#include <future>
#include <atomic>
#include <chrono>
#include <numeric>
uint64_t digest(std::vector<uint8_t> const&v){uint64_t h=1469598103934665603ULL;for(auto b:v)h=(h^b)*1099511628211ULL;return h;}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;terrain_kernel::J data;std::ifstream(argv[1])>>data;
 std::vector<std::pair<int,int>> coords;
 for(auto centre:{std::pair{-8,89},std::pair{89,137}})for(int z=-3;z<3;++z)for(int x=-3;x<3;++x)coords.emplace_back(centre.first+x,centre.second+z);
 std::vector<uint64_t> reference;terrain_kernel::J results=terrain_kernel::J::array();
 for(int workers:{2,4,4,2}){
  auto ctx=std::make_shared<terrain_kernel::Context>();std::vector<std::unique_ptr<terrain_kernel::Generator>> engines;
  for(int i=0;i<workers;++i)engines.push_back(std::make_unique<terrain_kernel::Generator>(data,terrain_kernel::Reservations{},false,nullptr,nullptr,ctx));
  // Both runs warm the same regional plans, then measure identical actual chunks.
  for(auto cp:coords)engines[0]->terrain().prepareRegions(cp.first*16+32768,cp.second*16+32768);
  std::vector<uint64_t> hashes(coords.size());std::atomic_size_t next=0;std::vector<std::future<void>> jobs;
  auto start=std::chrono::steady_clock::now();
  for(int w=0;w<workers;++w)jobs.push_back(std::async(std::launch::async,[&,w]{for(size_t i=next++;i<coords.size();i=next++){auto[x,z]=coords[i];hashes[i]=digest(engines[w]->chunk(x,z));}}));
  for(auto&job:jobs)job.get();double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  if(reference.empty())reference=hashes;else if(reference!=hashes)throw std::runtime_error("worker-count output mismatch");
  results.push_back({{"workers",workers},{"chunks",coords.size()},{"seconds",seconds},{"chunksPerSecond",coords.size()/seconds},{"identical",true}});
 }
 std::cout<<results.dump(2)<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
