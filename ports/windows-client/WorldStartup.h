#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace client::startup {
inline void requireSameSeed(std::string const& expected, std::string const& actual) {
    if (expected != actual)
        throw std::runtime_error("Native world seed differs from the validated creation settings");
}
// Check the resource selected by the native pack manager, not merely a file
// beside the world which may never have entered the server pack stack.
inline void requireHeightResource(std::string const& resource, int minY, int maxY) {
    auto definition=nlohmann::json::parse(resource).at("minecraft:dimension");
    auto const& components=definition.at("components");
    if(definition.at("description").at("identifier")!="minecraft:overworld" ||
       components.at("minecraft:dimension_bounds")!=nlohmann::json{{"min",minY},{"max",maxY}} ||
       components.at("minecraft:generation").at("generator_type")!="void")
        throw std::runtime_error("Selected overworld definition disagrees with saved world height");
}
}
