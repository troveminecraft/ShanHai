#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace nativegen {
struct NaturalLandformsRegression {
 static J check(J original){
  J fewer=original;fewer["config"]["cliffs"]["chance"]=.24;
  J next=fewer;next["config"]["mountainErosion"]=1;next["config"]["naturalForestRevision"]=1;next["config"]["cliffTrees"]=1;
  Field a(original),b(fewer),c(next);int before=0,after=0,cut=0,retained=0;double maxCut=0;J witnesses=J::array(),samples=J::array();
  for(int z=112;z<144;++z)for(int x=112;x<144;++x){auto p=a.patch(x,z),q=b.patch(x,z);before+=p.valid;after+=q.valid;
   if(q.valid)assert(p.valid&&q.x==p.x&&q.z==p.z&&q.drop==p.drop);
   int X=x*256+128,Z=z*256+128;double h=b.uncliff(X,Z),changed=c.uncliff(X,Z),delta=h-changed;
   assert(delta>=-.000001&&delta<=12.000001);cut+=delta>.25;retained+=std::abs(delta)<.000001;maxCut=std::max(maxCut,delta);
  }
  std::cout<<J{{"cliffsBefore",before},{"cliffsAfter",after},{"cut",cut},{"retained",retained},{"maxCut",maxCut}}<<std::endl;
  assert(before>=10&&after>=3&&after<before*.7&&cut>0&&retained>100);
  for(auto [X,Z]:{std::pair{2915,-3487},std::pair{2949,-3469},std::pair{2873,-2815}}){
   auto s=a.surface(X+32768,Z+32768,false);int step=0;
   for(int z=Z-16;z<=Z+16;++z)for(int x=X-16;x<=X+16;++x){auto p=a.surface(x+32768,z+32768,false);
    for(auto [dx,dz]:{std::pair{1,0},std::pair{0,1}})step=std::max(step,std::abs(p.height-a.surface(x+dx+32768,z+dz+32768,false).height));}
   samples.push_back({{"x",X},{"z",Z},{"height",s.height},{"biome",s.biome},{"largestAdjacentStepIn33x33",step}});
  }
  // A small cliff tree must actually attach to rock and cross chunk borders
  // identically regardless of the order in which neighbouring chunks arrive.
  Engine e(next);int inspected=0;
  for(int z=112;z<144&&witnesses.size()<2&&inspected<16;++z)for(int x=112;x<144&&witnesses.size()<2&&inspected<16;++x){
   if(!e.field.patch(x,z).valid||hash01(x,z,e.terrainSeed^89401)>.65)continue;++inspected;
   auto t=e.field.cliffTreeAnchor(x,z);if(!t.length)continue;
   auto s=e.field.surface(t.x,t.z,false);assert(e.field.material(s,t.x,t.y,t.z)==2);
   std::vector<std::pair<int,int>> chunks;int cx=int(std::floor(t.x/16.))*16,cz=int(std::floor(t.z/16.))*16;
   for(int dz=-16;dz<=16;dz+=16)for(int dx=-16;dx<=16;dx+=16)chunks.emplace_back(cx+dx,cz+dz);
   auto draw=[&]{std::map<std::pair<int,int>,std::vector<uint16_t>> result;
    for(auto [X,Z]:chunks){std::array<Surface,256> surfaces;std::vector<uint16_t> raw(256*e.Height);
     for(int i=0;i<256;++i)surfaces[i]=e.field.surface(X+i%16,Z+i/16,false);
     e.cliffSideTrees(X,Z,raw,surfaces);result.emplace(std::pair{X,Z},std::move(raw));}return result;};
   auto one=draw();std::reverse(chunks.begin(),chunks.end());assert(one==draw());int wood=0,leaves=0;
   for(auto const&[key,raw]:one)for(int id:raw){wood+=id==forest_palette::DarkWood;leaves+=id==forest_palette::CanopyDarkLeaves;}
   assert(wood>4&&leaves>10);witnesses.push_back({{"anchor",{t.x-32768,t.y,t.z-32768}},{"wood",wood},{"leaves",leaves}});
  }
  assert(!witnesses.empty());return {{"cliffsBefore",before},{"cliffsAfter",after},{"erodedSamples",cut},{"unchangedSamples",retained},
   {"maximumErosionCut",maxCut},{"feedback",samples},{"cliffTrees",witnesses},{"passed",true}};
 }
};}
int main(int argc,char**argv){try{if(argc!=3)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 auto result=nativegen::NaturalLandformsRegression::check(data);std::ofstream(argv[2])<<result.dump(2);std::cout<<result<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
