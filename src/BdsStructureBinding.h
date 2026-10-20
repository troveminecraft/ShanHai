#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace terrain_runtime {
// BDS has one terrain activation. A second worldName in structure settings can
// outlive a panel switch and silently disable every structure in the new world.
inline std::string structureWorldName(nlohmann::json const& activation) {
 auto name=activation.at("worldName").get<std::string>();
 if(name.empty()||name=="."||name==".."||name.find_first_of("/\\:")!=std::string::npos||name.find('\0')!=std::string::npos)
  throw std::runtime_error("Invalid structure world in terrain activation");
 return name;
}
inline void validateStructureBinding(nlohmann::json const& activation,nlohmann::json const& metadata,
 nlohmann::json const& input,nlohmann::json const& savedMarker) {
 structureWorldName(activation);
 if(activation!=savedMarker)throw std::runtime_error("Structure world activation marker mismatch");
 if(activation.at("sha256")!=metadata.at("sha256")||activation.at("terrainSeed")!=metadata.at("terrainSeed")||
    activation.at("worldSeed")!=metadata.at("worldSeed")||activation.at("terrainSeed")!=input.at("seedManifest").at("terrainSeed"))
  throw std::runtime_error("Structure terrain profile or seed mismatch");
}
}
