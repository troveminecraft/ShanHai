#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
namespace terrain_cache {
struct MixedShard {
 size_t operator()(uint64_t key)const noexcept {key^=key>>30;key*=0xbf58476d1ce4e5b9ULL;key^=key>>27;key*=0x94d049bb133111ebULL;return size_t(key^(key>>31));}
 size_t operator()(uint32_t key)const noexcept {
  // Mix both X and Z bits. key % shards maps every north/south flight to
  // one shard because the test world's row stride is 256.
  key^=key>>16;key*=0x7feb352dU;key^=key>>15;key*=0x846ca68bU;key^=key>>16;
  return key;
 }
};
template<class T,size_t Capacity=1024,size_t Shards=16,class Selector=MixedShard,class Key=uint32_t>
class ChunkCache {
 static_assert(Capacity>=Shards&&Capacity%Shards==0);
 using Value=std::shared_ptr<T const>;
 struct Shard {
  std::mutex mutex;
  std::list<std::pair<Key,Value>> lru;
  std::unordered_map<Key,typename decltype(lru)::iterator> index;
 };
 std::array<Shard,Shards> shards;
 Shard& at(Key key){return shards[Selector{}(key)%Shards];}
public:
 std::atomic_uint64_t evictions=0;
 // Call only after the world's generation jobs have stopped.
 void clear(){for(auto&s:shards){std::lock_guard lock(s.mutex);s.index.clear();s.lru.clear();}evictions=0;}
 Value get(Key key){
  auto&s=at(key);std::lock_guard lock(s.mutex);auto it=s.index.find(key);if(it==s.index.end())return {};
  s.lru.splice(s.lru.begin(),s.lru,it->second);return it->second->second;
 }
 void put(Key key,Value value){
  auto&s=at(key);std::lock_guard lock(s.mutex);
  if(auto it=s.index.find(key);it!=s.index.end()){it->second->second=std::move(value);s.lru.splice(s.lru.begin(),s.lru,it->second);return;}
  s.lru.emplace_front(key,std::move(value));s.index[key]=s.lru.begin();
  if(s.index.size()>Capacity/Shards){s.index.erase(s.lru.back().first);s.lru.pop_back();evictions.fetch_add(1,std::memory_order_relaxed);}
 }
 size_t size(){size_t n=0;for(auto&s:shards){std::lock_guard lock(s.mutex);n+=s.index.size();}return n;}
};
}
