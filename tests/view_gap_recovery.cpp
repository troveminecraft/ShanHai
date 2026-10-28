#include "ViewGapRecovery.h"
#include "StreamingPriority.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("View gap recovery invariant");}
int main(){using terrain_runtime::ViewGapRecovery;
 // A large unrequested frontier must never launch supplemental generation.
 ViewGapRecovery frontier;frontier.begin();
 for(int x=-64;x<=64;++x)for(int z=-64;z<=64;++z)frontier.observe(x,z,-1,false,0);
 frontier.end();check(frontier.gaps.empty()&&!frontier.next(100,0,0));
 // Existing unfinished outer dependency rings are also expected frontier,
 // not permanent pressure that closes the budget while the queue is idle.
 frontier.begin();unsigned excluded=0;
 for(int x=-52;x<=52;++x)for(int z=-52;z<=52;++z){
  if(x*x+z*z>52*52)continue;
  bool interior=terrain_runtime::interiorDependencyChunk(x,z,52,false);
  if(!interior){++excluded;frontier.observe(x,z,6,false,0,interior);}
 }
 frontier.end();check(excluded>512&&frontier.gaps.empty());
 check(terrain_runtime::interiorDependencyChunk(48,0,52,true));
 frontier.begin();frontier.observe(48,0,6,true,0,true);frontier.end();check(frontier.next(9,0,0));
 ViewGapRecovery v;v.begin();v.observe(12,0,-1,true,0);v.end();check(!v.next(7.9,0,0));
 auto g=v.next(8,0,0);check(g&&g->x==12);v.requested(*g,8);check(!v.next(37,0,0));
 g=v.next(38,0,0);check(g);v.requested(*g,38);check(!v.next(97,0,0));
 g=v.next(98,0,0);check(g);v.requested(*g,98);check(!v.next(1000,0,0));
 v.begin();v.observe(12,0,13,false,1001);v.end();check(v.gaps.empty());
 // Active generation is not a stalled hole: each stage advances the timer.
 v.begin();v.observe(-2,1,2,false,0);v.end();check(!v.next(7,0,0));
 v.begin();v.observe(-2,1,5,false,7);v.end();check(!v.next(14,0,0));check(v.next(15,0,0));
 v.begin();v.observe(-2,1,13,false,16);v.end();check(v.gaps.empty());
 // Abandoned/outer-view observations are discarded, including retry history.
 v.begin();v.observe(45,0,-1,true,0);v.end();check(v.next(8,0,0));
 v.begin();v.observe(45,0,-1,false,9);v.end();check(v.gaps.empty());
 std::vector<terrain::StreamFocus> focus{{0,0,8,0,0,true,30,0}};
 check(terrain::streamingScore(0,0,focus)<terrain::streamingScore(30,0,focus));
 check(terrain::streamingScore(40,0,focus)<terrain::streamingScore(41,0,focus));
 check(terrain_runtime::recoveryViewRadius(48)==48&&terrain_runtime::recoveryViewRadius(96)==64);
 check(terrain_runtime::RepairSlots==1&&terrain_runtime::RepairDependencyRadius-terrain_runtime::RepairCoreRadius==8);
 std::cout<<"PASS frontier exclusion, progress-aware stall timer, enclosed holes, bounded retries and dependency halo\n";
}
