#include "BiomeSearch.h"
#include <Windows.h>
#include <fstream>
#include <future>
#include <iostream>
uint64_t digest(std::vector<uint8_t>const&v){uint64_t h=1469598103934665603ULL;for(auto b:v)h=(h^b)*1099511628211ULL;return h;}
int main(int argc,char**argv){try{
 if(argc!=3)return 2;terrain_kernel::J data;std::ifstream(argv[1])>>data;
 std::vector<std::pair<int,int>> coords;
 for(auto centre:{std::pair{263,-160},std::pair{177,-178}})
  for(int z=-2;z<=1;++z)for(int x=-2;x<=1;++x)coords.emplace_back(centre.first+x,centre.second+z);
 terrain_kernel::J rows=terrain_kernel::J::array();std::vector<uint64_t> reference;
 for(bool search:{false,true,true,false}){
  auto ctx=std::make_shared<terrain_kernel::Context>();std::vector<std::unique_ptr<terrain_kernel::Generator>> engines;
  for(int i=0;i<2;++i)engines.push_back(std::make_unique<terrain_kernel::Generator>(data,terrain_kernel::Reservations{},false,nullptr,nullptr,ctx));
  for(auto [x,z]:coords)engines[0]->terrain().prepareRegions(x*16+32768,z*16+32768);
  std::unique_ptr<terrain_runtime::BiomeSearchJob> job;
  if(search)job=std::make_unique<terrain_runtime::BiomeSearchJob>(data,6000,6000,std::vector{999},true,
    []{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);});
  std::atomic_size_t next=0;std::vector<uint64_t> hashes(coords.size());std::vector<std::future<void>> workers;
  auto began=std::chrono::steady_clock::now();
  for(int w=0;w<2;++w)workers.push_back(std::async(std::launch::async,[&,w]{
   for(size_t i=next++;i<coords.size();i=next++){auto [x,z]=coords[i];hashes[i]=digest(engines[w]->chunk(x,z));}
  }));
  for(auto&w:workers)w.get();double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
  bool active=job&&!job->done();if(job){job->cancel();job.reset();}
  if(reference.empty())reference=hashes;else if(reference!=hashes)throw std::runtime_error("Search altered generation output");
  rows.push_back({{"search",search},{"searchActiveThroughout",active},{"chunks",coords.size()},{"seconds",seconds},{"identical",true}});
  std::cout<<rows.back()<<std::endl;
 }
 std::ofstream(argv[2])<<rows.dump(2);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
