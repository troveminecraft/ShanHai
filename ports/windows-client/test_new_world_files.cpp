#include "NewWorldFiles.h"
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs=std::filesystem;
void check(bool ok,char const* message){if(!ok)throw std::runtime_error(message);}
int main(){
 auto base=fs::temp_directory_path()/("karma-world-attachment-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directory(base);
 try {
  auto nativeRoot=fs::path(u8"X:/Minecraft Bedrock/Users/测试/minecraftWorlds/BtD1nceacNE=");
  auto lookup=nativeRoot/"behavior_packs";
  std::u8string expected=u8"X:/Minecraft Bedrock/Users/测试/minecraftWorlds/BtD1nceacNE=/behavior_packs";
  check(client::creation::nativePackPathKey(lookup)==std::string(expected.begin(),expected.end()),
    "native pack key must match the registered forward-slash UTF-8 path");
#ifdef _WIN32
  auto raw=lookup.u8string();
  check(raw!=expected,"Windows fixture must reproduce the mixed-separator lookup failure");
#endif
  auto package=base/"prepared";fs::create_directories(package/"karma/structures");
  std::ofstream(package/"karma/input.json")<<"terrain payload";
  std::ofstream(package/"karma/structures/registry.json")<<"structure payload";
  std::ofstream(package/"karma-worldgen.json")<<R"({"terrainSeed":"1","buildMaxYExclusive":512,"generatedMaxYExclusive":512})";
  fs::create_directory(package/"behavior_packs");
  fs::copy(fs::path(__FILE__).parent_path()/"../../data/build-height-pack",package/"behavior_packs"/client::creation::HeightPackFolder,fs::copy_options::recursive);
  auto root=base/"new-world";
  fs::create_directories(root/"resource_packs");fs::create_directory(root/"behavior_packs");
  std::ofstream(root/"world_resource_packs.json")<<"[]";
  std::ofstream(root/"world_behavior_packs.json")<<R"([{"pack_id":"existing-user-pack","version":[1,2,3],"subpack":"chosen"}])";
  client::creation::installPreparedWorld(root,package);
  check(fs::exists(root/"karma/structures/registry.json"),"missing structure data");
  check(fs::exists(root/"karma-worldgen.json"),"missing marker");
  check(!fs::exists(root/"level.dat"),"must not fabricate level.dat");
  check(!fs::exists(root/"db"),"must not open LevelDB");
  check(fs::file_size(root/"world_resource_packs.json")==2,"native pack config changed");
  auto selected=client::creation::readCreationJson(root/"world_behavior_packs.json");
  check(selected.size()==2&&selected[0].at("subpack")=="chosen","selected user behavior packs were replaced");
  check(fs::exists(root/"behavior_packs"/client::creation::HeightPackFolder/"dimensions/overworld.json"),"missing dimension package");
  client::creation::installPreparedHeightPack(root,package);
  check(client::creation::readCreationJson(root/"world_behavior_packs.json")==selected,"duplicate height selection on repeated handoff");
  // Native selected-pack copying happens after createWorld; startup must merge
  // its most recent result, not restore a stale selection from our first hook.
  std::ofstream(root/"world_behavior_packs.json")<<R"([{"pack_id":"later-user-pack","version":[4,5,6]}])";
  client::creation::installPreparedHeightPack(root,package);
  selected=client::creation::readCreationJson(root/"world_behavior_packs.json");
  check(selected.size()==2&&selected[0].at("pack_id")=="later-user-pack","late native selection was lost");
  auto retained=base/"failed-attachment";
  client::creation::preserveFailedAttachment(root,package,retained);
  check(fs::exists(retained/"karma/input.json")&&fs::exists(retained/"karma-worldgen.json"),"failed attachment was not retained");
  check(!fs::exists(root/"karma")&&!fs::exists(root/"karma-worldgen.json"),"retry root still has managed data");
  check(client::creation::readCreationJson(root/"world_behavior_packs.json")==selected,"retry cleanup changed selected packs");
  client::creation::installPreparedWorld(root,package);
  check(fs::exists(root/"karma/input.json"),"retry could not attach to the native directory");
  std::ofstream(root/"karma/user-added.txt")<<"keep";
  bool refused=false;
  try{client::creation::preserveFailedAttachment(root,package,base/"must-not-move");}catch(std::exception const&){refused=true;}
  check(refused&&fs::exists(root/"karma/user-added.txt")&&!fs::exists(base/"must-not-move"),"modified data was moved during rollback");
  fs::remove(root/"karma/user-added.txt");
  std::ofstream(root/"level.dat")<<"initialized sentinel";
  refused=false;
  try{client::creation::preserveFailedAttachment(root,package,base/"must-not-move-open-world");}catch(std::exception const&){refused=true;}
  check(refused&&fs::exists(root/"karma/input.json")&&fs::exists(root/"level.dat"),"initialized world was touched during rollback");
  auto malformed=base/"malformed";fs::create_directory(malformed);
  std::ofstream(malformed/"world_behavior_packs.json")<<"malformed sentinel";
  bool rejected=false;
  try{client::creation::installPreparedWorld(malformed,package);}catch(std::exception const&){rejected=true;}
  check(rejected&&!fs::exists(malformed/"karma")&&!fs::exists(malformed/"karma-worldgen.json"),"bad selection did not roll back terrain attachment");
  {std::ifstream in(malformed/"world_behavior_packs.json");std::string text;std::getline(in,text);check(text=="malformed sentinel","bad native selection was overwritten");}
  auto conflict=base/"conflict";fs::create_directories(conflict/"behavior_packs/user/dimensions");
  std::ofstream(conflict/"world_behavior_packs.json")<<R"([{"pack_id":"user-dimension","version":[1,0,0]}])";
  std::ofstream(conflict/"behavior_packs/user/manifest.json")<<R"({"header":{"uuid":"user-dimension"}})";
  std::ofstream(conflict/"behavior_packs/user/dimensions/world.json")<<R"({"minecraft:dimension":{"description":{"identifier":"minecraft:overworld"}}})";
  rejected=false;
  try{client::creation::installPreparedWorld(conflict,package);}catch(std::exception const&){rejected=true;}
  check(rejected&&!fs::exists(conflict/"karma")&&fs::exists(conflict/"behavior_packs/user/dimensions/world.json"),"conflicting selected dimension was replaced");
  for(auto forbidden:{"level.dat","level.dat_old","db","karma","karma-worldgen.json"}){
   auto existing=base/forbidden;fs::create_directory(existing);
   if(std::string(forbidden)=="db"||std::string(forbidden)=="karma")fs::create_directory(existing/forbidden);
   else std::ofstream(existing/forbidden)<<"existing world sentinel";
   bool rejected=false;
   try{client::creation::installPreparedWorld(existing,package);}catch(std::exception const&){rejected=true;}
   check(rejected,"existing world was accepted");
   check(fs::exists(existing/forbidden),"existing world was modified");
   check(std::distance(fs::directory_iterator(existing),fs::directory_iterator{})==1,"unexpected file in existing world");
  }
  auto partial=base/"partial";fs::create_directory(partial);
  fs::remove(package/"karma-worldgen.json");rejected=false;
  try{client::creation::installPreparedWorld(partial,package);}catch(std::exception const&){rejected=true;}
  check(rejected&&fs::is_empty(partial),"incomplete package published");
  fs::remove_all(base);
  std::cout<<"PASS: native UTF-8 pack path keys, height attachment, preserved selections, failed-attachment retention/retry, modified/initialized-world guards and dimension conflicts\n";
 }catch(std::exception const& e){std::cerr<<e.what()<<"; evidence at "<<base<<'\n';return 1;}
}
