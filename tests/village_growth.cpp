#include "rules/VillageGrowth.h"
#include <cassert>
#include <set>
using namespace worldgen_rules;
int main(){std::set<int>scales;
 for(int x=-100;x<100;++x){auto a=villageGrowthBudget({x,-3},42,12,80),b=villageGrowthBudget({x,-3},42,12,80);
  assert(a.scale==b.scale&&a.buildings==12*a.scale&&a.pieces<=4096&&a.depth<=24);scales.insert(a.scale);
 }assert((scales==std::set<int>{3,6,9}));
 assert(villageBuilding("village/snowy/houses/snowy_small_house_1"));assert(!villageBuilding("village/plains/streets/straight_1"));
}
