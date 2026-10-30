#include "TerrainKernel.h"
#include "native/Engine.h"

namespace terrain_kernel {
struct Context::Impl {
    nativegen::regional::Store store;
    std::mutex shapeMutex;J shapeRecipe;
    std::shared_ptr<nativegen::JungleShapes> jungleShapes;
    std::shared_ptr<nativegen::JungleShapes> shapes(J const& recipe){
        std::lock_guard lock(shapeMutex);
        if(!jungleShapes){shapeRecipe=recipe;jungleShapes=std::make_shared<nativegen::JungleShapes>();}
        // Alternate recipes may share regional storage, but never tree values.
        return shapeRecipe==recipe?jungleShapes:std::make_shared<nativegen::JungleShapes>();
    }
};
Context::Context() : impl(std::make_unique<Impl>()) {}
Context::~Context() = default;
void Context::storage(J const& recipe, std::filesystem::path const& directory) {
    nativegen::regional::StoreScope scope(impl->store);
    nativegen::regional::storage(recipe, directory);
}
J Context::statistics() const {
    nativegen::JungleShapes::Snapshot trees{};
    {std::lock_guard lock(impl->shapeMutex);if(impl->jungleShapes)trees=impl->jungleShapes->statistics();}
    std::lock_guard lock(impl->store.mutex);
    return {{"residentPlans", impl->store.plans.size()},
            {"pendingPlans", impl->store.pendingPlans.size()},
            {"storageIdentities", impl->store.folders.size()},
            {"memoryHits",impl->store.memoryHits.load()},{"diskHits",impl->store.diskHits.load()},
            {"builds",impl->store.builds.load()},{"waits",impl->store.waits.load()},
            {"buildMs",impl->store.buildUs.load()/1000.},{"waitMs",impl->store.waitUs.load()/1000.},
            {"jungleShapes",{{"entries",trees.entries},{"bytes",trees.bytes},{"pending",trees.pending},
                             {"hits",trees.hits},{"builds",trees.builds},{"waits",trees.waits},{"evictions",trees.evictions}}}};
}
struct Field::Impl {
    ContextPtr context;
    J const recipe; // Must precede the implementation that retains references.
    std::unique_ptr<nativegen::Field> value;
    explicit Impl(J data, ContextPtr owner)
        : context(owner ? std::move(owner) : std::make_shared<Context>()), recipe(std::move(data)) {
        nativegen::regional::StoreScope scope(context->impl->store);
        value = std::make_unique<nativegen::Field>(recipe);
    }
};
Field::Field(J data, ContextPtr context) : impl(std::make_shared<Impl>(std::move(data), std::move(context))) {}
Field::Field(std::shared_ptr<Impl> shared) : impl(std::move(shared)) {}
Field::~Field() = default;
struct Generator::Impl {
    J const recipe;
    std::unique_ptr<Field> ownedField;
    Field& field;
    std::shared_ptr<Field::Impl> fieldLifetime;
    std::unique_ptr<nativegen::Engine> value;
    Impl(J data, Reservations reservations, bool vegetationOnly, Field* shared,
         Generator const* vegetationSource, ContextPtr context)
        : recipe(std::move(data)), ownedField(shared ? new Field(shared->impl) : new Field(recipe, std::move(context))),
          field(*ownedField), fieldLifetime(field.impl) {
        nativegen::regional::StoreScope scope(fieldLifetime->context->impl->store);
        value = std::make_unique<nativegen::Engine>(recipe, std::move(reservations), vegetationOnly,
            fieldLifetime->value.get(), vegetationSource ? vegetationSource->impl->value.get() : nullptr,
            fieldLifetime->context->impl->shapes(recipe));
    }
};
Generator::Generator(J data, Reservations reservations, bool vegetationOnly,
                     Field* shared, Generator const* vegetationSource, ContextPtr context)
    : impl(std::make_unique<Impl>(std::move(data), std::move(reservations), vegetationOnly, shared, vegetationSource, std::move(context))) {}
Generator::~Generator() = default;
Field& Generator::terrain() { return impl->field; }
Metrics Generator::metrics() const {
    auto const& v = *impl->value;
    return {v.surfaceMs, v.columnMs, v.featureMs, v.undergroundMs, v.columnNoise, v.forestMs};
}

bool Field::highMountainUndergroundEnabled() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->highMountainUndergroundEnabled(); }
int Field::seaLevel() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->seaLevel(); }
int Field::generationHeight() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->generationHeight(); }
bool Field::streamed() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->streamed(); }
bool Field::unbounded() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->unbounded(); }
Surface Field::bootstrapSurface(int x, int z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->bootstrapSurface(x, z); }
Surface Field::surface(int x, int z, bool slope)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->surface(x, z, slope); }
Surface Field::base(int x, int z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->base(x, z); }
void Field::prepareRegions(int x,int z) { nativegen::regional::StoreScope scope(impl->context->impl->store); impl->value->prepareRegions(x,z); }
double Field::desertRegionWeight(int x, int z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->desertRegionWeight(x, z); }
double Field::minimumNaturalTemperature(int x, int z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->minimumNaturalTemperature(x, z); }
double Field::vcont(double x, double z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->vcont(x, z); }
double Field::giantWeight(int x, int z, Surface const& s) const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->giantWeight(x, z, s); }
uint8_t Field::material(Surface const& s, int x, int y, int z)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->material(s, x, y, z); }
void Field::materialColumn(Surface const& s, int x, int z, int top, uint16_t* column)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->materialColumn(s, x, z, top, column); }
J Field::inspectMushroomIsland(int gx, int gz)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectMushroomIsland(gx, gz); }
J Field::regionalStatistics() const { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->regionalStatistics(); }
J Field::inspectSmallPools(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectSmallPools(lo, hi); }
J Field::inspectElevationForms(int lo,int hi) { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectElevationForms(lo,hi); }
J Field::inspectIceRegions()  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectIceRegions(); }
J Field::bakeCavernPlan()  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->bakeCavernPlan(); }
J Field::inspectCavernGroups(int lo, int hi, bool geometry)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectCavernGroups(lo, hi, geometry); }
J Field::inspectFiniteCaves(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectFiniteCaves(lo, hi); }
J Field::inspectScarpCandidates()  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectScarpCandidates(); }
J Field::inspectWetlands(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectWetlands(lo, hi); }
J Field::inspectCascades()  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectCascades(); }
J Field::inspectJunctionBeds()  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectJunctionBeds(); }
J Field::inspectSmallStoneForests(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectSmallStoneForests(lo, hi); }
J Field::inspectVolcanoes(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->context->impl->store); return impl->value->inspectVolcanoes(lo, hi); }
std::vector<uint8_t> Generator::chunk(int cx, int cz)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->chunk(cx, cz); }
std::vector<uint16_t> Generator::vegetationChunk(int cx, int cz, bool includeGiants)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->vegetationChunk(cx, cz, includeGiants); }
void Generator::setSealedFluidDiagnostics(bool enabled)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->setSealedFluidDiagnostics(enabled); }
J Generator::inspectFeatureFlags() const { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectFeatureFlags(); }
J Generator::inspectSealedFluids(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectSealedFluids(lo, hi); }
J Generator::inspectForest(bool includeRuns, int limit, bool footprintsOnly)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectForest(includeRuns, limit, footprintsOnly); }
J Generator::landscapeSites(int lo, int hi)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->landscapeSites(lo, hi); }
J Generator::inspectBroadleaves(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectBroadleaves(x, z, size); }
J Generator::inspectConifers(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectConifers(x, z, size); }
J Generator::inspectBirches(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectBirches(x, z, size); }
J Generator::inspectJungles(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectJungles(x, z, size); }
J Generator::inspectBirchOaks(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectBirchOaks(x, z, size); }
J Generator::inspectWetDry(int x, int z, int size)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectWetDry(x, z, size); }
J Generator::inspectSnowRift(int x, int z)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectSnowRift(x, z); }
J Generator::inspectStandEdge(int x, int z)  { nativegen::regional::StoreScope scope(impl->fieldLifetime->context->impl->store); return impl->value->inspectStandEdge(x, z); }
}
