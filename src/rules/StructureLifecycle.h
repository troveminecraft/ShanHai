#pragma once
#include "StructurePlan.h"
#include <deque>
#include <optional>
#include <set>
#include <nlohmann/json.hpp>

namespace worldgen_rules {
using LifecycleJson = nlohmann::json;
inline constexpr unsigned StructureEntityTickInterval = 20, VillageReloadChecksPerTick = 2;
inline constexpr size_t StructureEntityBatchLimit = 16;
inline constexpr std::string_view NativeStructureEntityTag = "karma_native_structure",
                                  IglooResidentTag = "karma_igloo_resident";
struct StructureActorLocation {
    float x, y, z;
};
inline StructureActorLocation structureActorLocation(StructurePoint p) {
    return {float(p.x) + .5f, float(p.y), float(p.z) + .5f};
}
inline LifecycleJson structureBoxJson(StructureBox const &b) {
    return {b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z};
}
inline bool inStructureOwner(StructurePoint p, StructureBox const &clip) {
    return p.x >= clip.min.x && p.x <= clip.max.x && p.z >= clip.min.z && p.z <= clip.max.z;
}
inline std::string structureOwnerKey(std::string_view family, ChunkPosition anchor, ChunkPosition owner) {
    return std::string(family) + ":" + std::to_string(anchor.x) + "," + std::to_string(anchor.z) + ":" +
           std::to_string(owner.x) + "," + std::to_string(owner.z);
}
inline std::optional<std::string> structureEntityClaimKey(LifecycleJson const &ledger,
                                                          std::string_view family, ChunkPosition anchor,
                                                          StructureBox clip, bool villageHasActors) {
    std::string key =
        structureOwnerKey(family, anchor, {spacingCell(clip.min.x, 16), spacingCell(clip.min.z, 16)});
    static LifecycleJson const empty = LifecycleJson::object();
    auto found = ledger.find("structureEntries");
    auto const &entries = found == ledger.end() ? empty : *found;
    if (family == "village") {
        if (!villageHasActors)
            return {};
        for (auto suffix : {"", ":native-templates-v2", ":native-templates-v3"})
            if (auto it = entries.find(key + suffix);
                it != entries.end() && !it->value("actors", LifecycleJson::array()).empty())
                return {};
        key += ":native-templates-v4";
    }
    if (entries.contains(key))
        return {};
    return key;
}
template <class Handle, class Clip = StructureBox> struct StructureEntityJob {
    Handle start;
    Clip clip;
    std::string key;
    uint32_t seed;
};
// Handle is opaque (a native start pointer on either platform). Only the game
// adapter's ready/spawn calls may dereference it. Claimed jobs never replay.
template <class Handle, class Clip = StructureBox> class StructureEntityQueue {
  public:
    using Job = StructureEntityJob<Handle, Clip>;
    std::deque<Job> pending;
    std::set<std::string> queued;
    bool enqueue(LifecycleJson const &ledger, Handle handle, Clip clip, std::string_view family,
                 ChunkPosition anchor, uint32_t seed, bool villageHasActors = true) {
        auto key = structureEntityClaimKey(
            ledger, family, anchor,
            {{clip.min.x, clip.min.y, clip.min.z}, {clip.max.x, clip.max.y, clip.max.z}}, villageHasActors);
        if (!key || !queued.insert(*key).second)
            return false;
        pending.push_back({handle, clip, std::move(*key), seed});
        return true;
    }
    template <class Ready, class Persist>
    std::vector<Job> claimReady(LifecycleJson &ledger, Ready ready, Persist persist) {
        auto &entries = ledger["structureEntries"];
        if (entries.is_null())
            entries = LifecycleJson::object();
        size_t attempts = std::min(StructureEntityBatchLimit, pending.size());
        std::vector<Job> work;
        work.reserve(attempts);
        while (attempts-- && !pending.empty()) {
            auto job = std::move(pending.front());
            pending.pop_front();
            if (entries.contains(job.key)) {
                queued.erase(job.key);
                continue;
            }
            bool available = false;
            try {
                available = ready(job);
            } catch (...) {
                pending.push_front(std::move(job));
                for (auto it = work.rbegin(); it != work.rend(); ++it)
                    pending.push_front(std::move(*it));
                throw;
            }
            if (!available) {
                pending.push_back(std::move(job));
                continue;
            }
            work.push_back(std::move(job));
        }
        // Preflight the entire batch before claiming any job. A later readiness
        // exception must not leave earlier jobs claimed only in memory and lost.
        for (auto const &job : work) {
            entries[job.key] = {
                {"state", "claimed"},
                {"clip", structureBoxJson({{job.clip.min.x, job.clip.min.y, job.clip.min.z},
                                           {job.clip.max.x, job.clip.max.y, job.clip.max.z}})}};
            queued.erase(job.key);
        }
        if (!work.empty())
            persist(ledger); // No caller can spawn before durable claims.
        return work;
    }
    void clear() {
        pending.clear();
        queued.clear();
    }
    // Call only after deferred job coordinates have been persisted. Retiring a
    // native start must also retire every pending pointer to it; claims are not
    // removed and remain authoritative when a replacement handle is scheduled.
    template <class Retained> void discardRetiredHandles(Retained retained) {
        std::erase_if(pending, [&](auto const &job) {
            if (retained(job.start))
                return false;
            queued.erase(job.key);
            return true;
        });
    }
};
template <class Queue, class Handle, class Population, class Loaded>
size_t enqueueLoadedPopulationOwners(Queue &queue, LifecycleJson const &ledger, Handle handle,
                                     Population const &population, std::string_view family,
                                     ChunkPosition anchor, uint32_t seed, Loaded loaded, int maxY=319) {
    size_t count = 0;
    for (auto const &[owner, markers] : population) {
        ChunkPosition cp{owner.first, owner.second};
        if (markers.empty() || !loaded(cp))
            continue;
        StructureBox clip{{cp.x * 16, -64, cp.z * 16}, {cp.x * 16 + 15, maxY, cp.z * 16 + 15}};
        using Clip = decltype(typename Queue::Job{}.clip);
        if (queue.enqueue(ledger, handle,
                          Clip{{clip.min.x, clip.min.y, clip.min.z}, {clip.max.x, clip.max.y, clip.max.z}},
                          family, anchor, seed, true))
            ++count;
    }
    return count;
}
template <class Queue, class Handle, class Loaded>
size_t enqueueLoadedVillageOwners(Queue &queue, LifecycleJson const &ledger, Handle handle,
                                  VillageLayoutPlan const &plan, ChunkPosition anchor, uint32_t seed,
                                  Loaded loaded) {
    return enqueueLoadedPopulationOwners(queue, ledger, handle, plan.population, "village", anchor, seed,
                                         loaded, plan.maxY);
}
inline void completeStructureEntityClaim(LifecycleJson &ledger, std::string const &key,
                                         LifecycleJson actors) {
    auto &entries = ledger.at("structureEntries");
    if (!entries.contains(key) || entries.at(key).value("state", std::string{}) != "claimed")
        throw std::runtime_error("Missing durable entity claim");
    entries[key] = {{"state", "completed"}, {"actors", std::move(actors)}};
}
inline void failStructureEntityClaim(LifecycleJson &ledger, std::string const &key, std::string const &error,
                                     LifecycleJson observedActors) {
    auto &entries = ledger.at("structureEntries");
    auto &entry = entries.at(key);
    if (entry.value("state", std::string{}) != "claimed")
        throw std::runtime_error("Missing durable entity claim for failure");
    entry["state"] = "review-failed";
    entry["error"] = error;
    entry["actors"] = std::move(observedActors);
    entry["partialEffectsMayExist"] = true;
}
inline LifecycleJson structureLedgerSeed(LifecycleJson const &recipe) {
    if (sharedStructurePlan(recipe.at("config")
                                .value("sharedRules", LifecycleJson::object())
                                .value("revision", std::string{})))
        return recipe.at("seedManifest").at("terrainSeed");
    return recipe.at("config").at("seed");
}
inline uint32_t structureEntitySeed(ChunkPosition owner, uint32_t seed) {
    return placementSeed(owner.x, owner.z, 146731099, seed);
}
template <class Get>
std::optional<StructurePoint> supportedStructureEntityPosition(StructurePoint p, Get blockName, int maxY=319) {
    for (int dy : {0, 1, -1, 2, -2, 3}) {
        StructurePoint q{p.x, p.y + dy, p.z};
        if (q.y <= -64 || q.y >= maxY)
            continue;
        auto floor = blockName(StructurePoint{q.x, q.y - 1, q.z});
        if (floor == "minecraft:air" || floor == "minecraft:water" || floor == "minecraft:lava")
            continue;
        if (blockName(q) == "minecraft:air" &&
            blockName(StructurePoint{q.x, q.y + 1, q.z}) == "minecraft:air")
            return q;
    }
    return {};
}
// Readiness is checked BEFORE durable claims. A partially placed house or an
// unloaded fragment must remain queued, rather than consume its only claim.
// Neighbour witnesses are inspected only after a read-only loaded check.
template <class Loaded, class Get>
std::optional<std::vector<StructureEntityMarker>>
prepareVillagePopulation(VillageLayoutPlan const &plan, ChunkPosition owner, Loaded loaded, Get name) {
    std::map<VillageXZ, bool> readiness;
    auto ready = [&](ChunkPosition cp) {
        auto [it, added] = readiness.try_emplace({cp.x, cp.z}, false);
        if (added)
            it->second = loaded(cp);
        return it->second;
    };
    if (!ready(owner))
        return {};
    auto markers = villagePopulation(plan, owner);
    if (markers.empty())
        return {};
    bool witnessed = false;
    for (auto const &w : plan.witnesses) {
        auto p = w.position;
        ChunkPosition cp{spacingCell(p.x, 16), spacingCell(p.z, 16)};
        if (!ready(cp))
            continue;
        if (name(p) == w.block.name) {
            witnessed = true;
            break;
        }
    }
    if (!witnessed)
        return {};
    std::vector<StructureEntityMarker> result;
    result.reserve(markers.size());
    for (auto const &m : markers) {
        auto p = supportedStructureEntityPosition(m.position, name, plan.maxY);
        if (!p)
            return {};
        result.push_back({*p, m.definition});
    }
    return result;
}
struct StructureEntityDefinition {
    std::string nameSpace, name, event;
};
inline StructureEntityDefinition structureEntityDefinition(std::string_view marker) {
    auto split = marker.find(':');
    if (split == std::string_view::npos || !split || split + 1 == marker.size())
        throw std::runtime_error("Invalid native entity marker");
    auto event = marker.find(':', split + 1);
    auto name = marker.substr(split + 1, event == std::string_view::npos ? marker.size() - split - 1
                                                                         : event - split - 1);
    if (name.empty())
        throw std::runtime_error("Empty native entity name");
    return {std::string(marker.substr(0, split)), std::string(name),
            event == std::string_view::npos ? "minecraft:entity_spawned"
                                            : std::string(marker.substr(event + 1))};
}
inline std::optional<StructureEntityDefinition> iglooEntityDefinition(std::string_view marker) {
    if (marker == "Villager")
        return StructureEntityDefinition{"minecraft", "villager_v2", "minecraft:spawn_cleric"};
    if (marker == "Zombie Villager")
        return StructureEntityDefinition{"minecraft", "zombie_villager_v2", "minecraft:entity_spawned"};
    return {};
}
struct StructureLootAssignment {
    StructurePoint position;
    std::string table;
    uint32_t seed;
};
// A fragment only handles its own marker. Existing container state belongs to
// the save, and must never be reseeded merely because a neighbour is generated.
inline bool structureLootMarkerOwned(StructureLootAssignment const &assignment, StructureBox const &owner,
                                     bool existingContainer) {
    return inStructureOwner(assignment.position, owner) && !existingContainer;
}
template <class Existing>
bool structureLootMarkerOwned(StructureLootAssignment const &assignment, StructureBox const &owner,
                              Existing existingContainer) {
    return inStructureOwner(assignment.position, owner) && !existingContainer();
}
} // namespace worldgen_rules
