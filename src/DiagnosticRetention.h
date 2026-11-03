#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
namespace terrain_runtime {
inline void retainDiagnosticSessions(std::filesystem::path const&root,std::filesystem::path const&current,size_t keep=3){
 std::multimap<uint64_t,std::filesystem::path> sessions;std::error_code ec;
 if(std::filesystem::is_symlink(std::filesystem::symlink_status(root,ec)))return;
 for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec)){
  if(!std::filesystem::is_directory(it->symlink_status(ec)))continue;
  auto folder=it->path(),meta=folder/"session.json";
  if(!std::filesystem::is_regular_file(std::filesystem::symlink_status(meta,ec))||std::filesystem::file_size(meta,ec)>65536){ec.clear();continue;}
  try{nlohmann::json j;std::ifstream(meta)>>j;if(j.value("schema",0)!=1||!j.contains("startWallUnixMs"))continue;
   bool known=true;for(auto const&file:std::filesystem::directory_iterator(folder)){
    auto name=file.path().filename().string();known&=std::filesystem::is_regular_file(file.symlink_status())&&
     (name=="session.json"||name=="last-status.json"||name=="events-0.jsonl"||name=="events-1.jsonl"||name=="events-2.jsonl"||name=="events-3.jsonl");
   }
   if(known)sessions.emplace(j.at("startWallUnixMs").get<uint64_t>(),folder);
  }catch(...){continue;}
 }
 while(sessions.size()>keep){auto it=sessions.begin();if(it->second==current){++it;if(it==sessions.end())break;}
  auto folder=it->second;sessions.erase(it);
  // Never recurse into unknown directories or follow links. A foreign file
  // makes the entire session ineligible during the preceding validation.
  if(!std::filesystem::is_directory(std::filesystem::symlink_status(folder,ec)))continue;
  for(auto const&file:std::filesystem::directory_iterator(folder)){
   auto name=file.path().filename().string();
   if(std::filesystem::is_regular_file(file.symlink_status())&&(name=="session.json"||name=="last-status.json"||
      name=="events-0.jsonl"||name=="events-1.jsonl"||name=="events-2.jsonl"||name=="events-3.jsonl"))std::filesystem::remove(file.path(),ec);
  }
  std::filesystem::remove(folder,ec);
 }
}
}
