#pragma once
#include <string_view>
namespace terrain_runtime {
inline bool supportedRegionalAlgorithm(std::string_view name) {
    return name=="regional-drainage-v1" || name=="regional-drainage-v2" || name=="regional-drainage-v3";
}
}
