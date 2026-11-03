#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <chrono>

namespace nativegen {
struct ForestEcologyTest {
 static J run(J data){
  Field f(data);auto inventory=f.inspectElevationForms(-20000,20000);J basins=J::array();
  for(auto const& site:inventory["basins"]){
   if(site["mangrove"].get<bool>())continue;
   int x=roundJS(site["x"])+32768,z=roundJS(site["z"])+32768;
   int lo=1000,hi=-1000,belowSea=0;double radius=site["radiusX"];
   for(int dz=-4;dz<=4;++dz)for(int dx=-4;dx<=4;++dx){
    auto s=f.base(x+dx*60,z+dz*60);if(s.basinWeight<.65)continue;
    lo=std::min(lo,s.height);hi=std::max(hi,s.height);
    if(s.height<100){
     if(!s.closedBasin||s.kind==1||s.kind==3)std::cerr<<J{{"site",site},{"sample",{x+dx*60-32768,z+dz*60-32768}},{"height",s.height},{"water",s.water},{"kind",s.kind},{"basin",s.basinWeight}}<<std::endl;
     assert(s.closedBasin&&s.kind!=1&&s.kind!=3);++belowSea;
    }
   }
   assert(radius>=600&&hi-lo>=8&&belowSea>0);
   basins.push_back({{"center",{x-32768,z-32768}},{"diameter",2*radius},{"interiorMin",lo},{"interiorMax",hi},{"belowSeaInlandSamples",belowSea}});
   if(basins.size()==3)break;
  }
  assert(!basins.empty());
  // Exercise the production boulder placement over supported flat ground,
  // including features crossing chunk edges. No hand-written boulder mesh.
  data.erase("regionalGeneration");Engine e(data);e.field.skinCache=Cache<uint64_t,Surface>(131072);
  Surface s;s.height=140;s.water=s.lava=-32768;s.biome=7;s.T=.6;s.H=.5;
  for(int z=-16;z<272;++z)for(int x=-16;x<272;++x)e.field.skinCache.get(key2(x,z),[&]{return s;});
  // Keep only fully surveyed candidates inside the fixture.
  for(int z=-1;z<=4;++z)for(int x=-1;x<=4;++x)if(x<0||z<0||x>3||z>3)
   e.largeBoulders.get(key2(x,z),[]{return Engine::LargeBoulder{};});
  int rocks=0,highest=0,columns=0;std::map<std::pair<int,int>,std::vector<uint16_t>> chunks;
  auto draw=[&](int x,int z){std::vector<uint16_t> raw(256*e.Height);std::array<Surface,256> surface;surface.fill(s);
   for(int i=0;i<256;++i)raw[i*e.Height+204]=4;
   e.largeBoulderFeatures(x,z,raw,surface);return raw;};
  for(int z=0;z<256;z+=16)for(int x=0;x<256;x+=16){auto raw=draw(x,z);
   for(int i=0;i<256;++i){bool occupied=false;
    for(int y=141;y<160;++y){auto b=raw[i*e.Height+y+64];if(!b)continue;
     assert(b==2||b==20);int below=raw[i*e.Height+y+63];assert(below==4||below==2||below==20);
     ++rocks;occupied=true;highest=std::max(highest,y-140);
    }columns+=occupied;
   }chunks.emplace(std::pair{x,z},std::move(raw));
  }
  for(auto it=chunks.rbegin();it!=chunks.rend();++it)assert(it->second==draw(it->first.first,it->first.second));
  assert(rocks>100&&columns>40&&highest>=3);
  return {{"basins",basins},{"boulderBlocks",rocks},{"boulderColumns",columns},{"boulderHeight",highest},{"supportedAndOrderIndependent",true}};
 }
};
}
int main(int argc,char**argv){try{if(argc!=3)return 2;nativegen::J d;std::ifstream(argv[1])>>d;
 auto t=std::chrono::steady_clock::now();auto r=nativegen::ForestEcologyTest::run(d);
 r["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();std::ofstream(argv[2])<<r.dump(2);std::cout<<r<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
