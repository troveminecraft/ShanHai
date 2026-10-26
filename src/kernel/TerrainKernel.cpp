#include "TerrainKernel.h"
#include "native/Engine.h"

namespace terrain_kernel {
struct Field::Impl {
    J const recipe; // Must precede the implementation that retains references.
    nativegen::Field value;
    explicit Impl(J data) : recipe(std::move(data)), value(recipe) {}
};
Field::Field(J data) : impl(std::make_shared<Impl>(std::move(data))) {}
Field::Field(std::shared_ptr<Impl> shared) : impl(std::move(shared)) {}
Field::~Field() = default;
struct Generator::Impl {
    J const recipe;
    std::unique_ptr<Field> ownedField;
    Field& field;
    std::shared_ptr<Field::Impl> fieldLifetime;
    nativegen::Engine value;
    Impl(J data, Reservations reservations, bool vegetationOnly, Field* shared,
         Generator const* vegetationSource)
        : recipe(std::move(data)), ownedField(shared ? new Field(shared->impl) : new Field(recipe)),
          field(*ownedField), fieldLifetime(field.impl),
          value(recipe, std::move(reservations), vegetationOnly, &fieldLifetime->value,
                vegetationSource ? &vegetationSource->impl->value : nullptr) {}
};
Generator::Generator(J data, Reservations reservations, bool vegetationOnly,
                     Field* shared, Generator const* vegetationSource)
    : impl(std::make_unique<Impl>(std::move(data), std::move(reservations), vegetationOnly, shared, vegetationSource)) {}
Generator::~Generator() = default;
Field& Generator::terrain() { return impl->field; }
Metrics Generator::metrics() const {
    auto const& v = impl->value;
    return {v.surfaceMs, v.columnMs, v.featureMs, v.undergroundMs, v.columnNoise};
}
void regionalStorage(J const& data, std::filesystem::path const& directory) {
    nativegen::regional::storage(data, directory);
}
bool Field::highMountainUndergroundEnabled() const { return impl->value.highMountainUndergroundEnabled(); }
int Field::seaLevel() const { return impl->value.seaLevel(); }
int Field::generationHeight() const { return impl->value.generationHeight(); }
bool Field::streamed() const { return impl->value.streamed(); }
bool Field::unbounded() const { return impl->value.unbounded(); }
Surface Field::bootstrapSurface(int x, int z)  { return impl->value.bootstrapSurface(x, z); }
Surface Field::surface(int x, int z, bool slope)  { return impl->value.surface(x, z, slope); }
Surface Field::base(int x, int z)  { return impl->value.base(x, z); }
double Field::desertRegionWeight(int x, int z)  { return impl->value.desertRegionWeight(x, z); }
double Field::minimumNaturalTemperature(int x, int z)  { return impl->value.minimumNaturalTemperature(x, z); }
double Field::vcont(double x, double z)  { return impl->value.vcont(x, z); }
double Field::giantWeight(int x, int z, Surface const& s) const { return impl->value.giantWeight(x, z, s); }
uint8_t Field::material(Surface const& s, int x, int y, int z)  { return impl->value.material(s, x, y, z); }
void Field::materialColumn(Surface const& s, int x, int z, int top, uint16_t* column)  { return impl->value.materialColumn(s, x, z, top, column); }
J Field::inspectMushroomIsland(int gx, int gz)  { return impl->value.inspectMushroomIsland(gx, gz); }
J Field::regionalStatistics() const { return impl->value.regionalStatistics(); }
J Field::inspectSmallPools(int lo, int hi)  { return impl->value.inspectSmallPools(lo, hi); }
J Field::inspectIceRegions()  { return impl->value.inspectIceRegions(); }
J Field::bakeCavernPlan()  { return impl->value.bakeCavernPlan(); }
J Field::inspectCavernGroups(int lo, int hi, bool geometry)  { return impl->value.inspectCavernGroups(lo, hi, geometry); }
J Field::inspectFiniteCaves(int lo, int hi)  { return impl->value.inspectFiniteCaves(lo, hi); }
J Field::inspectScarpCandidates()  { return impl->value.inspectScarpCandidates(); }
J Field::inspectWetlands(int lo, int hi)  { return impl->value.inspectWetlands(lo, hi); }
J Field::inspectCascades()  { return impl->value.inspectCascades(); }
J Field::inspectJunctionBeds()  { return impl->value.inspectJunctionBeds(); }
J Field::inspectSmallStoneForests(int lo, int hi)  { return impl->value.inspectSmallStoneForests(lo, hi); }
J Field::inspectVolcanoes(int lo, int hi)  { return impl->value.inspectVolcanoes(lo, hi); }
std::vector<uint8_t> Generator::chunk(int cx, int cz)  { return impl->value.chunk(cx, cz); }
std::vector<uint16_t> Generator::vegetationChunk(int cx, int cz, bool includeGiants)  { return impl->value.vegetationChunk(cx, cz, includeGiants); }
void Generator::setSealedFluidDiagnostics(bool enabled)  { return impl->value.setSealedFluidDiagnostics(enabled); }
J Generator::inspectFeatureFlags() const { return impl->value.inspectFeatureFlags(); }
J Generator::inspectSealedFluids(int lo, int hi)  { return impl->value.inspectSealedFluids(lo, hi); }
J Generator::inspectForest(bool includeRuns, int limit, bool footprintsOnly)  { return impl->value.inspectForest(includeRuns, limit, footprintsOnly); }
J Generator::landscapeSites(int lo, int hi)  { return impl->value.landscapeSites(lo, hi); }
J Generator::inspectBroadleaves(int x, int z, int size)  { return impl->value.inspectBroadleaves(x, z, size); }
J Generator::inspectConifers(int x, int z, int size)  { return impl->value.inspectConifers(x, z, size); }
J Generator::inspectBirches(int x, int z, int size)  { return impl->value.inspectBirches(x, z, size); }
J Generator::inspectJungles(int x, int z, int size)  { return impl->value.inspectJungles(x, z, size); }
J Generator::inspectBirchOaks(int x, int z, int size)  { return impl->value.inspectBirchOaks(x, z, size); }
J Generator::inspectWetDry(int x, int z, int size)  { return impl->value.inspectWetDry(x, z, size); }
J Generator::inspectSnowRift(int x, int z)  { return impl->value.inspectSnowRift(x, z); }
J Generator::inspectStandEdge(int x, int z)  { return impl->value.inspectStandEdge(x, z); }
}
