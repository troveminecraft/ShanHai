#pragma once
#include "native/Surface.h"
#include "WorldBounds.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>
#include <vector>

// SDK-independent facade. Each instance owns its immutable recipe and caches.
// Instances are single-worker objects; shared fields require external serial use.
namespace terrain_kernel {
using J = nlohmann::json;
using Surface = nativegen::Surface;
using Reservations = std::vector<std::array<int, 4>>;
class Context {
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend class Field;
    friend class Generator;
public:
    Context();
    ~Context();
    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;
    void storage(J const& recipe, std::filesystem::path const& directory);
    J statistics() const;
};
using ContextPtr = std::shared_ptr<Context>;
struct Metrics {
    double surfaceMs, columnMs, featureMs, undergroundMs;
    uint64_t columnNoise;
};
class Generator;
class Field {
    struct Impl;
    std::shared_ptr<Impl> impl;
    friend class Generator;
    explicit Field(std::shared_ptr<Impl> shared);
public:
    explicit Field(J recipe, ContextPtr context = {});
    ~Field();
    Field(Field const&) = delete;
    Field& operator=(Field const&) = delete;
    bool highMountainUndergroundEnabled() const;
    int seaLevel() const;
    int generationHeight() const;
    bool streamed() const;
    bool unbounded() const;
    Surface bootstrapSurface(int x, int z);
    Surface surface(int x, int z, bool slope = true);
    Surface base(int x, int z);
    double desertRegionWeight(int x, int z);
    double minimumNaturalTemperature(int x, int z);
    double vcont(double x, double z);
    double giantWeight(int x, int z, Surface const& s) const;
    uint8_t material(Surface const& s, int x, int y, int z);
    void materialColumn(Surface const& s, int x, int z, int top, uint16_t* column);
    J inspectMushroomIsland(int gx, int gz);
    J regionalStatistics() const;
    J inspectSmallPools(int lo, int hi);
    J inspectIceRegions();
    J inspectElevationForms(int lo,int hi);
    J bakeCavernPlan();
    J inspectCavernGroups(int lo = terrain_bounds::MinBlock, int hi = terrain_bounds::MaxBlock, bool geometry = false);
    J inspectFiniteCaves(int lo = terrain_bounds::MinBlock, int hi = terrain_bounds::MaxBlock);
    J inspectScarpCandidates();
    J inspectWetlands(int lo = terrain_bounds::MinBlock, int hi = terrain_bounds::MaxBlock);
    J inspectCascades();
    J inspectJunctionBeds();
    J inspectSmallStoneForests(int lo, int hi);
    J inspectVolcanoes(int lo = terrain_bounds::MinBlock, int hi = terrain_bounds::MaxBlock);
// Diagnostic operations above are used by the offline CLI and habitat search.
};
class Generator {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit Generator(J recipe, Reservations reservations = {}, bool vegetationOnly = false,
                       Field* sharedField = nullptr, Generator const* vegetationSource = nullptr,
                       ContextPtr context = {});
    ~Generator();
    Generator(Generator const&) = delete;
    Generator& operator=(Generator const&) = delete;
    Field& terrain();
    Metrics metrics() const;
    std::vector<uint8_t> chunk(int cx, int cz);
    std::vector<uint16_t> vegetationChunk(int cx, int cz, bool includeGiants = false);
    void setSealedFluidDiagnostics(bool enabled);
    J inspectFeatureFlags() const;
    J inspectSealedFluids(int lo, int hi);
    J inspectForest(bool includeRuns = false, int limit = 0, bool footprintsOnly = false);
    J landscapeSites(int lo, int hi);
    J inspectBroadleaves(int x, int z, int size);
    J inspectConifers(int x, int z, int size);
    J inspectBirches(int x, int z, int size);
    J inspectJungles(int x, int z, int size);
    J inspectBirchOaks(int x, int z, int size);
    J inspectWetDry(int x, int z, int size);
    J inspectSnowRift(int x, int z);
    J inspectStandEdge(int x, int z);
};
}
