#include "BiomeSearch.h"
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=4)return 2;terrain_kernel::J data;std::ifstream(argv[1])>>data;
 int target=std::stoi(argv[2]);auto state=std::make_shared<terrain_runtime::BiomeSearchState>();
 terrain_kernel::J rows=terrain_kernel::J::array();
 for(int pass=0;pass<3;++pass){
  auto start=std::chrono::steady_clock::now();terrain_runtime::BiomeSearchResult r;
  if(!pass){terrain_kernel::Field field(data,std::make_shared<terrain_kernel::Context>());
   r=terrain_runtime::searchBiomes(0,0,std::vector{target},[&](int x,int z){return field.surface(x+32768,z+32768,false);},
     [](int,int){return true;},[&]{return std::chrono::steady_clock::now()-start>std::chrono::seconds(45);});
  }else{
   terrain_runtime::BiomeSearchJob job(data,0,0,{target},true,{},false,state);
   while(!job.done())std::this_thread::sleep_for(std::chrono::milliseconds(2));r=job.get();
  }
  if(!r.error.empty())throw std::runtime_error(r.error);
  auto row=terrain_kernel::J{{"mode",pass==0?"old-dense":pass==1?"adaptive-cold":"adaptive-warm"},{"samples",r.samples},
   {"seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()},{"interrupted",r.interrupted}};
  if(r.found){row["found"]=*r.found;terrain_kernel::Field check(data);auto p=*r.found;
   auto s=check.surface(p[0]+32768,p[2]+32768,false);if(s.biome!=target)throw std::runtime_error("Result differs from exact terrain");}
  rows.push_back(row);std::cout<<row<<std::endl;
 }
 std::ofstream(argv[3])<<rows.dump(2);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
