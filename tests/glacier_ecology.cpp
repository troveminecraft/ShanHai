#undef NDEBUG
#include "native/Engine.h"
#include "PolarBearHabitat.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <queue>
#include <tuple>
namespace nativegen {
struct GlacierEcologyRegression {
 static void ecology(J data){
  data["config"]["glacierEcologyRevision"]=1;
  Field field(data);Engine engine(data,{},false,&field);
  field.iceAtlas={{true,0,0,1600,1600,0,0}};field.iceAtlasReady=true;
  double previous=-1;int previousCount=-1;J fade=J::array();
  for(int distance:{-256,0,128,256,512,768,1024,1280}){
   Surface s;s.height=150;s.T=.5;s.H=.5;field.applySnowVariant(s,32768+1728+distance,32768);
   assert(s.glacierVegetation>=previous&&s.glacierVegetation>=0&&s.glacierVegetation<=1);
   int count=0;for(int x=0;x<4096;++x)count+=engine.glacierPlantKept(s,x,731);
   assert(count>=previousCount);previousCount=count;previous=s.glacierVegetation;
   Surface warm=s;warm.glacierVegetation=1;
   assert(std::abs(field.ecologyTreeWeight(s)-field.ecologyTreeWeight(warm)*s.glacierVegetation)<1e-10);
   fade.push_back({{"distance",distance},{"retention",s.glacierVegetation},{"plants",count}});
  }
  assert(previousCount==4096);
  std::array<Surface,256> surfaces;std::vector<uint16_t> raw(256*576);
  for(int i=0;i<256;++i){auto&s=surfaces[i];s.height=98;s.water=100;s.snowCover=i<128?1:0;raw[i*576+164]=8;raw[i*576+165]=86;}
  engine.variantSnowCover(32768,32768,raw,surfaces);
  for(int i=0;i<256;++i){assert(raw[i*576+164]==(i<128?9:8));assert(raw[i*576+165]==(i<128?0:86));}
  using namespace glacier_ecology;
  assert(polarBearHabitat(true,false,false,false));
  assert(polarBearHabitat(false,false,true,true));
  assert(!polarBearHabitat(false,true,true,true));
  assert(!polarBearHabitat(false,false,true,false));
  assert(!polarBearHabitat(false,false,false,true));
  assert(restrictPolarBear(true,true,true,false));
  assert(!restrictPolarBear(false,true,true,false));
  assert(!restrictPolarBear(true,false,true,false));
  assert(!restrictPolarBear(true,true,false,false));
  assert(!restrictPolarBear(true,true,true,true));
  std::cout<<J{{"vegetationFade",fade},{"frozenAndWarmLilies",256},{"bearPolicy",true}}.dump(2)<<std::endl;
 }
 static void spires(J data){
  J legacy=data;data["config"]["glacierEcologyRevision"]=1;legacy["config"].erase("glacierEcologyRevision");
  Field field(data);Engine after(data,{},false,&field),before(legacy,{},false,&field);
  J rows=J::array();
  for(auto const& region:field.inspectIceRegions()){
   int cx=int(region.at("x").get<double>()/96),cz=int(region.at("z").get<double>()/96);
   for(int gz=cz-10;gz<=cz+10&&rows.size()<3;++gz)for(int gx=cx-10;gx<=cx+10&&rows.size()<3;++gx){
    auto r=[&](int salt){return hash01(gx,gz,after.terrainSeed^salt);};
    int X=int(std::floor((gx+.18+.64*r(51301))*96)),Z=int(std::floor((gz+.18+.64*r(51303))*96));
    auto s=field.surface(X+32768,Z+32768,false);if(s.superGlacier<=.65||s.height>320)continue;
    auto d=after.iceSpireDescriptor(gx,gz,X,Z);if(!d.valid)continue;
    auto old=before.iceSpireDescriptor(gx,gz,X,Z);
    double lean=std::hypot(d.ax,d.az);assert(d.height>=88&&d.height<=159&&lean>=d.height*.4&&lean<=d.height*.7);
    double radius=0;for(auto v:d.distance)radius=std::max(radius,v);assert(radius<=20&&2*radius/d.height<.46);
    // Isolate one real accepted descriptor. Exercise the production clipping
    // across all intersecting chunks, then reverse generation order.
    Engine isolated(data,{},false,&field);
    for(int z=gz-5;z<=gz+5;++z)for(int x=gx-5;x<=gx+5;++x)isolated.iceSpireDescriptors.get(key2(x,z),[]{return Engine::IceSpireDescriptor{};});
    isolated.iceSpireDescriptors.m[key2(gx,gz)]=d;
    std::vector<std::pair<int,int>> chunks;
    for(int z=int(std::floor((Z-120)/16.));z<=int(std::floor((Z+120)/16.));++z)
    for(int x=int(std::floor((X-120)/16.));x<=int(std::floor((X+120)/16.));++x)chunks.emplace_back(x,z);
    using Cell=std::tuple<int,int,int>;std::set<Cell> voxels;
    std::map<std::pair<int,int>,uint64_t> hashes;
    int top=-64;double tipX=0,tipZ=0;int tipN=0;
    for(int pass=0;pass<2;++pass){
     for(auto [cx,cz]:chunks){
      std::array<Surface,256> surface;for(auto& v:surface){v.height=d.base;v.glacier=1;v.superGlacier=1;}
      std::vector<uint16_t> raw(256*576);isolated.refinedIceSpires(cx*16+32768,cz*16+32768,raw,surface);
      uint64_t hash=1469598103934665603ULL;
      for(int i=0;i<256;++i)for(int y=d.base+1;y<=511;++y){auto b=raw[i*576+y+64];hash=(hash^b)*1099511628211ULL;
       if(!pass&&b){int x=cx*16+i%16,z=cz*16+i/16;voxels.emplace(x,y,z);top=std::max(top,y);if(y>=d.base+d.height-2){tipX+=x;tipZ+=z;++tipN;}}
      }
      if(pass)assert(hashes.at({cx,cz})==hash);else hashes[{cx,cz}]=hash;
     }
     std::reverse(chunks.begin(),chunks.end());
    }
    assert(top>=d.base+d.height-1&&top<=502&&tipN>0);
    double displacement=std::hypot(tipX/tipN-X,tipZ/tipN-Z);assert(displacement>d.height*.35);
    std::set<Cell> reached;std::queue<Cell> queue;
    for(auto const& cell:voxels)if(std::get<1>(cell)==d.base+1){reached.insert(cell);queue.push(cell);}
    while(!queue.empty()){auto[x,y,z]=queue.front();queue.pop();for(auto[dx,dy,dz]:{Cell{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}){Cell n{x+dx,y+dy,z+dz};if(voxels.contains(n)&&reached.insert(n).second)queue.push(n);}}
    assert(reached.size()==voxels.size());
    rows.push_back({{"anchor",{X,Z}},{"base",d.base},{"height",d.height},{"radius",radius},{"lean",lean},{"voxelTop",top},{"tipDisplacement",displacement},{"connectedVoxels",voxels.size()},{"oldLean",std::hypot(old.ax,old.az)}});
    std::cerr<<"spire "<<rows.size()<<" checked\n";
   }if(rows.size()==3)break;
  }
  assert(rows.size()==3);std::cout<<J{{"spires",rows}}.dump(2)<<std::endl;
 }
};}
int main(int argc,char**argv){try{if(argc!=2)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 nativegen::regional::Store cache;nativegen::regional::StoreScope scope(cache);
 nativegen::regional::storage(data,std::filesystem::path(argv[1]).parent_path()/"ecology-regional-cache");
 nativegen::GlacierEcologyRegression::ecology(data);nativegen::GlacierEcologyRegression::spires(data);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
