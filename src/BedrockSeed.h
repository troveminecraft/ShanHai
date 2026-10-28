#pragma once
#include "mc/world/level/LevelSeed64.h"
#include "mc/deps/core/string/HashedString.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace bedrock_compat {
inline std::string seedText(LevelSeed64 const& seed) {
    return std::to_string(std::bit_cast<int64_t>(uint64_t(seed.mValue)));
}
struct Seed {
    uint64_t value;
    uint32_t to32BitRandomSeed() const { return uint32_t(value); }
    std::string toUiString() const { return std::to_string(std::bit_cast<int64_t>(value)); }
};
template<class T> Seed seed(T const& value) {
    if constexpr(requires { value.getSeed(); }) return {uint64_t(value.getSeed().mValue)};
    else if constexpr(requires { value.mSeed; }) return {uint64_t(value.mSeed->mValue)};
    else {
        // 26.51 LevelData uses "seed"; "RandomSeed" belongs to level.dat NBT.
        // Keep native lookup semantics, including mOverrides taking precedence.
        auto* entry=value._getValue(HashedString("seed"));
        auto* native=entry?std::get_if<LevelSeed64>(&*entry->mValue):nullptr;
        if(!native)throw std::runtime_error("Native LevelData seed property is missing or is not LevelSeed64");
        return {uint64_t(native->mValue)};
    }
}
}
