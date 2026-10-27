#undef NDEBUG
#include "native/Engine.h"
#include <fstream>
#include <iostream>
#include <cassert>
namespace nativegen {
struct IceSpireRegression {
 static void run(J data){
  data["config"]["tallIceSpires"]=false;J next=data;next["config"]["tallIceSpires"]=true;
  Field field(data);Engine before(data,{},false,&field),after(next,{},false,&field);J rows=J::array();
  for(auto const& region:field.inspectIceRegions()){
   int cx=int(region.at("x").get<double>()/96),cz=int(region.at("z").get<double>()/96);
   for(int gz=cz-10;gz<=cz+10&&rows.size()<3;++gz)for(int gx=cx-10;gx<=cx+10&&rows.size()<3;++gx){
    auto r=[&](int salt){return hash01(gx,gz,before.terrainSeed^salt);};
    int X=int(std::floor((gx+.18+.64*r(51301))*96)),Z=int(std::floor((gz+.18+.64*r(51303))*96));
    auto s=field.surface(X+32768,Z+32768,false);if(s.superGlacier<=.65||s.height>320)continue;
    auto a=before.iceSpireDescriptor(gx,gz,X,Z),b=after.iceSpireDescriptor(gx,gz,X,Z);if(!a.valid)continue;
    assert(b.valid&&a.base==b.base&&a.distance==b.distance&&a.height>=48&&a.height<=87&&b.height>=88&&b.height<=159);
    int oldTop=-64,newTop=-64;
    for(int z=int(std::floor((Z-40)/16.));z<=int(std::floor((Z+40)/16.));++z)
    for(int x=int(std::floor((X-40)/16.));x<=int(std::floor((X+40)/16.));++x){
     std::array<Surface,256> surfaces;for(int i=0;i<256;++i)surfaces[i]=field.surface(x*16+i%16+32768,z*16+i/16+32768);
     std::vector<uint16_t> old(256*576),now(old.size());before.refinedIceSpires(x*16+32768,z*16+32768,old,surfaces);after.refinedIceSpires(x*16+32768,z*16+32768,now,surfaces);
     for(int i=0;i<256;++i)if(std::hypot(x*16+i%16-X,z*16+i/16-Z)<38)for(int y=s.height+1;y<=511;++y){
      if(old[i*576+y+64])oldTop=std::max(oldTop,y);if(now[i*576+y+64])newTop=std::max(newTop,y);
     }
    }
    assert(newTop>oldTop+15&&newTop<=502);
    rows.push_back({{"anchor",{X,Z}},{"base",s.height},{"oldHeight",a.height},{"newHeight",b.height},{"oldVoxelTop",oldTop},{"newVoxelTop",newTop}});
   }if(rows.size()==3)break;
  }
  assert(rows.size()==3);std::cout<<rows.dump(2)<<'\n';
 }
};}
int main(int argc,char**argv){try{if(argc!=2)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 nativegen::regional::Store cache;nativegen::regional::StoreScope scope(cache);
 nativegen::regional::storage(data,std::filesystem::path(argv[1]).parent_path()/"regional-cache");
 nativegen::IceSpireRegression::run(data);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
