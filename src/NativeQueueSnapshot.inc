// Main-thread, read-only samples. Never traverse the undocumented neighbour
// grid or edit the engine's queues/state. A contended lock skips this sample.
terrain_runtime::ReadyAges readyAges;
terrain_runtime::SupplementalBudget supplementalBudget;
terrain_runtime::ActiveStreamingClock streamingClock;
terrain_runtime::PipelinePressure pipelinePressure;
std::atomic<std::shared_ptr<std::unordered_map<uint64_t,int> const>> readyAgeCredits{
    std::make_shared<std::unordered_map<uint64_t,int> const>()};
std::vector<terrain_runtime::ReadyChunk> nativeReadySample;
unsigned nativeInFlight=0;
uint64_t queueSampleSkips=0,queuePumps=0,supplementalThrottles=0;
double lastStreamingWall=0,streamingBusyUntil=0;
Json stalledChunkSample=Json::array();
void sampleNativeQueue(ChunkSource& source){
    auto* data=source.mLevelChunkBuilderData;
    pipelinePressure.queueSampleValid=false;
    if(!data){++queueSampleSkips;return;}
    nativeInFlight=std::max(0,data->mChunkGenerationTasksInFlight->load());
    std::vector<terrain_runtime::ReadyChunk> sample;sample.reserve(16384);
    unsigned queued=0;
    bool complete=true;
    {
        std::unique_lock lock(*data->mChunksReadyForProcessingSpinLock,std::try_to_lock);
        if(lock.owns_lock()){
            queued+=unsigned(data->mChunksReadyForProcessing->size());
            for(auto const& [cp,state]:*data->mChunksReadyForProcessing){
                if(sample.size()==16384){complete=false;break;}
                sample.push_back({cp.x,cp.z,int(state)});
            }
        }else complete=false;
    }
    {
        std::unique_lock lock(*data->mChunksToAddToProcessingSpinLock,std::try_to_lock);
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
    readyAges.clear();supplementalBudget={};streamingClock={};pipelinePressure={};
    readyAgeCredits.store(std::make_shared<std::unordered_map<uint64_t,int> const>());
    nativeReadySample.clear();nativeInFlight=0;queueSampleSkips=queuePumps=supplementalThrottles=0;
    lastStreamingWall=streamingBusyUntil=0;
    stalledChunkSample=Json::array();
}
Json pipelineReport(){
    Json events=Json::array();
    for(auto const&e:worldSession->pipelineTrace.snapshot())events.push_back({
        {"kind",e.kind},{"chunk",{e.x,e.z}},{"stage",e.stage},{"otherChunk",{e.otherX,e.otherZ}},
        {"observedState",e.observed},{"result",e.result},{"activeUs",e.micros}});
    return {{"queued",pipelinePressure.queued},{"queueSampleValid",pipelinePressure.queueSampleValid},
        {"inFlight",nativeInFlight},{"postprocessPending",pipelinePressure.postprocess},
        {"visibleUnfinished",pipelinePressure.unfinished},{"supplementalOpen",supplementalBudget.open},
        {"supplementalThrottles",supplementalThrottles},{"queuePumps",queuePumps},{"sampleSkips",queueSampleSkips},
        {"activeSeconds",streamingClock.seconds},{"excludedTickGapSeconds",streamingClock.excluded},
        {"stalled",stalledChunkSample},{"trace",events},
        {"traceKinds",{{"0","dispatch check returned"},{"1","native task began"},{"2","native task ended"},{"3","native neighbour stage predicate failed"}}},
        {"scope","bounded read-only engine queue/dependency trace; no GPU publication measurement"}};
}
