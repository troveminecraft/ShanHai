#undef NDEBUG
#include "BiomeSearch.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=2)return 2;terrain_kernel::J recipe;std::ifstream(argv[1])>>recipe;
 auto began=std::chrono::steady_clock::now();
 {
  terrain_runtime::BiomeSearchJob cancelled(recipe,0,0,{999},true);
  cancelled.cancel();
 } // Closing a world cancels and joins; no native world object is retained.
 int X=2088,Z=-3256;
 terrain_kernel::Field independent(recipe,std::make_shared<terrain_kernel::Context>());
 auto expected=independent.surface(X+32768,Z+32768,false);
 terrain_runtime::BiomeSearchJob job(recipe,X,Z,{expected.biome},true);
 while(!job.done()){
  assert(std::chrono::steady_clock::now()-began<std::chrono::seconds(60));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 auto const&r=job.get();assert(r.error.empty()&&r.found&&!r.interrupted);
 auto p=*r.found;auto s=independent.surface(p[0]+32768,p[2]+32768,false);
 assert(s.biome==expected.biome&&p[0]==X&&p[2]==Z);
 assert(p[1]==std::max(s.height,s.water)+1);
 // Query using the public catalog, not the many-to-one native climate names.
 int catalogId=-1;
 auto const& entries=terrain_runtime::biome_catalog::entries();
 for(size_t i=0;i<entries.size();++i)
  if(terrain_runtime::biome_catalog::matches(entries[i],s,recipe.at("config").value("elevationRevision",0)>=1)){
   catalogId=int(i);break;
  }
 assert(catalogId>=0);
 terrain_runtime::BiomeSearchJob catalogJob(recipe,p[0],p[2],{catalogId},true,{},true);
 while(!catalogJob.done())std::this_thread::sleep_for(std::chrono::milliseconds(10));
 auto const& catalogResult=catalogJob.get();
 assert(catalogResult.error.empty()&&catalogResult.found);
 assert((*catalogResult.found==std::array{p[0],p[1],p[2],catalogId}));
 std::cout<<"background recipe query rechecked in independent field: "<<p[0]<<','<<p[1]<<','<<p[2]<<"; cancellation joined\n";
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
