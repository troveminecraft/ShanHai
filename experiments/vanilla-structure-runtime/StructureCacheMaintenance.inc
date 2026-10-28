// Suppress independent retirement of adapter-owned starts. The bounded
// collector below holds stageMutex and prunes adapter references atomically.
LL_TYPE_INSTANCE_HOOK(OwnedStructureCollection,ll::memory::HookPriority::Normal,StructureFeature,
                     &StructureFeature::garbageCollectBlueprints,void,buffer_span<ChunkPos> active,uint border) {
    if (ownedNativeFeatures.permitNativeCollection(this)) origin(active,border);
}
// Follow Bedrock's active-chunk lifetime instead of retaining every blueprint
// ever visited. The native feature performs the lifetime decision; afterwards
// remove all adapter references to starts it retired.
std::atomic_uint64_t structureCollections = 0, structurePlansRetired = 0;
void retireStructureReferences(std::set<StructureStart *> const &live) {
    auto prune = [&](auto &plans) {
        bool pruneRejected = plans.size() > 2048;
        std::erase_if(plans, [&](auto const &entry) {
            auto *start = entry.second.start;
            bool remove = start ? !live.contains(start) : pruneRejected;
            if (remove)
                ++structurePlansRetired;
            return remove;
        });
    };
    prune(villages);
    prune(automaticBlueprints);
    prune(extraPlans);
    std::erase_if(villageSpawns, [&](auto const &entry) { return !live.contains(entry.first); });
    std::erase_if(villagePopulationPlans, [](auto const &key) { return !villages.contains(key); });
    std::erase_if(iglooPopulationPlans, [](auto const &key) { return !automaticBlueprints.contains(key); });
    nativeEntityQueue.discardRetiredHandles([&](auto *start) { return live.contains(start); });
    vegetationPairs.clear();
    cityNeighbourCache.clear();
    cityLinks.clear();
    // Skeleton block programs can be restored from the durable fossil journal.
    if (frozenFossils.size() > 64)
        frozenFossils.clear();
}
unsigned structureCollectionTicks = 0;
size_t structureCollectionCursor = 0;
void collectStructureCaches(Level &level) {
    if (++structureCollectionTicks % 20 || !automaticStructures || structureStageFailed ||
        bedrock_compat::levelName(level.getLevelData()) != activeWorld)
        return;
    worldgen_rules::StructureStageSiteScope site(3,0,0);
    std::unique_lock guard(stageMutex, std::try_to_lock);
    if (!guard.owns_lock())
        return;
    if (structureCollectionTicks % 200 &&
        villages.size() + automaticBlueprints.size() + extraPlans.size() < 256)
        return;
    auto dimension = level.getDimension(0).lock();
    if (!dimension)
        return;
    try {
        // Persist unclaimed population ownership before native starts can be
        // retired. These records contain coordinates, never native pointers.
        auto &deferred = entityLedger["deferredStructureJobs"];
        if (deferred.is_null())
            deferred = J::object();
        bool changed = false;
        for (auto const &job : nativeEntityJobs) {
            if (deferred.contains(job.key))
                continue;
            deferred[job.key] = {{"family", job.family},
                                 {"anchor", {job.anchor.x, job.anchor.z}},
                                 {"clip", boxJson(job.clip)},
                                 {"seed", job.seed}};
            changed = true;
        }
        if (changed)
            persistEntityLedger();
        std::vector<StructureStart *> records;
        for (auto *feature : ownedNativeFeatures.snapshot()) {
            std::unique_lock cacheLock(*feature->mCacheMutex);
            for (auto const &[cp, start] : *feature->mCachedStructures)
                records.push_back(start.get());
        }
        std::vector<ChunkPos> keep;
        keep.reserve(records.size());
        size_t checked = std::min<size_t>(32, records.size());
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
        if (!records.empty())
            structureCollectionCursor %= records.size();
        for (size_t i = 0; i < records.size(); ++i) {
            auto *start = records[i];
            size_t distance = (i + records.size() - structureCollectionCursor) % records.size();
            bool retained = distance >= checked || std::chrono::steady_clock::now() >= deadline;
            if (!retained) {
                auto b = *start->boundingBox;
                // A generation or decoration task holds a chunk reference too. Never
                // require Loaded here: that would retire an in-progress structure.
                int minX = chunk(b.min.x - 28), maxX = chunk(b.max.x + 28);
                int minZ = chunk(b.min.z - 28), maxZ = chunk(b.max.z + 28);
                if (maxX < minX || maxZ < minZ || maxX - minX > 128 || maxZ - minZ > 128)
                    retained = true;
                else
                    for (int z = minZ; z <= maxZ && !retained; ++z) {
                        if (std::chrono::steady_clock::now() >= deadline) {
                            retained = true;
                            break;
                        }
                        for (int x = minX; x <= maxX; ++x)
                            if (dimension->getChunkSource().getExistingChunk({x, z})) {
                                retained = true;
                                break;
                            }
                    }
            }
            if (retained)
                keep.push_back({start->chunkX, start->chunkZ});
        }
        structureCollectionCursor += checked;
        // Do not hook FlatWorldGenerator's folded no-op collector. Invoke the
        // feature's real collector with retained anchors and its normal safety halo.
        ChunkPos empty{};
        buffer_span<ChunkPos> activeChunks;
        activeChunks.mBegin = keep.empty() ? &empty : keep.data();
        activeChunks.mEnd = activeChunks.mBegin + keep.size();
        std::set<StructureStart *> live;
        for (auto *feature : ownedNativeFeatures.snapshot()) {
            ownedNativeFeatures.collect(feature, [&] { feature->garbageCollectBlueprints(activeChunks, 16); });
            std::unique_lock cacheLock(*feature->mCacheMutex);
            for (auto const &[cp, start] : *feature->mCachedStructures)
                live.insert(start.get());
        }
        retireStructureReferences(live);
        ++structureCollections;
    } catch (std::exception const &e) {
        ++stageErrors;
        reportStructureFailure("structure cache collection", e);
    }
}

std::deque<std::tuple<bool, int, int>> populationRecovery;
std::set<std::tuple<bool, int, int>> populationRecoveryQueued;
unsigned populationRecoveryTicks = 0;
std::string deferredRecoveryCursor;
void clearPopulationRecovery() {
    populationRecovery.clear();
    populationRecoveryQueued.clear();
    populationRecoveryTicks = 0;
    structureCollectionTicks = 0;
    structureCollectionCursor = 0;
    deferredRecoveryCursor.clear();
}
void recoverLoadedStructurePlans(Level &level) {
    try {
        if (++populationRecoveryTicks % 20 || !automaticStructures || structureStageFailed ||
            !entityLedgerReady || bedrock_compat::levelName(level.getLevelData()) != activeWorld)
            return;
        worldgen_rules::StructureStageSiteScope site(2,0,0);
        std::unique_lock lock(stageMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;
        auto dimension = level.getDimension(0).lock();
        if (!dimension)
            return;
        uint seed = structureSeed(*dimension);
        if (auto deferred = entityLedger.find("deferredStructureJobs"); deferred != entityLedger.end()) {
            auto it = deferred->find(deferredRecoveryCursor);
            if (it != deferred->end())
                ++it;
            if (it == deferred->end())
                it = deferred->begin();
            size_t attempts = std::min<size_t>(16, deferred->size());
            while (attempts-- && !deferred->empty()) {
                if (it == deferred->end())
                    it = deferred->begin();
                deferredRecoveryCursor = it.key();
                if (entityLedger["structureEntries"].contains(it.key())) {
                    it = deferred->erase(it);
                    continue;
                }
                if (queuedEntityKeys.contains(it.key())) {
                    ++it;
                    continue;
                }
                auto const &job = it.value();
                auto const &b = job.at("clip");
                ChunkPos owner{chunk(b[0].get<int>()), chunk(b[2].get<int>())};
                auto loaded = dimension->getChunkSource().getExistingChunk(owner);
                if (!loaded || bedrock_compat::loadState(*loaded).load() != ChunkState::Loaded) {
                    ++it;
                    continue;
                }
                ChunkPos anchor{job.at("anchor")[0].get<int>(), job.at("anchor")[1].get<int>()};
                auto family = job.at("family").get<std::string>();
                StructureStart *start = nullptr;
                if (family == "village")
                    start = villagePlan(*dimension, anchor, seed).start;
                else if (family == "igloo" || family.starts_with("minecraft:"))
                    start = autoBlueprint(*dimension, anchor, seed).start;
                else
                    for (auto const &spec : extraSpecs)
                        if (family == extraNames[spec.type]) {
                            start = extraPlan(*dimension, spec.type, anchor).start;
                            break;
                        }
                if (!start)
                    throw std::runtime_error("Deferred structure blueprint is no longer available");
                queueStructureEntities(start,
                                       {{b[0].get<int>(), b[1].get<int>(), b[2].get<int>()},
                                        {b[3].get<int>(), b[4].get<int>(), b[5].get<int>()}},
                                       family, anchor, job.at("seed").get<uint>());
                return;
            }
        }
        level.forEachPlayer([&](Player &player) {
            if (static_cast<int>(player.getDimensionId()) != 0)
                return true;
            auto pos = bedrock_compat::position(player);
            int cx = int(std::floor(pos.x / 16)), cz = int(std::floor(pos.z / 16));
            auto queue = [&](bool village, ChunkPos anchor) {
                if (!structureChunkInside(anchor.x, anchor.z) || populationRecovery.size() >= 64)
                    return;
                if (village ? villages.contains({anchor.x, anchor.z})
                            : automaticBlueprints.contains({anchor.x, anchor.z}))
                    return;
                auto key = std::tuple{village, anchor.x, anchor.z};
                if (populationRecoveryQueued.insert(key).second)
                    populationRecovery.push_back(key);
            };
            for (int z = villageCell(cz - 10); z <= villageCell(cz + 10); ++z)
                for (int x = villageCell(cx - 10); x <= villageCell(cx + 10); ++x) {
                    auto v = villageCandidate(x, z, seed);
                    if (std::abs(v.x - cx) <= 10 && std::abs(v.z - cz) <= 10)
                        queue(true, v);
                    auto s = nativeCandidateForCell(x, z, seed);
                    if (std::abs(s.x - cx) <= 3 && std::abs(s.z - cz) <= 3)
                        queue(false, s);
                }
            return true;
        });
        // At most one reconstruction per second, no chunk loading or block writes.
        // Actors still require loaded owner chunks and native template witnesses.
        if (populationRecovery.empty())
            return;
        auto key = populationRecovery.front();
        populationRecovery.pop_front();
        populationRecoveryQueued.erase(key);
        auto [village, x, z] = key;
        bool nearbyLoaded = false;
        int reach = village ? 10 : 3;
        for (int dz = -reach; dz <= reach && !nearbyLoaded; ++dz)
            for (int dx = -reach; dx <= reach; ++dx) {
                auto c = dimension->getChunkSource().getExistingChunk({x + dx, z + dz});
                if (c && bedrock_compat::loadState(*c).load() == ChunkState::Loaded) {
                    nearbyLoaded = true;
                    break;
                }
            }
        if (!nearbyLoaded)
            return;
        if (village)
            villagePlan(*dimension, {x, z}, seed);
        else
            autoBlueprint(*dimension, {x, z}, seed);
    } catch (std::exception const &e) {
        ++stageErrors;
        reportStructureFailure("structure population recovery", e);
    }
}
