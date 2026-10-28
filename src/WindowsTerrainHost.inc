// Windows adapter operations compiled in the terrain host translation unit.
ll::mod::NativeMod& clientMod() { return Mod::getInstance().self; }
void clientPrepare(Level& level, std::filesystem::path const& root, std::string const& name,
                   std::string const& seed, Json const& marker) {
    clientWorldPath = root;
    Base = root / "karma";
    readData();
    worldSession->bind(&level);
    if (marker.value("buildMaxYExclusive",320) != buildHeight.maxYExclusive ||
        marker.value("generatedMaxYExclusive",320) != buildHeight.generatedMaxYExclusive)
        throw std::runtime_error("World marker height differs from its immutable terrain profile");
    if (worldSession->nativeData.at("seedManifest").at("terrainSeed").get<std::string>() != seed)
        throw std::runtime_error("Terrain was baked for a different world seed");
    worldName = name;
}
void clientActivate() {
    generated = 0;
    errors = 0;
    flatGenerated=0; voidGenerated=0; observedMinY=0; observedMaxYExclusive=0;
    observedDetached=0; observedCoalesced=0; observationDrops=0; oldestPendingMs=0;
    pendingByState.fill(0); loadRequests=0; loadReady=0; loadReleased=0;
    nearPending=farPending=outsidePending=0;
    arrivalStarts=arrivalResends=arrivalRebinds=arrivalTimeouts=0;
    viewGapRepairs=viewGapResolved=viewGapResends=0;
    priorityCalls=0; priorityBoosts=0;
    mainThread = GetCurrentThreadId();
    startPolarBearSpawning();
    active = true;
}
void clientRelease() {
    stopPolarBearSpawning();
    worldSession->close();
    active = false;
    unboundedWorld = false;
    requested = false;
    view.reset();
    playerStreams.clear();
    glacierWeatherPlayers.clear();
    streamingPolicy = nullptr;
    streamingFocus.store(std::make_shared<std::vector<StreamFocus> const>());
    streamTicks = 0;
    logTicks = 0;
    palette.clear();
    biomes.clear();
    registryOnce = std::make_unique<std::once_flag>();
    { std::lock_guard lock(loadMutex); loadEvents.clear(); }
    pendingLoads.clear();
}
void clientTick(Level& level) {
    auto now=Clock::now();
    if(lastTick!=Clock::time_point{})worldSession->tickGapLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(now-lastTick).count());
    lastTick=now;
    updatePlayerStreaming(level);
    pollLoads(level);
    glacierWeatherTick(level);
}
terrain_runtime::BuildHeight clientHeight() { return buildHeight; }
Json clientStatus() {
    Json result={{"world",worldName},{"kernel",NativeRuntime::KernelVersion},
            {"generatedChunks",generated.load()},{"flatGeneratedChunks",flatGenerated.load()},
            {"voidGeneratedChunks",voidGenerated.load()},{"terrainErrors",errors.load()},
            {"buildMaxYExclusive",buildHeight.maxYExclusive},
            {"generatedMaxYExclusive",buildHeight.generatedMaxYExclusive},
            {"worldPath",worldPath().string()},
            {"streaming",{{"loadRequests",loadRequests},{"ready",loadReady},
                {"pending",pendingLoads.size()},{"gapRepairs",viewGapRepairs},
                {"gapResolved",viewGapResolved},{"priorityBoosts",priorityBoosts.load()}}}};
    result["polarBears"]={{"allowed",polarBearAllowed.load()},{"rejected",polarBearRejected.load()}};
    result["nativeWork"]=workReport();
    result["pipeline"]=pipelineReport();
    result["latency"]={{"request",latencyJson(worldSession->requestLatency)},{"calculatorQueue",latencyJson(worldSession->queueLatency)},
        {"compute",latencyJson(worldSession->computeLatency)},{"fill",latencyJson(worldSession->fillLatency)},
        {"observedReady",latencyJson(worldSession->readyLatency)},{"tickInterval",latencyJson(worldSession->tickGapLatency)}};
    result["streaming"].update({{"nearPending",nearPending},{"farPending",farPending},{"oldestPendingMs",oldestPendingMs},
        {"calculatorWaiting",worldSession->waitingCalculators.load()},{"calculatorBusy",worldSession->busyCalculators.load()},
        {"repairsPerPlayer",terrain_runtime::RepairSlots},{"stallSeconds",8},{"frontierRepair",false},
        {"clientMeshMeasured",true},{"states",Json::array()}});
    for(int i=0;i<14;++i)result["streaming"]["states"].push_back({{"name",loadStateName(i)},
        {"pending",pendingByState[i]},{"residence",latencyJson(worldSession->stageLatency[i])}});
    result["playerViews"]=Json::array();
    for(auto const&[name,s]:playerStreams)result["playerViews"].push_back({{"name",name},{"chunk",{s.cx,s.cz}},
        {"radius",s.viewRadius},{"unfinished",s.visible.gaps.size()},{"repairActive",bool(s.patches[0].view)}});
    return result;
}
void clientHooks(terrain_runtime::HookTransaction& hooks) {
    calibrateStreamingPriority();
    nativeTimingHooks(hooks);
    hooks.add<Generate>("terrain::Generate");
    hooks.add<GenerateVoid>("terrain::GenerateVoid");
    hooks.add<ObserveLoad>("terrain::ObserveLoad");
    hooks.add<StreamPriority>("terrain::StreamPriority");
}
void clientUnhook() {
    nativeTimingUnhook();
    Generate::unhook(); GenerateVoid::unhook(); ObserveLoad::unhook(); StreamPriority::unhook();
}
