#pragma once
#include <nlohmann/json.hpp>
namespace terrain_cache {
// Keep cache ownership tools and the BDS adapter on exactly the same JSON identity.
inline nlohmann::json identity(nlohmann::json profile){
 for(auto key:{"sha256","runtimeFiles","maxCachedRegions","calculatorWorkers","cachedChunks"})profile.erase(key);
 return profile;
}
inline std::string identityText(nlohmann::json const&metadata){
 auto key=identity(metadata);
 key["nativeInput"]=metadata.at("runtimeFiles").at("native/native-input.json");
 key["nativeBinary"]=metadata.at("pluginSha256");
 return key.dump();
}
}
