#undef NDEBUG
#include "ForestCapabilities.h"
#include "PaletteContract.h"
#include "native/ForestPalette.h"
#include "native/RegionalPlanStore.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace forest_capabilities;using J=nlohmann::json;
 J legacy={{"config",{{"seed",32}}},{"regionalGeneration",{{"algorithm","regional-drainage-v3"}}},{"seedManifest",{{"terrainSeed","32"},{"templateSha256","test"}}}};
 assert(recipe(legacy)==Profile::Legacy);
 auto old=nativegen::regional::identity(legacy);
 auto common=legacy;common["config"]["forestEcologyRevision"]=1;common["capabilityProfile"]=Common;
 auto modern=common;modern["capabilityProfile"]=Modern;
 assert(recipe(common)==Profile::Bedrock2620&&!dappled(recipe(common)));
 assert(recipe(modern)==Profile::Bedrock2651&&dappled(recipe(modern)));
 requireHost(common,false);requireHost(modern,true);requireMatch(common,common);
 auto basins=common;basins["config"]["forestEcologyRevision"]=2;
 assert(recipe(basins)==Profile::Bedrock2620);
 auto canopy=basins;canopy["config"]["forestEcologyRevision"]=3;canopy["capabilityProfile"]=CommonCanopy;
 assert(recipe(canopy)==Profile::Bedrock2620);requireHost(canopy,false);
 auto const&contract=terrain_runtime::paletteContract();auto metadata=canopy;
 metadata["palette"]=contract["palette"];metadata["biomeNames"]=J::array();
 for(int i=0;i<62;++i)metadata["biomeNames"].push_back(contract["biomeNames"][i]);
 for(int i=223;i<239;++i)metadata["palette"][i]="air";
 terrain_runtime::validatePaletteContract(metadata);
 auto mismatched=metadata;mismatched["palette"][223]=contract["palette"][223];bool rejected=false;
 try{terrain_runtime::validatePaletteContract(mismatched);}catch(std::exception const&){rejected=true;}assert(rejected);
 rejected=false;try{requireMatch(basins,canopy);}catch(std::exception const&){rejected=true;}assert(rejected);
 assert(nativegen::regional::identity(basins)!=nativegen::regional::identity(common));
 assert(old!=nativegen::regional::identity(common));
 assert(nativegen::regional::identity(modern)!=nativegen::regional::identity(common));
 auto changed=common;changed["config"]["villageSpacing"]=16;
 assert(nativegen::regional::identity(changed)==nativegen::regional::identity(common));
 for(int mode=0;mode<5;++mode){
  auto invalid=common;bool rejected=false;
  try {
   if(mode==0){invalid.erase("capabilityProfile");recipe(invalid);}
   if(mode==1){invalid["config"].erase("forestEcologyRevision");recipe(invalid);}
   if(mode==2){invalid["capabilityProfile"]="future";recipe(invalid);}
   if(mode==3)requireHost(modern,false);
   if(mode==4)requireMatch(modern,common);
  }catch(std::exception const&){rejected=true;}
  assert(rejected);
 }
 assert(nativegen::forest_palette::Shelf1West<256);
 std::cout<<"forest target isolation, recipe validation and cache identities passed\n";
}
