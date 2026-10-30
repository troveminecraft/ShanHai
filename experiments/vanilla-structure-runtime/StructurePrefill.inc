// Snapshot native footprints under the registry lock. Pure vegetation work
// uses two independent Fields and only touches the caller-owned block buffer.
// Neither the shared planning Field nor native StructureStart escapes the lock.
worldgen_rules::StructureReservationCache reservationCache;
std::mutex prefilteredMutex;
std::atomic_uint64_t prefillSnapshotHits=0,prefillSnapshotMisses=0,decorationSnapshotSkips=0;
worldgen_rules::BoundedHints<std::pair<int, int>, 4096> prefilteredChunks;
unsigned prefillLogRows = 0;
using ReservationAreas = std::vector<std::array<int, 4>>;
struct PrefillWorker {
    std::mutex mutex;
    std::unique_ptr<terrain_kernel::Field> field;
    std::unique_ptr<terrain_kernel::Generator> normal, reserved;
    ReservationAreas areas;
    void clear() {
        reserved.reset();
        normal.reset();
        field.reset();
        areas.clear();
    }
    void prepare(ReservationAreas const &next) {
        if (!field) {
            field = std::make_unique<terrain_kernel::Field>(input, kernelContext);
        }
        if (!normal) {
            normal = std::make_unique<terrain_kernel::Generator>(input, ReservationAreas{}, true, field.get());
        }
        if (!reserved || areas != next) {
            reserved = std::make_unique<terrain_kernel::Generator>(input, next, true, field.get(),normal.get());
            areas = next;
        }
    }
};
std::array<PrefillWorker, 2> prefillWorkers;
std::atomic_uint64_t prefillJobs = 0, prefillComputeNanos = 0;
std::atomic_uint64_t regionalWarmJobs=0;
terrain_runtime::Latency regionalWarmLatency;
// This Field belongs to one worker. Only immutable regional plans are shared
// with the registry's planning Field; native starts/pointers remain locked.
void warmStructureRegions(std::vector<ChunkPos> const& points){
    auto& worker=prefillWorkers[regionalWarmJobs.fetch_add(1)%prefillWorkers.size()];
    terrain_runtime::Timer timer(regionalWarmLatency);
    std::lock_guard lock(worker.mutex);
    if(!worker.field)worker.field=std::make_unique<terrain_kernel::Field>(input,kernelContext);
    for(auto cp:points)for(int dz:{-192,0,192})for(int dx:{-192,0,192})
        worker.field->prepareRegions(cp.x*16+32768+dx,cp.z*16+32768+dz);
}
void clearPrefillWorkers() {
    reservationCache.clear();prefillSnapshotHits=0;prefillSnapshotMisses=0;decorationSnapshotSkips=0;
    for (auto &worker : prefillWorkers) {
        std::lock_guard lock(worker.mutex);
        worker.clear();
    }
}
ReservationAreas pieceReservations(StructureStart const &start) {
    ReservationAreas areas;
    for (auto const &piece : *start.pieces)
        if (auto area = worldgen_rules::structurePlantReservation(sharedBox(*piece->mBoundingBox)))
            areas.push_back(*area);
    return areas;
}
unsigned applyVegetationDifference(BlockVolume const &volume, std::vector<uint16_t> const &before,
                                   std::vector<uint16_t> const &after) {
    auto air = Block::tryGetFromRegistry("minecraft:air");
    if (!air)
        throw std::runtime_error("Air block unavailable during structure prefill");
    unsigned count = 0;
    auto data = volume.mBlocks->mBegin;
    int height=worldgen_rules::vegetationColumnHeight(before,after);
    if(volume.mWidth!=16||volume.mDepth!=16||volume.mHeight<height||
       (volume.mHeight!=384&&volume.mHeight!=576)||volume.mDimensionBottom!=-64)
        throw std::runtime_error("Structure prefill native volume height mismatch");
    worldgen_rules::visitVegetationReservations(before,after,[&](int x,int y,int z,int previous,int reserved){
                auto idx = bedrock_compat::volumeIndex(volume,BlockPos{x, y, z});
                if (!worldgen_rules::structureReservationRemoves(previous, reserved,
                                                                 data[idx]->getTypeName()))
                    return;
                data[idx] = air;
                ++count;
                if (y+1 < volume.mHeight) {
                    auto above = bedrock_compat::volumeIndex(volume,BlockPos{x, y + 1, z});
                    if (data[above]->getTypeName() == "minecraft:snow_layer") {
                        data[above] = air;
                        ++count;
                    }
                }
    });
    return count;
}
// The explicit memory-audit command already owns the planning lock.
unsigned filterVegetationVolume(BlockVolume const &volume, ChunkPos cp, VegetationPair &pair) {
    auto before = pair.normal->vegetationChunk(cp.x, cp.z);
    auto after = pair.reserved->vegetationChunk(cp.x, cp.z);
    return applyVegetationDifference(volume, before, after);
}
unsigned filterVegetationVolume(BlockVolume const &volume, ChunkPos cp,
                                std::vector<ReservationAreas> const &groups) {
    auto &worker = prefillWorkers[prefillJobs.fetch_add(1) % prefillWorkers.size()];
    std::lock_guard lock(worker.mutex);
    auto began = std::chrono::steady_clock::now();
    worker.prepare(groups.front());
    auto before = worker.normal->vegetationChunk(cp.x, cp.z);
    unsigned count = 0;
    for (auto const &areas : groups) {
        worker.prepare(areas);
        auto after = worker.reserved->vegetationChunk(cp.x, cp.z);
        count += applyVegetationDifference(volume, before, after);
    }
    prefillComputeNanos +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
            .count();
    return count;
}
LL_TYPE_INSTANCE_HOOK(StructurePrefill, ll::memory::HookPriority::Normal, LevelChunk,
                      &LevelChunk::setBlockVolume, void, BlockVolume const &volume, uint yOffset) {
    auto cp = bedrock_compat::position(*this);
    auto &dim = bedrock_compat::dimension(*this);
    if (!automaticStructures || volume.mWidth != 16 || volume.mDepth != 16 ||
        (volume.mHeight != 384 && volume.mHeight != 576) ||
        volume.mDimensionBottom != -64 || yOffset != 0 || bedrock_compat::loadState(*this).load() > ChunkState::Generated ||
        static_cast<int>(dim.getDimensionId()) != 0 ||
        bedrock_compat::levelName(bedrock_compat::level(dim).getLevelData()) != activeWorld || !structureChunkInside(cp.x, cp.z)) {
        origin(volume, yOffset);
        return;
    }
    auto work=structureTasks.enter();
    // Vanilla void initialization precedes the custom terrain buffer.
    if (volume.mBlocks->mBegin[bedrock_compat::volumeIndex(volume,BlockPos{0, 0, 0})]->getTypeName() !=
        "minecraft:bedrock") {
        origin(volume, yOffset);
        return;
    }
    if (structureStageFailed)
        throw std::runtime_error("Structure generation paused after an earlier error");
    uint seed = bedrock_compat::seed(bedrock_compat::level(dim).getLevelData()).to32BitRandomSeed();
    std::vector<ChunkPos> vc, sc;
    for (int z = villageCell(cp.z - 10); z <= villageCell(cp.z + 10); ++z)
        for (int x = villageCell(cp.x - 10); x <= villageCell(cp.x + 10); ++x) {
            auto c = villageCandidate(x, z, seed);
            if (std::abs(c.x - cp.x) <= 10 && std::abs(c.z - cp.z) <= 10)
                vc.push_back(c);
        }
    for (int z = structureCell(cp.z - 3); z <= structureCell(cp.z + 2); ++z)
        for (int x = structureCell(cp.x - 3); x <= structureCell(cp.x + 2); ++x) {
            auto c = nativeCandidateForCell(x, z, seed);
            if (c.x <= cp.x + 2 && c.x + 3 >= cp.x && c.z <= cp.z + 2 && c.z + 3 >= cp.z)
                sc.push_back(c);
        }
    auto ec = nearbyExtra(cp, seed);
    std::erase_if(ec, [](auto const &e) { return !extraSurface(e.first); });
    if (vc.empty() && sc.empty() && ec.empty()) {
        origin(volume, yOffset);
        return;
    }
    try {
        worldgen_rules::StructureStageSiteScope site(0,cp.x,cp.z);
        std::vector<ReservationAreas> groups;
        std::vector<worldgen_rules::StructureReservationCache::Key> keys;
        for(auto c:vc)keys.push_back({-2,c.x,c.z});
        for(auto c:sc)keys.push_back({-1,c.x,c.z});
        for(auto [type,c]:ec)keys.push_back({type,c.x,c.z});
        if (!storedChunk(dim,cp)) {
            if(reservationCache.get(keys,cp.x,cp.z,groups)) ++prefillSnapshotHits;
            else {
                ++prefillSnapshotMisses;
                std::vector<ChunkPos> warming=vc;warming.insert(warming.end(),sc.begin(),sc.end());
                for(auto const& [type,c]:ec)warming.push_back(c);
                {terrain_runtime::LoadingSpan warm(terrain_runtime::loadingRecorder,"structure.prefill_regional_warm",cp.x,cp.z);warmStructureRegions(warming);}
                terrain_runtime::DeferredDiagnosticBatch deferredLogs;
                std::unique_lock guard(stageMutex,std::defer_lock);
                {terrain_runtime::LoadingSpan waiting(terrain_runtime::loadingRecorder,"structure.prefill_lock_wait",cp.x,cp.z);guard.lock();}
                terrain_runtime::LoadingSpan held(terrain_runtime::loadingRecorder,"structure.prefill_lock_hold",cp.x,cp.z);
                if (structureStageFailed) throw std::runtime_error("Structure generation paused");
                auto save=[&](auto key,StructureStart const* start,bool scattered=false){
                    worldgen_rules::StructureReservationCache::Entry e;
                    if(start){
                        auto b=*start->boundingBox;e.bounds=std::array{b.min.x,b.min.z,b.max.x,b.max.z};
                        if(scattered){
                            if(start->getStructureName()!="minecraft:desert_pyramid")
                                e.areas={{b.min.x+32756,b.min.z+32756,b.max.x+32780,b.max.z+32780}};
                        }else e.areas=pieceReservations(*start);
                        if(!(b.max.x+28<cp.x*16||b.min.x-28>cp.x*16+15||b.max.z+28<cp.z*16||b.min.z-28>cp.z*16+15)&&!e.areas.empty())
                            groups.push_back(e.areas);
                    }
                    reservationCache.put(key,std::move(e));
                };
                for(auto c:vc){
                    auto& p=villagePlan(dim,c,seed);
                    // "Templates not ready" is transient, not a negative plan.
                    if(villageTemplatesReady)save(std::array{-2,c.x,c.z},p.start);
                }
                for(auto c:sc)save(std::array{-1,c.x,c.z},autoBlueprint(dim,c,seed).start,true);
                for(auto [type,c]:ec){auto& p=extraPlan(dim,type,c);
                    if(type!=7||(mansionHabitatAvailable&&mansionTemplatesReady))save(std::array{type,c.x,c.z},p.start);}
            }
        }
        // Underground/native starts can have no plant reservation at all.
        // Avoid rebuilding two identical vegetation buffers for those starts.
        std::erase_if(groups,[](auto const& areas){return areas.empty();});
        if (!groups.empty()) {
            auto began = std::chrono::steady_clock::now();
            auto cleared = filterVegetationVolume(volume, cp, groups);
            std::lock_guard guard(prefilteredMutex);
            if (structureStageFailed)
                throw std::runtime_error("Structure generation paused during prefill");
            prefilteredChunks.insert({cp.x, cp.z});
            if (prefillLogRows++ < 2048)
                terrain_runtime::DeferredDiagnosticLine(folder / "structure-prefill.jsonl")
                    << J{{"chunk", {cp.x, cp.z}},
                         {"cleared", cleared},
                         {"plans", groups.size()},
                         {"ms",
                          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                              .count()}}
                           .dump()
                    << '\n';
        }
    } catch (std::exception const &e) {
        ++stageErrors;
        reportStructureFailure("prefill", e, std::array<int,2>{bedrock_compat::position(*this).x,bedrock_compat::position(*this).z});
        // Generate's failure handler stops the process instead of saving an
        // unfiltered terrain buffer as successful, permanently losing structures.
        throw;
    }
    origin(volume, yOffset);
}
