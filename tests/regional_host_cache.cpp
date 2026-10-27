#undef NDEBUG
#include "TerrainKernel.h"
#include "native/RegionalPlanStore.h"
#include <fstream>
#include <iostream>
#include <cassert>
#include <future>
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe and unique scratch directory required");
 using namespace nativegen;
 J data;std::ifstream(argv[1])>>data;auto changed=data;
 changed["config"]["sharedRules"]={{"revision","test-structure-policy"}};
 changed["config"]["villageSpacing"]=24;
 assert(regional::identity(data)==regional::identity(changed));
 auto terrainChanged=data;terrainChanged["config"]["seaLevel"]=99;
 assert(regional::identity(data)!=regional::identity(terrainChanged));
 auto dir=std::filesystem::path(argv[2]);if(std::filesystem::exists(dir))throw std::runtime_error("scratch already exists");
 std::filesystem::create_directories(dir);
 std::array<std::pair<int,int>,6> points{{{0,0},{-480,-1088},{2048,2048},{-2049,2048},{4096,-4096},{4097,-4096}}};
 auto sample=[&](bool storage,J recipe){
  auto context=std::make_shared<terrain_kernel::Context>();if(storage)context->storage(recipe,dir/"regional-cache");
  terrain_kernel::Field field(recipe,context);std::vector<std::array<int,4>> values;
  auto t=std::chrono::steady_clock::now();
  for(auto [x,z]:points){auto s=field.surface(x+32768,z+32768);values.push_back({s.height,s.water,s.biome,s.lava});}
  auto stats=context->statistics();stats["elapsedMs"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
  return std::pair{values,stats};
 };
 auto terrain=sample(true,data),before=sample(false,changed),after=sample(true,changed);
 assert(terrain.first==before.first&&before.first==after.first);
 assert(before.second["builds"].get<int>()>0&&after.second["builds"]==0&&after.second["diskHits"].get<int>()>0);
 // Two host stores can produce one file at once. Both must see complete JSON.
 std::promise<void> enterA,enterB;auto waitA=enterA.get_future().share(),waitB=enterB.get_future().share();
 auto writer=[&](int which){regional::Store store;regional::StoreScope scope(store);regional::storage(data,dir/"racing");
  return regional::get(regional::identity(data),"test",1,2,[&]{(which?enterB:enterA).set_value();(which?waitA:waitB).wait();return J{{"payload",std::string(40000,'a')}};});};
 auto a=std::async(std::launch::async,writer,0),b=std::async(std::launch::async,writer,1);assert(*a.get()==*b.get());
 regional::Store reader;regional::StoreScope scope(reader);regional::storage(data,dir/"racing");
 auto result=regional::get(regional::identity(data),"test",1,2,[]()->J{throw std::runtime_error("concurrent cache file invalid");});
 assert(reader.diskHits==1&&result->at("payload").get<std::string>().size()==40000);
 std::cout<<J{{"terrain",terrain.second},{"unwiredStructureHost",before.second},{"sharedDiskStructureHost",after.second},{"outputEqual",true},{"concurrentWriters",true}}.dump(2)<<'\n';
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
