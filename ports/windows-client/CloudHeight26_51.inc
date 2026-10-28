#include <atomic>
#include "CloudHeightPolicy.h"
#include "CloudNative26_51.h"
#include "NativeCloudCallsite.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/minecraft_renderer/objects/CloudRenderObject.h"
#include "ll/api/thread/GlobalThreadPauser.h"

namespace client::clouds {
static_assert(offsetof(LevelRendererCamera,mCameraPos)==0x654);
static_assert(offsetof(LevelRendererCamera,mCameraTargetPos)==0x660);
static_assert(offsetof(LevelRendererCamera,mDimension)==0x990);
static_assert(offsetof(LevelRendererCamera,mCloudHeight)==0x998);
static_assert(offsetof(LevelRendererPlayer,mCloudMeshes)==0xce8);
static_assert(offsetof(LevelRendererPlayer,mLastCloudSide)==0xd28);
static_assert(offsetof(CloudRenderObject,mYTranslation)==0x40);
std::array<std::atomic_uint64_t,2> calls{},overrides{};
std::atomic_uint64_t contextMismatches{};
std::atomic_int lastNativeHeight{},lastOutputHeight{};
NativeCloudCallsite meshCall,extractCall;

short cloudHeight(Dimension* dimension,LevelRendererPlayer* camera,unsigned stage) noexcept {
    // These are the two native getCloudHeight virtual-call sites. Do not hook
    // the folded getter itself, or change camera coordinates in a larger frame.
    auto original=dimension->getCloudHeight();
    if(stage>1)return original;
    ++calls[stage];
    if(!camera||camera->mDimension!=dimension){++contextMismatches;return original;}
    if(!appliesToDimension(int(dimension->getDimensionId()),dimension->mHeightRange->mMax))
        return original;
    // The same extracted view also compares camera altitude to this field for
    // cloud fog. Keep it in the same frame as the mesh and world translation.
    camera->mCloudHeight=Height;
    lastNativeHeight=original;lastOutputHeight=Height;++overrides[stage];
    return Height;
}
struct NativeCloudHooks {
    static int hook() {
        auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
        auto read=[&](uint32_t rva,void* out,size_t size){
            if(!base||uint64_t(rva)+size>0x12c0d000)return false;
            MEMORY_BASIC_INFORMATION page{};auto* address=base+rva;
            if(!VirtualQuery(address,&page,sizeof(page))||page.State!=MEM_COMMIT||
               (page.Protect&(PAGE_NOACCESS|PAGE_GUARD))||
               size>size_t(static_cast<unsigned char*>(page.BaseAddress)+page.RegionSize-address))return false;
            std::memcpy(out,address,size);return true;
        };
        ll::thread::GlobalThreadPauser pause;
        if(!native26::inspect(read))return -1;
        try {
            meshCall.install(base+native26::MeshCallRva,native26::MeshCallBytes,reinterpret_cast<void*>(&cloudHeight),0);
            extractCall.install(base+native26::ExtractCallRva,native26::ExtractCallBytes,reinterpret_cast<void*>(&cloudHeight),1);
        }catch(...){extractCall.remove();meshCall.remove();throw;}
        return 0;
    }
    static void unhook() {
        ll::thread::GlobalThreadPauser pause;
        extractCall.remove();meshCall.remove();
    }
};
nlohmann::json status() {
    return {{"targetY",Height},{"adapter","1.26.51.01-inline-cloud-calls"},
        {"meshCalls",calls[0].load()},{"viewCalls",calls[1].load()},
        {"meshOverrides",overrides[0].load()},{"viewOverrides",overrides[1].load()},
        {"contextMismatches",contextMismatches.load()},
        {"lastNativeHeight",lastNativeHeight.load()},{"lastOutputHeight",lastOutputHeight.load()},
        {"renderPath","shared-original-and-vibrant"},{"gameplayTested",false}};
}
void install(terrain_runtime::HookTransaction& hooks){hooks.add<NativeCloudHooks>("NativeCloudCalls26.51");}
void remove(){NativeCloudHooks::unhook();}
}
