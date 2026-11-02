#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace terrain_runtime {
// Palette indices are part of the frozen world recipe. Two indices may name
// the same requested permutation; they must not be renumbered or rejected.
// Compare requests, not resolved blocks, so ignored or invalid states cannot
// accidentally become aliases. Bare leaves are made persistent by the native
// resolver, and therefore have the same semantics as these explicit states.
inline nlohmann::json canonicalPaletteRequest(nlohmann::json const& entry) {
    auto name = entry.is_string() ? entry.get<std::string>() : entry.at("name").get<std::string>();
    if (name.find(':') == std::string::npos) name = "minecraft:" + name;
    auto states = entry.is_object() ? entry.at("states") : nlohmann::json::object();
    if (entry.is_string() && name.ends_with("_leaves"))
        states = {{"persistent_bit", true}, {"update_bit", false}};
    return {{"name", name}, {"states", states},
            {"extraWater", entry.is_object() && entry.value("extraWater", false)}};
}
inline bool samePaletteRequest(nlohmann::json const& a, nlohmann::json const& b) {
    return canonicalPaletteRequest(a) == canonicalPaletteRequest(b);
}
}
