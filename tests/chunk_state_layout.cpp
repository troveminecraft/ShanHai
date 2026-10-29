#undef NDEBUG
#include "ChunkStateLayout.h"
#include "ViewGapRecovery.h"
#include "StreamingBudget.h"
#include <cassert>
#include <iostream>
using namespace terrain_runtime;
template<int Loaded> void verify() {
    using Layout=ChunkStateLayout<Loaded>;
    assert(Layout::canonical(Loaded)==13);
    assert(Layout::canonical(Loaded-1)==12);
    assert(Layout::canonical(Loaded-2)==11);
    assert(Layout::canonical(Loaded-3)==10);
    assert(Layout::canonical(-1)==-1 && Layout::canonical(255)==255);
    for(int i=0;i<8;++i)assert(Layout::canonical(i)==i);
    ViewGapRecovery gaps;gaps.begin();
    // Reproduce the real snapshot: 554 completed chunks must exert no pressure.
    for(int x=0;x<554;++x)gaps.observe(x,0,Layout::canonical(Loaded),true,0);
    gaps.end();assert(gaps.gaps.empty());
    SupplementalBudget budget;assert(budget.update({0,0,unsigned(gaps.gaps.size()),true},0,false));
    gaps.begin();gaps.observe(0,0,Layout::canonical(Loaded-3),true,0);gaps.end();
    assert(gaps.next(9,0,0));
    gaps.begin();gaps.observe(0,0,Layout::canonical(Loaded),true,10);gaps.end();assert(gaps.gaps.empty());
}
int main(){verify<11>();verify<13>();std::cout<<"PASS native/canonical lifecycle and completed-chunk pressure regression\n";}
