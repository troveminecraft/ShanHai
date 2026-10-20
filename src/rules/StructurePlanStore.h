#pragma once
#include "StructureStore.h"
#include <cctype>
#include <optional>

namespace worldgen_rules::plan_store {
using J = nlohmann::json;

// A placement checkpoint is immutable. It records the chosen layout before
// the first fragment can write, not a "completed" flag that could outlive an
// unsaved chunk. Saved block/entity state still belongs to Bedrock's database.
inline void verifyOrCreate(std::filesystem::path const &file, J const &expected) {
    auto backup = file;
    backup += ".bak";
    if (!std::filesystem::exists(file) && !std::filesystem::exists(backup)) {
        structure_store::persist(file, expected);
        return;
    }
    std::optional<J> actual;
    try {
        actual = worldgen_persistence::read(file);
    } catch (std::exception const &) {
        // Recovery is only for unreadable data, never for a valid but different
        // layout. An incompatible generator must not overwrite the old record.
        actual = worldgen_persistence::read(backup);
        if (*actual != expected)
            throw std::runtime_error("Structure plan checkpoint mismatch: " +
                                     worldgen_persistence::label(file));
        worldgen_persistence::atomicWrite(file, *actual);
    }
    if (*actual != expected)
        throw std::runtime_error("Structure plan changed since its first fragment: " +
                                 worldgen_persistence::label(file));
}

inline std::filesystem::path checkpointPath(std::filesystem::path const &world, std::string family, int x,
                                            int z) {
    if (family.empty() || !std::all_of(family.begin(), family.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || c == '_' || (c >= '0' && c <= '9');
        }))
        throw std::runtime_error("Invalid structure checkpoint family");
    // Bound directory fan-out in an infinite world.
    auto shard = std::to_string((uint32_t(x) * 31u ^ uint32_t(z)) & 63u);
    return world / "karma-structure-plans" / family / shard /
           (std::to_string(x) + "_" + std::to_string(z) + ".json");
}
} // namespace worldgen_rules::plan_store
