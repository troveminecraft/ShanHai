// Windows adapter operations compiled in the terrain host translation unit.
ll::mod::NativeMod& clientMod() { return Mod::getInstance().self; }
void clientPrepare(std::filesystem::path const& root, std::string const& name,
                   std::string const& seed, Json const& marker) {
    clientWorldPath = root;
    Base = root / "karma";
    readData();
    if (marker.value("buildMaxYExclusive",320) != buildHeight.maxYExclusive ||
        marker.value("generatedMaxYExclusive",320) != buildHeight.generatedMaxYExclusive)
        throw std::runtime_error("World marker height differs from its immutable terrain profile");
    if (nativeData.at("seedManifest").at("terrainSeed").get<std::string>() != seed)
        throw std::runtime_error("Terrain was baked for a different world seed");
    worldName = name;
}
void clientActivate() {
    generated = 0;
    errors = 0;
    mainThread = GetCurrentThreadId();
    active = true;
}
void clientRelease() {
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
    cacheWriter.stop();
    for (auto &r : runtimes)
        r.stop();
    memoryCache.clear();
    palette.clear();
    biomes.clear();
    registryOnce = std::make_unique<std::once_flag>();
    loadEvents.clear();
    pendingLoads.clear();
}
void clientTick(Level& level) {
    updatePlayerStreaming(level);
    pollLoads(level);
    glacierWeatherTick(level);
}
terrain_runtime::BuildHeight clientHeight() { return buildHeight; }
Json clientStatus() {
    return {{"world",worldName},{"kernel",NativeRuntime::KernelVersion},
            {"generatedChunks",generated.load()},{"flatGeneratedChunks",flatGenerated.load()},
            {"voidGeneratedChunks",voidGenerated.load()},{"terrainErrors",errors.load()},
            {"buildMaxYExclusive",buildHeight.maxYExclusive},
            {"generatedMaxYExclusive",buildHeight.generatedMaxYExclusive},
            {"worldPath",worldPath().string()},
            {"streaming",{{"loadRequests",loadRequests},{"ready",loadReady},
                {"pending",pendingLoads.size()},{"gapRepairs",viewGapRepairs},
                {"gapResolved",viewGapResolved},{"priorityBoosts",priorityBoosts.load()}}}};
}
void clientHooks(terrain_runtime::HookTransaction& hooks) {
    calibrateStreamingPriority();
    hooks.add<Generate>("terrain::Generate");
    hooks.add<GenerateVoid>("terrain::GenerateVoid");
    hooks.add<ObserveLoad>("terrain::ObserveLoad");
    hooks.add<StreamPriority>("terrain::StreamPriority");
}
void clientUnhook() {
    Generate::unhook(); GenerateVoid::unhook(); ObserveLoad::unhook(); StreamPriority::unhook();
}
