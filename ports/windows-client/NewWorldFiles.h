#pragma once
#include <filesystem>
#include <stdexcept>

namespace client::creation {
inline void requireFreshWorldDirectory(std::filesystem::path const& root) {
 if(!root.is_absolute()||!std::filesystem::is_directory(root))
  throw std::runtime_error("Native new-world directory is unavailable");
 // Native creation has made pack folders, but has not saved level.dat or opened
 // LevelDB yet. Never attach to a previously initialized or managed world.
 for(auto name:{"level.dat","level.dat_old","db","karma","karma-worldgen.json"})
  if(std::filesystem::exists(root/name))
   throw std::runtime_error("Refusing to attach terrain to an existing world");
}
inline void installPreparedWorld(std::filesystem::path const& root,std::filesystem::path const& package) {
 requireFreshWorldDirectory(root);
 if(!std::filesystem::is_directory(package/"karma")||!std::filesystem::is_regular_file(package/"karma-worldgen.json"))
  throw std::runtime_error("Prepared world package is incomplete");
 auto staging=root/"karma-creation-pending";
 if(!std::filesystem::create_directory(staging))
  throw std::runtime_error("An unfinished attachment already exists");
 bool moved=false;
 try {
  std::filesystem::copy(package/"karma",staging/"karma",std::filesystem::copy_options::recursive);
  std::filesystem::copy_file(package/"karma-worldgen.json",staging/"karma-worldgen.json");
  requireFreshWorldDirectory(root);
  std::filesystem::rename(staging/"karma",root/"karma");moved=true;
  std::filesystem::rename(staging/"karma-worldgen.json",root/"karma-worldgen.json");
 } catch(...) {
  // These paths were exclusively created by this call in the verified new root.
  std::error_code ignored;
  if(moved)std::filesystem::remove_all(root/"karma",ignored);
  std::filesystem::remove_all(staging,ignored);throw;
 }
 std::error_code ignored;std::filesystem::remove(staging,ignored);
}
}
