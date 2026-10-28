// Read-only timers around native tasks. Never alter stage results or readiness.
template<class F> void measureNativeWork(LevelChunk& chunk,int stage,F run){
 auto owner=publishedSession.load();auto observing=owner->tasks.tryEnter();
 if(!observing||!active||static_cast<int>(bedrock_compat::dimension(chunk).getDimensionId())!=0||
    bedrock_compat::levelName(bedrock_compat::level(bedrock_compat::dimension(chunk)).getLevelData())!=worldName){run();return;}
 auto cp=bedrock_compat::position(chunk);
 owner->pipelineTrace.record({1,cp.x,cp.z,stage,0,0,int(bedrock_compat::loadState(chunk).load()),true,owner->streamActiveMicros.load()});
 auto start=Clock::now();run();
 owner->pipelineTrace.record({2,cp.x,cp.z,stage,0,0,int(bedrock_compat::loadState(chunk).load()),true,owner->streamActiveMicros.load()});
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
#include "ClientMeshDiagnostics.inc"
using NativeExpectedChunk=std::pair<ChunkPos,ChunkState>;
LL_TYPE_INSTANCE_HOOK(NativeDispatchTrace,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_checkAndDispatchTaskForLevelChunk,bool,NativeExpectedChunk const& expected,bool inTask){
 auto owner=publishedSession.load();
 bool watching=active&&owner->belongsTo(mLevel)&&owner->pipelineTrace.contains(expected.first.x,expected.first.z);
 if(!watching)return origin(expected,inTask);
 auto observing=owner->tasks.tryEnter();if(!observing)return origin(expected,inTask);
 bool result=origin(expected,inTask);
 owner->pipelineTrace.record({0,expected.first.x,expected.first.z,int(expected.second),0,0,-1,result,owner->streamActiveMicros.load()});
 return result;
}
struct DependencyTraceContext {Session* owner;int x,z,stage;};
thread_local DependencyTraceContext* dependencyTrace=nullptr;
using NativeNeighbourGrid=LevelChunkGridAreaElement<std::weak_ptr<LevelChunk>>;
LL_TYPE_INSTANCE_HOOK(NativeDependencyTrace,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_checkLevelChunkForNextStage,void,LevelChunk const& lc,NativeNeighbourGrid& grid,ChunkState state){
 auto owner=publishedSession.load();auto cp=bedrock_compat::position(lc);
 bool watching=active&&owner->belongsTo(mLevel)&&owner->pipelineTrace.contains(cp.x,cp.z);
 if(!watching){origin(lc,grid,state);return;}
 auto observing=owner->tasks.tryEnter();if(!observing){origin(lc,grid,state);return;}
 DependencyTraceContext context{owner.get(),cp.x,cp.z,int(state)};
 auto* previous=dependencyTrace;dependencyTrace=&context;
 struct Restore {DependencyTraceContext* previous;~Restore(){dependencyTrace=previous;}} restore{previous};
 origin(lc,grid,state);
}
LL_TYPE_INSTANCE_HOOK(NativeNeighbourTrace,ll::memory::HookPriority::Normal,ChunkSource,
 &ChunkSource::_chunkAtStage,bool,std::weak_ptr<LevelChunk> weak,ChunkState state){
 if(!dependencyTrace)return origin(std::move(weak),state);
 auto chunk=weak.lock();bool result=origin(std::move(weak),state);
 if(!result){auto& c=*dependencyTrace;auto cp=chunk?bedrock_compat::position(*chunk):ChunkPos{};
  c.owner->pipelineTrace.record({3,c.x,c.z,int(state),cp.x,cp.z,chunk?int(bedrock_compat::loadState(*chunk).load()):-1,false,c.owner->streamActiveMicros.load()});}
 return result;
}
void nativeTimingHooks(terrain_runtime::HookTransaction& hooks){
 hooks.add<NativeDispatchTrace>("NativeDispatchTrace");hooks.add<NativeDependencyTrace>("NativeDependencyTrace");hooks.add<NativeNeighbourTrace>("NativeNeighbourTrace");
 hooks.add<NativeLoadTiming>("NativeLoadTiming");hooks.add<NativeStructureTiming>("NativeStructureTiming");
 hooks.add<NativeDecorationTiming>("NativeDecorationTiming");hooks.add<NativeLightingTiming>("NativeLightingTiming");
#ifdef KARMA_CLIENT
 hooks.add<ClientMeshTiming>("ClientMeshTiming");
#endif
}
void nativeTimingUnhook(){
 NativeDispatchTrace::unhook();NativeDependencyTrace::unhook();NativeNeighbourTrace::unhook();
 NativeLoadTiming::unhook();NativeStructureTiming::unhook();NativeDecorationTiming::unhook();NativeLightingTiming::unhook();
#ifdef KARMA_CLIENT
 ClientMeshTiming::unhook();
#endif
}
