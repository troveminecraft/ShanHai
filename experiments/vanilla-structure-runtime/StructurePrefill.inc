// Snapshot native footprints under the registry lock. Pure vegetation work
// uses two independent Fields and only touches the caller-owned block buffer.
// Neither the shared planning Field nor native StructureStart escapes the lock.
worldgen_rules::BoundedHints<std::pair<int, int>, 4096> prefilteredChunks;
unsigned prefillLogRows = 0;
using ReservationAreas = std::vector<std::array<int, 4>>;
struct PrefillWorker {
    std::mutex mutex;
    std::unique_ptr<nativegen::Field> field;
    std::unique_ptr<nativegen::Engine> normal, reserved;
    ReservationAreas areas;
    void clear() {
        reserved.reset();
        normal.reset();
        field.reset();
        areas.clear();
    }
    void prepare(ReservationAreas const &next) {
        if (!field) {
            field = std::make_unique<nativegen::Field>(input);
            normal = std::make_unique<nativegen::Engine>(input, ReservationAreas{}, true, field.get());
        }
        if (!reserved || areas != next) {
            reserved = std::make_unique<nativegen::Engine>(input, next, true, field.get());
            areas = next;
        }
    }
};
std::array<PrefillWorker, 2> prefillWorkers;
std::atomic_uint64_t prefillJobs = 0, prefillComputeNanos = 0;
void clearPrefillWorkers() {
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
                auto idx = volume.indexNoBoundsCheck(BlockPos{x, y, z});
                if (!worldgen_rules::structureReservationRemoves(previous, reserved,
                                                                 data[idx]->getTypeName()))
                    return;
                data[idx] = air;
                ++count;
                if (y+1 < volume.mHeight) {
                    auto above = volume.indexNoBoundsCheck(BlockPos{x, y + 1, z});
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
    auto cp = getPosition();
    auto &dim = getDimension();
    if (!automaticStructures || volume.mWidth != 16 || volume.mDepth != 16 ||
        (volume.mHeight != 384 && volume.mHeight != 576) ||
        volume.mDimensionBottom != -64 || yOffset != 0 || getState().load() > ChunkState::Generated ||
        static_cast<int>(dim.getDimensionId()) != 0 ||
        dim.getLevel().getLevelData().getLevelName() != activeWorld || !structureChunkInside(cp.x, cp.z)) {
        origin(volume, yOffset);
        return;
    }
    // Vanilla void initialization precedes the custom terrain buffer.
    if (volume.mBlocks->mBegin[volume.indexNoBoundsCheck(BlockPos{0, 0, 0})]->getTypeName() !=
        "minecraft:bedrock") {
        origin(volume, yOffset);
        return;
    }
    if (structureStageFailed)
        throw std::runtime_error("Structure generation paused after an earlier error");
    uint seed = dim.getLevel().getLevelData().getSeed().to32BitRandomSeed();
    std::vector<ChunkPos> vc, sc;
    for (int z = structureCell(cp.z - 10); z <= structureCell(cp.z + 10); ++z)
        for (int x = structureCell(cp.x - 10); x <= structureCell(cp.x + 10); ++x) {
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
        std::vector<ReservationAreas> groups;
        {
            std::lock_guard guard(stageMutex);
            if (structureStageFailed)
                throw std::runtime_error("Structure generation paused");
            if (!storedChunk(dim, cp)) {
                auto intersects = [&](BoundingBox b) {
                    return !(b.max.x + 28 < cp.x * 16 || b.min.x - 28 > cp.x * 16 + 15 ||
                             b.max.z + 28 < cp.z * 16 || b.min.z - 28 > cp.z * 16 + 15);
                };
                for (auto c : vc) {
                    auto &p = villagePlan(dim, c, seed);
                    if (p.start && intersects(*p.start->boundingBox))
                        groups.push_back(pieceReservations(*p.start));
                }
                for (auto c : sc) {
                    auto &p = autoBlueprint(dim, c, seed);
                    if (!p.start || p.start->getStructureName() == "minecraft:desert_pyramid" ||
                        !intersects(*p.start->boundingBox))
                        continue;
                    auto b = *p.start->boundingBox;
                    groups.push_back({{b.min.x + 32756, b.min.z + 32756, b.max.x + 32780, b.max.z + 32780}});
                }
                for (auto [type, c] : ec) {
                    auto &p = extraPlan(dim, type, c);
                    if (p.start && intersects(*p.start->boundingBox))
                        groups.push_back(pieceReservations(*p.start));
                }
            }
        }
        if (!groups.empty()) {
            auto began = std::chrono::steady_clock::now();
            auto cleared = filterVegetationVolume(volume, cp, groups);
            std::lock_guard guard(stageMutex);
            if (structureStageFailed)
                throw std::runtime_error("Structure generation paused during prefill");
            prefilteredChunks.insert({cp.x, cp.z});
            if (prefillLogRows++ < 2048)
                std::ofstream(folder / "structure-prefill.jsonl", std::ios::app)
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
        reportStructureFailure("prefill", e, std::array<int,2>{getPosition().x,getPosition().z});
        // Generate's failure handler stops the process instead of saving an
        // unfiltered terrain buffer as successful, permanently losing structures.
        throw;
    }
    origin(volume, yOffset);
}
