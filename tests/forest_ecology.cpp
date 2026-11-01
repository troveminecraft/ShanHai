#undef NDEBUG
#include "native/Engine.h"
#include "rules/StructureRules.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <queue>
#include <tuple>

namespace nativegen {
struct ForestEcologyTest {
 using Cell=std::tuple<int,int,int>;
 static J scan(J const&data,int expected){
  Field f(data);Engine e(data,{},false,&f);J trees=J::object();std::map<int,int> counts;
  int scanned=0;
  for(int z=-4096;z<4096;z+=64){
   for(int x=-4096;x<4096;x+=64){
    auto s=f.surface(x+32768,z+32768,false);++scanned;++counts[s.biome];
    if(!forest_capabilities::biome(s.biome))continue;
    auto key=std::to_string(s.biome);if(trees.contains(key))continue;
    int gx=int(std::floor((x+32768)/12.)),gz=int(std::floor((z+32768)/12.));
    for(int dz=-2;dz<=2&&!trees.contains(key);++dz)for(int dx=-2;dx<=2;++dx){
     auto a=e.specialTreeCandidate(gx+dx,gz+dz);if(!a.height||a.biome!=s.biome)continue;
     trees[key]={{"x",a.x-32768},{"z",a.z-32768},{"ground",a.ground},{"height",a.height},{"temperature",s.T},{"humidity",s.H},{"biome",a.biome}};break;
    }
   }
   if(int(trees.size())>=expected)break;
   if(z%1024==0)std::cerr<<"forest scan z="<<z<<" candidates="<<trees.size()<<'\n';
  }
  J histogram=J::object();for(auto[k,v]:counts)histogram[std::to_string(k)]=v;
  J out={{"samples",scanned},{"trees",trees},{"biomes",histogram}};std::cout<<out.dump()<<std::endl;
  assert(int(trees.size())>=expected);return out;
 }
 static void habitats(J const&data){
  Field f(data);std::set<int> found;
  for(int z=-4096;z<4096;z+=79)for(int x=-4096;x<4096;x+=83){
   Surface s;s.biome=9;s.height=180;s.H=.70;s.T=.52;s.ridge=.20;s.seaLevel=100;
   auto base=s;f.forestHabitat(s,x+32768,z+32768);found.insert(s.biome);
   if(forest_capabilities::biome(s.biome)){
    for(int problem=0;problem<7;++problem){auto b=base;
     if(problem==0)b.badlands=1;if(problem==1)b.desert=.8;if(problem==2)b.water=b.height;
     if(problem==3)b.river=.8;if(problem==4)b.glacier=.8;if(problem==5)b.height=400;
     if(problem==6)b.cliffLow=70;
     f.forestHabitat(b,x+32768,z+32768);assert(b.biome==9);
    }
   }
  }
  assert(found.contains(forest_capabilities::Dark)&&found.contains(forest_capabilities::Pale));
  J legacy=data;legacy.erase("capabilityProfile");legacy["config"].erase("forestEcologyRevision");Field old(legacy);
  Surface s;s.biome=9;s.height=180;s.H=.70;s.T=.52;old.forestHabitat(s,32768,32768);assert(s.biome==9);
  s.biome=forest_capabilities::Dark;s.height=160;assert(worldgen_rules::extraAnchorAllowed(7,s,true));
  s.biome=9;assert(!worldgen_rules::extraAnchorAllowed(7,s,true));
 }
 static J model(J const&data,int biome){
  Field field(data);Engine e(data,{},false,&field);
  constexpr int X=32767,Z=32767,gx=X/12,gz=Z/12;
  Engine::ForestTree a;a.x=X;a.z=Z;a.ground=150;a.feet.fill(150);a.height=21;a.width=2;a.biome=biome;a.crown=4.2;a.angle=.7;a.heart=biome==forest_capabilities::Pale;
  for(int z=gz-8;z<=gz+8;++z)for(int x=gx-8;x<=gx+8;++x)e.specialTreeCache.get(key2(x,z),[]{return Engine::ForestTree{};});
  e.specialTreeCache.m[key2(gx,gz)]=a;
  auto model=e.specialTreeModel(gx,gz);
  std::map<Cell,int> expected;
  for(int phase=0;phase<2;++phase)for(auto c:model->cells)if(c.phase==phase)expected[{c.x,c.y,c.z}]=c.block;
  int hearts=0;std::set<Cell> wood;
  for(auto const&[p,b]:expected){
   if(b==forest_palette::Heart){++hearts;auto[x,y,z]=p;
    for(auto[dx,dy,dz]:{Cell{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})
     assert(expected.at({x+dx,y+dy,z+dz})==forest_palette::PaleLog);
   }
   if(!forest_palette::leaves(b))wood.insert(p);
  }
  assert(hearts==(a.heart?1:0));
  std::queue<Cell> q;std::set<Cell> reached;
  for(auto const&p:wood)if(std::get<1>(p)==151){q.push(p);reached.insert(p);}
  while(!q.empty()){auto[x,y,z]=q.front();q.pop();for(auto[dx,dy,dz]:{Cell{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}){Cell next{x+dx,y+dy,z+dz};if(wood.contains(next)&&reached.insert(next).second)q.push(next);}}
  assert(reached==wood);
  std::vector<std::pair<int,int>> chunks;for(int z=-2;z<=1;++z)for(int x=-2;x<=1;++x)chunks.emplace_back(x,z);
  std::map<std::pair<int,int>,uint64_t> hashes;int actualHearts=0,actualWood=0;
  for(int pass=0;pass<2;++pass){
   for(auto[cx,cz]:chunks){std::array<Surface,256> surfaces;std::vector<uint16_t> raw(256*576);
    for(int i=0;i<256;++i){surfaces[i].height=150;surfaces[i].biome=biome;raw[i*576+214]=4;}
    e.specialForests(32768+cx*16,32768+cz*16,raw,surfaces);
    uint64_t hash=1469598103934665603ULL;
    for(int i=0;i<int(raw.size());++i){auto b=raw[i];hash=(hash^b)*1099511628211ULL;if(!pass){actualHearts+=b==forest_palette::Heart;if(b==forest_palette::DarkLog||b==forest_palette::DarkWood||b==forest_palette::PaleLog||b==forest_palette::PaleWood||b==forest_palette::Heart)++actualWood;}}
    if(pass)assert(hashes.at({cx,cz})==hash);else hashes[{cx,cz}]=hash;
   }
   std::reverse(chunks.begin(),chunks.end());
  }
  assert(actualHearts==hearts&&actualWood==int(wood.size()));
  return {{"biome",biome},{"connectedWood",wood.size()},{"heart",hearts},{"negativeBoundaryChunks",chunks.size()},{"orderIndependent",true}};
 }
};
}
int main(int argc,char**argv){try{
 if(argc<2)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 int expected=argc>3?std::stoi(argv[3]):2;
 data["capabilityProfile"]=expected==5?forest_capabilities::Modern:forest_capabilities::Common;data["config"]["forestEcologyRevision"]=1;
 nativegen::regional::Store store;nativegen::regional::StoreScope scope(store);
 if(argc>2&&std::string(argv[2])=="--scan"){nativegen::ForestEcologyTest::scan(data,expected);return 0;}
 using T=nativegen::ForestEcologyTest;T::habitats(data);
 std::cout<<T::model(data,forest_capabilities::Dark)<<'\n'<<T::model(data,forest_capabilities::Pale)<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
