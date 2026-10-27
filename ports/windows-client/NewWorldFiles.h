#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <array>
#include <algorithm>

namespace client::creation {
inline constexpr char HeightPackFolder[]="shan-hai-build-height";

// Native PackSourceFactory compares UTF-8 path strings byte for byte. Its world
// sources use '/', while filesystem::operator/ appends '\\' on Windows. Core::Path
// does not normalize that difference, even when both paths name the same folder.
inline std::string nativePackPathKey(std::filesystem::path const& path) {
 auto bytes=path.generic_u8string();
 return {bytes.begin(),bytes.end()};
}
inline nlohmann::json readCreationJson(std::filesystem::path const& path) {
 std::ifstream input(path);if(!input)throw std::runtime_error("Cannot read new-world package: "+path.string());
 return nlohmann::json::parse(input);
}
inline void requireUnopenedWorld(std::filesystem::path const& root) {
 if(!root.is_absolute()||!std::filesystem::is_directory(root))
  throw std::runtime_error("Native new-world directory is unavailable");
 for(auto name:{"level.dat","level.dat_old","db"})
  if(std::filesystem::exists(root/name))throw std::runtime_error("Refusing to alter an initialized world");
}
inline void requireFreshWorldDirectory(std::filesystem::path const& root) {
 requireUnopenedWorld(root);
 // Native creation has made pack folders, but has not saved level.dat or opened
 // LevelDB yet. Never attach to a previously initialized or managed world.
 for(auto name:{"level.dat","level.dat_old","db","karma","karma-worldgen.json"})
  if(std::filesystem::exists(root/name))
   throw std::runtime_error("Refusing to attach terrain to an existing world");
}
// A second call immediately before server startup is intentional: native pack
// copying can rewrite world_behavior_packs.json after createWorld returns.
// Merge the current selection, preserving every unrelated entry and its order.
inline std::string installPreparedHeightPack(std::filesystem::path const& root,std::filesystem::path const& package) {
 namespace fs=std::filesystem;using J=nlohmann::json;
 requireUnopenedWorld(root);
 auto marker=readCreationJson(package/"karma-worldgen.json");
 if(marker!=readCreationJson(root/"karma-worldgen.json"))throw std::runtime_error("Height pack belongs to another world");
 if(marker.value("buildMaxYExclusive",320)==320)return {};
 if(marker.at("buildMaxYExclusive")!=512)throw std::runtime_error("Unsupported new-world build height");
 auto source=package/"behavior_packs"/HeightPackFolder;
 auto manifest=readCreationJson(source/"manifest.json");auto const&header=manifest.at("header");
 auto dimension=readCreationJson(source/"dimensions/overworld.json").at("minecraft:dimension");
 if(dimension.at("description").at("identifier")!="minecraft:overworld"||
    dimension.at("components").at("minecraft:dimension_bounds")!=J{{"min",-64},{"max",512}}||
    dimension.at("components").at("minecraft:generation")!=J{{"generator_type","void"}})
  throw std::runtime_error("Invalid prepared height definition");
 auto uuid=header.at("uuid").get<std::string>();
 J reference={{"pack_id",uuid},{"version",header.at("version")}};
 auto packs=root/"behavior_packs",target=packs/HeightPackFolder,selection=root/"world_behavior_packs.json";
 if(fs::is_symlink(packs)||fs::is_symlink(target)||fs::is_symlink(selection))
  throw std::runtime_error("Refusing linked world pack paths");
 J selected=fs::exists(selection)?readCreationJson(selection):J::array();
 if(!selected.is_array())throw std::runtime_error("Native behavior pack selection is not an array");
 bool found=false;
 for(auto const&entry:selected){
  if(!entry.is_object()||!entry.contains("pack_id")||!entry.contains("version"))throw std::runtime_error("Invalid native pack selection");
  if(entry.at("pack_id")==uuid){
   if(found||entry.at("version")!=header.at("version"))throw std::runtime_error("Conflicting height pack selection");
   found=true;
  }
 }
 if(fs::exists(packs))for(auto const&entry:fs::directory_iterator(packs)){
  if(!entry.is_directory()||entry.path()==target||!fs::exists(entry.path()/"manifest.json"))continue;
  auto other=readCreationJson(entry.path()/"manifest.json").at("header").at("uuid");
  bool active=false;for(auto const&ref:selected)active|=ref.at("pack_id")==other;
  if(!active||!fs::is_directory(entry.path()/"dimensions"))continue;
  for(auto const&file:fs::recursive_directory_iterator(entry.path()/"dimensions")){
   if(file.path().extension()!=".json")continue;
   auto definition=readCreationJson(file.path());
   if(definition.contains("minecraft:dimension")&&definition["minecraft:dimension"].value("description",J::object()).value("identifier",std::string{})=="minecraft:overworld")
    throw std::runtime_error("Selected behavior pack already overrides the overworld height");
  }
 }
 bool existing=fs::exists(target);
 if(existing){
  // Never replace an existing folder, even when its name is ours.
  for(auto const&entry:fs::recursive_directory_iterator(target)){
   if(fs::is_symlink(entry.path())||(!entry.is_directory()&&!fs::exists(source/entry.path().lexically_relative(target))))
    throw std::runtime_error("Existing height pack contains unprepared files");
  }
  for(auto const&entry:fs::recursive_directory_iterator(source))if(entry.is_regular_file()){
   auto counterpart=target/entry.path().lexically_relative(source);
   if(!fs::is_regular_file(counterpart)||fs::is_symlink(counterpart)||readCreationJson(entry.path())!=readCreationJson(counterpart))
    throw std::runtime_error("Existing height pack differs from the prepared package");
  }
 }
 if(existing&&found)return uuid;
 if(!found)selected.push_back(reference);
 auto staging=root/"karma-height-pending";
 if(!fs::create_directory(staging))throw std::runtime_error("An unfinished height attachment already exists");
 bool madePacks=false,movedPack=false,backedUp=false,movedSelection=false;
 try{
  if(!existing)fs::copy(source,staging/"pack",fs::copy_options::recursive);
  {std::ofstream output(staging/"selection.json");output<<selected.dump(2);output.close();if(!output)throw std::runtime_error("Cannot write behavior pack selection");}
  requireUnopenedWorld(root);
  if(!fs::exists(packs)){fs::create_directory(packs);madePacks=true;}
  if(!existing){fs::rename(staging/"pack",target);movedPack=true;}
  if(fs::exists(selection)){fs::rename(selection,staging/"previous.json");backedUp=true;}
  fs::rename(staging/"selection.json",selection);movedSelection=true;
 }catch(...){
  std::error_code ignored;
  if(movedSelection)fs::remove(selection,ignored);
  bool restoreFailed=false;
  if(backedUp){fs::rename(staging/"previous.json",selection,ignored);restoreFailed=bool(ignored);}
  if(movedPack)fs::remove_all(target,ignored);
  if(madePacks)fs::remove(packs,ignored);
  // Keep the original selection backup if the OS could not restore it. Never
  // destroy a user's only copy while reporting a failed attachment.
  if(!restoreFailed)fs::remove_all(staging,ignored);throw;
 }
 std::error_code ignored;fs::remove_all(staging,ignored);return uuid;
}
inline void installPreparedWorld(std::filesystem::path const& root,std::filesystem::path const& package) {
 requireFreshWorldDirectory(root);
 if(!std::filesystem::is_directory(package/"karma")||!std::filesystem::is_regular_file(package/"karma-worldgen.json"))
  throw std::runtime_error("Prepared world package is incomplete");
 auto staging=root/"karma-creation-pending";
 if(!std::filesystem::create_directory(staging))
  throw std::runtime_error("An unfinished attachment already exists");
 bool moved=false,markerMoved=false;
 try {
  std::filesystem::copy(package/"karma",staging/"karma",std::filesystem::copy_options::recursive);
  std::filesystem::copy_file(package/"karma-worldgen.json",staging/"karma-worldgen.json");
  requireFreshWorldDirectory(root);
  std::filesystem::rename(staging/"karma",root/"karma");moved=true;
  std::filesystem::rename(staging/"karma-worldgen.json",root/"karma-worldgen.json");
  markerMoved=true;
  installPreparedHeightPack(root,package);
 } catch(...) {
  // These paths were exclusively created by this call in the verified new root.
  std::error_code ignored;
  if(moved)std::filesystem::remove_all(root/"karma",ignored);
  if(markerMoved)std::filesystem::remove(root/"karma-worldgen.json",ignored);
  std::filesystem::remove_all(staging,ignored);throw;
 }
 std::error_code ignored;std::filesystem::remove(staging,ignored);
}

// Only called for an attachment made by the current in-memory creation Job,
// before startLocalServer was invoked. Preserve it in the job for diagnosis;
// never discard a world or loosen the fresh-world gate for a subsequent retry.
inline void preserveFailedAttachment(std::filesystem::path const& root,
                                     std::filesystem::path const& package,
                                     std::filesystem::path const& backup) {
 namespace fs=std::filesystem;
 requireUnopenedWorld(root);
 if(fs::is_symlink(root)||fs::is_symlink(root/"karma")||fs::is_symlink(root/"karma-worldgen.json"))
  throw std::runtime_error("Refusing linked failed-attachment paths");
 if(readCreationJson(root/"karma-worldgen.json")!=readCreationJson(package/"karma-worldgen.json"))
  throw std::runtime_error("Failed attachment identity changed; preserved in place");
 // Both directions: an added file may contain user/native data. Do not move it
 // out of a world merely because our original files are still present.
 auto identical=[](fs::path const& a,fs::path const& b){
  if(!fs::is_regular_file(a)||!fs::is_regular_file(b)||fs::is_symlink(a)||fs::is_symlink(b)||fs::file_size(a)!=fs::file_size(b))return false;
  std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);
  if(!x||!y)return false;
  std::array<char,65536> left{},right{};
  while(x){x.read(left.data(),left.size());y.read(right.data(),right.size());
   if(x.gcount()!=y.gcount()||!std::equal(left.begin(),left.begin()+x.gcount(),right.begin()))return false;
  }
  return x.eof()&&y.eof();
 };
 for(auto pair:{std::pair{root/"karma",package/"karma"},std::pair{package/"karma",root/"karma"}}){
  if(!fs::is_directory(pair.first)||!fs::is_directory(pair.second))throw std::runtime_error("Failed attachment data missing");
  for(auto const& entry:fs::recursive_directory_iterator(pair.first)){
   auto other=pair.second/entry.path().lexically_relative(pair.first);
   if(fs::is_symlink(entry.path())||fs::is_symlink(other)||
      (entry.is_directory()?!fs::is_directory(other):!identical(entry.path(),other)))
    throw std::runtime_error("Failed attachment data changed; preserved in place");
  }
 }
 if(!backup.is_absolute()||fs::exists(backup)||!fs::is_directory(backup.parent_path()))
  throw std::runtime_error("Failed-attachment backup is unavailable");
 requireUnopenedWorld(root);
 if(!fs::create_directory(backup))throw std::runtime_error("Failed-attachment backup was concurrently created");
 bool moved=false;
 try{
  fs::rename(root/"karma",backup/"karma");moved=true;
  fs::rename(root/"karma-worldgen.json",backup/"karma-worldgen.json");
 }catch(...){
  std::error_code ignored;if(moved)fs::rename(backup/"karma",root/"karma",ignored);
  fs::remove(backup,ignored);throw;
 }
 // Keep native pack selections/folders. The next attachment merges the height
 // reference again; these files may also contain unrelated user selections.
}
}
