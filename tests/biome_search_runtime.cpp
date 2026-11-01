#undef NDEBUG
#include "BiomeSearch.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=2)return 2;terrain_kernel::J recipe;std::ifstream(argv[1])>>recipe;
 recipe["config"]["forestEcologyRevision"]=2;
 auto began=std::chrono::steady_clock::now();
 {
  terrain_runtime::BiomeSearchJob cancelled(recipe,0,0,{999},true);
  cancelled.cancel();
 } // Closing a world cancels and joins; no native world object is retained.
 int X=34731,Z=11099;
 terrain_runtime::BiomeSearchJob job(recipe,X,Z,{61},true);
 while(!job.done()){
  assert(std::chrono::steady_clock::now()-began<std::chrono::seconds(60));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 auto const&r=job.get();assert(r.error.empty()&&r.found&&!r.interrupted);
 terrain_kernel::Field independent(recipe,std::make_shared<terrain_kernel::Context>());
 auto p=*r.found;auto s=independent.surface(p[0]+32768,p[2]+32768,false);
 assert(s.biome==61&&s.closedBasin&&s.water>s.height);
 assert(p[1]==s.water+1);
 std::cout<<"background recipe query rechecked in independent field: "<<p[0]<<','<<p[1]<<','<<p[2]<<"; cancellation joined\n";
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
