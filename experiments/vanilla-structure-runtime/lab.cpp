#include "rules/StructureReservations.h"
#ifdef KARMA_CLIENT
#include "WindowsWorldHost.h"
#endif
#include "../../src/rules/VillageDecorExecution.h"
#include "../../src/rules/FossilExecution.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/block/MultifaceBlock.h"
// Native structure adapter, bound to the terrain runtime's active world.
#include "TerrainKernel.h"
#include "WorldTasks.h"
#include "ll/api/service/GamingStatus.h"
#include "native/Noise.h"
#include "BdsStructureBinding.h"
#include "BuildHeight.h"
#include "rules/StructureRules.h"
#include "rules/StructureWater.h"
#include "rules/VillageGrowth.h"
#include "mc/world/level/levelgen/structure/LegacyJigsawPlacement.h"
#include "mc/world/level/block/components/BlockLiquidDetectionComponent.h"
#include "rules/StructureRandom.h"
#include "rules/StructureCandidates.h"
#include "rules/ExtraExecution.h"
#include "rules/VillageFit.h"
#include "rules/StructurePlan.h"
#include "rules/ScatteredExecution.h"
#include "rules/CityPlan.h"
#include "rules/CityEnvironment.h"
#include "rules/CityExecution.h"
#include "mc/world/level/levelgen/v2/StructureSection.h"
#include "rules/FrozenTemplates.h"
#include "rules/FrozenCapture.h"
#include "rules/FrozenGround.h"
#include "rules/StructureLifecycle.h"
#include "rules/StructureStage.h"
#include "HookTransaction.h"
#include "FailureRecord.h"
#include "ReleaseVersions.h"
#include "rules/StructurePlanStore.h"
#include "rules/SmallFeaturePlan.h"
#include "EntityLedgerStore.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/Versions.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/runtime/RuntimeOverload.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionHeightRange.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockVolume.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include "mc/world/level/block/actor/BrushableBlockActor.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/SaveContext.h"
#include "mc/world/level/biome/registry/BiomeRegistry.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/biome/source/FixedBiomeSource.h"
#include "mc/world/level/levelgen/v1/IPreliminarySurfaceProvider.h"
#include "mc/world/level/levelgen/structure/random_scattered_large_feature_details/ScatteredFeatureStart.h"
#include "mc/world/level/levelgen/structure/RandomScatteredLargeFeature.h"
#include "mc/world/level/LevelSeed64.h"
#include "mc/world/level/biome/GetBiomeOptions.h"
#include "mc/util/Random.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/level/levelgen/WorldGenerator.h"
#include "mc/world/level/levelgen/VoidGenerator.h"
#include "mc/world/level/levelgen/structure/StructureFeatureRegistry.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <mutex>
#include <deque>
#include <atomic>
#include "mc/world/level/storage/LevelStorage.h"
#include "mc/world/level/storage/DBChunkStorage.h"
#include "mc/world/level/chunk/LevelChunkBlockActorAccessToken.h"
#include "mc/world/level/levelgen/structure/ScatteredFeaturePiece.h"
#include "mc/world/level/levelgen/structure/Igloo.h"
#include "mc/world/level/levelgen/structure/StructureManager.h"
#include "mc/world/level/levelgen/structure/LegacyStructureTemplate.h"
#include "mc/world/level/levelgen/structure/LegacyStructureBlockInfo.h"
#include "mc/world/level/levelgen/structure/VillageFeature.h"
#include "mc/world/level/levelgen/structure/PillagerOutpostFeature.h"
#include "mc/world/level/levelgen/structure/PillagerOutpostPieces.h"
#include "mc/world/level/levelgen/structure/RuinedPortalFeature.h"
#include "mc/world/level/levelgen/structure/ShipwreckFeature.h"
#include "mc/world/level/levelgen/structure/OceanRuinFeature.h"
#include "mc/world/level/levelgen/structure/OceanRuinPieces.h"
#include "mc/world/level/levelgen/structure/ShipwreckPiece.h"
#include "mc/world/level/levelgen/structure/RuinedPortalPiece.h"
#include "mc/world/level/levelgen/structure/BuriedTreasurePiece.h"
#include "mc/world/level/levelgen/structure/WoodlandMansionPieces.h"
#include "mc/world/level/levelgen/structure/MonumentBuilding.h"
#include "mc/world/level/levelgen/structure/StructureHelpers.h"
#include "mc/world/level/levelgen/v2/JigsawStructure.h"
#include "mc/world/level/levelgen/v2/GenerationContext.h"
#include "mc/world/level/levelgen/v2/ChunkAccessor.h"
#include "mc/world/level/levelgen/v2/StructureInstance.h"
#include "mc/world/level/levelgen/v2/StructureSection.h"
#include "mc/world/level/levelgen/structure/BuriedTreasureFeature.h"
#include "mc/world/level/levelgen/structure/OceanMonumentFeature.h"
#include "mc/world/level/levelgen/structure/StrongholdFeature.h"
#include "mc/world/level/levelgen/structure/WoodlandMansionFeature.h"
#include "mc/world/level/levelgen/structure/AncientCityFeature.h"
#include "mc/world/level/levelgen/structure/MineshaftStart.h"
#include "mc/world/level/levelgen/structure/MineshaftCorridor.h"
#include "mc/world/level/levelgen/structure/registry/JigsawStructureRegistry.h"
#include "mc/world/level/levelgen/structure/PoolElementStructurePiece.h"
#include "mc/world/level/levelgen/structure/structurepools/StructurePoolElement.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/LegacyStructureActorInfo.h"
#include <Windows.h>
#include "ll/api/service/Bedrock.h"
#include "ll/api/memory/Memory.h"
#include "mc/world/level/levelgen/flat/FlatWorldGenerator.h"
#include "mc/world/level/levelgen/feature/registry/FeatureRegistry.h"
#include "mc/world/level/levelgen/feature/gamerefs_feature/GameRefsFeature.h"
#include "mc/world/level/levelgen/feature/MonsterRoomFeature.h"
#include "mc/world/level/levelgen/feature/DesertWellFeature.h"
#include "mc/world/level/levelgen/feature/FossilFeature.h"
#include "mc/world/level/WorldBlockTarget.h"
#include "mc/world/actor/RenderParams.h"
#include "mc/util/MolangVariableMap.h"
#include "mc/world/level/levelgen/feature/FeaturePlacementFailureLog.h"
#include "mc/world/level/block/actor/MobSpawnerBlockActor.h"
#include "mc/world/level/BaseMobSpawner.h"
#include "mc/world/level/chunk/ChunkViewSource.h"
#include "FossilJournal.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/level/block/actor/SculkShriekerBlockActor.h"
#include "mc/world/actor/ai/village/Village.h"
#include "mc/world/actor/ai/village/VillageManager.h"
#include "mc/world/actor/ai/village/Raid.h"
#include "mc/world/level/block/chunk_volume/VolumeOf.h"
#include "mc/world/level/block/WallBlock.h"
#ifndef KARMA_CLIENT
#include "StructureTestCatalog.h"
#include "TestTeleportPolicy.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "mc/world/level/GameType.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "mc/server/ServerInstance.h"
#endif

namespace lab {
using J = nativegen::J;
using Start = RandomScatteredLargeFeatureDetails::ScatteredFeatureStart;
#ifdef KARMA_CLIENT
std::filesystem::path folder, clientTerrainFolder, clientWorldFolder;
std::filesystem::path clientVanillaFolder;
std::filesystem::path vanillaFolder() {
    return clientVanillaFolder;
}
std::string clientWorldIdentity;
std::string const &worldIdentity() {
    return clientWorldIdentity;
}
std::filesystem::path terrainFolder() {
    return clientTerrainFolder;
}
std::filesystem::path worldFolder() {
    return clientWorldFolder;
}
#else
std::filesystem::path const folder = "plugins/vanilla-structure-lab";
std::filesystem::path terrainFolder() {
    return "plugins/karma-worldgen";
}
std::filesystem::path vanillaFolder() {
    return "behavior_packs/vanilla";
}
#endif
std::string activeWorld;
#ifndef KARMA_CLIENT
std::filesystem::path worldFolder() {
    return std::filesystem::path("worlds") /
           std::filesystem::path(std::u8string(activeWorld.begin(), activeWorld.end()));
}
std::string const &worldIdentity() {
    return activeWorld;
}
#endif
J report;
std::atomic_int liveVillageSpacing{32};
thread_local std::optional<int> auditVillageSpacing;
struct StructureSession {
    terrain_runtime::WorldTasks tasks;
    J input;
    terrain_kernel::ContextPtr kernelContext;
    std::unique_ptr<terrain_kernel::Field> field;
};
StructureSession structureSession;
auto& input=structureSession.input;
auto& field=structureSession.field;
auto& kernelContext=structureSession.kernelContext;
auto& structureTasks=structureSession.tasks;
bool structureIncluded(std::string_view name) {
    return worldgen_rules::publicStructureIncluded(name);
}
bool structureInfinite() {
    return field && field->unbounded();
}
bool structureChunkInside(int x, int z) {
    return structureInfinite() ? terrain_bounds::gameContainsChunk(x, z)
                               : terrain_bounds::containsChunk(x, z);
}
bool structureBoxInside(int x0, int z0, int x1, int z1) {
    return structureInfinite() ? (x0 >= -30000000 && z0 >= -30000000 && x1 < 30000000 && z1 < 30000000)
                               : (x0 >= terrain_bounds::MinBlock && z0 >= terrain_bounds::MinBlock &&
                                  x1 <= terrain_bounds::MaxBlock && z1 <= terrain_bounds::MaxBlock);
}
std::unique_ptr<Start> pending;
int chunk(int x) {
    return int(std::floor(x / 16.));
}
void save() {
    std::ofstream(folder / "report.json") << report.dump(2);
}
class SurfaceProvider final : public IPreliminarySurfaceProvider {
  public:
    mutable unsigned calls = 0;
    int overrideHeight = -32768;
    mutable J samples = J::array();
    std::optional<short> getPreliminarySurfaceLevel(DividedPos2d<4> p) const override {
        ++calls;
        int h = overrideHeight > -32768 ? overrideHeight
                                        : field->surface(p.x * 4 + 32768, p.z * 4 + 32768).height;
        if (samples.size() < 32)
            samples.push_back(J{{"x", p.x}, {"z", p.z}, {"sampleHeight", h}});
        return short(h + 1);
    }
};
SurfaceProvider provider;
thread_local std::optional<uint> auditPlanningSeed;
thread_local std::vector<std::unique_ptr<StructureStart>> *auditPlanningStarts = nullptr;
thread_local VillageFeature *auditVillageDelegate = nullptr;
uint structureSeed(Dimension &d) {
    return auditPlanningSeed ? *auditPlanningSeed : d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
}

Dimension &dimension(CommandOrigin const &origin) {
    if (origin.getOriginType() != CommandOriginType::DedicatedServer)
        throw std::runtime_error("Console only");
    auto level = origin.getLevel();
    if (!level || level->getLevelData().getLevelName() != activeWorld)
        throw std::runtime_error("Structure world does not match activation");
    return *level->getOrCreateDimension(0).lock();
}
BoundingBox boxFromJson(J const &j) {
    return {{j[0].get<int>(), j[1].get<int>(), j[2].get<int>()},
            {j[3].get<int>(), j[4].get<int>(), j[5].get<int>()}};
}
J boxJson(BoundingBox const &b) {
    return J::array({b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z});
}
void ensureLoaded(Dimension &dim, BoundingBox const &b) {
    for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
        for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x) {
            auto c = dim.getChunkSource().getExistingChunk({x, z});
            if (!c || c->getState().load() != ChunkState::Loaded)
                throw std::runtime_error("Required local chunks are not yet loaded");
        }
}
void initialize() {
    if (!field) {
        input = entity_store::read((terrainFolder() / "native/native-input.json"));
        // The executable owns structure policy. Old seed packages must not silently
        // select a different planner on BDS than on the Windows client.
        auto &rules = input["config"]["sharedRules"];
        auto previous = rules.value("revision", std::string{});
        rules["source"] = "llbds";
        rules["revision"] = worldgen_rules::StructurePlanRevision;
        rules["villageSizeClasses"] = true;
        kernelContext = std::make_shared<terrain_kernel::Context>();
        // Private to this host/world; never retain a previous world's directories.
        field = std::make_unique<terrain_kernel::Field>(input, kernelContext);
        liveVillageSpacing.store(input.at("config").value("villageSpacing",32)==20?20:32);
        rules["villageSpacing"] = liveVillageSpacing.load();
        std::ofstream(folder / "effective-structure-rules.json")
            << J{{"world", activeWorld}, {"savedRevision", previous}, {"effective", rules}}.dump(2);
        structureTasks.open();
    }
}
// Point-query adapter for legacy scattered-feature eligibility only.
// Inherited bulk fill/area methods are intentionally not installed in world generation.
class TerrainPointBiomes final : public FixedBiomeSource {
    std::vector<Biome const *> entries;

  public:
    TerrainPointBiomes(Dimension &d, Biome const &fallback) : FixedBiomeSource(fallback) {
        J metadata = entity_store::read((terrainFolder() / "terrain.json"));
        for (std::string name : metadata["biomeNames"]) {
            auto b = d.getBiomeRegistry().lookupByName(name);
            if (!b)
                b = d.getBiomeRegistry().lookupByName("minecraft:" + name);
            if (!b)
                throw std::runtime_error("Unknown actual biome");
            entries.push_back(b);
        }
    }
    Biome const *_getBiome(GetBiomeOptions const &options) const override {
        auto const &p = *options.mBlockPos;
        auto s = field->surface(p.x + 32768, p.z + 32768);
        return entries.at(s.biome);
    }
};
thread_local bool adaptHeightMap = false;
thread_local unsigned heightMapCalls = 0;
thread_local std::map<std::pair<int, int>, std::vector<short>> *planningHeights = nullptr;
std::vector<short> terrainHeightMap(ChunkPos cp) {
    auto key = std::pair{cp.x, cp.z};
    if (planningHeights) {
        auto it = planningHeights->find(key);
        if (it != planningHeights->end())
            return it->second;
    }
    std::vector<short> heights(256);
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x)
            heights[z * 16 + x] =
                short(field->surface(cp.x * 16 + x + 32768, cp.z * 16 + z + 32768).height + 1);
    if (planningHeights)
        planningHeights->emplace(key, heights);
    return heights;
}
LL_TYPE_INSTANCE_HOOK(SurfaceMapProbe, ll::memory::HookPriority::Normal, WorldGenerator,
                      &WorldGenerator::computeChunkHeightMap, std::vector<short>, ChunkPos const &cp) {
    if (!adaptHeightMap)
        return origin(cp);
    ++heightMapCalls;
    return terrainHeightMap(cp);
}
bool structureGeneratorMatches(void const *generator) {
    // An ICF stub may belong to unrelated classes too: compare the known
    // generator pointer before reading any member through the incoming this.
    auto level=ll::service::getLevel();
    if (!level || level->getLevelData().getLevelName() != activeWorld)
        return false;
    auto dimension=level->getDimension(0).lock();
    return dimension && dimension->getWorldGenerator() == generator;
}
// The hook type is the common base because an ICF target can be entered with
// either generator's this pointer. Never read derived Flat-only fields here.
LL_TYPE_INSTANCE_HOOK(VillageHeightProbe, ll::memory::HookPriority::Normal, WorldGenerator,
                      ll::memory::unchecked(&FlatWorldGenerator::$prepareHeights), void, BlockVolume &volume, ChunkPos const &cp,
                      std::vector<short> *heights, bool flag) {
    origin(volume, cp, heights, flag);
    if (adaptHeightMap && heights && structureGeneratorMatches(this))
        *heights = terrainHeightMap(cp);
}
J scanCandidates(Dimension &dim, int mode) {
    bool reverse = mode == 1 || mode == 4 || mode == 6;
    adaptHeightMap = mode >= 3;
    heightMapCalls = 0;
    struct ResetMap {
        ~ResetMap() {
            adaptHeightMap = false;
        }
    } resetMap;
    provider.overrideHeight = mode == 2 ? 80 : -32768;
    struct Reset {
        ~Reset() {
            provider.overrideHeight = -32768;
        }
    } reset;
    initialize();
    auto biome = dim.getBiomeRegistry().lookupByName("desert");
    if (!biome)
        biome = dim.getBiomeRegistry().lookupByName("minecraft:desert");
    if (!biome)
        throw std::runtime_error("Vanilla desert missing");
    FixedBiomeSource fixed(*biome);
    TerrainPointBiomes actual(dim, *biome);
    BiomeSource const &biomes = mode >= 5 ? static_cast<BiomeSource const &>(actual) : fixed;
    auto worldSeed = dim.getLevel().getLevelData().getSeed();
    uint seed = worldSeed.to32BitRandomSeed();
    RandomScatteredLargeFeature feature(seed);
    Random random(seed, true);
    J positions = J::array();
    unsigned candidates = 0;
    provider.calls = 0;
    provider.samples = J::array();
    auto testBiome = StructureFeature::getBiomeForFeatureGeneration(biomes, ChunkPos{200, -442}, provider);
    std::ofstream(folder / "selection-initialization.json")
        << J{{"spacing", feature.mSpacing},
             {"separation", feature.mMinSeparation},
             {"allowedBiomeCount", feature.mAllowedBiomes->size()},
             {"dimensionSeaLevel", dim.mSeaLevel},
             {"nativeBiomeMatchesDesert", testBiome.second == biome},
             {"fixedSourceMatchesRequired",
              biomes.containsOnly(testBiome.first.x, testBiome.first.y, testBiome.first.z, 0,
                                  feature.getRequiredBiomes())},
             {"nativeBiomeQueryPosition", {testBiome.first.x, testBiome.first.y, testBiome.first.z}}}
               .dump(2);
    auto t = std::chrono::steady_clock::now();
    for (int iz = 0; iz < 200; ++iz)
        for (int ix = 0; ix < 256; ++ix) {
            int x = reverse ? 383 - ix : 128 + ix, z = reverse ? -301 - iz : -500 + iz;
            ChunkPos cp{x, z};
            if (!feature.isFeatureChunk(biomes, random, cp, seed, provider, dim))
                continue;
            ++candidates;
            auto s = field->surface(x * 16 + 32768, z * 16 + 32768);
            if (s.biome != 11 || s.water >= s.height || s.lake || s.river || s.volcanic || s.stone)
                continue;
            int lo = INT_MAX, hi = -64;
            bool fit = true;
            for (int dz = -8; dz <= 32; dz += 8)
                for (int dx = -8; dx <= 32; dx += 8) {
                    auto a = field->surface(x * 16 + dx + 32768, z * 16 + dz + 32768);
                    lo = std::min(lo, a.height);
                    hi = std::max(hi, a.height);
                    fit &= a.biome == 11 && !a.lake && !a.river && a.water < a.height;
                }
            if (fit && hi - lo <= 6)
                positions.push_back(J{{"x", x * 16}, {"z", z * 16}, {"groundMin", lo}, {"groundMax", hi}});
        }
    J result{{"adaptedHeightMap", adaptHeightMap},
             {"heightMapCalls", heightMapCalls},
             {"nativeCandidateCount", candidates},
             {"eligiblePositions", positions},
             {"scannedChunks", 51200},
             {"reverse", reverse},
             {"controlHeight80", mode == 2},
             {"fixedBiomePrototype", mode < 5},
             {"actualHabitatFilter", true},
             {"worldSeed", worldSeed.toUiString()},
             {"legacySeed", seed},
             {"heightQueries", provider.calls},
             {"ms", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count()}};
    std::ofstream(folder / (mode == 5   ? "candidate-actual-forward.json"
                            : mode == 6 ? "candidate-actual-reverse.json"
                            : mode == 3 ? "candidate-adapted-forward.json"
                            : mode == 4 ? "candidate-adapted-reverse.json"
                            : mode == 2 ? "candidate-height-control.json"
                            : reverse   ? "candidate-reverse.json"
                                        : "candidate-forward.json"))
        << result.dump(2);
    return result;
}
void plan(Dimension &dim, int x, int z) {
    initialize();
    int lo = INT_MAX, hi = -64;
    unsigned desert = 0;
    for (int dz = -8; dz <= 32; dz += 4)
        for (int dx = -8; dx <= 32; dx += 4) {
            auto s = field->surface(x + dx + 32768, z + dz + 32768);
            lo = std::min(lo, s.height);
            hi = std::max(hi, s.height);
            desert += s.biome == 11 && !s.lake && !s.river && s.water < s.height;
        }
    if (desert < 110 || hi - lo > 6)
        throw std::runtime_error("Candidate is not a sufficiently dry, gently sloping desert footprint");
    auto biome = dim.getBiomeRegistry().lookupByName("desert");
    if (!biome)
        biome = dim.getBiomeRegistry().lookupByName("minecraft:desert");
    if (!biome)
        throw std::runtime_error("Vanilla desert missing");
    FixedBiomeSource biomes(*biome);
    provider.calls = 0;
    provider.samples = J::array();
    auto t = std::chrono::steady_clock::now();
    pending = std::make_unique<Start>(biomes, field->seaLevel(), chunk(x), chunk(z), provider);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
    if (!pending->isValid() || pending->pieces->empty())
        throw std::runtime_error("Vanilla blueprint returned no structure");
    report = J{{"phase", "planned"},
               {"automaticSelection", false},
               {"fixedBiomePrototype", true},
               {"type", std::string(pending->getStructureName())},
               {"pieces", pending->pieces->size()},
               {"box", boxJson(*pending->boundingBox)},
               {"groundMin", lo},
               {"groundMax", hi},
               {"heightQueries", provider.calls},
               {"heightQuerySamples", provider.samples},
               {"planMs", ms},
               {"terrainSeed", input["config"]["seed"]}};
    save();
}
void naturalPlan(Dimension &dim, int x, int z) {
    initialize();
    auto b = dim.getBiomeRegistry().lookupByName("minecraft:desert");
    if (!b)
        throw std::runtime_error("No desert");
    TerrainPointBiomes biomes(dim, *b);
    uint seed = dim.getLevel().getLevelData().getSeed().to32BitRandomSeed();
    RandomScatteredLargeFeature feature(seed);
    Random random(seed, true);
    ChunkPos requested{chunk(x), chunk(z)};
    auto candidate = StructureFeature::getChunkPosInSpace(requested, random, seed, feature.mSpacing,
                                                          feature.mMinSeparation, 0xdb1471, false);
    adaptHeightMap = true;
    heightMapCalls = 0;
    struct Reset {
        ~Reset() {
            adaptHeightMap = false;
        }
    } reset;
    if (!feature.isFeatureChunk(biomes, random, candidate, seed, provider, dim))
        throw std::runtime_error("Native candidate rejected actual biome or terrain");
    plan(dim, candidate.x * 16, candidate.z * 16);
    report["automaticSelection"] = true;
    report["selection"] = {{"engine", "RandomScatteredLargeFeature"},
                           {"worldSeed", dim.getLevel().getLevelData().getSeed().toUiString()},
                           {"legacySeed", seed},
                           {"spacing", int(feature.mSpacing)},
                           {"separation", int(feature.mMinSeparation)},
                           {"salt", 0xdb1471},
                           {"actualPointBiome", true},
                           {"heightMapCalls", heightMapCalls},
                           {"candidate", {candidate.x, candidate.z}}};
    report["automaticGenerationStage"] = false;
    save();
}
void place(Dimension &dim) {
    if (!pending || report.value("phase", std::string{}) != "planned")
        throw std::runtime_error("Plan first; no repeat placement");
    auto b = *pending->boundingBox;
    BoundingBox local{{b.min.x - 16, -64, b.min.z - 16}, {b.max.x + 16, (field->generationHeight()-65), b.max.z + 16}};
    ensureLoaded(dim, local);
    auto &blocks = dim.getBlockSourceFromMainChunkSource();
    J timings = J::array();
    for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
        for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x) {
            Random random(input["config"]["seed"].get<uint32_t>() ^ uint(x) * 73856093u ^ uint(z) * 19349663u,
                          true);
            BoundingBox clip{{x * 16, -64, z * 16}, {x * 16 + 15, (field->generationHeight()-65), z * 16 + 15}};
            auto t = std::chrono::steady_clock::now();
            bool ok = pending->postProcess(blocks, random, clip);
            timings.push_back(
                J{{"x", x},
                  {"z", z},
                  {"ok", ok},
                  {"ms",
                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count()}});
        }
    pending->calculateBoundingBox();
    report["box"] = boxJson(*pending->boundingBox);
    report["phase"] = "placed";
    report["placementChunks"] = timings;
    save();
    pending.reset();
}
void inspect(Dimension &dim) {
    if (report.empty()) {
        std::ifstream f(folder / "report.json");
        f >> report;
    }
    auto b = boxFromJson(report["box"]);
    b.min.y = std::max(-60, b.min.y - 16);
    b.max.y = std::min((field->generationHeight()-65), b.max.y + 8);
    ensureLoaded(dim, b);
    auto &blocks = dim.getBlockSourceFromMainChunkSource();
    J counts = J::object(), chests = J::array(), interior = J::object(), functional = J::array();
    uint64_t hash = 1469598103934665603ull, stateHash = hash, weatherHash = hash;
    auto airHash = uint64_t(Block::tryGetFromRegistry("minecraft:air")->mSerializationIdHash);
    auto core = boxFromJson(report["box"]);
    for (int z = b.min.z; z <= b.max.z; ++z)
        for (int x = b.min.x; x <= b.max.x; ++x)
            for (int y = b.min.y; y <= b.max.y; ++y) {
                BlockPos p{x, y, z};
                auto const &block = blocks.getBlock(p);
                auto const &n = block.getTypeName();
                counts[n] = counts.value(n, 0) + 1;
                for (unsigned char c : n)
                    hash = (hash ^ c) * 1099511628211ull;
                if (y >= core.min.y && y <= core.max.y)
                    interior[n] = interior.value(n, 0) + 1;
                uint64_t state = block.mSerializationIdHash;
                for (int shift = 0; shift < 64; shift += 8)
                    stateHash = (stateHash ^ ((state >> shift) & 255)) * 1099511628211ull;
                uint64_t stableState = n == "minecraft:snow_layer" ? airHash : state;
                for (int shift = 0; shift < 64; shift += 8)
                    weatherHash = (weatherHash ^ ((stableState >> shift) & 255)) * 1099511628211ull;
                if (n == "minecraft:chest") {
                    auto actor = blocks.getBlockEntity(p);
                    J c{{"x", x}, {"y", y}, {"z", z}, {"blockActor", actor != nullptr}};
                    if (actor) {
                        auto chest = static_cast<ChestBlockActor const *>(actor);
                        c["lootTable"] = *chest->mLootTable;
                        c["lootSeed"] = chest->mLootTableSeed;
                        unsigned filled = 0;
                        for (int slot = 0; slot < chest->getContainerSize(); ++slot)
                            if (!chest->getItem(slot).isNull())
                                ++filled;
                        c["filledSlots"] = filled;
                    }
                    chests.push_back(c);
                }
                // Console-only readback: native blocks need their original configuration,
                // not merely a material-count check. Clone saving does not open containers.
                if (n == "minecraft:trial_spawner" || n == "minecraft:vault" ||
                    n == "minecraft:suspicious_sand" || n == "minecraft:suspicious_gravel" ||
                    n == "minecraft:mob_spawner" || n == "minecraft:sculk_shrieker" ||
                    n == "minecraft:dispenser" || n == "minecraft:barrel") {
                    auto actor = blocks.getBlockEntity(p);
                    J row{{"pos", {x, y, z}}, {"block", n}, {"blockActor", actor != nullptr}};
                    auto state =
                        J::parse(block.mSerializationId->toSnbt(SnbtFormat::Jsonify), nullptr, false, true);
                    if (!state.is_discarded())
                        row["state"] = std::move(state);
                    if (actor) {
                        CompoundTag tag;
                        SaveContext context;
                        row["saved"] = actor->save(tag, context);
                        row["nbt"] = tag.toSnbt(SnbtFormat::Minimize);
                        auto data = J::parse(tag.toSnbt(SnbtFormat::Jsonify), nullptr, false, true);
                        if (!data.is_discarded())
                            row["data"] = std::move(data);
                    }
                    functional.push_back(std::move(row));
                }
            }
    report["inspection"] = {{"counts", counts},
                            {"chests", chests},
                            {"functionalBlocks", functional},
                            {"blockNameHash", std::to_string(hash)},
                            {"blockStateHash", std::to_string(stateHash)},
                            {"weatherNormalizedStateHash", std::to_string(weatherHash)},
                            {"boundingBoxMaterials", interior}};
    save();
}
#include "Igloo.inc"
std::optional<IglooLayout> pendingIgloo;
#include "Automatic.inc"
#include "IglooEntities.inc"
#include "StructureCacheMaintenance.inc"
#include "Acceptance.inc"
#include "InteractionAudit.inc"
#include "MemoryStructureAudit.inc"
#include "AcceptanceDiagnostics.inc"
#include "AtlasCatalog.inc"
#ifndef KARMA_CLIENT
#include "TestTeleport.inc"
#endif
class Mod {
  public:
    static Mod &getInstance() {
        static Mod m;
        return m;
    }
    bool load() {
        std::ofstream(folder / "runtime-version.txt") << ll::getGameVersion().to_string();
        return std::filesystem::exists(folder / "config.json") &&
               ll::getGameVersion().to_string() == "1.26.20+1f6d1b1";
    }
    bool enable() {
        terrain_runtime::HookTransaction hooks;
        J marker = entity_store::read(folder / "config.json");
        automaticStructures = marker.value("automaticStructures", false);
        initialize();
#ifndef KARMA_CLIENT
        auto activation = entity_store::read(terrainFolder() / "active-world.json");
        activeWorld = terrain_runtime::structureWorldName(activation);
        terrain_runtime::validateStructureBinding(activation,
                                                  entity_store::read(terrainFolder() / "terrain.json"), input,
                                                  entity_store::read(worldFolder() / "karma-worldgen.json"));
        if (auto level = ll::service::getLevel();
            level &&
            (level->getLevelData().getLevelName() != activeWorld ||
             level->getLevelData().getSeed().toUiString() != activation.at("worldSeed").get<std::string>()))
            throw std::runtime_error("Running world does not match structure terrain activation");
        ll::mod::NativeMod::current()->getLogger().info(
            "Structures follow terrain world={} seed={}; automatic={} (legacy config worldName is ignored)",
            activeWorld, activation.at("worldSeed").get<std::string>(), automaticStructures.load());
#endif
        {
            std::lock_guard guard(stageMutex);
            initializeSmallHistory();
            initializeFossilJournal();
            initializeEntityLedger();
        }
        structureStageFailed = false;
        hooks.add<OwnedStructureCollection>("OwnedStructureCollection");
        hooks.add<SurfaceMapProbe>("SurfaceMapProbe");
        hooks.add<ModernGround>("ModernGround");
        hooks.add<VillageBuilderDepth>("VillageBuilderDepth");
        hooks.add<VillageBuilderBudget>("VillageBuilderBudget");
        hooks.add<ShipGround>("ShipGround");
        hooks.add<ShipGroundLegacy>("ShipGroundLegacy");
        hooks.add<PortalGround>("PortalGround");
        hooks.add<RuinGround>("RuinGround");
        hooks.add<MineSupport>("MineSupport");
        hooks.add<MineWeb>("MineWeb");
        hooks.add<SeededStructureRead>("SeededStructureRead");
        hooks.add<SeededStructureLayer>("SeededStructureLayer");
        hooks.add<SeededStructureSolid>("SeededStructureSolid");
        hooks.add<FragmentChunkWrite>("FragmentChunkWrite");
        hooks.add<FragmentChunkFastWrite>("FragmentChunkFastWrite");
        hooks.add<StructurePrefill>("StructurePrefill");
        hooks.add<DecorationTask>("DecorationTask");
        installStructureGeneratorHooks(hooks);
        smallFeaturesEnabled = marker.value("smallFeatures", false);
        hooks.add<SmallWrite>("SmallWrite");
        hooks.add<SmallWriteSimple>("SmallWriteSimple");
        hooks.add<SmallWriteFast>("SmallWriteFast");
        hooks.add<FossilHeightXZ>("FossilHeightXZ");
        hooks.add<FossilHeightPos>("FossilHeightPos");
        hooks.add<FossilCorners>("FossilCorners");
        if (auto level = ll::service::getLevel()) {
            warmMansionTemplates(*level);
            warmVillageTemplates(*level);
            warmFossilTemplates(*level);
        }
        entityTickListener =
            ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto &e) {
                try {
                    warmMansionTemplates(e.level());
                    warmVillageTemplates(e.level());
                    warmFossilTemplates(e.level());
                    collectStructureCaches(e.level());
                    recoverLoadedStructurePlans(e.level());
                    tickIglooEntities(e.level());
                    tickStructureEntities(e.level());
                    tickInteractionAudit(e.level());
                    tickAtlasCatalog(e.level());
                } catch (std::exception const &ex) {
                    ++stageErrors;
                    reportStructureFailure("structure tick", ex);
                }
            });
#ifndef KARMA_CLIENT
        registerStructureTest(marker);
#endif
        auto &cmd = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
            "structurelab", "Native structure diagnostics", CommandPermissionLevel::Owner);
        cmd.runtimeOverload().text("atlas").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    atlasCatalog.start(dimension(o));
                    out.success("LAB atlas started total={}", atlasCatalog.total);
                } catch (std::exception const &e) {
                    out.error("LAB atlas error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("atlasstatus")
            .execute([](CommandOrigin const &, CommandOutput &out, ll::command::RuntimeCommand const &) {
                out.success("LAB atlas active={} visited={}/{} candidates={} maxBatchMs={} error={}",
                            atlasCatalog.active, atlasCatalog.visited, atlasCatalog.total,
                            atlasCatalog.rows.size(), atlasCatalog.maxBatchMs, atlasCatalog.error);
            });
        cmd.runtimeOverload()
            .text("regsurvey")
            .required("profile", ll::command::ParamKind::Int)
            .required("type", ll::command::ParamKind::Int)
            .required("offset", ll::command::ParamKind::Int)
            .required("count", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    surveyAuditPlans(dimension(o), std::get<int>(*c["profile"].as_ptr()),
                                     std::get<int>(*c["type"].as_ptr()), std::get<int>(*c["offset"].as_ptr()),
                                     std::get<int>(*c["count"].as_ptr()));
                    out.success("LAB audit survey");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("memplan")
            .required("profile", ll::command::ParamKind::Int)
            .required("type", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .required("order", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    runMemoryPlanAudit(dimension(o), std::get<int>(*c["profile"].as_ptr()),
                                       std::get<int>(*c["type"].as_ptr()), std::get<int>(*c["x"].as_ptr()),
                                       std::get<int>(*c["z"].as_ptr()), std::get<int>(*c["order"].as_ptr()));
                    out.success("LAB memory plan");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("memtemple")
            .required("order", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    memoryTempleAudit(dimension(o), std::get<int>(*c["order"].as_ptr()));
                    out.success("LAB memory temple");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("memprobe")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    memoryAuditProbe(dimension(o));
                    out.success("LAB memory probe");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("simaudit")
            .required("action", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("y", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .required("value", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    interactionAudit(dimension(o), std::get<int>(*c["action"].as_ptr()),
                                     {std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["y"].as_ptr()),
                                      std::get<int>(*c["z"].as_ptr())},
                                     std::get<int>(*c["value"].as_ptr()));
                    out.success("LAB simulated audit");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("fossilfind")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .required("radius", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    findFrozenFossils(dimension(o), std::get<int>(*c["x"].as_ptr()),
                                      std::get<int>(*c["z"].as_ptr()), std::get<int>(*c["radius"].as_ptr()));
                    out.success("LAB fossil candidates");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("fossilinspect")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .required("deep", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    inspectFrozenFossil(dimension(o), std::get<int>(*c["x"].as_ptr()),
                                        std::get<int>(*c["z"].as_ptr()),
                                        std::get<int>(*c["deep"].as_ptr()) != 0);
                    out.success("LAB fossil inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("fossilstatus")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    dimension(o);
                    std::lock_guard guard(stageMutex);
                    fossilStatus();
                    out.success("LAB fossil status");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("smallcatalog")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    nativeFeatureCatalog(dimension(o));
                    out.success("LAB small catalog");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("smallfind")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    findSmallCandidates(dimension(o), std::get<int>(*c["x"].as_ptr()),
                                        std::get<int>(*c["z"].as_ptr()));
                    out.success("LAB small candidates");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("smallinspect")
            .required("x", ll::command::ParamKind::Int)
            .required("y", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    inspectSmallFeature(dimension(o),
                                        {std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["y"].as_ptr()),
                                         std::get<int>(*c["z"].as_ptr())});
                    out.success("LAB small inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("safespot")
            .required("type", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    safeDestination(dimension(o), std::get<int>(*c["type"].as_ptr()),
                                    std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["z"].as_ptr()));
                    out.success("LAB safe destination");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("actoraudit")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    auto &d = dimension(o);
                    std::lock_guard guard(stageMutex);
                    actorAudit(d.getLevel());
                    out.success("LAB actor audit");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villageentityqueue")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    ChunkPos anchor{chunk(std::get<int>(*c["x"].as_ptr())),
                                    chunk(std::get<int>(*c["z"].as_ptr()))};
                    auto &p = villagePlan(d, anchor, seed);
                    if (!p.start)
                        throw std::runtime_error("No village");
                    auto b = *p.start->boundingBox;
                    ensureLoaded(d, b);
                    villageActorSources(p);
                    for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
                        for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x)
                            queueStructureEntities(p.start,
                                                   {{x * 16, -64, z * 16}, {x * 16 + 15, (field->generationHeight()-65), z * 16 + 15}},
                                                   "village", anchor, seed);
                    out.success("LAB village entities queued {}", nativeEntityJobs.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("scatteredfind")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    std::map<std::string, int> counts;
                    J rows = J::array();
                    for (int z = structureCell(-terrain_bounds::HalfChunks);
                         z <= structureCell(terrain_bounds::HalfChunks - 1); ++z)
                        for (int x = structureCell(-terrain_bounds::HalfChunks);
                             x <= structureCell(terrain_bounds::HalfChunks - 1); ++x) {
                            auto cp = nativeCandidateForCell(x, z, seed);
                            if (cp.x * 16 < terrain_bounds::MinBlock + 160 ||
                                cp.z * 16 < terrain_bounds::MinBlock + 160 ||
                                cp.x * 16 > terrain_bounds::MaxBlock - 160 ||
                                cp.z * 16 > terrain_bounds::MaxBlock - 160)
                                continue;
                            auto &p = autoBlueprint(d, cp, seed);
                            if (!p.start)
                                continue;
                            std::string type(p.start->getStructureName());
                            if (counts[type] >= 2)
                                continue;
                            ++counts[type];
                            rows.push_back({{"family", type},
                                            {"anchor", {cp.x * 16, cp.z * 16}},
                                            {"box", boxJson(*p.start->boundingBox)},
                                            {"basement", bool(p.igloo && p.igloo->ladders)}});
                        }
                    std::ofstream(folder / "scattered-found.json") << rows.dump(2);
                    out.success("LAB scattered found {}", rows.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        for (bool village : {false, true})
            cmd.runtimeOverload()
                .text(village ? "villageplants" : "iglooplants")
                .required("x", ll::command::ParamKind::Int)
                .required("z", ll::command::ParamKind::Int)
                .required("apply", ll::command::ParamKind::Int)
                .execute([village](CommandOrigin const &o, CommandOutput &out,
                                   ll::command::RuntimeCommand const &c) {
                    try {
                        std::lock_guard guard(stageMutex);
                        repairStructurePlants(dimension(o), std::get<int>(*c["x"].as_ptr()),
                                              std::get<int>(*c["z"].as_ptr()), village,
                                              std::get<int>(*c["apply"].as_ptr()) != 0);
                        out.success("LAB plants inspected/repaired");
                    } catch (std::exception const &e) {
                        out.error("LAB error {}", e.what());
                    }
                });
        cmd.runtimeOverload()
            .text("iglootestremove")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = autoBlueprint(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.igloo)
                        throw std::runtime_error("No igloo");
                    auto rows = iglooActors(d, p.igloo->bounds);
                    unsigned removed = 0;
                    for (auto *a : d.getLevel().getRuntimeActorList())
                        for (auto const &r : rows)
                            if (r["type"].get<std::string>().find("zombie_villager") != std::string::npos &&
                                a->getOrCreateUniqueID().rawID == r["id"].get<int64_t>()) {
                                a->remove();
                                ++removed;
                            }
                    out.success("LAB removed residents {}", removed);
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("iglooentities")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = autoBlueprint(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.igloo)
                        throw std::runtime_error("No igloo");
                    J result{{"actors", iglooActors(d, p.igloo->bounds)}, {"ledger", entityLedger}};
                    std::ofstream(folder / "igloo-entity-inspection.json") << result.dump(2);
                    out.success("LAB entities {}", result.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("preheight")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    auto &d = dimension(o);
                    int x = std::get<int>(*c["x"].as_ptr()), z = std::get<int>(*c["z"].as_ptr());
                    auto ch = d.getChunkSource().getExistingChunk({chunk(x), chunk(z)});
                    if (!ch || !ch->mPreWorldGenHeightmap)
                        throw std::runtime_error("Not loaded or map missing");
                    auto const &hm = *ch->mPreWorldGenHeightmap;
                    out.success("LAB preheight stored={} actual={}", hm.at((z & 15) * 16 + (x & 15)),
                                field->surface(x + 32768, z + 32768).height);
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villagecanopy")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = villagePlan(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.start)
                        throw std::runtime_error("No village");
                    inspectVillage(d, p, 64);
                    out.success("LAB village inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villageinspect")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = villagePlan(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.start)
                        throw std::runtime_error("No village");
                    inspectVillage(d, p);
                    out.success("LAB village inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villagegrass")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .required("apply", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = villagePlan(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.start)
                        throw std::runtime_error("No village");
                    auto b = *p.start->boundingBox;
                    b.min.x -= 6;
                    b.min.z -= 6;
                    b.max.x += 6;
                    b.max.z += 6;
                    ensureLoaded(d, b);
                    bool apply = std::get<int>(*c["apply"].as_ptr()) != 0;
                    auto &blocks = d.getBlockSourceFromMainChunkSource();
                    J changes = J::array();
                    plantRepairChanges = &changes;
                    plantRepairPreview = !apply;
                    struct Reset {
                        ~Reset() {
                            plantRepairChanges = nullptr;
                            plantRepairPreview = false;
                        }
                    } reset;
                    unsigned count = 0;
                    for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
                        for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x) {
                            auto n = clearUnsupportedGroundPlants(blocks, {x, z},
                                                                  p.diagnostics["groundMin"].get<int>() - 8,
                                                                  p.diagnostics["groundMax"].get<int>() + 8);
                            count += n;
                            if (n && apply) {
                                auto ch = d.getChunkSource().getExistingChunk({x, z});
                                ch->setUnsaved();
                                ch->markSaveNonActorDataIfDirty();
                            }
                        }
                    std::ofstream(folder / "grass-validation.json")
                        << J{{"apply", apply}, {"invalid", count}, {"changes", changes}}.dump(2);
                    out.success("LAB grass invalid={}", count);
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villagevariants")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    J rows = J::array();
                    for (int biomeId : {5, 6, 7, 11, 12}) {
                        auto name = villageBiome(biomeId);
                        auto b = d.getBiomeRegistry().lookupByName(name);
                        FixedBiomeSource bs(*b);
                        VillageFeature f(seed, 32, 8, d.getBiomeRegistry());
                        Random rng(seed, true);
                        ChunkPos cp{169, -595};
                        StructureFeature::setRandomSeedFor(rng, cp.x, cp.z, 10387312, seed);
                        std::map<std::pair<int, int>, std::vector<short>> h;
                        planningHeights = &h;
                        adaptHeightMap = true;
                        struct Reset {
                            ~Reset() {
                                adaptHeightMap = false;
                                planningHeights = nullptr;
                            }
                        } reset;
                        auto start = f.createStructureStart(d, bs, rng, cp, provider);
                        J row = {{"biome", name},
                                 {"valid", bool(start) && start->isValid()},
                                 {"fixtureOnly", true}};
                        if (start) {
                            row["box"] = boxJson(*start->boundingBox);
                            row["templates"] = J::array();
                            for (auto const &p : *start->pieces)
                                row["templates"].push_back(
                                    *static_cast<PoolElementStructurePiece *>(p.get())->mElement.mLocation);
                        }
                        rows.push_back(row);
                    }
                    std::ofstream(folder / "village-variants.json") << rows.dump(2);
                    out.success("LAB variants checked");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("extranear")
            .required("type", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    int type = std::get<int>(*c["type"].as_ptr()), wx = std::get<int>(*c["x"].as_ptr()),
                        wz = std::get<int>(*c["z"].as_ptr());
                    auto *s = extraSpec(type);
                    if (!s)
                        throw std::runtime_error("Family not enabled");
                    int gx = spacingCell(chunk(wx), s->spacing), gz = spacingCell(chunk(wz), s->spacing);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    J rows = J::array();
                    for (int z = gz - 2; z <= gz + 2; ++z)
                        for (int x = gx - 2; x <= gx + 2; ++x) {
                            auto &p = extraPlan(d, type, extraCandidate(*s, x, z, seed));
                            if (p.start)
                                rows.push_back(p.diagnostics);
                        }
                    std::ofstream(folder / "extra-near.json") << rows.dump(2);
                    out.success("LAB extra near {}", rows.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("extracatalog")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    J rows = J::array();
                    for (auto const &s : extraSpecs)
                        for (int z = spacingCell(-terrain_bounds::HalfChunks, s.spacing);
                             z <= spacingCell(terrain_bounds::HalfChunks - 1, s.spacing); ++z)
                            for (int x = spacingCell(-terrain_bounds::HalfChunks, s.spacing);
                                 x <= spacingCell(terrain_bounds::HalfChunks - 1, s.spacing); ++x) {
                                auto &p = extraPlan(d, s.type, extraCandidate(s, x, z, seed));
                                if (p.start)
                                    rows.push_back(p.diagnostics);
                            }
                    std::ofstream(folder / "extra-catalog.json") << rows.dump(2);
                    out.success("LAB extra catalog {}", rows.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("cityinspect")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = extraPlan(
                        d, 8,
                        {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))});
                    if (!p.start)
                        throw std::runtime_error("No ancient city");
                    inspectCityEnvironment(d, p);
                    out.success("LAB city environment inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("extrainspect")
            .required("type", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = extraPlan(
                        d, std::get<int>(*c["type"].as_ptr()),
                        {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))});
                    if (!p.start)
                        throw std::runtime_error("No extra structure");
                    report = p.diagnostics;
                    inspect(d);
                    out.success("LAB extra inspected");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("extraprobe")
            .required("type", ll::command::ParamKind::Int)
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    extraProbe(dimension(o), std::get<int>(*c["type"].as_ptr()),
                               std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["z"].as_ptr()));
                    out.success("LAB extra probed");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("extrafind")
            .required("type", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    int type = std::get<int>(*c["type"].as_ptr());
                    auto *s = extraSpec(type);
                    if (!s)
                        throw std::runtime_error("Family not enabled");
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    std::vector<ChunkPos> candidates;
                    for (int z = spacingCell(-terrain_bounds::HalfChunks, s->spacing);
                         z <= spacingCell(terrain_bounds::HalfChunks - 1, s->spacing); ++z)
                        for (int x = spacingCell(-terrain_bounds::HalfChunks, s->spacing);
                             x <= spacingCell(terrain_bounds::HalfChunks - 1, s->spacing); ++x)
                            candidates.push_back(extraCandidate(*s, x, z, seed));
                    std::sort(candidates.begin(), candidates.end(), [](auto a, auto b) {
                        return int64_t(a.x) * a.x + int64_t(a.z) * a.z <
                               int64_t(b.x) * b.x + int64_t(b.z) * b.z;
                    });
                    J rows = J::array();
                    for (auto cp : candidates) {
                        auto &p = extraPlan(d, type, cp);
                        if (!p.start)
                            continue;
                        rows.push_back(p.diagnostics);
                        if (rows.size() == 2)
                            break;
                    }
                    std::ofstream(folder / "extra-found.json") << rows.dump(2);
                    out.success("LAB extra found {}", rows.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villagecatalog")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    J rows = J::array();
                    for (int z = villageCell(-terrain_bounds::HalfChunks);
                         z <= villageCell(terrain_bounds::HalfChunks - 1); ++z)
                        for (int x = villageCell(-terrain_bounds::HalfChunks);
                             x <= villageCell(terrain_bounds::HalfChunks - 1); ++x) {
                            auto cp = villageCandidate(x, z, seed);
                            auto &p = villagePlan(d, cp, seed);
                            if (p.start)
                                rows.push_back(p.diagnostics);
                        }
                    std::ofstream(folder / "village-catalog.json") << rows.dump(2);
                    out.success("LAB villages {}", rows.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villageplace")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    ChunkPos cp{chunk(std::get<int>(*c["x"].as_ptr())),
                                chunk(std::get<int>(*c["z"].as_ptr()))};
                    auto &p = villagePlan(d, cp, seed);
                    if (!p.start)
                        throw std::runtime_error("Village rejected");
                    auto b = *p.start->boundingBox;
                    auto area = b;
                    area.min.x -= 28;
                    area.min.z -= 28;
                    area.max.x += 28;
                    area.max.z += 28;
                    ensureLoaded(d, area);
                    auto &blocks = d.getBlockSourceFromMainChunkSource();
                    for (int zz = chunk(area.min.z); zz <= chunk(area.max.z); ++zz)
                        for (int xx = chunk(area.min.x); xx <= chunk(area.max.x); ++xx) {
                            reserveVillageTrees(blocks, {xx, zz}, p);
                            auto ch = d.getChunkSource().getExistingChunk({xx, zz});
                            ch->setUnsaved();
                            ch->markSaveIfNeverSaved();
                            ch->markSaveNonActorDataIfDirty();
                        }
                    unsigned n = 0;
                    for (int z = chunk(b.min.z - 6); z <= chunk(b.max.z + 6); ++z)
                        for (int x = chunk(b.min.x - 6); x <= chunk(b.max.x + 6); ++x) {
                            n += placeVillage(blocks, {x, z}, p, seed);
                            auto c = d.getChunkSource().getExistingChunk({x, z});
                            c->setUnsaved();
                            c->markSaveIfNeverSaved();
                            c->markSaveNonActorDataIfDirty();
                        }
                    report = J{{"box", boxJson(b)}, {"village", p.diagnostics}};
                    inspect(d);
                    out.success("LAB village placed {}", n);
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villageaudit")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &registry = d.getLevel().getJigsawStructureRegistry();
                    J pools = J::array();
                    for (auto const &[name, pool] : *registry.mJigsawPoolLookupMap)
                        if (name.find("village") != std::string::npos)
                            pools.push_back(name);
                    J result{{"villagePools", pools},
                             {"poolCount", registry.mJigsawPoolLookupMap->size()},
                             {"blocksWritten", false}};
                    if (!pools.empty()) {
                        uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                        VillageFeature feature(seed, 32, 8, d.getBiomeRegistry());
                        auto biome = d.getBiomeRegistry().lookupByName("minecraft:plains");
                        FixedBiomeSource bs(*biome);
                        Random rng(seed, true);
                        ChunkPos cp{chunk(std::get<int>(*c["x"].as_ptr())),
                                    chunk(std::get<int>(*c["z"].as_ptr()))};
                        StructureFeature::setRandomSeedFor(rng, cp.x, cp.z, 10387312, seed);
                        adaptHeightMap = true;
                        struct Reset {
                            ~Reset() {
                                adaptHeightMap = false;
                            }
                        } reset;
                        auto t = std::chrono::steady_clock::now();
                        auto start = feature.createStructureStart(d, bs, rng, cp, provider);
                        J pieces = J::array();
                        if (start)
                            for (auto const &p : *start->pieces) {
                                auto *pool = static_cast<PoolElementStructurePiece *>(p.get());
                                J row{{"box", boxJson(*p->mBoundingBox)}};
                                if (pool) {
                                    row["location"] = *pool->mElement.mLocation;
                                    row["position"] = {pool->mPosition->x, pool->mPosition->y,
                                                       pool->mPosition->z};
                                    row["projection"] = int(pool->mElement.getProjection());
                                    row["junction"] = {pool->mJigsawJunction->mSourceBlockPos->x,
                                                       pool->mJigsawJunction->mSourceBlockPos->y,
                                                       pool->mJigsawJunction->mSourceBlockPos->z};
                                }
                                pieces.push_back(row);
                            }
                        result["valid"] = start && start->isValid();
                        result["pieces"] = pieces;
                        result["fixedPlainsPrototype"] = true;
                        result["naturalSelectionVerified"] = false;
                        if (start) {
                            result["type"] = start->getStructureName();
                            result["box"] = boxJson(*start->boundingBox);
                        }
                        result["planMs"] =
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t)
                                .count();
                    }
                    std::ofstream(folder / "village-audit.json") << result.dump(2);
                    out.success("LAB village audit {}", result.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("iglooplan")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    ChunkPos cp{chunk(std::get<int>(*c["x"].as_ptr())),
                                chunk(std::get<int>(*c["z"].as_ptr()))};
                    int floor = field->surface(cp.x * 16 + 32768, cp.z * 16 + 32768).height;
                    pendingIgloo =
                        planIgloo(d, cp, d.getLevel().getLevelData().getSeed().to32BitRandomSeed(), floor);
                    J parts = J::array();
                    for (auto const &p : pendingIgloo->parts)
                        parts.push_back(J{{"box", boxJson(p.bounds)}, {"pos", {p.pos.x, p.pos.y, p.pos.z}}});
                    report = J{{"box", boxJson(pendingIgloo->bounds)},
                               {"rotation", int(pendingIgloo->rotation)},
                               {"ladders", pendingIgloo->ladders},
                               {"parts", parts}};
                    save();
                    out.success("LAB igloo plan {}", report.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("iglooplace")
            .required("order", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    if (!pendingIgloo)
                        throw std::runtime_error("Plan first");
                    auto b = pendingIgloo->bounds;
                    ensureLoaded(d, b);
                    std::vector<ChunkPos> chunks;
                    for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
                        for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x)
                            chunks.push_back({x, z});
                    int order = std::get<int>(*c["order"].as_ptr());
                    if (order == 1)
                        std::reverse(chunks.begin(), chunks.end());
                    auto &blocks = d.getBlockSourceFromMainChunkSource();
                    auto air = Block::tryGetFromRegistry("minecraft:air");
                    for (int z = b.min.z; z <= b.max.z; ++z)
                        for (int x = b.min.x; x <= b.max.x; ++x)
                            for (int y = b.min.y; y <= b.max.y; ++y)
                                blocks.setBlockNoUpdate({x, y, z}, *air);
                    if (order == 2)
                        placeIgloo(blocks, *pendingIgloo, b);
                    else
                        for (auto cp : chunks)
                            placeIgloo(blocks, *pendingIgloo,
                                       {{cp.x * 16, -64, cp.z * 16}, {cp.x * 16 + 15, (field->generationHeight()-65), cp.z * 16 + 15}});
                    for (auto cp : chunks)
                        d.getChunkSource().getExistingChunk(cp)->setUnsaved();
                    inspect(d);
                    pendingIgloo.reset();
                    out.success("LAB igloo placed {}", report.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("igloowide")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    auto &d = dimension(o);
                    int x = std::get<int>(*c["x"].as_ptr()), z = std::get<int>(*c["z"].as_ptr());
                    report = J{{"box", {x - 16, -60, z - 16, x + 24, (field->generationHeight()-65), z + 24}}};
                    inspect(d);
                    out.success("LAB inspected wide");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("templates")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    auto &d = dimension(o);
                    auto manager = d.getLevel().getStructureManager();
                    J rows = J::array();
                    for (auto const &name :
                         {Igloo::STRUCTURE_LOCATION_IGLOO_NO_TRAPDOOR(),
                          Igloo::STRUCTURE_LOCATION_IGLOO_TRAPDOOR(), Igloo::STRUCTURE_LOCATION_LABORATORY(),
                          Igloo::STRUCTURE_LOCATION_LADDER()}) {
                        auto &t = manager->getOrCreateLegacy(name);
                        auto size = t.rawSize();
                        rows.push_back(J{{"name", name}, {"size", {size.x, size.y, size.z}}});
                    }
                    std::ofstream(folder / "templates.json") << rows.dump(2);
                    out.success("LAB templates {}", rows.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("sample")
            .required("x", ll::command::ParamKind::Int)
            .required("y", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    sampleAcceptance(dimension(o),
                                     {std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["y"].as_ptr()),
                                      std::get<int>(*c["z"].as_ptr())});
                    out.success("LAB acceptance sampled");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("villageblueprint")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = villagePlan(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        structureSeed(d));
                    if (!p.start)
                        throw std::runtime_error("No village");
                    inspectVillageTemplates(p);
                    out.success("LAB templates sampled");
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("autostatus")
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                dimension(o);
                out.success("LAB auto calls={} errors={} savedSkips={} entitySpawns={}", stageCalls.load(),
                            stageErrors.load(), savedStructureSkips.load(), entityLedgerReady.load());
            });
        cmd.runtimeOverload().text("metrics").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                dimension(o);
                J bins = J::array();
                for (auto &v : adapterHistogram)
                    bins.push_back(v.load());
                J result{{"calls", stageCalls.load()},
                         {"adapterNanos", adapterNanos.load()},
                         {"peakNanos", adapterPeakNanos.load()},
                         {"histogram", bins},
                         {"includesLockWait", true},
                         {"spacingRejectCalls", spacingRejectCalls.load()},
                         {"spacingRejectNanos", spacingRejectNanos.load()},
                         {"errors", stageErrors.load()}};
                std::ofstream(folder / "metrics.json") << result.dump(2);
                out.success("LAB metrics {}", result.dump());
            });
        cmd.runtimeOverload()
            .text("inspectat")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &p = autoBlueprint(
                        d, {chunk(std::get<int>(*c["x"].as_ptr())), chunk(std::get<int>(*c["z"].as_ptr()))},
                        d.getLevel().getLevelData().getSeed().to32BitRandomSeed());
                    if (!p.start)
                        throw std::runtime_error("No accepted native structure at anchor");
                    report =
                        J{{"box", boxJson(*p.start->boundingBox)}, {"type", p.start->getStructureName()}};
                    inspect(d);
                    out.success("LAB inspected {}", report["inspection"].dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload().text("catalog").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    uint seed = d.getLevel().getLevelData().getSeed().to32BitRandomSeed();
                    J items = J::array();
                    auto t = std::chrono::steady_clock::now();
                    for (int z = structureCell(-terrain_bounds::HalfChunks);
                         z <= structureCell(terrain_bounds::HalfChunks - 1); ++z)
                        for (int x = structureCell(-terrain_bounds::HalfChunks);
                             x <= structureCell(terrain_bounds::HalfChunks - 1); ++x) {
                            auto cp = nativeCandidateForCell(x, z, seed);
                            auto &p = autoBlueprint(d, cp, seed);
                            if (p.start) {
                                auto b = *p.start->boundingBox;
                                auto &f = registeredFeature(d);
                                bool nativeRecord = f.getStructureAt((b.min.x + b.max.x) / 2, b.min.y,
                                                                     (b.min.z + b.max.z) / 2) == p.start;
                                items.push_back(J{{"type", p.start->getStructureName()},
                                                  {"box", boxJson(b)},
                                                  {"nativeRecord", nativeRecord}});
                            }
                        }
                    std::ofstream(folder / "catalog.json") << J{
                        {"items", items},
                        {"ms", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t)
                                   .count()}}.dump(2);
                    out.success("LAB catalog={}", items.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload().text("records").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto &d = dimension(o);
                    auto &f = registeredFeature(d);
                    J records = J::array();
                    for (auto const &[cp, s] : *f.mCachedStructures)
                        records.push_back(J{{"chunk", {cp.x, cp.z}},
                                            {"type", s->getStructureName()},
                                            {"box", boxJson(*s->boundingBox)}});
                    std::ofstream(folder / "records.json") << records.dump(2);
                    out.success("LAB native records={}", records.size());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("scan")
            .required("mode", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    auto j = scanCandidates(dimension(o), std::get<int>(*c["mode"].as_ptr()));
                    out.success("LAB scanned {}", j.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("natural")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    naturalPlan(dimension(o), std::get<int>(*c["x"].as_ptr()),
                                std::get<int>(*c["z"].as_ptr()));
                    out.success("LAB planned {}", report.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload()
            .text("plan")
            .required("x", ll::command::ParamKind::Int)
            .required("z", ll::command::ParamKind::Int)
            .execute([](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &c) {
                try {
                    std::lock_guard guard(stageMutex);
                    plan(dimension(o), std::get<int>(*c["x"].as_ptr()), std::get<int>(*c["z"].as_ptr()));
                    out.success("LAB planned {}", report.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload().text("place").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    place(dimension(o));
                    out.success("LAB placed {}", report.dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        cmd.runtimeOverload().text("inspect").execute(
            [](CommandOrigin const &o, CommandOutput &out, ll::command::RuntimeCommand const &) {
                try {
                    std::lock_guard guard(stageMutex);
                    inspect(dimension(o));
                    out.success("LAB inspected {}", report["inspection"].dump());
                } catch (std::exception const &e) {
                    out.error("LAB error {}", e.what());
                }
            });
        hooks.commit();
        return true;
    }
    bool disable() {
#ifndef KARMA_CLIENT
        if (ll::getGamingStatus()==ll::GamingStatus::Running || ll::getGamingStatus()==ll::GamingStatus::Starting) {
            ll::mod::NativeMod::current()->getLogger().error("Structure runtime cannot be disabled while its world is running; stop the server normally first");
            return false;
        }
#endif
        structureTasks.closeAndWait();
#ifndef KARMA_CLIENT
        stopStructureTest();
#endif
        if (auto level = ll::service::getLevel())
            stopInteractionAudit(*level);
        if (entityTickListener)
            ll::event::EventBus::getInstance().removeListener(entityTickListener);
        automaticStructures = false;
        uninstallStructureGeneratorHooks();
        DecorationTask::unhook();
        StructurePrefill::unhook();
        SurfaceMapProbe::unhook();
        OwnedStructureCollection::unhook();
        VillageBuilderDepth::unhook();
        VillageBuilderBudget::unhook();
        SmallWrite::unhook();
        SmallWriteSimple::unhook();
        SmallWriteFast::unhook();
        FossilHeightXZ::unhook();
        FossilHeightPos::unhook();
        FossilCorners::unhook();
        fossilTemplatesReady = false;
        frozenFossils.clear();
        fossilJournalRows.clear();
        rejectedFossils.clear();
        rejectedFossilOrder.clear();
        recentFossilFragments.clear();
        recentFossilFragmentOrder.clear();
        smallFeaturesEnabled = false;
        nativeSmallDefinitions.clear();
        completedSmallChunks.clear();
        recentSmallChunks.clear();
        recentSmallOrder.clear();
        ModernGround::unhook();
        ShipGround::unhook();
        ShipGroundLegacy::unhook();
        PortalGround::unhook();
        RuinGround::unhook();
        MineSupport::unhook();
        MineWeb::unhook();
        SeededStructureRead::unhook();
        SeededStructureLayer::unhook();
        SeededStructureSolid::unhook();
        FragmentChunkWrite::unhook();
        FragmentChunkFastWrite::unhook();
        std::lock_guard guard(stageMutex);
        extraPlans.clear();
        extraDelegates.clear();
        prefilteredChunks.clear();
        clearPrefillWorkers();
        clearPopulationRecovery();
        villages.clear();
        automaticBlueprints.clear();
        vegetationPairs.clear();
        pending.reset();
        pendingIgloo.reset();
        nativeEntityJobs.clear();
        queuedEntityKeys.clear();
        villageSpawns.clear();
        villagePopulationPlans.clear();
        iglooPopulationPlans.clear();
        scatteredFeatureRecord = nullptr;
        villageFeatureRecord = nullptr;
        extraFeatureRecords.clear();
        // The registry owns our polymorphic records. Remove them before DLL unload.
        if (auto level = ll::service::getLevel())
            if (auto dim = level->getDimension(0).lock()) {
                auto &entries = *dim->getWorldGenerator()->mStructureFeatureRegistry->mStructureFeatures;
                std::erase_if(entries, [](auto const &p) { return ownedNativeFeatures.contains(p.get()); });
            }
        ownedNativeFeatures.clear();
        mansionTemplatesReady = false;
        villageTemplatesReady = false;
        field.reset();
        kernelContext.reset();
        return true;
    }
    bool unload() {
#ifndef KARMA_CLIENT
        return ll::getGamingStatus()!=ll::GamingStatus::Running && ll::getGamingStatus()!=ll::GamingStatus::Starting;
#else
        return true;
#endif
    }
};
#ifdef KARMA_CLIENT
#include "WindowsStructureHost.inc"
#endif
} // namespace lab
#ifndef KARMA_CLIENT
LL_REGISTER_MOD(lab::Mod, lab::Mod::getInstance());
#endif
