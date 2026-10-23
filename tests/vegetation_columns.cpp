#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rules/StructureReservations.h"
#include <array>
#include <cassert>
#include <set>
#include <vector>
#include <iostream>
int main(){
 using namespace worldgen_rules;
 for(int height:{384,576}){
  std::vector<uint16_t>before(256*height,2),after=before;
  std::set<std::array<int,3>>expected{{0,4,0},{1,63,0},{1,height-1,0},{15,height-1,15}};
  for(auto p:expected){size_t index=size_t(p[2]*16+p[0])*height+p[1];before[index]=10;after[index]=0;}
  // An unchanged plant and the protected four bottom cells must not be visited.
  before[7*height+4]=after[7*height+4]=10;before[3]=10;after[3]=0;
  std::set<std::array<int,3>>seen;
  visitVegetationReservations(before,after,[&](int x,int y,int z,int old,int replacement){
   assert(old==10&&replacement==0);assert(structureReservationRemoves(old,replacement,"minecraft:oak_log"));
   assert(seen.insert({x,y,z}).second);
  });
  assert(seen==expected&&vegetationColumnHeight(before,after)==height);
  // These exact production iterators feed both native volume and live repair.
  // y is relative; restoring world Y must reach 319/511 at the last cell.
  assert(seen.contains({15,height-1,15})&&height-1-64==(height==384?319:511));
 }
 for(auto sizes:{std::pair{256*384,256*576},std::pair{256*512,256*512},std::pair{0,0},std::pair{256*576-1,256*576-1}}){
  bool rejected=false;std::vector<uint16_t>before(sizes.first),after(sizes.second);
  try{visitVegetationReservations(before,after,[](auto...){assert(false);});}catch(std::runtime_error const&){rejected=true;}
  assert(rejected);
 }
 std::cout<<"Production vegetation traversal: 384/576 columns, last X/Z/Y, unchanged substrate and malformed buffers verified\n";
}
