// Windows adapter operations compiled in the structure host translation unit.
void clientVanillaPath(std::filesystem::path path) { clientVanillaFolder = std::move(path); }
void clientPrepare(std::filesystem::path const& root, std::string const& name, std::string const& identity) {
    activeWorld = name;
    clientWorldIdentity = identity;
    clientTerrainFolder = root / "karma";
    clientWorldFolder = root;
    folder = clientTerrainFolder / "structures";
    std::filesystem::create_directories(folder);
    saveGeneratorHookReport();
    initialize();
    std::lock_guard lock(stageMutex);
    initializeSmallHistory(); initializeFossilJournal(); initializeEntityLedger();
}
void clientActivate() {
    stageCalls = 0; stageErrors = 0; structureStageFailed = false;
    prefillJobs=0; prefillComputeNanos=0; prefillLogRows=0; storageLogRows=0;
    regionalWarmJobs=0;regionalWarmLatency.reset();
    savedStructureSkips=0; structurePlansRetired=0; structureCollections=0;
    stageMutex.waitLatency.reset();stageMutex.holdLatency.reset();
    stageMutex.clearSlowSites();
    stageMutex.acquisitions=0; stageMutex.waitNanos=0; stageMutex.holdNanos=0;
    automaticStructures = true; smallFeaturesEnabled = true;
}
void clientRelease(Level& level) {
    structureTasks.closeAndWait();
    automaticStructures = false;
    smallFeaturesEnabled = false;
    atlasCatalog.active = false;
    std::lock_guard lock(stageMutex);
    structurePalette.reset();
    cityNaturalBlockCache.clear();
    extraPlans.clear();
    extraDelegates.clear();
    prefilteredChunks.clear();
    clearPrefillWorkers();
    clearPopulationRecovery();
    cityNeighbourCache.clear();
    cityLinks.clear();
    villages.clear();
    iglooPopulationPlans.clear();
    automaticBlueprints.clear();
    vegetationPairs.clear();
    pending.reset();
    pendingIgloo.reset();
    nativeEntityJobs.clear();
    queuedEntityKeys.clear();
    villageSpawns.clear();
    villagePopulationPlans.clear();
    scatteredFeatureRecord = nullptr;
    villageFeatureRecord = nullptr;
    extraFeatureRecords.clear();
    if (auto dim = level.getDimension(0).lock()) {
        auto *generator = bedrock_compat::generator(*dim);
        if (generator && generator->mStructureFeatureRegistry) {
            auto &entries = *generator->mStructureFeatureRegistry->mStructureFeatures;
            std::erase_if(entries, [](auto const &p) { return ownedNativeFeatures.contains(p.get()); });
        }
    }
    ownedNativeFeatures.clear();
    fossilTemplatesReady = false;
    frozenFossils.clear();
    fossilJournalRows.clear();
    rejectedFossils.clear();
    rejectedFossilOrder.clear();
    recentFossilFragments.clear();
    recentFossilFragmentOrder.clear();
    nativeSmallDefinitions.clear();
    completedSmallChunks.clear();
    recentSmallChunks.clear();
    recentSmallOrder.clear();
    mansionTemplatesReady = false;
    villageTemplatesReady = false;
    entityLedgerReady = false;
    entityLedger = J::object();
    field.reset();
    kernelContext.reset();
    atlasCatalog.terrain.reset();
    atlasCatalog.delegate.reset();
    input=J::object();
}
void clientWarmTemplates(Level& level) { warmVillageTemplates(level); warmFossilTemplates(level); }
void clientTick(Level& level) {
    collectStructureCaches(level); recoverLoadedStructurePlans(level);
    tickIglooEntities(level); tickStructureEntities(level);
}
void clientHooks(terrain_runtime::HookTransaction& hooks) {
        hooks.add<OwnedStructureCollection>("OwnedStructureCollection");
        hooks.add<SurfaceMapProbe>("SurfaceMapProbe");
        hooks.add<ModernGround>("ModernGround");
        hooks.add<VillageBuilderDepth>("VillageBuilderDepth");
        hooks.add<VillageBuilderBudget>("VillageBuilderBudget");
#ifdef SHANHAI_MC_26_51
        hooks.add<RuinPieceScope>("RuinPieceScope");
        hooks.add<MansionOrigin>("MansionOrigin");
#else
        hooks.add<ShipGround>("ShipGround");
        hooks.add<ShipGroundLegacy>("ShipGroundLegacy");
#endif
        hooks.add<PortalGround>("PortalGround");
        hooks.add<RuinGround>("RuinGround");
#ifndef SHANHAI_MC_26_51
        hooks.add<MineSupport>("MineSupport");
#endif
        hooks.add<MineWeb>("MineWeb");
        hooks.add<SeededStructureRead>("SeededStructureRead");
        hooks.add<SeededStructureLayer>("SeededStructureLayer");
        hooks.add<SeededStructureSolid>("SeededStructureSolid");
        hooks.add<FragmentChunkWrite>("FragmentChunkWrite");
        hooks.add<FragmentChunkFastWrite>("FragmentChunkFastWrite");
        hooks.add<StructurePrefill>("StructurePrefill");
        hooks.add<DecorationTask>("DecorationTask");
        installStructureGeneratorHooks(hooks);
        hooks.add<SmallWrite>("SmallWrite");
#ifndef SHANHAI_MC_26_51
        hooks.add<SmallWriteSimple>("SmallWriteSimple");
#endif
        hooks.add<SmallWriteFast>("SmallWriteFast");
        hooks.add<FossilHeightXZ>("FossilHeightXZ");
        hooks.add<FossilHeightPos>("FossilHeightPos");
        hooks.add<FossilCorners>("FossilCorners");
}
void clientUnhook() { Mod::getInstance().disable(); }
J clientStatus() {
    J status{{"performance",structurePerformance()},{"regionalPlans",kernelContext?kernelContext->statistics():J{}},{"structureCalls",stageCalls.load()},{"structureErrors",stageErrors.load()},
             {"entityLedgerReady",entityLedgerReady.load()},{"structurePaused",structureStageFailed.load()},
             {"villageTemplatesReady",villageTemplatesReady},{"fossilTemplatesReady",fossilTemplatesReady},
             {"structureLock",{{"acquisitions",stageMutex.acquisitions.load()},
                {"waitNs",stageMutex.waitNanos.load()},{"holdNs",stageMutex.holdNanos.load()}}}};
    std::unique_lock lock(stageMutex,std::try_to_lock);
    if(lock.owns_lock()) status["structureCaches"] = {
        {"villages",villages.size()},{"scattered",automaticBlueprints.size()},{"extras",extraPlans.size()},
        {"retired",structurePlansRetired.load()},{"collections",structureCollections.load()},
        {"prefillJobs",prefillJobs.load()},{"prefillComputeNs",prefillComputeNanos.load()}};
    return status;
}
