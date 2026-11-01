#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <queue>

namespace nativegen {
struct ForestEcologyTest {
 static void run(J data){
  data["capabilityProfile"]=forest_capabilities::Common;
  data["config"]["forestEcologyRevision"]=2;
  Field f(data);Engine engine(data,{},false,&f);
  auto oldData=data;oldData["config"]["forestEcologyRevision"]=1;Field old(oldData);
  if(data.at("seedManifest").at("terrainSeed")=="34"){
   // Regression: this warm water-filled bowl lies inside the glacier collar;
   // the previous candidate had biome 61 but zero vegetation and zero trees.
   int x=961+32768,z=-14012+32768;auto s=f.surface(x,z);
   assert(s.closedBasin&&s.basinWeight>.75&&s.water>s.height&&s.glacierVegetation==0);
   assert(s.biome!=forest_capabilities::Mangrove&&!f.mangroveBasinAt(x,z));
   std::cout<<"Seed34 warm basin inside glacier collar correctly rejected\n";
  }
  auto sites=f.inspectElevationForms(-40000,40000);int checked=0,treeCount=0;
  std::cerr<<"basin candidates="<<sites["basins"].size()<<'\n';
  for(auto const&site:sites["basins"]){
   if(!site.value("mangrove",false))continue;
   int cx=roundJS(site["x"].get<double>())+32768,cz=roundJS(site["z"].get<double>())+32768;
   std::cerr<<"checking "<<site.dump()<<'\n';
   constexpr int N=65,step=8;std::array<bool,N*N> wet{};
   int cells=0;
   for(int iz=0;iz<N;++iz)for(int ix=0;ix<N;++ix){
    int x=cx+(ix-N/2)*step,z=cz+(iz-N/2)*step;auto s=f.surface(x,z);
    if(ix%8==0&&iz%8==0){auto o=old.surface(x,z);assert(s.height==o.height&&s.water==o.water&&s.lava==o.lava);}
    if(s.biome!=forest_capabilities::Mangrove)continue;
    assert(s.closedBasin&&s.basinWeight>.75&&s.water>s.height&&s.water-s.height<=6);
    assert(s.basinWater==s.water&&s.lava<s.height);wet[iz*N+ix]=true;++cells;
   }
   if(!cells)continue;
   int largestSpan=0,components=0;
   for(int i=0;i<N*N;++i)if(wet[i]){
    ++components;std::queue<int> q;q.push(i);wet[i]=false;
    int minX=N,maxX=0,minZ=N,maxZ=0;
    while(!q.empty()){int a=q.front();q.pop();int x=a%N,z=a/N;
     minX=std::min(minX,x);maxX=std::max(maxX,x);minZ=std::min(minZ,z);maxZ=std::max(maxZ,z);
     for(auto [dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){int X=x+dx,Z=z+dz;
      if(X>=0&&X<N&&Z>=0&&Z<N&&wet[Z*N+X]){wet[Z*N+X]=false;q.push(Z*N+X);}
     }
    }
    largestSpan=std::max(largestSpan,step*std::max(maxX-minX,maxZ-minZ));
   }
   assert(largestSpan>=160);
   int trees=0;
   for(int z=(cz-64)/8;z<=(cz+64)/8;++z)for(int x=(cx-64)/8;x<=(cx+64)/8;++x){
    auto a=engine.specialTreeCandidate(x,z,true);if(!a.height)continue;++trees;
    auto s=f.surface(a.x,a.z);assert(s.biome==forest_capabilities::Mangrove&&s.water>a.ground);
    assert(a.stemBottom>=s.water+2&&a.stemBottom<s.seaLevel);
    for(auto p:a.rootFeet){auto b=f.surface(p[0],p[2]);assert(p[1]==b.height&&b.water>p[1]);}
   }
   // The 128-square core should contain several trees per chunk, not isolated
   // coastal candidates. This tests actual accepted anchors, not raw probability.
   std::cout<<J{{"basin",{cx-32768,cz-32768}},{"span",largestSpan},{"waterCells",cells},{"components",components},{"coreTrees",trees}}.dump()<<std::endl;
   if(trees<80){auto s=f.surface(cx,cz);std::cerr<<J{{"biome",s.biome},{"height",s.height},{"water",s.water},{"glacier",s.glacier},{"ice",s.iceHabitat},{"vegetation",s.glacierVegetation},{"T",s.T},{"ecologyT",f.ecologyTemperature(s)},{"soil",f.material(s,cx,s.height,cz)},{"blocked",engine.blocked(cx,cz,false)},{"treeWeight",f.ecologyTreeWeight(s)}}.dump()<<'\n';}
   assert(trees>=80);treeCount+=trees;++checked;
   if(checked==3)break;
  }
  assert(checked>=1);
  // A sea-level mudflat and an inland rift must never become new mangroves.
  Surface coast;coast.biome=10;coast.height=98;coast.water=100;coast.seaLevel=100;coast.T=.8;coast.H=.8;
  for(int i=0;i<100;++i){auto s=coast;f.forestHabitat(s,32000+i*32,32000);assert(s.biome!=forest_capabilities::Mangrove);
   s=coast;s.closedBasin=true;s.riftWeight=.4;f.forestHabitat(s,32000+i*32,32000);assert(s.biome!=forest_capabilities::Mangrove);}
  std::cout<<"basins="<<checked<<" actual rooted trees="<<treeCount<<"; geometry unchanged\n";
 }
};
}
int main(int argc,char**argv){try{if(argc!=2)return 2;nativegen::J data;std::ifstream(argv[1])>>data;nativegen::ForestEcologyTest::run(data);}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
