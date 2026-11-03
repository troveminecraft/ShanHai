#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
namespace terrain_cache {
// Owned exclusively by the cache writer. Directory scans, recency accounting
// and eviction never run on a generation or game thread.
class DiskCacheBudget {
 struct Entry {uint64_t bytes,stamp;};
 std::filesystem::path root;std::map<std::filesystem::path,Entry> entries;
 std::set<std::pair<uint64_t,std::filesystem::path>> age;uint64_t sequence=0;
 static uint64_t charge(uint64_t size){return ((size+4095)/4096)*4096;}
 bool owned(std::filesystem::path const&p)const{
  if(root.empty()||p.parent_path()!=root||p.extension()!=L".bin")return false;
  auto name=p.stem().string();return !name.empty()&&std::all_of(name.begin(),name.end(),[](char c){return c>='0'&&c<='9';});
 }
public:
 static constexpr uint64_t DefaultLimit=512ull*1024*1024;
 uint64_t bytes=0,evicted=0,limit=DefaultLimit;
 void open(std::filesystem::path directory,uint64_t maximum=DefaultLimit){
  root=std::move(directory);entries.clear();age.clear();bytes=sequence=evicted=0;limit=maximum;if(root.empty())return;
  std::error_code ec;auto state=std::filesystem::symlink_status(root,ec);if(ec||std::filesystem::is_symlink(state)){root.clear();return;}
  std::multimap<std::filesystem::file_time_type,std::pair<std::filesystem::path,uint64_t>> files;
  for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec)){
   auto const&p=it->path();auto status=it->symlink_status(ec);if(ec)break;
   if(!owned(p)||!std::filesystem::is_regular_file(status))continue;
   auto n=it->file_size(ec);if(ec)break;auto time=it->last_write_time(ec);if(ec)break;files.emplace(time,std::pair{p,charge(n)});
  }
  for(auto const&[time,file]:files){entries.emplace(file.first,Entry{file.second,++sequence});age.emplace(sequence,file.first);bytes+=file.second;}
 }
 void touch(std::filesystem::path const&p){auto it=entries.find(p);if(it==entries.end())return;age.erase({it->second.stamp,p});it->second.stamp=++sequence;age.emplace(sequence,p);}
 void written(std::filesystem::path const&p){
  if(!owned(p))return;std::error_code ec;auto status=std::filesystem::symlink_status(p,ec);if(ec||!std::filesystem::is_regular_file(status))return;
  auto n=std::filesystem::file_size(p,ec);if(ec)return;
  if(auto it=entries.find(p);it!=entries.end()){bytes-=it->second.bytes;age.erase({it->second.stamp,p});entries.erase(it);}
  entries.emplace(p,Entry{charge(n),++sequence});age.emplace(sequence,p);bytes+=charge(n);
  for(int attempts=0;bytes>limit&&!age.empty()&&attempts<64;++attempts){
   auto victim=age.begin()->second;if(victim==p&&entries.size()==1)break;
   auto it=entries.find(victim);auto st=std::filesystem::symlink_status(victim,ec);
   if((!ec&&!std::filesystem::is_regular_file(st))||ec==std::errc::no_such_file_or_directory){
    bytes-=it->second.bytes;age.erase({it->second.stamp,victim});entries.erase(it);ec.clear();
   }else if(!ec&&std::filesystem::is_regular_file(st)&&std::filesystem::remove(victim,ec)){
    bytes-=it->second.bytes;age.erase({it->second.stamp,victim});entries.erase(it);++evicted;
   }else{ec.clear();touch(victim);}
  }
 }
};
}
