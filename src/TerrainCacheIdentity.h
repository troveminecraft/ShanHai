#pragma once
#include <nlohmann/json.hpp>
#include "ReleaseVersions.h"
namespace terrain_cache {
// Keep cache ownership tools and the BDS adapter on exactly the same JSON identity.
inline nlohmann::json identity(nlohmann::json profile){
 for(auto key:{"sha256","runtimeFiles","maxCachedRegions","calculatorWorkers","cachedChunks"})profile.erase(key);
 return profile;
}
inline std::string identityText(nlohmann::json const&metadata){
 auto key=identity(metadata);
 // Older saved profiles retain their original DLL hash. Invalidate derived
 // chunks after the column-boundary fix without rewriting world identity.
 key["nativeSafetyRevision"]=shanhai_release::SafetyRevision;
 key["nativeInput"]=metadata.at("runtimeFiles").at("native/native-input.json");
 key["nativeBinary"]=metadata.at("pluginSha256");
 return key.dump();
}
}
