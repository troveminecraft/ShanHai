#pragma once
#include "WorldBounds.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace terrain_runtime {
struct WorldGenerationMode {
 bool regional=false,unbounded=false;
 bool contains(int x,int z) const {return unbounded?terrain_bounds::gameContainsChunk(x,z):terrain_bounds::containsChunk(x,z);}
 uint64_t cacheKey(int x,int z) const {
  if(!contains(x,z))throw std::out_of_range("Chunk outside generation bounds");
  return unbounded?terrain_bounds::coordinateKey(x,z):uint64_t((z+terrain_bounds::HalfChunks)*terrain_bounds::Chunks+x+terrain_bounds::HalfChunks);
 }
 static WorldGenerationMode read(nlohmann::json const&profile,nlohmann::json const&data){
  WorldGenerationMode result;
  result.regional=data.contains("regionalGeneration");
  if(result.regional){
   auto const&recipe=data.at("regionalGeneration");
   auto algorithm=recipe.value("algorithm",std::string{});
   if((algorithm!="regional-drainage-v1"&&algorithm!="regional-drainage-v2")||data.at("seedManifest").value("generationMode",std::string{})!="regional-on-demand")
    throw std::runtime_error("Unsupported regional generation recipe");
   result.unbounded=recipe.value("boundsMode",std::string{})=="infinite";
  }
  if((profile.value("boundsMode",std::string{})=="infinite")!=result.unbounded)
   throw std::runtime_error("World profile and regional bounds disagree");
  if(!result.unbounded&&profile.at("sizeChunks")!=terrain_bounds::Chunks)
   throw std::runtime_error("Terrain bounds mismatch");
  return result;
 }
};
}
