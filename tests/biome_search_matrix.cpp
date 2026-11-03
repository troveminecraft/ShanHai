#include "BiomeSearch.h"
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=3)return 2;terrain_kernel::J recipe;std::ifstream(argv[1])>>recipe;
 auto state=std::make_shared<terrain_runtime::BiomeSearchState>();terrain_kernel::J results=terrain_kernel::J::array();
 for(auto name:{"roofed_forest","glacier","super_glacier","cherry_grove","mangrove_swamp","inland_basin"}){
  auto id=terrain_runtime::biome_catalog::find(std::string("peakarma:")+name);
  for(int pass=0;pass<2;++pass){auto began=std::chrono::steady_clock::now();
   terrain_runtime::BiomeSearchJob job(recipe,0,0,{id},true,{},true,state);
   while(!job.done())std::this_thread::sleep_for(std::chrono::milliseconds(5));auto r=job.get();
   if(!r.error.empty())throw std::runtime_error(r.error);
   terrain_kernel::J row={{"name",name},{"warm",pass==1},{"seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()},
      {"samples",r.samples},{"interrupted",r.interrupted}};
   if(r.found){auto p=*r.found;terrain_kernel::Field check(recipe);auto s=check.surface(p[0]+32768,p[2]+32768,false);
    if(!terrain_runtime::biome_catalog::matches(terrain_runtime::biome_catalog::entries().at(id),s,true))throw std::runtime_error("Approximate locate result escaped exact verification");row["found"]=p;}
   results.push_back(row);std::cout<<row<<std::endl;
  }
 }
 std::ofstream(argv[2])<<results.dump(2);
 }catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
