// Both graphics modes enter the same cloud extraction method; vibrantEnabled
// selects the lighting path inside it. Keep all native weather/mesh parameters.
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/minecraft_renderer/objects/CloudRenderObject.h"
#include "mc/deps/minecraft_renderer/framebuilder/SkyWeatherParameters.h"

namespace client::clouds {
constexpr short Height=500;
std::array<std::atomic_uint64_t,2> extractions{};
bool applies(LevelRendererCamera const& camera) {
    auto dim=camera.mDimension;
    return dim && int(dim->getDimensionId())==0 && dim->getHeightRange().mMax==512;
}
// This is a renderer-owned cached value. Change it on its rendering thread,
// before both mesh-side selection and extraction, not through the shared
// constant-return Dimension getter (which may have ICF aliases).
LL_TYPE_INSTANCE_HOOK(CloudTick,ll::memory::HookPriority::Normal,LevelRendererPlayer,
                      &LevelRendererPlayer::tickClouds,void,bool bothSides,float a) {
    if(applies(*this))mCloudHeight=Height;
    origin(bothSides,a);
}
LL_TYPE_INSTANCE_HOOK(CloudExtract,ll::memory::HookPriority::Normal,LevelRendererPlayer,
                      &LevelRendererPlayer::_extractClouds,CloudRenderObject,
                      ScreenContext& context,mce::framebuilder::SkyWeatherParameters const& weather,bool vibrant) {
    if(applies(*this)) {
        mCloudHeight=Height;
        ++extractions[vibrant?1:0];
    }
    return origin(context,weather,vibrant);
}
void install(terrain_runtime::HookTransaction& hooks) {
    hooks.add<CloudTick>("CloudTick");
    hooks.add<CloudExtract>("CloudExtract");
}
void remove(){CloudExtract::unhook();CloudTick::unhook();}
}

