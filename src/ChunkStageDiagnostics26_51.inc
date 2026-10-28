// Native closure invoke adapters preserve the actual task owner. The optimized
// _launchChunkTask body drops its SDK ChunkPos argument and cannot supply it.
static_assert(sizeof(TaskResult)==16);
template<class F> TaskResult* measureNativeTask(void* closure,int stage,F run){
 auto* chunk=terrain_runtime::native26::taskOwner<LevelChunk>(closure);
 auto owner=publishedSession.load();auto observing=owner->tasks.tryEnter();
 if(!chunk||!observing||!active||static_cast<int>(bedrock_compat::dimension(*chunk).getDimensionId())!=0||
    !owner->belongsTo(&bedrock_compat::level(*chunk)))return run();
 auto cp=bedrock_compat::position(*chunk);
 owner->pipelineTrace.record({1,cp.x,cp.z,stage,0,0,int(bedrock_compat::loadState(*chunk).load()),true,owner->streamActiveMicros.load()});
 auto start=Clock::now();auto* result=run();
 owner->recordWork(stage,cp.x,cp.z,std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());
 owner->pipelineTrace.record({2,cp.x,cp.z,stage,0,0,int(bedrock_compat::loadState(*chunk).load()),true,owner->streamActiveMicros.load()});
 return result;
}
#define SHANHAI_NATIVE_TASK_TIMER(Name,Stage) \
 LL_STATIC_HOOK(Name,ll::memory::HookPriority::Normal,bedrock_compat::generationTaskEntry(Stage), \
 TaskResult*,TaskResult* result,void* closure){ \
  return measureNativeTask(closure,Stage,[&]{return origin(result,closure);}); \
 }
SHANHAI_NATIVE_TASK_TIMER(NativeLoadTiming,0)
SHANHAI_NATIVE_TASK_TIMER(NativeStructureTiming,1)
SHANHAI_NATIVE_TASK_TIMER(NativeDecorationTiming,2)
SHANHAI_NATIVE_TASK_TIMER(NativeLightingTiming,3)
#undef SHANHAI_NATIVE_TASK_TIMER
#include "ClientMeshDiagnostics.inc"
void nativeTimingHooks(terrain_runtime::HookTransaction& hooks){
 hooks.add<NativeLoadTiming>("NativeLoadTiming");hooks.add<NativeStructureTiming>("NativeStructureTiming");
 hooks.add<NativeDecorationTiming>("NativeDecorationTiming");hooks.add<NativeLightingTiming>("NativeLightingTiming");
#ifdef KARMA_CLIENT
 hooks.add<ClientMeshTiming>("ClientMeshTiming");
#endif
}
void nativeTimingUnhook(){
#ifdef KARMA_CLIENT
 ClientMeshTiming::unhook();
#endif
 NativeLightingTiming::unhook();NativeDecorationTiming::unhook();NativeStructureTiming::unhook();NativeLoadTiming::unhook();
}
