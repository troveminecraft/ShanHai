#ifdef KARMA_CLIENT
LL_TYPE_INSTANCE_HOOK(ClientMeshTiming,ll::memory::HookPriority::Normal,RenderChunkBuilder,
 &RenderChunkBuilder::build,void,RenderChunkGeometry& geometry,bool lightingType,BakedBlockLightType forExport,
 bool capabilities,mce::framebuilder::FrameLightingModelCapabilities const& models){
 auto owner=publishedSession.load();auto observing=owner->tasks.tryEnter();
 auto* source=mLocalSource.get();
 auto* dimension=source?static_cast<Dimension*>(source->mDimension):nullptr;
 if(!observing||!active||!dimension||static_cast<int>(dimension->getDimensionId())!=0||
    !owner->belongsTo(static_cast<Level*>(source->mLevel))){
  origin(geometry,lightingType,forExport,capabilities,models);return;
 }
 auto start=Clock::now();origin(geometry,lightingType,forExport,capabilities,models);
 // Builder duration only; GPU upload/queue visibility is not measured here.
 owner->nativeWorkLatency[4].observe(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());
}
#endif
