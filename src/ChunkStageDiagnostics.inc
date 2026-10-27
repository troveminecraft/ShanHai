// Read-only timers around native tasks. Never alter stage results or readiness.
template<class F> void measureNativeWork(LevelChunk& chunk,int stage,F run){
 auto owner=publishedSession.load();auto observing=owner->tasks.tryEnter();
 if(!observing||!active||static_cast<int>(chunk.getDimension().getDimensionId())!=0||
    chunk.getDimension().getLevel().getLevelData().getLevelName()!=worldName){run();return;}
 auto start=Clock::now();run();auto cp=chunk.getPosition();
 owner->recordWork(stage,cp.x,cp.z,std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());
}
LL_TYPE_INSTANCE_HOOK(NativeLoadTiming,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_loadChunkTask,void,LevelChunk& lc){measureNativeWork(lc,0,[&]{origin(lc);});}
LL_TYPE_INSTANCE_HOOK(NativeStructureTiming,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_structurePostProcessingTask,void,LevelChunk& lc,ChunkViewSource& chunks){measureNativeWork(lc,1,[&]{origin(lc,chunks);});}
LL_TYPE_INSTANCE_HOOK(NativeDecorationTiming,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_decorationPostProcessingTask,void,LevelChunk& lc,ChunkViewSource& chunks){measureNativeWork(lc,2,[&]{origin(lc,chunks);});}
LL_TYPE_INSTANCE_HOOK(NativeLightingTiming,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_lightingTask,void,std::shared_ptr<LevelChunk> const& lc,ChunkViewSource& chunks){
 if(!lc){origin(lc,chunks);return;}measureNativeWork(*lc,3,[&]{origin(lc,chunks);});
}
#ifdef KARMA_CLIENT
LL_TYPE_INSTANCE_HOOK(ClientMeshTiming,ll::memory::HookPriority::Normal,RenderChunkBuilder,
 &RenderChunkBuilder::build,void,RenderChunkGeometry& geometry,bool lightingType,BakedBlockLightType forExport,
 bool capabilities,mce::framebuilder::FrameLightingModelCapabilities const& models){
 auto owner=publishedSession.load();auto observing=owner->tasks.tryEnter();
 auto* source=mLocalSource.get();
 if(!observing||!active||!source||static_cast<int>(source->getDimension().getDimensionId())!=0){
  origin(geometry,lightingType,forExport,capabilities,models);return;
 }
 auto start=Clock::now();origin(geometry,lightingType,forExport,capabilities,models);
 // Builder duration only; GPU upload/queue visibility is not measured here.
 owner->nativeWorkLatency[4].observe(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());
}
#endif
void nativeTimingHooks(terrain_runtime::HookTransaction& hooks){
 hooks.add<NativeLoadTiming>("NativeLoadTiming");hooks.add<NativeStructureTiming>("NativeStructureTiming");
 hooks.add<NativeDecorationTiming>("NativeDecorationTiming");hooks.add<NativeLightingTiming>("NativeLightingTiming");
#ifdef KARMA_CLIENT
 hooks.add<ClientMeshTiming>("ClientMeshTiming");
#endif
}
void nativeTimingUnhook(){
 NativeLoadTiming::unhook();NativeStructureTiming::unhook();NativeDecorationTiming::unhook();NativeLightingTiming::unhook();
#ifdef KARMA_CLIENT
 ClientMeshTiming::unhook();
#endif
}
