#undef NDEBUG
#include "TerrainKernel.h"
#include "ForestCapabilities.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc,char**argv){try{
 if(argc!=2)return 2;
 terrain_kernel::J recipe;std::ifstream(argv[1])>>recipe;
 recipe.erase("capabilityProfile");recipe["config"].erase("forestEcologyRevision");
 terrain_kernel::Field legacy(recipe,std::make_shared<terrain_kernel::Context>());
 recipe["capabilityProfile"]=forest_capabilities::Common;recipe["config"]["forestEcologyRevision"]=1;
 terrain_kernel::Field common(recipe,std::make_shared<terrain_kernel::Context>());
 recipe["capabilityProfile"]=forest_capabilities::Modern;
 terrain_kernel::Field modern(recipe,std::make_shared<terrain_kernel::Context>());
 int samples=0,forests=0;
 for(int z=-3400;z<=4000;z+=251)for(int x=-3400;x<=3400;x+=257){
  auto a=legacy.surface(x+32768,z+32768),b=common.surface(x+32768,z+32768),c=modern.surface(x+32768,z+32768);
  for(auto const&s:{b,c}){
   assert(s.height==a.height&&s.water==a.water&&s.lava==a.lava);
   assert(s.cliffLow==a.cliffLow&&s.cliffHigh==a.cliffHigh&&s.lake==a.lake);
   assert(s.riverCutLow==a.riverCutLow&&s.riverCutHigh==a.riverCutHigh&&s.riverCutWater==a.riverCutWater);
   assert(s.snowCover==a.snowCover&&s.glacierVegetation==a.glacierVegetation);
  }
  assert(b.biome!=forest_capabilities::Dappled);
  forests+=forest_capabilities::biome(b.biome)||forest_capabilities::biome(c.biome);++samples;
 }
 assert(forests>0);
 std::cout<<"Unchanged elevation, water, rivers, cliffs and snow fields across "<<samples<<" real columns; forest columns="<<forests<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
