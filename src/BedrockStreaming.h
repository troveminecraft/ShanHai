#pragma once
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/chunk/ChunkViewSource.h"
#include "mc/platform/threading/SpinLockImpl.h"
#include "runtime/NativeSpinTry.h"
#include <Windows.h>
#ifdef SHANHAI_MC_26_51
#include "mc/server/NetworkChunkPublisher.h"
#include "mc/world/level/ticking/TickingAreaView.h"
#endif

namespace bedrock_compat {
class QueueSampleLock {
    SpinLockImpl& native;
public:
    explicit QueueSampleLock(SpinLockImpl& lock):native(lock){}
    bool try_lock() {
#ifdef SHANHAI_MC_26_51
        return terrain_runtime::tryNativeSpinLock(*native.mOwnerThread,native.mOwnerRefCount,
            native.mNoThreadId,terrain_runtime::nativeThreadOwner(GetCurrentThreadId()));
#else
        return native.try_lock();
#endif
    }
    void unlock(){native.unlock();}
};
#ifdef SHANHAI_MC_26_51
using View = std::shared_ptr<ChunkViewSource>;
#else
using View = std::unique_ptr<ChunkViewSource>;
#endif
inline View deferredView(ChunkSource& source) {
#ifdef SHANHAI_MC_26_51
    // The native factory owns a fully constructed view, referencing source
    // directly. Its destructor only releases the shared_ptr. Move that owner
    // out before destruction; the level's view tracker keeps only a weak_ptr.
    // Do not use createChildSource(): that child would reference the temporary.
    static_assert(sizeof(ChunkViewSource)==0x1b0);
    TickingAreaView factory(source);
    auto view=std::move(factory.mTickingArea);
    view->mParentLoadMode=ChunkSource::LoadMode::Deferred;
    return view;
#else
    return std::make_unique<ChunkViewSource>(source,ChunkSource::LoadMode::Deferred);
#endif
}
inline void resendChunks(Player& player) {
#ifdef SHANHAI_MC_26_51
    // Old Player::resendAllChunks tail-called this native publisher operation.
    // Keep native discard batching, reset position/radius and queued requests.
    if(auto const& publisher=player.mChunkPublisherView)publisher->clearRegion();
#else
    player.resendAllChunks();
#endif
}
}
