#ifdef SHANHAI_MC_26_51
#include "CloudHeight26_51.inc"
#else
#include "CloudHeightPolicy.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/minecraft_renderer/objects/CloudRenderObject.h"
#include "mc/deps/minecraft_renderer/framebuilder/SkyWeatherParameters.h"

namespace client::clouds {
static_assert(offsetof(CloudRenderObject, mYTranslation) == 0x34);
static_assert(offsetof(LevelRendererCamera, mCameraPos) == 0x704);
static_assert(offsetof(LevelRendererCamera, mCameraTargetPos) == 0x710);
std::array<std::atomic_uint64_t, 2> extractions{};
std::atomic_uint64_t meshTicks{}, invalidExtractions{};
struct Sample {
    bool available = false, hasMeshes = false;
    uint64_t extraction = 0;
    short nativeHeight = 0;
    float cameraY = 0, nativeWorldY = 0, outputWorldY = 0;
};
std::mutex sampleMutex;
std::array<Sample, 2> samples{};

bool applies(LevelRendererCamera const& camera) {
    auto dim = camera.mDimension;
    return dim && appliesToDimension(int(dim->getDimensionId()), dim->getHeightRange().mMax);
}

// Do not hook the constant-return Dimension getter: ICF may share its address
// with unrelated functions. Setting mCloudHeight alone is insufficient because
// both tickClouds and _extractClouds read the dimension's virtual getter anew.
LL_TYPE_INSTANCE_HOOK(CloudTick,ll::memory::HookPriority::Normal,LevelRendererPlayer,
                      &LevelRendererPlayer::tickClouds,void,bool bothSides,float a) {
    if (!applies(*this)) return origin(bothSides, a);
    mCloudHeight = Height;
    ScopedCloudCameraY comparison(mCameraPos->y, mDimension->getCloudHeight());
    ++meshTicks;
    origin(bothSides, a);
}

LL_TYPE_INSTANCE_HOOK(CloudExtract,ll::memory::HookPriority::Normal,LevelRendererPlayer,
                      &LevelRendererPlayer::_extractClouds,CloudRenderObject,
                      ScreenContext& context,mce::framebuilder::SkyWeatherParameters const& weather,bool vibrant) {
    auto active = applies(*this);
    if (active) mCloudHeight = Height;
    auto result = origin(context, weather, vibrant);
    if (!active) return result;

    auto nativeHeight = mDimension->getCloudHeight();
    float cameraY = mCameraTargetPos->y;
    float before = result.mYTranslation;
    if (!std::isfinite(before) || !std::isfinite(cameraY)) {
        ++invalidExtractions;
        return result;
    }
    // This value feeds the final matrix AND cloud frustum bounds in renderClouds
    // for both original graphics and Vibrant Visuals. Keep native wind, lighting,
    // thickness, mesh quality and visibility settings intact.
    result.mYTranslation = translatedCloudY(before, nativeHeight);
    auto index = vibrant ? 1 : 0;
    auto count = ++extractions[index];
    if (count == 1 || count % 120 == 0) {
        std::lock_guard lock(sampleMutex);
        samples[index] = {true, bool(result.mCloudMeshes), count, nativeHeight,
                         cameraY, before + cameraY, float(result.mYTranslation) + cameraY};
    }
    return result;
}

nlohmann::json status() {
    nlohmann::json result = {{"targetY", Height}, {"classicExtractions", extractions[0].load()},
        {"vibrantExtractions", extractions[1].load()}, {"meshTicks", meshTicks.load()},
        {"invalidExtractions", invalidExtractions.load()}, {"scope", "client-process"},
        {"gameplayTested", false}};
    std::lock_guard lock(sampleMutex);
    for (size_t i = 0; i < samples.size(); ++i) {
        auto const& s = samples[i];
        auto& mode = result[i == 0 ? "classic" : "vibrant"];
        mode = nullptr;
        if (s.available) mode = {{"extraction", s.extraction}, {"hasMeshes", s.hasMeshes},
            {"nativeHeight", s.nativeHeight}, {"cameraY", s.cameraY},
            {"nativeWorldY", s.nativeWorldY}, {"outputWorldY", s.outputWorldY}};
    }
    return result;
}

void install(terrain_runtime::HookTransaction& hooks) {
    hooks.add<CloudTick>("CloudTick");
    hooks.add<CloudExtract>("CloudExtract");
}
void remove(){CloudExtract::unhook();CloudTick::unhook();}
}
#endif

