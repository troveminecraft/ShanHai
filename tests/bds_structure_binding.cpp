#include "BdsStructureBinding.h"
#include <cassert>
using J=nlohmann::json;
using namespace terrain_runtime;
int main(){
 J activation={{"worldName","1"},{"worldSeed","-180712423969634352"},{"terrainSeed","-180712423969634352"},{"sha256","profile-1"}};
 J metadata=activation;metadata.erase("worldName");
 J input={{"seedManifest",{{"terrainSeed",activation["terrainSeed"]}}}};
 validateStructureBinding(activation,metadata,input,activation);
 assert(structureWorldName(activation)=="1");
 auto rejected=[&](J const&a,J const&m,J const&i,J const&w){try{validateStructureBinding(a,m,i,w);}catch(std::exception const&){return true;}return false;};
 auto old=activation;old["worldName"]="previous-test-world";
 assert(rejected(activation,metadata,input,old));
 auto wrongSeed=input;wrongSeed["seedManifest"]["terrainSeed"]="42";assert(rejected(activation,metadata,wrongSeed,activation));
 auto wrongProfile=metadata;wrongProfile["sha256"]="previous-profile";assert(rejected(activation,wrongProfile,input,activation));
 auto wrongWorldSeed=metadata;wrongWorldSeed["worldSeed"]="42";assert(rejected(activation,wrongWorldSeed,input,activation));
 auto renamed=activation;renamed["worldName"]="renamed-world";
 validateStructureBinding(renamed,metadata,input,renamed);
 for(auto name:{"",".","..","../world","folder/world","folder\\world","C:world"}){auto bad=activation;bad["worldName"]=name;assert(rejected(bad,metadata,input,bad));}
 // World and terrain seeds may deliberately differ in a historical profile.
 auto independent=activation;independent["worldSeed"]="42";auto profile=metadata;profile["worldSeed"]="42";
 validateStructureBinding(independent,profile,input,independent);
}
