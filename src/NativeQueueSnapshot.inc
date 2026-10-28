// Main-thread, read-only samples. Never traverse the undocumented neighbour
// grid or edit the engine's queues/state. A contended lock skips this sample.
terrain_runtime::ReadyAges readyAges;
terrain_runtime::IdleQueuePump idleQueuePump;
terrain_runtime::SupplementalBudget supplementalBudget;
terrain_runtime::ActiveStreamingClock streamingClock;
terrain_runtime::PipelinePressure pipelinePressure;
std::atomic<std::shared_ptr<std::unordered_map<uint64_t,int> const>> readyAgeCredits{
    std::make_shared<std::unordered_map<uint64_t,int> const>()};
std::vector<terrain_runtime::ReadyChunk> nativeReadySample;
unsigned nativeInFlight=0,frontierPendingTotal=0;
uint64_t queueSampleSkips=0,queuePumps=0,supplementalThrottles=0;
double lastStreamingWall=0,streamingBusyUntil=0;
Json stalledChunkSample=Json::array(),pipelineTimeline=Json::array();
double pipelineWallStart=-1,nextPipelineHistory=0,firstNearReadySeconds=-1;
unsigned readyPlayers=0;
void sampleNativeQueue(ChunkSource& source){
    auto* data=source.mLevelChunkBuilderData;
    pipelinePressure.queueSampleValid=false;
    if(!data){++queueSampleSkips;return;}
    nativeInFlight=std::max(0,data->mChunkGenerationTasksInFlight->load());
    std::vector<terrain_runtime::ReadyChunk> sample;sample.reserve(16384);
    unsigned queued=0;
    bool complete=true;
    {
        bedrock_compat::QueueSampleLock sampleLock(*data->mChunksReadyForProcessingSpinLock);
        std::unique_lock lock(sampleLock,std::try_to_lock);
        if(lock.owns_lock()){
            queued+=unsigned(data->mChunksReadyForProcessing->size());
            for(auto const& [cp,state]:*data->mChunksReadyForProcessing){
                if(sample.size()==16384){complete=false;break;}
                sample.push_back({cp.x,cp.z,int(state)});
            }
        }else complete=false;
    }
    {
        bedrock_compat::QueueSampleLock sampleLock(*data->mChunksToAddToProcessingSpinLock);
        std::unique_lock lock(sampleLock,std::try_to_lock);
        if(lock.owns_lock()){
            queued+=unsigned(data->mChunksToAddToProcessing->size());
            for(auto const& [cp,state]:*data->mChunksToAddToProcessing){
                if(sample.size()==16384){complete=false;break;}
                sample.push_back({cp.x,cp.z,int(state)});
            }
        }else complete=false;
    }
    // Failed/partial samples cannot clear pressure or refresh priority ages.
    if(!complete){++queueSampleSkips;return;}
    nativeReadySample=std::move(sample);
    pipelinePressure.queued=queued;pipelinePressure.queueSampleValid=true;
    readyAgeCredits.store(std::make_shared<std::unordered_map<uint64_t,int> const>(
        readyAges.update(nativeReadySample,streamingClock.seconds)));
}
void resetStreamingPipeline(){
    readyAges.clear();idleQueuePump={};supplementalBudget={};streamingClock={};pipelinePressure={};
    readyAgeCredits.store(std::make_shared<std::unordered_map<uint64_t,int> const>());
    nativeReadySample.clear();nativeInFlight=frontierPendingTotal=0;queueSampleSkips=queuePumps=supplementalThrottles=0;
    lastStreamingWall=streamingBusyUntil=0;
    stalledChunkSample=Json::array();pipelineTimeline=Json::array();
    pipelineWallStart=-1;nextPipelineHistory=0;firstNearReadySeconds=-1;readyPlayers=0;
}
void recordPipelineHistory(double wall){
    if(pipelineWallStart<0)pipelineWallStart=wall;
    if(readyPlayers&&firstNearReadySeconds<0)firstNearReadySeconds=wall-pipelineWallStart;
    if(wall<nextPipelineHistory)return;
    nextPipelineHistory=wall+5.;
    // Retain the first two minutes and a bounded recent tail. Wall time keeps
    // long main-thread pauses visible instead of discounting them as inactivity.
    if(pipelineTimeline.size()==64)pipelineTimeline.erase(pipelineTimeline.begin()+24);
    pipelineTimeline.push_back({{"wallSeconds",wall-pipelineWallStart},{"queued",pipelinePressure.queued},
        {"sampleValid",pipelinePressure.queueSampleValid},{"inFlight",nativeInFlight},{"nearReadyPlayers",readyPlayers},
        {"generated",generated.load()},{"calculatorBusy",worldSession->busyCalculators.load()},
        {"calculatorWaiting",worldSession->waitingCalculators.load()},{"interiorPending",pipelinePressure.unfinished},
        {"frontierPending",frontierPendingTotal},{"queuePumps",queuePumps}});
}
Json pipelineReport(){
#ifdef SHANHAI_MC_26_51
    Json traceKinds={{"1","native task began"},{"2","native task ended"}};
    constexpr bool predicateTraceAvailable=false;
#else
    Json traceKinds={{"0","dispatch check returned"},{"1","native task began"},{"2","native task ended"},{"3","native neighbour stage predicate failed"}};
    constexpr bool predicateTraceAvailable=true;
#endif
    Json events=Json::array();
    for(auto const&e:worldSession->pipelineTrace.snapshot())events.push_back({
        {"kind",e.kind},{"chunk",{e.x,e.z}},{"stage",e.stage},{"otherChunk",{e.otherX,e.otherZ}},
        {"observedState",e.observed},{"result",e.result},{"activeUs",e.micros}});
    return {{"queued",pipelinePressure.queued},{"queueSampleValid",pipelinePressure.queueSampleValid},
        {"inFlight",nativeInFlight},{"frontierPending",frontierPendingTotal},{"postprocessPending",pipelinePressure.postprocess},
        {"visibleUnfinished",pipelinePressure.unfinished},{"unfinishedScope","interior dependency halo or enclosed holes; frontier reported separately"},{"supplementalOpen",supplementalBudget.open},
        {"supplementalThrottles",supplementalThrottles},{"queuePumps",queuePumps},{"sampleSkips",queueSampleSkips},
        {"activeSeconds",streamingClock.seconds},{"excludedTickGapSeconds",streamingClock.excluded},
        {"stalled",stalledChunkSample},{"trace",events},{"timeline",pipelineTimeline},{"firstNearReadyWallSeconds",firstNearReadySeconds},
        {"traceKinds",traceKinds},{"dispatchPredicateTraceAvailable",predicateTraceAvailable},
        {"neighbourPredicateTraceAvailable",predicateTraceAvailable},
        {"scope","bounded read-only engine queue sample and native task trace; predicate coverage reported separately; no GPU publication measurement"}};
}
