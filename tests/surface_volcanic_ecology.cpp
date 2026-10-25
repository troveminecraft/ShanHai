#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct EngineBoundaryTest {
 static J run(Engine&e){
  std::array<Surface,256>s;for(auto&a:s){a.height=100;a.T=.6;a.H=.8;}
  auto volume=[](){std::vector<uint16_t> r(256*576);for(int i=0;i<256;++i){std::fill(r.begin()+i*576,r.begin()+i*576+164,2);r[i*576+164]=4;r[i*576+163]=3;r[i*576+162]=3;}return r;};
  int moss=0,azalea=0;
  for(int cz=-8;cz<8;++cz)for(int cx=-8;cx<8;++cx){
   auto raw=volume();for(int i=0;i<256;++i){raw[i*576+84]=80;std::fill(raw.begin()+i*576+85,raw.begin()+i*576+99,0);raw[i*576+99]=80;}
   auto before=raw;e.surfaceMoss(32768+cx*16,32768+cz*16,raw,s);
   for(int i=0;i<256;++i){moss+=raw[i*576+164]==80;azalea+=raw[i*576+165]==121||raw[i*576+165]==122;
    for(int y=0;y<164;++y)assert(raw[i*576+y]==before[i*576+y]);
   }
   auto dry=s;for(auto&a:dry)a.H=.2;auto unchanged=before;e.surfaceMoss(32768+cx*16,32768+cz*16,unchanged,dry);assert(unchanged==before);
  }
  assert(moss>0&&azalea>0);
  int baseDiamonds=0,hotDiamonds=0;
  for(int z=-4;z<4;++z)for(int x=-4;x<4;++x){
   auto baseline=volume(),hot=baseline;auto heat=s;for(auto&a:heat)a.volcanoHeat=1;
   e.vanillaOreProfiles(32768+x*16,32768+z*16,baseline,s);
   e.vanillaOreProfiles(32768+x*16,32768+z*16,hot,heat);
   // A bonus vein can claim stone before a later ore batch does. It cannot
   // replace soil/air/fluid, nor remove an existing baseline diamond vein.
   for(size_t i=0;i<baseline.size();++i){bool a=baseline[i]==106||baseline[i]==114,b=hot[i]==106||hot[i]==114;baseDiamonds+=a;hotDiamonds+=b;if(a)assert(b);if(b&&!a)assert(baseline[i]==2||baseline[i]==95||(baseline[i]>=100&&baseline[i]<=115));}
  }
  double ratio=double(hotDiamonds)/baseDiamonds;assert(ratio>1.6&&ratio<2.4);
  return {{"mossBlocks",moss},{"azaleasAboveLushCaves",azalea},{"baselineDiamonds",baseDiamonds},{"centralDiamonds",hotDiamonds},{"diamondRatio",ratio},{"dryGroundUnchanged",true},{"undergroundUnchangedBySurfaceMoss",true}};
 }
};
}
int main(int argc,char**argv){try{using namespace nativegen;assert(argc==3);J d;std::ifstream(argv[1])>>d;d["config"]["surfaceMoss"]=1;d["config"]["volcanoRevision"]=1;d["config"]["generatedMaxYExclusive"]=512;Engine e(d,{},false);auto r=EngineBoundaryTest::run(e);std::ofstream(argv[2])<<r.dump(2);std::cout<<r.dump()<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
