#pragma once
#include <array>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nativegen {
struct JungleSite {int x=0,z=0,ground=0,h=0,width=1,layer=0,style=0;double priority=0,phase=0,radius=0;};
struct JungleCell {int x,y,z;uint16_t block;};
struct JungleTree {
 JungleSite site;std::vector<JungleCell> cells;std::vector<std::array<int,4>> stem;
 std::array<uint32_t,151> tileBegin{};std::vector<uint32_t> tileCells;
 int primaryBranches=0,secondaryBranches=0,brokenBranches=0,attachedVines=0,epiphytes=0;
 size_t storageBytes()const{return sizeof(*this)+cells.capacity()*sizeof(JungleCell)+stem.capacity()*sizeof(stem[0])+tileCells.capacity()*sizeof(uint32_t);}
};

// One world/recipe owns this bounded cache. A worker builds against its own
// Field outside the mutex; other workers requesting that tree await its value.
// Published trees are immutable. No lock spans terrain sampling or placement.
class JungleShapes {
public:
 using Value=std::shared_ptr<JungleTree const>;
 struct Snapshot {size_t entries,bytes,pending;uint64_t hits,builds,waits,evictions;};
private:
 struct Entry {Value value;size_t bytes;uint64_t used;};
 mutable std::mutex mutex;
 std::unordered_map<uint64_t,Entry> values;
 std::unordered_map<uint64_t,std::shared_future<Value>> pending;
 size_t entryLimit,byteLimit,residentBytes=0;
 uint64_t clock=0,hits=0,builds=0,waits=0,evictions=0;
public:
 explicit JungleShapes(size_t entries=512,size_t bytes=48*1024*1024):entryLimit(entries),byteLimit(bytes){}
 Snapshot statistics()const{std::lock_guard lock(mutex);return {values.size(),residentBytes,pending.size(),hits,builds,waits,evictions};}
 template<class Build> Value get(uint64_t key,Build build){
  std::shared_future<Value> waiting;std::shared_ptr<std::promise<Value>> promise;
  {
   std::lock_guard lock(mutex);
   if(auto i=values.find(key);i!=values.end()){++hits;i->second.used=++clock;return i->second.value;}
   if(auto i=pending.find(key);i!=pending.end()){++waits;waiting=i->second;}
   else {promise=std::make_shared<std::promise<Value>>();pending.emplace(key,promise->get_future().share());++builds;}
  }
  if(!promise)return waiting.get();
  try{
   Value value=build();if(!value)throw std::runtime_error("Jungle shape builder returned null");auto bytes=value->storageBytes();
   {
    std::lock_guard lock(mutex);
    if(entryLimit&&bytes<=byteLimit){
     // At most 512 entries: scan only on insertion to avoid per-hit allocation
     // or linked-list bookkeeping in the far more frequent read path.
     while(!values.empty()&&(values.size()>=entryLimit||residentBytes>byteLimit-bytes)){
      auto oldest=values.begin();for(auto i=values.begin();i!=values.end();++i)if(i->second.used<oldest->second.used)oldest=i;
      residentBytes-=oldest->second.bytes;values.erase(oldest);++evictions;
     }
     values.emplace(key,Entry{value,bytes,++clock});residentBytes+=bytes;
    }
    pending.erase(key);
   }
   promise->set_value(value);return value;
  }catch(...){auto error=std::current_exception();{std::lock_guard lock(mutex);pending.erase(key);}promise->set_exception(error);std::rethrow_exception(error);}
 }
};
}
