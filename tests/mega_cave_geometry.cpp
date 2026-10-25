#include "native/CavernGroups.h"
#include <iostream>
#include <cassert>
#undef NDEBUG
#include <cassert>
int main(){
 size_t rooms=0,columns=0;
 for(uint32_t seed:{288493980u,77242676u,987654321u}){
  nativegen::CavernGroups a(seed,true),b(seed,true);
  auto h=[](int,int){return 140.;};
  auto list=a.inventory(-10000,9999,h);
  for(auto const&g:list)for(auto const&r:g.at("nodes"))if(r.at("mega").get<bool>()){
   ++rooms;int x=int(r.at("x").get<double>()),z=int(r.at("z").get<double>());
   std::vector<nativegen::CavernGroups::Column> expected;
   for(int d=-128;d<=128;d+=8)expected.push_back(a.column(x+d,z,h));
   int i=int(expected.size())-1;
   for(int d=128;d>=-128;d-=8){auto got=b.column(x+d,z,h);if(got!=expected[i--])throw std::runtime_error("order mismatch");
    // Y=-56 is the first permitted cave layer; protect -64 through -57.
    if(got[0]&255ULL)throw std::runtime_error("bedrock cover carved");++columns;}
  }
 }
 if(rooms<10)throw std::runtime_error("missing rare rooms");
 std::cout<<"{\"seeds\":3,\"finiteRooms\":"<<rooms<<",\"columns\":"<<columns<<",\"orderIndependent\":true}"<<std::endl;
}
