#include "ViewGapRecovery.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("View gap recovery invariant");}
int main(){using terrain_runtime::ViewGapRecovery;ViewGapRecovery v;
 v.begin();v.observe(12,0,false,0);v.observe(0,0,true,0);v.end();check(!v.next(4,0,0));
 auto g=v.next(5,0,0);check(g&&g->x==12);g->attempted=5;check(!v.next(100,0,0));
 v.begin();v.observe(12,0,true,101);v.end();check(v.gaps.empty());
 v.begin();v.observe(-20,-8,false,102);v.end();check(v.next(108,0,0));
 v.begin();v.observe(50,50,false,109);v.end();check(v.gaps.size()==1&&!v.next(110,50,50));
 v.begin();v.observe(50,50,false,115);v.observe(51,50,false,115);v.end();check(v.next(115,50,50)->x==50);
 std::cout<<"PASS distant holes, grace, one attempt per stall, loaded release, negative coordinates, view movement\n";
}
