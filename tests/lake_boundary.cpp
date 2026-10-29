#undef NDEBUG
#include "native/LakeBoundary.h"
#include <cassert>
#include <set>
#include <iostream>
struct Ground {int height; bool safe=true;};
int main(){using namespace nativegen::regional;
 auto eligible=[](Ground s){return s.safe;};
 auto basin=[](int x,int z){return Ground{10+(x*x+z*z)/40};};
 auto lake=closedLake(-20,-20,41,20,20,15,12,basin,eligible,32);
 assert(lake.level==15&&!lake.rows.empty());std::set<std::pair<int,int>> wet;
 for(auto row:lake.rows)for(int x=row[1];x<=row[2];++x)wet.emplace(x,row[0]);
 for(auto [x,z]:wet)for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}})
  if(basin(x+dx,z+dz).height<lake.level)assert(wet.contains({x+dx,z+dz}));
 // A single-block escape between coarse sample points must reject the mask.
 auto leak=[&](int x,int z){return z==1&&x>=0?Ground{10}:basin(x,z);};
 assert(closedLake(-20,-20,41,20,20,15,12,leak,eligible,32).rows.empty());
 // A spill saddle allows a lower, fully closed water level instead of a cut.
 auto saddle=[&](int x,int z){return z==1&&x>=0?Ground{13}:basin(x,z);};
 auto lower=closedLake(-20,-20,41,20,20,15,12,saddle,eligible,32);assert(lower.level==13);
 assert(lower.samples<=41*41); // heights are sampled once across all attempts
 auto repeat=closedLake(-20,-20,41,20,20,15,12,saddle,eligible,32);assert(repeat.rows==lower.rows);
 std::cout<<"PASS exact shoreline, narrow escape, lower spill level, bounded deterministic sampling\n";
}
