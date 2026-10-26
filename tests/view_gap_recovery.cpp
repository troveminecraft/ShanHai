#include "ViewGapRecovery.h"
#include "StreamingPriority.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("View gap recovery invariant");}
int main(){using terrain_runtime::ViewGapRecovery;ViewGapRecovery v;
 v.begin();v.observe(12,0,false,0);v.observe(0,0,true,0);v.end();check(!v.next(4,0,0));
 auto g=v.next(5,0,0);check(g&&g->x==12);v.requested(*g,5);check(!v.next(34,0,0));
 g=v.next(35,0,0);check(g);v.requested(*g,35);check(!v.next(94,0,0));
 g=v.next(95,0,0);check(g);v.requested(*g,95);check(!v.next(1000,0,0));
 v.begin();v.observe(12,0,true,101);v.end();check(v.gaps.empty());
 v.begin();v.observe(-20,-8,false,102);v.end();check(v.next(108,0,0));
 v.begin();v.observe(50,50,false,109);v.end();check(v.gaps.size()==1&&!v.next(110,50,50));
 v.begin();v.observe(50,50,false,115);v.observe(51,50,false,115);v.end();check(v.next(115,50,50)->x==50);
 std::vector<terrain::StreamFocus> focus{{0,0,8,0,0,true,30,0}};
 check(terrain::streamingScore(0,0,focus)<terrain::streamingScore(30,0,focus));
 check(terrain::streamingScore(40,0,focus)<terrain::streamingScore(41,0,focus));
 check(terrain::streamingScore(30,0,focus)<terrain::streamingScore(-30,0,focus));
 check(terrain_runtime::recoveryViewRadius(48)==48&&terrain_runtime::recoveryViewRadius(96)==64);
 ViewGapRecovery area;area.begin();for(int x=35;x<=47;++x)area.observe(x,0,false,0);area.end();
 auto first=area.next(6,0,0);check(first&&first->x==35);area.requested(*first,6);
 auto second=area.next(6,0,0,{{35,0}});check(second&&second->x==40);
 focus[0].repair2=true;focus[0].repair2X=46;focus[0].repair2Z=0;
 check(terrain::streamingScore(56,0,focus)<terrain::streamingScore(57,0,focus));
 check(terrain_runtime::RepairDependencyRadius-terrain_runtime::RepairCoreRadius==8);
 std::cout<<"PASS bounded retries, full 48-chunk view, disjoint cores, two complete dependency halos\n";
}
