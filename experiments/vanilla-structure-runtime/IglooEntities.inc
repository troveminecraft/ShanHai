// Main-thread, once-only igloo population using vanilla entity definitions. A durable claim precedes the
// spawn calls: an interrupted claim is retained for explicit diagnosis, never replayed.
ll::event::ListenerPtr entityTickListener;
extern J entityLedger;
void persistEntityLedger();
void tickStructureEntities(Level &level) {
    static unsigned ticks = 0;
    if (++ticks % worldgen_rules::StructureEntityTickInterval || !automaticStructures ||
        structureStageFailed || !entityLedgerReady || level.getLevelData().getLevelName() != activeWorld)
        return;
    std::unique_lock guard(stageMutex, std::try_to_lock);
    if (!guard.owns_lock())
        return;
    auto d = level.getDimension(0).lock();
    if (!d)
        return;
    // Saved chunks can skip decoration completely. Check two cached village
    // plans per second; require an actual native door/bed before scheduling their
    // finite template marker chunks. A locate blueprint alone cannot spawn mobs.
    static size_t cursor = 0;
    for (int check = 0; check < worldgen_rules::VillageReloadChecksPerTick && !villagePopulationPlans.empty();
         ++check) {
        cursor %= villagePopulationPlans.size();
        auto it = villages.find(villagePopulationPlans[cursor++]);
        if (it == villages.end())
            continue;
        auto &plan = it->second;
        if (!plan.start || plan.spawns.empty())
            continue;
        if (plan.sharedLayout) {
            worldgen_rules::enqueueLoadedVillageOwners(
                nativeEntityQueue, entityLedger, plan.start, *plan.sharedLayout,
                {it->first.first, it->first.second}, level.getLevelData().getSeed().to32BitRandomSeed(),
                [&](worldgen_rules::ChunkPosition cp) {
                    auto c = d->getChunkSource().getExistingChunk({cp.x, cp.z});
                    return c && c->getState().load() == ChunkState::Loaded;
                });
            continue;
        }
        bool present = false;
        auto &blocks = d->getBlockSourceFromMainChunkSource();
        for (auto const &[pos, name] : plan.witnesses) {
            auto c = d->getChunkSource().getExistingChunk({chunk(pos.x), chunk(pos.z)});
            if (c && c->getState().load() == ChunkState::Loaded &&
                blocks.getBlock(pos).getTypeName() == name) {
                present = true;
                break;
            }
        }
        if (!present)
            continue;
        std::set<std::pair<int, int>> chunks;
        for (auto const &spawn : plan.spawns)
            chunks.emplace(chunk(spawn.pos.x), chunk(spawn.pos.z));
        for (auto [x, z] : chunks) {
            auto c = d->getChunkSource().getExistingChunk({x, z});
            if (c && c->getState().load() == ChunkState::Loaded)
                queueStructureEntities(plan.start, {{x * 16, -64, z * 16}, {x * 16 + 15, 319, z * 16 + 15}},
                                       "village", {it->first.first, it->first.second},
                                       level.getLevelData().getSeed().to32BitRandomSeed());
        }
    }
    auto &entries = entityLedger["structureEntries"];
    if (entries.is_null())
        entries = J::object();
    std::map<std::string, std::vector<worldgen_rules::StructureEntityMarker>> preparedVillagers;
    std::vector<NativeEntityJob> ready;
    try {
        ready = nativeEntityQueue.claimReady(
            entityLedger,
            [&](NativeEntityJob const &job) {
                auto cp = ChunkPos{chunk(job.clip.min.x), chunk(job.clip.min.z)};
                if (job.key.starts_with("igloo:")) {
                    auto it = automaticBlueprints.find({int(job.start->chunkX), int(job.start->chunkZ)});
                    if (it == automaticBlueprints.end() || !it->second.igloo || !it->second.igloo->program)
                        return false;
                    auto &blocks = d->getBlockSourceFromMainChunkSource();
                    auto markers = worldgen_rules::prepareIglooPopulation(
                        *it->second.igloo->program, {cp.x, cp.z},
                        [&](worldgen_rules::ChunkPosition p) {
                            auto c = d->getChunkSource().getExistingChunk({p.x, p.z});
                            return c && c->getState().load() == ChunkState::Loaded;
                        },
                        [&](worldgen_rules::StructurePoint p) -> std::string_view {
                            return blocks.getBlock({p.x, p.y, p.z}).getTypeName();
                        });
                    if (!markers)
                        return false;
                    preparedVillagers.emplace(job.key, std::move(*markers));
                    return true;
                }
                if (job.key.starts_with("village:")) {
                    auto it = villages.find({int(job.start->chunkX), int(job.start->chunkZ)});
                    if (it != villages.end() && it->second.sharedLayout) {
                        auto &blocks = d->getBlockSourceFromMainChunkSource();
                        auto prepared = worldgen_rules::prepareVillagePopulation(
                            *it->second.sharedLayout, {cp.x, cp.z},
                            [&](worldgen_rules::ChunkPosition c) {
                                auto chunk = d->getChunkSource().getExistingChunk({c.x, c.z});
                                return chunk && chunk->getState().load() == ChunkState::Loaded;
                            },
                            [&](worldgen_rules::StructurePoint p) -> std::string_view {
                                return blocks.getBlock({p.x, p.y, p.z}).getTypeName();
                            });
                        if (!prepared)
                            return false;
                        preparedVillagers.emplace(job.key, std::move(*prepared));
                        return true;
                    }
                }
                auto ch = d->getChunkSource().getExistingChunk(cp);
                return ch && ch->getState().load() == ChunkState::Loaded;
            },
            [](J const &) { persistEntityLedger(); });
    } catch (std::exception const &e) {
        if (!entityLedgerReady)
            throw;
        ll::mod::NativeMod::current()->getLogger().warn("Structure population readiness deferred: {}",
                                                        e.what());
        return;
    }
    if (ready.empty())
        return;
    for (auto const &job : ready) {
        auto cp = ChunkPos{chunk(job.clip.min.x), chunk(job.clip.min.z)};
        auto ch = d->getChunkSource().getExistingChunk(cp);
        auto &blocks = d->getBlockSourceFromMainChunkSource();
        std::set<int64_t> before;
        for (auto *a : level.getRuntimeActorList())
            before.insert(a->getOrCreateUniqueID().rawID);
        std::string error;
        try {
            Random rng(worldgen_rules::structureEntitySeed({cp.x, cp.z}, job.seed), true);
            if (job.key.starts_with("trail_ruins:") || job.key.starts_with("trial_chambers:"))
                static_cast<ModernStart *>(job.start)->instance->placeEntities(blocks, job.clip);
            else if (job.key.starts_with("village:") || job.key.starts_with("igloo:")) {
                auto spawnMarker = [&](worldgen_rules::StructurePoint p, std::string const &label) {
                    auto definition = worldgen_rules::structureEntityDefinition(label);
                    ActorDefinitionIdentifier id(definition.nameSpace, definition.name, definition.event);
                    auto location = worldgen_rules::structureActorLocation(p);
                    auto *mob = level.getSpawner().spawnMob(
                        blocks, id, nullptr, {location.x, location.y, location.z}, false, false, false);
                    if (!mob)
                        throw std::runtime_error("Village template actor spawn failed");
                };
                if (auto it = preparedVillagers.find(job.key); it != preparedVillagers.end())
                    for (auto const &m : it->second)
                        spawnMarker(m.position, m.definition);
                else
                    for (auto const &spawn : villageSpawns.at(job.start)) {
                        auto p = spawn.pos;
                        if (p.x < job.clip.min.x || p.x > job.clip.max.x || p.z < job.clip.min.z ||
                            p.z > job.clip.max.z)
                            continue;
                        auto supported = worldgen_rules::supportedStructureEntityPosition(
                            {p.x, p.y, p.z}, [&](worldgen_rules::StructurePoint q) {
                                return blocks.getBlock({q.x, q.y, q.z}).getTypeName();
                            });
                        if (!supported)
                            throw std::runtime_error("Village native actor marker has no supported headroom");
                        spawnMarker(*supported, spawn.definition);
                    }
            } else
                for (auto const &piece : *job.start->pieces) {
                    auto b = *piece->mBoundingBox;
                    if (b.max.x < job.clip.min.x || b.min.x > job.clip.max.x || b.max.z < job.clip.min.z ||
                        b.min.z > job.clip.max.z)
                        continue;
                    piece->postProcessMobsAt(blocks, rng, job.clip);
                }
        } catch (std::exception const &e) {
            error = e.what();
        }
        J actors = J::array();
        try {
            for (auto *a : level.getRuntimeActorList())
                if (!before.contains(a->getOrCreateUniqueID().rawID)) {
                    a->setPersistent();
                    a->addTag(std::string(worldgen_rules::NativeStructureEntityTag));
                    if (job.key.starts_with("igloo:"))
                        a->addTag(std::string(worldgen_rules::IglooResidentTag));
                    auto p = a->getPosition();
                    actors.push_back({{"id", a->getOrCreateUniqueID().rawID},
                                      {"type", a->getTypeName()},
                                      {"pos", {p.x, p.y, p.z}}});
                }
            ch->setUnsaved();
            ch->markSaveIfNeverSaved();
            ch->markSaveNonActorDataIfDirty();
        } catch (std::exception const &e) {
            if (error.empty())
                error = e.what();
        }
        if (error.empty())
            worldgen_rules::completeStructureEntityClaim(entityLedger, job.key, std::move(actors));
        else {
            worldgen_rules::failStructureEntityClaim(entityLedger, job.key, error, std::move(actors));
            ll::mod::NativeMod::current()->getLogger().warn(
                "Structure population retained for review: {}: {}", job.key, error);
        }
    }
    persistEntityLedger();
}
J entityLedger = J::object();
std::filesystem::path entityLedgerPath() {
    return worldFolder() / "karma-native-entities.json";
}
void persistEntityLedger() {
    if (!entityLedgerReady)
        throw std::runtime_error("Entity spawns paused: invalid claim ledger");
    try {
        entity_store::persist(entityLedgerPath(), entityLedger);
    } catch (...) {
        entityLedgerReady = false;
        throw;
    }
}
void initializeEntityLedger() {
    entityLedgerReady = false;
    auto ledgerSeed = worldgen_rules::structureLedgerSeed(input);
    auto f = entityLedgerPath();
    auto legacy = folder / "igloo-entities.json";
    if (!std::filesystem::exists(f) && std::filesystem::exists(legacy)) {
        auto old = entity_store::read(legacy);
        if (old.value("world", std::string{}) == worldIdentity() && old["terrainSeed"] == ledgerSeed) {
            entity_store::validate(old, worldIdentity(), ledgerSeed);
            entity_store::persist(f, old);
        }
    }
    auto loaded = entity_store::load(f, worldIdentity(), ledgerSeed);
    entityLedger = std::move(loaded.data);
    entityLedgerReady = loaded.enabled;
    J status = {{"world", activeWorld},
                {"path", entity_store::label(f)},
                {"entitySpawnsEnabled", loaded.enabled},
                {"backupRecovered", loaded.recovered},
                {"reason", loaded.error}};
    std::ofstream(folder / "entity-ledger-status.json") << status.dump(2);
    if (!loaded.enabled || loaded.recovered)
        ll::mod::NativeMod::current()->getLogger().warn("Entity ledger: {}", status.dump());
}
J iglooActors(Dimension &d, BoundingBox const &b) {
    J rows = J::array();
    for (auto *a : d.getLevel().getRuntimeActorList()) {
        if (&a->getDimension() != &d)
            continue;
        auto const &type = a->getTypeName();
        if (type != "minecraft:villager" && type != "minecraft:villager_v2" &&
            type != "minecraft:zombie_villager" && type != "minecraft:zombie_villager_v2")
            continue;
        auto p = a->getPosition();
        if (p.x >= b.min.x - 2 && p.x <= b.max.x + 3 && p.y >= b.min.y - 2 && p.y <= b.max.y + 3 &&
            p.z >= b.min.z - 2 && p.z <= b.max.z + 3)
            rows.push_back(J{{"type", a->getTypeName()},
                             {"alive", a->isAlive()},
                             {"removed", a->isRemoved()},
                             {"id", a->getOrCreateUniqueID().rawID},
                             {"pos", {p.x, p.y, p.z}}});
    }
    return rows;
}
void tickIglooEntities(Level &level) {
    static unsigned ticks = 0;
    if (++ticks % worldgen_rules::StructureEntityTickInterval || !automaticStructures ||
        structureStageFailed || !entityLedgerReady || level.getLevelData().getLevelName() != activeWorld)
        return;
    std::unique_lock guard(stageMutex, std::try_to_lock);
    if (!guard.owns_lock())
        return;
    auto d = level.getDimension(0).lock();
    if (!d)
        return;
    if (worldgen_rules::sharedStructurePlan(
            input.at("config").value("sharedRules", J::object()).value("revision", std::string{}))) {
        static size_t cursor = 0;
        for (int check = 0;
             check < worldgen_rules::VillageReloadChecksPerTick && !iglooPopulationPlans.empty(); ++check) {
            cursor %= iglooPopulationPlans.size();
            auto it = automaticBlueprints.find(iglooPopulationPlans[cursor++]);
            if (it == automaticBlueprints.end() || !it->second.start || !it->second.igloo ||
                !it->second.igloo->program)
                continue;
            auto const &plan = it->second;
            worldgen_rules::enqueueLoadedPopulationOwners(
                nativeEntityQueue, entityLedger, plan.start, plan.igloo->program->population, "igloo",
                {it->first.first, it->first.second}, level.getLevelData().getSeed().to32BitRandomSeed(),
                [&](worldgen_rules::ChunkPosition cp) {
                    auto c = d->getChunkSource().getExistingChunk({cp.x, cp.z});
                    return c && c->getState().load() == ChunkState::Loaded;
                });
        }
        return;
    }
    for (auto &[anchor, p] : automaticBlueprints) {
        if (!p.igloo || !p.igloo->ladders)
            continue;
        auto key = std::to_string(anchor.first) + "," + std::to_string(anchor.second);
        if (entityLedger["entries"].contains(key))
            continue;
        auto const &bottom = p.igloo->parts.back();
        bool loaded = true;
        for (int z = chunk(bottom.bounds.min.z); z <= chunk(bottom.bounds.max.z); ++z)
            for (int x = chunk(bottom.bounds.min.x); x <= chunk(bottom.bounds.max.x); ++x) {
                auto c = d->getChunkSource().getExistingChunk({x, z});
                loaded &= c && c->getState().load() == ChunkState::Loaded;
            }
        if (!loaded)
            continue;
        auto &blocks = d->getBlockSourceFromMainChunkSource();
        LegacyStructureSettings settings(Mirror::None, p.igloo->rotation, nullptr, bottom.bounds);
        std::optional<BlockPos> v, z;
        for (auto const &[pos, name] : bottom.model->getMarkers(bottom.pos, settings)) {
            if (name == "Villager")
                v = pos;
            if (name == "Zombie Villager")
                z = pos;
        }
        if (!v || !z)
            throw std::runtime_error("Igloo entity markers absent");
        bool ready = true;
        for (auto pos : {*v, *z})
            ready &= blocks.getBlock(pos).getTypeName() == "minecraft:air" &&
                     blocks.getBlock({pos.x, pos.y - 1, pos.z}).getTypeName() != "minecraft:air";
        if (!ready)
            continue;
        auto before = iglooActors(*d, bottom.bounds);
        if (!before.empty()) {
            entityLedger["entries"][key] = {{"state", "existing-actors-review"}, {"actors", before}};
            persistEntityLedger();
            continue;
        }
        entityLedger["entries"][key] = {{"state", "claimed"}, {"box", boxJson(bottom.bounds)}};
        persistEntityLedger();
        auto spawn = [&](BlockPos pos, char const *type, char const *event) {
            ActorDefinitionIdentifier id("minecraft", type, event);
            auto *mob = level.getSpawner().spawnMob(blocks, id, nullptr,
                                                    {float(pos.x) + .5f, float(pos.y), float(pos.z) + .5f},
                                                    false, false, false);
            if (!mob)
                throw std::runtime_error("Igloo resident spawn failed");
            mob->setPersistent();
            mob->addTag(std::string(worldgen_rules::IglooResidentTag));
            auto c = d->getChunkSource().getExistingChunk({chunk(pos.x), chunk(pos.z)});
            if (c) {
                c->setUnsaved();
                c->markSaveIfNeverSaved();
                c->markSaveNonActorDataIfDirty();
            }
        };
        auto vdef = *worldgen_rules::iglooEntityDefinition("Villager"),
             zdef = *worldgen_rules::iglooEntityDefinition("Zombie Villager");
        spawn(*v, vdef.name.c_str(), vdef.event.c_str());
        spawn(*z, zdef.name.c_str(), zdef.event.c_str());
        auto after = iglooActors(*d, bottom.bounds);
        for (auto *a : level.getRuntimeActorList())
            for (auto const &r : after)
                if (a->getOrCreateUniqueID().rawID == r["id"].get<int64_t>()) {
                    a->setPersistent();
                    a->addTag(std::string(worldgen_rules::IglooResidentTag));
                }
        entityLedger["entries"][key]["state"] = after.size() == 2 ? "completed" : "review-count";
        entityLedger["entries"][key]["actors"] = after;
        persistEntityLedger();
        std::ofstream(folder / "igloo-entity-events.jsonl", std::ios::app)
            << entityLedger["entries"][key].dump() << '\n';
        break;
    }
}
