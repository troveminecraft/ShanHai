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
  auto package=base/"prepared";fs::create_directories(package/"karma/structures");
  std::ofstream(package/"karma/input.json")<<"terrain payload";
  std::ofstream(package/"karma/structures/registry.json")<<"structure payload";
  std::ofstream(package/"karma-worldgen.json")<<"seed marker";
  auto root=base/"new-world";
  fs::create_directories(root/"resource_packs");fs::create_directory(root/"behavior_packs");
  std::ofstream(root/"world_resource_packs.json")<<"[]";
  client::creation::installPreparedWorld(root,package);
  check(fs::exists(root/"karma/structures/registry.json"),"missing structure data");
  check(fs::exists(root/"karma-worldgen.json"),"missing marker");
  check(!fs::exists(root/"level.dat"),"must not fabricate level.dat");
  check(!fs::exists(root/"db"),"must not open LevelDB");
  check(fs::file_size(root/"world_resource_packs.json")==2,"native pack config changed");
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
  fs::remove(package/"karma-worldgen.json");bool rejected=false;
  try{client::creation::installPreparedWorld(partial,package);}catch(std::exception const&){rejected=true;}
  check(rejected&&fs::is_empty(partial),"incomplete package published");
  fs::remove_all(base);
  std::cout<<"PASS: packs-only new world, native settings untouched, five existing-world guards, incomplete package rejected\n";
 }catch(std::exception const& e){std::cerr<<e.what()<<"; evidence at "<<base<<'\n';return 1;}
}
