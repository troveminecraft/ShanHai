#undef NDEBUG
#include "native/VillageCandidatePlanner.h"
#include <cassert>
#include <fstream>
#include <future>
#include <iostream>
using namespace nativegen;
int main(int argc,char**argv){try{
 assert(argc==3);J recipe;std::ifstream(argv[1])>>recipe;
 uint32_t seed=recipe.at("config").at("seed");VillageCandidatePlanner planner;
 std::atomic_int loads=0;
 // No native registry/global planning Field exists here. These are the cells
 // reached by prefill for the reported spawn chunk (-12,-92).
 std::vector<std::pair<int,int>> cells{{-1,-4},{-1,-3},{-2,-3},{0,-3},{-1,-2}};
 auto load=[&]{++loads;return recipe;};
 std::vector<worldgen_rules::ChunkPosition> reference;
 {Field f(recipe);for(auto[x,z]:cells)reference.push_back(worldgen_rules::villageCandidateForTerrain(x,z,seed,[&](int X,int Z){return f.surface(X+32768,Z+32768);}));}
 std::vector<std::future<void>> tasks;
 for(int worker=0;worker<8;++worker)tasks.push_back(std::async(std::launch::async,[&,worker]{
  for(int n=0;n<40;++n){int i=(n+worker)%cells.size();auto[x,z]=cells[i];auto p=planner.get("seed16-first-chunk",seed,x,z,load);assert(p.x==reference[i].x&&p.z==reference[i].z);}
 }));
 for(auto&task:tasks)task.get();assert(loads==1);
 // The loader's JSON can disappear after construction; Field references only
 // the planner-owned copy. Native stage input resets cannot invalidate it.
 J saved=recipe;recipe=nullptr;
 auto p=planner.get("seed16-first-chunk",seed,cells[0].first,cells[0].second,load);assert(p.x==reference[0].x&&p.z==reference[0].z&&loads==1);
 auto uncached=planner.get("seed16-first-chunk",seed,-2,-4,load);
 assert(worldgen_rules::spacingCell(uncached.x,32)==-2&&worldgen_rules::spacingCell(uncached.z,32)==-4&&loads==1);
 bool rejected=false;try{planner.get("broken-world",seed,0,0,load);}catch(std::exception const&){rejected=true;}assert(rejected);
 recipe=saved;planner.get("broken-world",seed,0,0,load);assert(loads==3);
 planner.clear();planner.get("broken-world",seed,0,0,load);assert(loads==4);
 J report{{"workers",8},{"concurrentRequests",320},{"coldInitializationLoads",1},{"reportedChunk",{-12,-92}},{"candidateCells",cells.size()},{"sameCandidatesAsSerialPolicy",true},{"ownsRecipeLifetime",true},{"failedInitializationRetry",true},{"worldResetVerified",true},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump(2);std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
