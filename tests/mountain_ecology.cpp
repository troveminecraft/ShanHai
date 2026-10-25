#undef NDEBUG
#include "native/Engine.h"
#include "native/EcologyClimate.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace nativegen;
namespace nativegen {
struct EngineBoundaryTest {
 static J surfaceSignature(Surface const&s){
  return {s.height,s.preRiverHeight,s.biome,s.surf,s.T,s.H,s.water,s.lava,s.kind,
          s.river,s.mainRiver,s.lake,s.lakeFrozen,s.wetland,s.stone,s.volcanic,
          s.cliffLow,s.cliffHigh,s.treeD,s.treeK,s.soilSand,s.rock};
 }
 static J altitudeBands(Engine&old,Engine&current){
  auto&f=current.field;Surface s;s.T=.05;s.ecologyT=.30;s.H=.5;s.biome=14;
  // The former 250-block cutoff must no longer discard bare alpine ground.
  s.height=339;assert(!old.coniferHabitat(s)&&current.coniferHabitat(s));
  s.H=.2;assert(!old.mountainHabitat(s)&&current.mountainHabitat(s));s.H=.5;
  J bands=J::array();double previous=1.;
  for(int y=96;y<=480;++y){
   s.height=y;double h=f.ecologyHeight(s),weight=f.ecologyTreeWeight(s);
   assert(h<=y&&weight>=0&&weight<=1&&weight<=previous);previous=weight;
   assert(old.field.ecologyHeight(s)==y&&old.field.ecologyTemperature(s)==s.T&&old.field.ecologyTreeWeight(s)==1.);
   if(y%24==0||y==339)bands.push_back({{"y",y},{"ecologyHeight",h},{"treeWeight",weight}});
  }
  assert(previous==0.);s.height=480;assert(!current.coniferHabitat(s));
  // Every specialised tree family obeys the same cold/upper tree line, while
  // the old recipe preserves its previous biome-only climate choices.
  s.height=300;s.ecologyT=.70;
  s.biome=6;assert(current.coniferHabitat(s));
  s.biome=9;assert(current.broadleafHabitat(s));
  s.biome=23;assert(current.birchHabitat(s));
  s.biome=13;assert(current.jungleHabitat(s));
  s.biome=12;assert(current.wetDryHabitat(s));
  s.height=150;s.ecologyT=.10;
  s.biome=6;assert(old.coniferHabitat(s)&&!current.coniferHabitat(s));
  s.biome=9;assert(old.broadleafHabitat(s)&&!current.broadleafHabitat(s));
  s.biome=23;assert(old.birchHabitat(s)&&!current.birchHabitat(s));
  s.biome=13;assert(old.jungleHabitat(s)&&!current.jungleHabitat(s));
  s.biome=12;assert(old.wetDryHabitat(s)&&!current.wetDryHabitat(s));
  for(int y=-64;y<=96;++y){s.height=y;assert(f.ecologyHeight(s)==y&&f.ecologyTemperature(s)==s.T&&f.ecologyTreeWeight(s)==1.);}
  s.height=300;s.stone=.1;assert(f.ecologyHeight(s)==300&&f.ecologyTreeWeight(s)==1.);
  s.stone=0;s.volcanic=.1;assert(f.ecologyHeight(s)==300&&f.ecologyTreeWeight(s)==1.);
  return bands;
 }
 static J actualSites(Engine&before,Engine&after){
  J points=J::array();int eligible=0,fallbackTrees=0;
  for(auto [x,z]:{std::pair{3584,-3328},{3968,-3328},{-2304,-2432},{3840,-3840},
       {3328,-3712},{3456,-3712},{3712,-3840},{3584,-3456},{3712,-3456},
       {3968,-3584},{3968,-3456},{3968,1152}}){
   int X=x+32768,Z=z+32768;auto a=before.field.surface(X,Z),b=after.field.surface(X,Z);
   assert(surfaceSignature(a)==surfaceSignature(b));
   for(int dy:{-8,-1,0,1})assert(before.field.material(a,X,a.height+dy,Z)==after.field.material(b,X,b.height+dy,Z));
   double temperature=after.field.ecologyTemperature(b),height=after.field.ecologyHeight(b);
   assert(height<b.height&&temperature>b.T);
   bool oldConifer=before.coniferHabitat(a),newConifer=after.coniferHabitat(b);
   eligible+=newConifer&&!oldConifer;
   if(x==3968&&z==-3328){
    assert(b.height==219&&b.biome==14&&std::abs(b.T-.040290113256338356)<1e-10);
    assert(temperature>=.22&&temperature<.42&&newConifer&&!oldConifer);
    assert(before.forestFamily(a)==0&&after.forestFamily(b)==1);
   }
   auto structures=after.structures(X,Z,16);
   for(auto const&t:after.smallTrees(X,Z,16,structures)){
    auto at=after.field.surface(t.x,t.z,false);
    assert(after.field.ecologyTreeWeight(at)>0);++fallbackTrees;
   }
   points.push_back({{"x",x},{"z",z},{"height",b.height},{"baseT",b.T},
      {"ecologyT",temperature},{"ecologyHeight",height},{"oldConiferHabitat",oldConifer},
      {"newConiferHabitat",newConifer},{"treeWeight",after.field.ecologyTreeWeight(b)}});
  }
  assert(eligible>0);
  return {{"points",points},{"newConiferHabitatSites",eligible},{"fallbackTreeAnchorsChecked",fallbackTrees},
          {"terrainClimateHydrologyUnchanged",true},{"pointMaterialParity",true}};
 }
};
}

int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("seed-13 recipe and output report required");
 J data;std::ifstream(argv[1])>>data;
 if(data.at("config").at("seed")!=13)throw std::runtime_error("This regression uses the audited seed 13 sites");
 if(data.at("config").value("mountainSnowline",0)!=1||data.at("config").value("mountainPeakY",0)!=480)
  throw std::runtime_error("Expected the existing 480-peak/raised-snowline profile");
 std::filesystem::path output(argv[2]);
 std::filesystem::create_directories(output.parent_path());
 J legacy=data,disabled=data,enabled=data;
 legacy["config"].erase("mountainEcology");disabled["config"]["mountainEcology"]=0;enabled["config"]["mountainEcology"]=1;
 regional::storage(legacy,output.parent_path()/"ecology-legacy-cache");
 regional::storage(disabled,output.parent_path()/"ecology-disabled-cache");
 regional::storage(enabled,output.parent_path()/"ecology-enabled-cache");
 Engine before(legacy),explicitOff(disabled),after(enabled);J report;
 report["altitudeBands"]=EngineBoundaryTest::altitudeBands(before,after);
 report["actualSites"]=EngineBoundaryTest::actualSites(before,after);
 J oldChunks=J::array();
 for(auto [cx,cz]:{std::pair{-24,32},{248,-208}}){
  auto old=before.chunk(cx,cz),off=explicitOff.chunk(cx,cz);assert(old==off);
  oldChunks.push_back({{"cx",cx},{"cz",cz},{"bytes",old.size()},{"identical",true}});
 }
 report["missingFlagVsExplicitZeroChunks"]=oldChunks;
 J lowlands=J::array();
 for(auto [cx,cz]:{std::pair{0,0},{-24,32}}){
  auto a=before.vegetationChunk(cx,cz),b=after.vegetationChunk(cx,cz);assert(a==b);
  lowlands.push_back({{"cx",cx},{"cz",cz},{"identical",true}});
 }
 report["lowlandVegetationChunks"]=lowlands;
 report["gameplayTestPerformed"]=false;
 std::ofstream file(output);file<<report.dump(2);if(!file)throw std::runtime_error("Report write failed");
 std::cout<<report.dump()<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
