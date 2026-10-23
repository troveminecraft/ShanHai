#include <set>
#include <iostream>
#include "TerrainCacheIdentity.h"
#include "native/RegionalPlans.h"
int main(){try{
 std::string line;
 while(std::getline(std::cin,line)){
  auto request=nlohmann::json::parse(line);auto const&data=request.at("native");
  nlohmann::json result={{"chunkIdentity",terrain_cache::identityText(request.at("profile"))},{"regional",nullptr}};
  if(data.contains("regionalGeneration")){
   if(!nativegen::regional::supportedAlgorithm(data.at("regionalGeneration").at("algorithm")))throw std::runtime_error("Unsupported regional cache algorithm");
   result["regional"]=nativegen::regional::identity(data);
  }
  std::cout<<result.dump()<<std::endl;
 }
 return 0;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
