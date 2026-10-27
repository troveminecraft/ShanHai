#pragma once
#include "Noise.h"
#include "../RegionalAlgorithms.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <deque>
#include <future>
#include <atomic>
#include <chrono>
#include <thread>
namespace nativegen::regional {
inline constexpr char Algorithm[]="regional-drainage-v1";
inline bool supportedAlgorithm(std::string const&name){return terrain_runtime::supportedRegionalAlgorithm(name);}
inline uint64_t digest(std::string const&s){uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
inline std::string identity(J const&data){
 auto config=data.at("config");
 // These fields select native structures, never regional terrain. The structure
 // adapter changes them at startup; that must not duplicate drainage planning.
 config.erase("sharedRules");config.erase("villageSpacing");
 J key{{"algorithm",data.at("regionalGeneration").value("algorithm",std::string(Algorithm))},{"seed",data.at("seedManifest").at("terrainSeed")},
       {"config",config},{"template",data.at("seedManifest").at("templateSha256")}};
 return std::to_string(digest(key.dump()));
}
// A world owns this store; concurrent workers may share it. The default is
// retained only for direct internal algorithm tests, never host runtime state.
struct Store {
    std::recursive_mutex mutex;
    std::unordered_map<std::string,std::shared_ptr<J const>> plans;
    std::unordered_map<std::string,std::shared_future<std::shared_ptr<J const>>> pendingPlans;
    std::deque<std::string> residentOrder;
    std::unordered_map<std::string,std::filesystem::path> folders;
    std::atomic_uint64_t memoryHits=0,diskHits=0,builds=0,waits=0,buildUs=0,waitUs=0;
};
inline Store defaultStore;
inline thread_local Store* selectedStore = nullptr;
inline Store& store() { return selectedStore ? *selectedStore : defaultStore; }
class StoreScope {
    Store* previous;
public:
    explicit StoreScope(Store& selected) : previous(selectedStore) { selectedStore = &selected; }
    ~StoreScope() { selectedStore = previous; }
    StoreScope(StoreScope const&) = delete;
    StoreScope& operator=(StoreScope const&) = delete;
};
inline void storage(J const&data,std::filesystem::path const&folder){
 auto& cache=store();
 if(!data.contains("regionalGeneration"))return;
 std::lock_guard lock(cache.mutex);cache.folders[identity(data)]=folder;
}
template<class F> std::shared_ptr<J const> get(std::string const&id,char const*kind,int gx,int gz,F make){
 auto& cache=store();
 auto name=std::string(kind)+"-"+std::to_string(gx)+"-"+std::to_string(gz);
 auto key=id+":"+name;
 std::filesystem::path path;
 std::shared_future<std::shared_ptr<J const>> waiting;
 std::promise<std::shared_ptr<J const>> produced;
 {
  std::lock_guard lock(cache.mutex);
  if(auto i=cache.plans.find(key);i!=cache.plans.end()){++cache.memoryHits;return i->second;}
  if(auto i=cache.pendingPlans.find(key);i!=cache.pendingPlans.end())waiting=i->second;
  else {
   cache.pendingPlans.emplace(key,produced.get_future().share());
   if(auto dir=cache.folders.find(id);dir!=cache.folders.end())path=dir->second/id/(name+".json");
  }
 }
 // Same-region callers share one result. Unrelated regions, and nested desert
 // lookups made by a water planner, never wait while holding the global lock.
 if(waiting.valid()){
  ++cache.waits;auto start=std::chrono::steady_clock::now();auto result=waiting.get();
  cache.waitUs+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();return result;
 }
 try {
 J value;
 if(!path.empty()&&std::filesystem::exists(path))try{
  std::ifstream in(path);J saved;in>>saved;
  if(saved.at("identity")==id&&saved.at("algorithm")==Algorithm&&saved.at("checksum")==std::to_string(digest(saved.at("plan").dump()))){value=saved.at("plan");++cache.diskHits;}
 }catch(std::exception const&){} // Regenerate an incomplete cache from the same recipe.
 if(value.is_null()){
  auto start=std::chrono::steady_clock::now();value=make();++cache.builds;
  cache.buildUs+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  if(!path.empty())try{
   std::filesystem::create_directories(path.parent_path());auto pending=path;
   // Terrain/structure DLLs may write the same deterministic plan concurrently.
   // Never let one writer truncate or rename the other writer's staging file.
   static std::atomic_uint64_t serial=0;
   pending+="."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+
     std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))+"."+std::to_string(serial++)+".tmp";
   {std::ofstream out(pending);out<<J{{"identity",id},{"algorithm",Algorithm},{"checksum",std::to_string(digest(value.dump()))},{"plan",value}}.dump();if(!out)throw std::runtime_error("Regional cache write failed");}
   std::error_code ec;std::filesystem::rename(pending,path,ec);
   if(ec)std::filesystem::remove(pending,ec); // The deterministic recipe remains usable without disk cache.
  }catch(std::exception const&){}
 }
 auto result=std::make_shared<J const>(std::move(value));
 {
  std::lock_guard lock(cache.mutex);cache.plans.emplace(key,result);cache.residentOrder.push_back(key);
  if(cache.residentOrder.size()>384){cache.plans.erase(cache.residentOrder.front());cache.residentOrder.pop_front();}
  produced.set_value(result);cache.pendingPlans.erase(key);
 }
 return result;
 }catch(...){
  std::lock_guard lock(cache.mutex);produced.set_exception(std::current_exception());cache.pendingPlans.erase(key);throw;
 }
}
}
