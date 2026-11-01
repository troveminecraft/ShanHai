#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;using namespace nativegen::regional;
J straight(){J segments=J::array();for(int i=0;i<128;++i)segments.push_back({400+i*4.,600.,404+i*4.,600.,160.,160.,6.,6.,200+i*4.,204+i*4.,1.,0.});
 return {{"segments",segments},{"routes",J::array({{{"firstSegment",0},{"segmentCount",128}}})}};}
int main(int argc,char**argv){
 int branches=0;for(uint32_t seed=0;seed<64;++seed){auto a=straight(),b=a;
  auto ground=[](int,int){return Ground{162,.7,.5,0,0};};addRiverBranches(a,0,0,seed,ground,J::array());addRiverBranches(b,0,0,seed,ground,J::array());assert(a==b);
  for(auto const&r:a["branches"]){++branches;size_t first=r["firstSegment"],last=first+size_t(r["segmentCount"])-1;auto const&p=a["segments"][int(r["parentFirst"])],&q=a["segments"][int(r["parentLast"])];
   auto const&s=a["segments"][first],&e=a["segments"][last];assert(s[0]==p[0]&&s[1]==p[1]&&s[4]==p[4]&&e[2]==q[2]&&e[3]==q[3]&&e[5]==q[5]);
   double excursion=0;for(size_t i=first;i<=last;++i){auto const&t=a["segments"][i];excursion=std::max(excursion,std::abs(double(t[1])-600));assert(double(t[4])>=double(t[5]));if(i>first)assert(t[0]==a["segments"][i-1][2]&&t[1]==a["segments"][i-1][3]);}
   assert(excursion>17);assert(a["branchDiagnostics"]["attempts"]<=12);
  }
  for(int kind=0;kind<3;++kind){auto fail=straight();addRiverBranches(fail,0,0,seed,[&](int,int){return Ground{kind==0?150:kind==1?185:162,.7,.5,0,kind==2?1.:0.};},J::array());assert(fail["branches"].empty());}
 }
 assert(branches>20);J out={{"flatCatchments",64},{"branches",branches},{"unsafeBankCases",192},{"passed",true}};
 if(argc==3){J data;std::ifstream(argv[1])>>data;data["config"]["riverBranches"]=1;Field field(data);J sites=J::array();
  int wet=0;
  for(auto [x,z]:{std::pair{0,0},{1,2},{-1,0},{2,2},{-2,-2},{3,1},{-3,1},{1,-3}}){auto p=field.inspectRegionalDrainage(x,z);for(auto const&r:p.at("branches")){
   sites.push_back({{"region",{x,z}},{"branch",r}});int first=r["firstSegment"],end=first+int(r["segmentCount"]);
   for(int i=first;i<end;++i){auto const&s=p["segments"][i];int X=roundJS((double(s[0])+double(s[2]))*.5),Z=roundJS((double(s[1])+double(s[3]))*.5);
    for(int z=Z-5;z<=Z+5;++z)for(int x=X-5;x<=X+5;++x){auto a=field.surface(x+32768,z+32768,false);if(a.water<=a.height)continue;++wet;
     for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=field.surface(x+dx+32768,z+dz+32768,false);
      assert(b.height>=a.water||b.water>b.height);if(b.water>b.height)assert(a.height<b.water&&b.height<a.water);
     }
    }
   }
  }}
  out["checkedWetColumns"]=wet;
  out["sites"]=sites;std::ofstream(argv[2])<<out.dump(2);
 }
 std::cout<<out.dump()<<std::endl;
}
