#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "CacheCompression.h"
#include "DiskCacheBudget.h"
namespace terrain_cache {
inline std::string digest(std::vector<uint8_t>const& data){
 std::array<uint8_t,32> value{};
 if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(data.data()),ULONG(data.size()),value.data(),32)<0)throw std::runtime_error("Cache SHA256 failed");
 std::string s;for(auto b:value){s+="0123456789abcdef"[b>>4];s+="0123456789abcdef"[b&15];}return s;
}
enum class ReadState{Missing,Valid,Invalid};
struct ReadResult{ReadState state=ReadState::Missing;std::vector<uint8_t> bytes;bool legacy=false;};
inline ReadResult readDerived(std::filesystem::path const& file){
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)return {};
 try{auto size=in.tellg();if(size<72||size>1024*1024)return {ReadState::Invalid,{}};
  in.seekg(0);std::array<uint8_t,8> header{};in.read(reinterpret_cast<char*>(header.data()),8);
  bool compressed=header[0]=='K'&&header[1]=='W'&&header[2]=='C'&&header[3]==1;
  if(!compressed&&size<320)return {ReadState::Invalid,{}};
  in.seekg(compressed?8:0);std::string hash(64,' ');in.read(hash.data(),64);std::vector<uint8_t> bytes(size_t(size)-(compressed?72:64));in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
  if(!in)return {ReadState::Invalid,{}};
  if(compressed){uint32_t n=uint32_t(header[4])|(uint32_t(header[5])<<8)|(uint32_t(header[6])<<16)|(uint32_t(header[7])<<24);bytes=decompressDerived(bytes,n);}
  if(digest(bytes)!=hash)return {ReadState::Invalid,{}};return {ReadState::Valid,std::move(bytes),!compressed};
 }catch(...){return {ReadState::Invalid,{}};}
}
inline void writeDerived(std::filesystem::path const& file,std::vector<uint8_t>const& bytes){
 auto temp=file;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
 try{
  auto hash=digest(bytes);auto packed=compressDerived(bytes);std::ofstream out(temp,std::ios::binary);
  if(!packed.empty()){uint32_t n=uint32_t(bytes.size());std::array<uint8_t,8> header{'K','W','C',1,uint8_t(n),uint8_t(n>>8),uint8_t(n>>16),uint8_t(n>>24)};out.write(reinterpret_cast<char const*>(header.data()),header.size());}
  auto const&payload=packed.empty()?bytes:packed;out.write(hash.data(),hash.size());out.write(reinterpret_cast<char const*>(payload.data()),payload.size());out.close();
  if(!out||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Derived cache write failed");
 }catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
class CacheWriter {
 struct Job{std::filesystem::path file;std::vector<uint8_t> bytes;};
 std::mutex mutex;std::condition_variable ready;std::deque<Job> jobs;std::set<std::filesystem::path> touches;bool stopping=true;std::thread worker;
 using Sink=std::function<void(std::filesystem::path const&,std::vector<uint8_t>const&)>;Sink sink;
public:
 static constexpr size_t QueueLimit=64;
 std::atomic_uint64_t completed=0,dropped=0,failures=0,coalesced=0,diskBytes=0,evicted=0;
 explicit CacheWriter(Sink write=writeDerived):sink(std::move(write)){}
 void start(std::filesystem::path directory={}){std::lock_guard lock(mutex);if(worker.joinable())throw std::runtime_error("Cache writer already started");stopping=false;worker=std::thread([this,directory=std::move(directory)]{
  DiskCacheBudget budget;try{budget.open(directory);diskBytes=budget.bytes;}catch(...){++failures;}
  for(;;){Job j;bool touchOnly=false;{std::unique_lock lock(mutex);ready.wait(lock,[this]{return stopping||!jobs.empty()||!touches.empty();});if(stopping&&jobs.empty()&&touches.empty())break;
   if(!jobs.empty()){j=std::move(jobs.front());jobs.pop_front();}else{j.file=*touches.begin();touches.erase(touches.begin());touchOnly=true;}}
   try{if(touchOnly)budget.touch(j.file);else{sink(j.file,j.bytes);budget.written(j.file);diskBytes=budget.bytes;evicted=budget.evicted;++completed;}}catch(...){++failures;}
  }
 });}
 void touch(std::filesystem::path const&file){{std::lock_guard lock(mutex);if(stopping||touches.size()>=256)return;touches.insert(file);}ready.notify_one();}
 void submit(std::filesystem::path file,std::vector<uint8_t> bytes){
  {std::lock_guard lock(mutex);if(stopping){++dropped;return;}
   for(auto&j:jobs)if(j.file==file){j.bytes=std::move(bytes);++coalesced;return;}
   if(jobs.size()>=QueueLimit){++dropped;return;}jobs.push_back({std::move(file),std::move(bytes)});
  }ready.notify_one();
 }
 size_t pending(){std::lock_guard lock(mutex);return jobs.size();}
 void stop(){{std::lock_guard lock(mutex);stopping=true;}ready.notify_all();if(worker.joinable())worker.join();}
 ~CacheWriter(){stop();}
};
}
