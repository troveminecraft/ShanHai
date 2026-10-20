#include "rules/StructureWater.h"
#include <cassert>
using namespace worldgen_rules;
struct Env {
 std::map<std::array<int,3>,int> primary,secondary;
 int get(StructurePoint p){return primary[{p.x,p.y,p.z}];}
 bool air(int b){return b==0;}
 bool canContainWater(int b){return b==2;}
 void water(StructurePoint p,int layer){(layer?secondary:primary)[{p.x,p.y,p.z}]=8;}
};
int main(){
 Env e;e.primary[{0,30,0}]=1;e.primary[{1,30,0}]=2;e.primary[{2,32,0}]=2;
 submergeStructure({{0,29,0},{31,35,0}},{{0,-64,0},{15,319,15}},[](int,int){return 31;},e);
 assert((e.primary[{0,30,0}]==1&&e.secondary[{0,30,0}]==0));
 assert((e.primary[{1,30,0}]==2&&e.secondary[{1,30,0}]==8));
 assert((e.secondary[{2,32,0}]==0&&e.primary[{2,31,0}]==8));
 assert((!e.primary.contains({16,30,0})));
}
