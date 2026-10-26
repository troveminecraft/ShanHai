#include "SharedStructureAdapter.inc"
#include "VillageGrowth.inc"
// Native village blueprints, cached per spacing cell. Only gently sloping dry
// sites are accepted; roads keep vanilla terrain-matching projection.
class TerrainVillageFeature final : public VillageFeature {
  public:
    TerrainVillageFeature(uint seed, BiomeRegistry const &b) : VillageFeature(seed, 32, 8, b) {}
};
TerrainVillageFeature *villageFeatureRecord = nullptr;
TerrainVillageFeature &registeredVillage(Dimension &d) {
    if (villageFeatureRecord)
        return *villageFeatureRecord;
    auto &entries = *d.getWorldGenerator()->mStructureFeatureRegistry->mStructureFeatures;
    auto v = std::make_unique<TerrainVillageFeature>(structureSeed(d), d.getBiomeRegistry());
    auto *ptr = v.get();
    villageFeatureRecord = ptr;
    ownedNativeFeatures.insert(ptr);
    entries.push_back(std::move(v));
    return *ptr;
}
struct VillagePad {
    BoundingBox box;
    int height;
};
struct VillageSpawn {
    BlockPos pos;
    std::string definition;
};
std::unordered_map<StructureStart *, std::vector<VillageSpawn>> villageSpawns;
bool villageActorChunk(StructureStart *start, BoundingBox const &clip) {
    auto it = villageSpawns.find(start);
    return it != villageSpawns.end() && std::any_of(it->second.begin(), it->second.end(), [&](auto const &s) {
               return s.pos.x >= clip.min.x && s.pos.x <= clip.max.x && s.pos.z >= clip.min.z &&
                      s.pos.z <= clip.max.z;
           });
}
std::vector<std::pair<int, int>> villagePopulationPlans;
bool villageTemplatesReady = false;
void warmVillageTemplates(Level &level) {
    if (villageTemplatesReady || level.getLevelData().getLevelName() != activeWorld)
        return;
    std::lock_guard guard(stageMutex);
    if (villageTemplatesReady)
        return;
    auto began = std::chrono::steady_clock::now();
    J names = J::array();
    auto root = vanillaFolder() / "__brarchive/structures/village";
    auto manager = level.getStructureManager();
    for (auto const &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".brarchive")
            continue;
        std::ifstream f(entry.path(), std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(f)), {});
        uint32_t count = 0;
        if (bytes.size() < 16)
            throw std::runtime_error("Village archive unavailable");
        std::memcpy(&count, bytes.data() + 8, 4);
        if (count > 512 || bytes.size() != 16 + size_t(count) * 256)
            throw std::runtime_error("Village archive index mismatch");
        auto prefix =
            entry.path().lexically_relative(root.parent_path()).replace_extension().generic_string();
        for (uint32_t i = 0; i < count; ++i) {
            auto offset = 16 + size_t(i) * 256;
            auto len = uint8_t(bytes[offset]);
            if (len == 0 || len >= 128)
                throw std::runtime_error("Village archive name invalid");
            std::string name(bytes.data() + offset + 1, len);
            if (!name.ends_with(".nbt"))
                continue;
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
                std::string::npos)
                throw std::runtime_error("Village archive entry invalid");
            name = prefix + "/" + name.substr(0, name.size() - 4);
            auto size = manager->getOrCreateLegacy(name).rawSize();
            if (size.x <= 0 || size.y <= 0 || size.z <= 0)
                throw std::runtime_error("Village template missing: " + name);
            names.push_back(name);
        }
    }
    if (names.empty())
        throw std::runtime_error("Village native templates missing");
    villageTemplatesReady = true;
    std::ofstream(folder / "village-template-warmup.json")
        << J{{"world", activeWorld},
             {"count", names.size()},
             {"names", names},
             {"ms",
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count()}}
               .dump(2);
}
struct VillageOasisCell {
    int floor, water;
    bool green;
};
struct VillagePlan {
    std::shared_ptr<worldgen_rules::VillageLayoutPlan> sharedLayout;
    worldgen_rules::VillageBlueprint sharedBlueprint;
    std::map<std::pair<int, int>, std::pair<int, int>> roadSurfaceFix;
    std::map<std::pair<int, int>, std::vector<std::pair<BlockPos, Block const *>>> paddingGroundWrites;
    std::map<std::pair<int, int>, VillageOasisCell> oasis;
    StructureStart *start = nullptr;
    J diagnostics;
    std::vector<VillagePad> pads;
    std::map<std::pair<int, int>, int> foundations;
    std::map<std::pair<int, int>, int> fittedGround;
    std::map<std::array<int, 3>, Block const *> templateBlocks;
    std::set<StructurePiece const *> blockedFeatures;
    std::vector<std::pair<BlockPos, std::string>> witnesses;
    std::vector<VillageSpawn> spawns;
    std::vector<BlockPos> wallPositions;
    bool decorReady = false;
    std::map<std::array<int, 3>, Block const *> decorWrites;
};
thread_local VillagePlan const *capturedVillageGround = nullptr;
void placeVillageDecor(BlockSource &, ChunkPos, VillagePlan &, uint);
std::map<std::pair<int, int>, VillagePlan> villages;
int villageSpacing(){return auditVillageSpacing?*auditVillageSpacing:liveVillageSpacing.load();}
int villageCell(int v){return worldgen_rules::spacingCell(v,villageSpacing());}
ChunkPos villageCandidate(int x, int z, uint seed) {
    int spacing=villageSpacing(),separation=spacing==16?8:spacing==20?16:worldgen_rules::VillageSeparation;
    struct Slot {
        int x = 0, z = 0, spacing = 0;
        uint seed = 0;
        ChunkPos cp;
        bool valid = false;
    };
    thread_local std::array<Slot, 64> cache{};
    auto &e = cache[(uint(x) * 73856093u ^ uint(z) * 19349663u) & 63u];
    if (!e.valid || e.x != x || e.z != z || e.seed != seed || e.spacing != spacing) {
        auto cp =
            worldgen_rules::candidateForCell(x, z, seed, spacing,
                                             separation, worldgen_rules::VillageSalt);
        e = {x, z, spacing, seed, {cp.x, cp.z}, true};
    }
    return e.cp;
}
int villageStyle(int biome) {
    return worldgen_rules::villageStyle(biome);
}

char const *villageBiome(int biome) {
    static char const *names[] = {"minecraft:ice_plains", "minecraft:taiga", "minecraft:plains",
                                  "minecraft:desert", "minecraft:savanna"};
    return names[villageStyle(biome)];
}
bool buildSharedVillage(StructureStart &start, VillagePlan &result, ChunkPos cp, uint seed, int biome) {
    worldgen_rules::VillageBlueprint blueprint{{cp.x, cp.z}, seed, biome, {}};
    for (auto const &p : *start.pieces)
        blueprint.pieces.push_back(sharedPoolPiece(*static_cast<PoolElementStructurePiece *>(p.get())));
    auto plan = worldgen_rules::planVillageLayout(
        blueprint, [](int x, int z) { return field->surface(x + 32768, z + 32768); },
        [](int x, int z, uint32_t seed) { return nativegen::hash01(x, z, seed); }, field->generationHeight()-65);
    if (!plan.accepted)
        return false;
    // The entire plan was validated before committing any native move.
    for (size_t i = 0; i < plan.verticalMoves.size(); ++i)
        if (int dy = plan.verticalMoves[i])
            (*start.pieces)[i]->moveBoundingBox(0, dy, 0);
    start.calculateBoundingBox();
    result.foundations = std::move(plan.foundations);
    result.fittedGround = std::move(plan.ground.fitted);
    result.roadSurfaceFix = std::move(plan.roadSurfaceFix);
    for (auto const &[q, b] : plan.templateBlocks)
        result.templateBlocks[q] = sharedBlock(b);
    for (auto const &[cp, writes] : plan.paddingWrites)
        for (auto const &w : writes)
            result.paddingGroundWrites[{cp.x, cp.z}].push_back(
                {{w.position.x, w.position.y, w.position.z}, sharedBlock(w.block)});
    for (auto const &[xz, c] : plan.oasis)
        result.oasis[xz] = {c.floor, c.water, c.green};
    for (auto const &[owner, markers] : plan.population)
        for (auto const &m : markers)
            result.spawns.push_back({{m.position.x, m.position.y, m.position.z}, m.definition});
    for (auto const &w : plan.witnesses)
        result.witnesses.push_back({{w.position.x, w.position.y, w.position.z}, std::string(w.block.name)});
    for (auto p : plan.wallPositions)
        result.wallPositions.push_back({p.x, p.y, p.z});
    J pieces = J::array(), features = J::array();
    for (size_t i = 0; i < blueprint.pieces.size(); ++i) {
        auto &p = *static_cast<PoolElementStructurePiece *>((*start.pieces)[i].get());
        auto const &b = blueprint.pieces[i];
        if (plan.blockedFeatures[i])
            result.blockedFeatures.insert(&p);
        if (b.kind == worldgen_rules::StructurePieceKind::Feature) {
            features.push_back({{"location", b.location},
                                {"position", {p.mPosition->x, p.mPosition->y, p.mPosition->z}},
                                {"blocked", plan.blockedFeatures[i]}});
            continue;
        }
        auto bb = *p.mBoundingBox;
        pieces.push_back({{"location", *p.mElement.mLocation},
                          {"box", boxJson(bb)},
                          {"base", p.mPosition->y},
                          {"projection", int(p.mElement.getProjection())}});
    }
    for (auto &p : blueprint.pieces) {
        p.cells.clear();
        p.cells.shrink_to_fit();
    }
    result.sharedBlueprint = std::move(blueprint);
    result.sharedLayout = std::make_shared<worldgen_rules::VillageLayoutPlan>(std::move(plan));
    auto const &stats = *result.sharedLayout;
    result.diagnostics = {{"rules", worldgen_rules::StructurePlanRevision},
                          {"biome", villageBiome(biome)},
                          {"nativeDecor", features},
                          {"oasisColumns", result.oasis.size()},
                          {"selection", "shared-structure-plan"},
                          {"anchor", {cp.x * 16, cp.z * 16}},
                          {"box", boxJson(*start.boundingBox)},
                          {"pieces", pieces},
                          {"groundMin", stats.groundMin},
                          {"groundMax", stats.groundMax},
                          {"surfaceQueries", stats.surfaceQueries}};
    return true;
}
VillagePlan &villagePlan(Dimension &d, ChunkPos cp, uint seed) {
    if (!structureIncluded("village") || !villageTemplatesReady) {
        static VillagePlan pending;
        return pending;
    }
    auto key = std::pair{cp.x, cp.z};
    if (auto it = villages.find(key); it != villages.end())
        return it->second;
    auto &result = villages[key];
    int wx = cp.x * 16, wz = cp.z * 16;
    if (!structureBoxInside(wx - 128, wz - 128, wx + 128, wz + 128))
        return result;
    auto fit = [](auto const &s) {
        return worldgen_rules::villageSurfaceAllowed(s, worldgen_rules::villageStyle(s.biome));
    };
    auto center = field->surface(wx + 32768, wz + 32768);
    if (!fit(center))
        return result;
    int lo = INT_MAX, hi = -64;
    // Cheap centre screen only. The old 193x193 square rejected villages for
    // slopes/biomes they never occupied; every actual native piece is checked
    // below before registration, including roads and houses.
    for (int z = -32; z <= 32; z += 32)
        for (int x = -32; x <= 32; x += 32) {
            auto s = field->surface(wx + x + 32768, wz + z + 32768);
            if (!fit(s) || villageStyle(s.biome) != villageStyle(center.biome))
                return result;
            lo = std::min(lo, s.height);
            hi = std::max(hi, s.height);
            if (hi - lo > 10)
                return result;
        }
    auto biome = d.getBiomeRegistry().lookupByName(villageBiome(center.biome));
    if (!biome)
        return result;
    FixedBiomeSource bs(*biome);
    VillageFeature f(seed, 32, 8, d.getBiomeRegistry());
    Random rng(seed, true);
    StructureFeature::setRandomSeedFor(rng, cp.x, cp.z, 10387312, seed);
    std::map<std::pair<int, int>, std::vector<short>> heights;
    planningHeights = &heights;
    adaptHeightMap = true;
    struct Reset {
        ~Reset() {
            adaptHeightMap = false;
            planningHeights = nullptr;
        }
    } reset;
    auto t = std::chrono::steady_clock::now();
    auto start = f.createStructureStart(d, bs, rng, cp, provider);
    if (!start || !start->isValid())
        return result;
    if (input.at("config").value("sharedRules", J::object()).value("villageSizeClasses", false)) {
        int baseline = 0;
        for (auto const &p : *start->pieces)
            baseline += worldgen_rules::villageBuilding(
                *static_cast<PoolElementStructurePiece *>(p.get())->mElement.mLocation);
        auto budget =
            worldgen_rules::villageGrowthBudget({cp.x, cp.z}, seed, baseline, int(start->pieces->size()));
        VillageGrowthContext growth{budget};
        villageGrowth = &growth;
        struct GrowthReset {
            ~GrowthReset() {
                villageGrowth = nullptr;
            }
        } growthReset;
        Random expandedRandom(seed, true);
        StructureFeature::setRandomSeedFor(expandedRandom, cp.x, cp.z, 10387312, seed);
        auto expanded = f.createStructureStart(d, bs, expandedRandom, cp, provider);
        if (expanded && expanded->isValid())
            start = std::move(expanded);
        growth.count(*start->pieces);
        result.diagnostics["size"] = {{"scale", budget.scale},
                                      {"baselineBuildings", baseline},
                                      {"targetBuildings", budget.buildings},
                                      {"actualBuildings", growth.buildings},
                                      {"pieceLimit", budget.pieces}};
    }
    auto size = result.diagnostics.value("size", J::object());
    if (!buildSharedVillage(*start, result, cp, seed, center.biome))
        return result;
    if (!size.empty())
        result.diagnostics["size"] = std::move(size);
    J savedPieces = J::array(), savedGround = J::array();
    for (auto const &piece : result.sharedBlueprint.pieces)
        savedPieces.push_back({{"template", std::string(piece.location)},
                               {"origin", {piece.origin.x, piece.origin.y, piece.origin.z}},
                               {"rotation", piece.rotation},
                               {"rigid", piece.rigid}});
    for (auto const &[xz, y] : result.fittedGround)
        savedGround.push_back({xz.first, xz.second, y});
    checkpointStructure(*start, "village",
                        J{{"templates", std::move(savedPieces)}, {"ground", std::move(savedGround)}});
    result.start = start.get();
    if (auditPlanningStarts) {
        auditPlanningStarts->push_back(std::move(start));
        return result;
    }
    villageSpawns.emplace(result.start, result.spawns);
    villagePopulationPlans.push_back(key);
    auto &record = registeredVillage(d);
    std::unique_lock lock(*record.mCacheMutex);
    if (record.mCachedStructures->contains(cp)) {
        villageSpawns.erase(result.start);
        result.start = nullptr;
        throw std::runtime_error("Native structure cache owner collision");
    }
    record.mCachedStructures->emplace(cp, std::move(start));
    return result;
}

int villageGround(VillagePlan const &plan, int x, int z, int natural) {
    return worldgen_rules::structureGround(plan.fittedGround, plan.pads, x, z, natural);
}
void villageTerrain(BlockSource &blocks, ChunkPos cp, VillagePlan const &plan) {
    auto air = Block::tryGetFromRegistry("minecraft:air"), dirt = Block::tryGetFromRegistry("minecraft:dirt"),
         grass = Block::tryGetFromRegistry("minecraft:grass_block"),
         sandstone = Block::tryGetFromRegistry("minecraft:sandstone"),
         sand = Block::tryGetFromRegistry("minecraft:sand");
    if (!air || !dirt || !grass || !sandstone || !sand)
        throw std::runtime_error("Missing village ground material");
    for (int z = cp.z * 16; z < cp.z * 16 + 16; ++z)
        for (int x = cp.x * 16; x < cp.x * 16 + 16; ++x) {
            auto s = field->surface(x + 32768, z + 32768);
            int target = villageGround(plan, x, z, s.height);
            worldgen_rules::writeStructureGroundColumn(x, z, s.height, target, s.biome, [&](auto p, auto b) {
                auto block = b.name == "minecraft:air"           ? air
                             : b.name == "minecraft:dirt"        ? dirt
                             : b.name == "minecraft:grass_block" ? grass
                             : b.name == "minecraft:sandstone"   ? sandstone
                                                                 : sand;
                blocks.setBlockNoUpdate({p.x, p.y, p.z}, *block);
            }, field->generationHeight()-65);
        }
}

bool groundPlant(std::string const &name) {
    return worldgen_rules::structureGroundPlant(name);
}
unsigned clearUnsupportedGroundPlants(BlockSource &blocks, ChunkPos cp, int lo, int hi) {
    // Validate after ALL terrain and structure writes. Tall plants have independent
    // lower/upper states: checking only the first block above a path misses orphan
    // uppers and plants whose former ground has become water or air.
    auto air = Block::tryGetFromRegistry("minecraft:air");
    return worldgen_rules::clearUnsupportedStructurePlants(
        {cp.x, cp.z}, lo, hi, [&](auto p) { return blocks.getBlock({p.x, p.y, p.z}).getTypeName(); },
        [&](auto p) {
            BlockPos q{p.x, p.y, p.z};
            return blocks.getBlock(q).canSurvive(blocks, q);
        },
        [&](auto p) { clearPlantBlock(blocks, {p.x, p.y, p.z}, *air); }, field->generationHeight()-65);
}
unsigned placeVillage(BlockSource &blocks, ChunkPos cp, VillagePlan &plan, uint seed) {
    villageTerrain(blocks, cp, plan);
    if (!plan.oasis.empty()) {
        auto water = Block::tryGetFromRegistry("minecraft:water"),
             air = Block::tryGetFromRegistry("minecraft:air"),
             soil = Block::tryGetFromRegistry("minecraft:dirt"),
             grass = Block::tryGetFromRegistry("minecraft:grass_block"),
             sand = Block::tryGetFromRegistry("minecraft:sand");
        for (auto const &[xz, c] : plan.oasis) {
            if (chunk(xz.first) != cp.x || chunk(xz.second) != cp.z)
                continue;
            int h = field->surface(xz.first + 32768, xz.second + 32768).height;
            for (int y = c.floor - 1; y <= std::min(field->generationHeight()-65, h + 2); ++y) {
                auto block = y < c.floor    ? soil
                             : y == c.floor ? (c.green ? grass : sand)
                             : y <= c.water ? water
                                            : air;
                blocks.setBlockNoUpdate({xz.first, y, xz.second}, *block);
            }
        }
    }

    // Vanilla terrain-matching roads consume the pre-world-generation map,
    // which the base plugin computed after vegetation. Rebind only this owner chunk
    // to deterministic bare terrain plus building pads (world Y, first air block).
    auto chunkPtr = blocks.getChunkAt({cp.x * 16, 0, cp.z * 16});
    if (!chunkPtr)
        throw std::runtime_error("Village owner chunk absent");
    auto heights = std::make_unique<std::vector<short>>(256);
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            int wx = cp.x * 16 + x, wz = cp.z * 16 + z;
            (*heights)[z * 16 + x] =
                short(villageGround(plan, wx, wz, field->surface(wx + 32768, wz + 32768).height) + 1);
        }
    chunkPtr->setPreWorldGenHeightMap(std::move(heights));
    BoundingBox clip{{cp.x * 16, -64, cp.z * 16}, {cp.x * 16 + 15, (field->generationHeight()-65), cp.z * 16 + 15}};
    unsigned count = 0;
    for (auto op :
         worldgen_rules::villagePlacementOrder(plan.sharedBlueprint, *plan.sharedLayout, {cp.x, cp.z})) {
        auto const &p = (*plan.start->pieces)[op.index];
        Random rng(op.seed, true);
        p->postProcess(blocks, rng, clip);
        ++count;
    }
    worldgen_rules::applyVillageRepairs(
        *plan.sharedLayout, {cp.x, cp.z},
        [&](worldgen_rules::StructurePoint p) { return blocks.getBlock({p.x, p.y, p.z}).getTypeName(); },
        [&](worldgen_rules::StructurePoint p, worldgen_rules::StructureBlock b) {
            blocks.setBlockNoUpdate({p.x, p.y, p.z}, *sharedBlock(b));
        });
    placeVillageDecor(blocks, cp, plan, seed);
    // Only planned original walls near this owner chunk are revisited. Vanilla
    // connection calculation sees newly placed adjacent chunks without replaying
    // templates, loot or any saved block type. No ordinary-chunk volume scan.
    for (auto const &pos : plan.wallPositions) {
        if (pos.x < clip.min.x - 1 || pos.x > clip.max.x + 1 || pos.z < clip.min.z - 1 ||
            pos.z > clip.max.z + 1)
            continue;
        auto const &block = blocks.getBlock(pos);
        if (block.getTypeName().ends_with("_wall"))
            static_cast<WallBlock const &>(block.getBlockType()).tryFixWallStates(blocks, pos, 2);
    }
    clearUnsupportedGroundPlants(blocks, cp, plan.diagnostics["groundMin"].get<int>() - 8,
                                 plan.diagnostics["groundMax"].get<int>() + 8);
    return count;
}

void inspectVillage(Dimension &d, VillagePlan const &plan, int canopyHeight = 3) {
    auto b = *plan.start->boundingBox;
    b.min.x -= 6;
    b.min.z -= 6;
    b.max.x += 6;
    b.max.z += 6;
    b.min.y -= 10;
    b.max.y = std::min((field->generationHeight()-65), b.max.y + canopyHeight);
    ensureLoaded(d, b);
    auto &blocks = d.getBlockSourceFromMainChunkSource();
    std::map<std::string, uint16_t> ids;
    J palette = J::array();
    std::ofstream raw(folder / "village-blocks.bin", std::ios::binary);
    unsigned unsupported = 0;
    for (int z = b.min.z; z <= b.max.z; ++z)
        for (int x = b.min.x; x <= b.max.x; ++x)
            for (int y = b.min.y; y <= b.max.y; ++y) {
                auto n = blocks.getBlock({x, y, z}).getTypeName();
                auto it = ids.find(n);
                if (it == ids.end()) {
                    uint16_t i = uint16_t(ids.size());
                    it = ids.emplace(n, i).first;
                    palette.push_back(n);
                }
                auto id = it->second;
                raw.write(reinterpret_cast<char *>(&id), 2);
            }
    J result = plan.diagnostics;
    result["sampleBox"] = boxJson(b);
    result["palette"] = palette;
    std::ofstream(folder / "village-inspection.json") << result.dump(2);
}

VegetationPair &villageVegetationPair(VillagePlan const &plan) {
    auto const &b = *plan.start->boundingBox;
    auto key = std::pair{b.min.x, b.min.z};
    auto it = vegetationPairs.find(key);
    if (it == vegetationPairs.end()) {
        if (vegetationPairs.size() >= 2)
            vegetationPairs.erase(vegetationPairs.begin());
        std::vector<std::array<int, 4>> reservations;
        for (auto const &p : *plan.start->pieces) {
            auto bb = *p->mBoundingBox;
            if (auto area = worldgen_rules::structurePlantReservation(sharedBox(bb)))
                reservations.push_back(*area);
        }
        VegetationPair pair;
        pair.normal =
            std::make_unique<terrain_kernel::Generator>(input, std::vector<std::array<int, 4>>{}, true, field.get());
        pair.reserved = std::make_unique<terrain_kernel::Generator>(input, reservations, true, field.get());
        it = vegetationPairs.emplace(key, std::move(pair)).first;
    }
    return it->second;
}
unsigned reserveVillageTrees(BlockSource &blocks, ChunkPos cp, VillagePlan const &plan,
                             ProtectedPlants const *keep = nullptr) {
    return clearReservedPlants(blocks, cp, villageVegetationPair(plan), keep);
}
bool locateVillage(Dimension &d, BlockPos const &from, BlockPos &result, bool fresh) {
    uint seed = structureSeed(d);
    std::vector<std::pair<int64_t, ChunkPos>> candidates;
    for (int z = structureInfinite() ? villageCell(from.z / 16) - 32
                                     : villageCell(-terrain_bounds::HalfChunks);
         z <= (structureInfinite() ? villageCell(from.z / 16) + 32
                                   : villageCell(terrain_bounds::HalfChunks - 1));
         ++z)
        for (int x = structureInfinite() ? villageCell(from.x / 16) - 32
                                         : villageCell(-terrain_bounds::HalfChunks);
             x <= (structureInfinite() ? villageCell(from.x / 16) + 32
                                       : villageCell(terrain_bounds::HalfChunks - 1));
             ++x) {
            auto cp = villageCandidate(x, z, seed);
            int64_t dx = int64_t(cp.x) * 16 - from.x, dz = int64_t(cp.z) * 16 - from.z;
            candidates.emplace_back(dx * dx + dz * dz, cp);
        }
    std::sort(candidates.begin(), candidates.end(),
              [](auto const &a, auto const &b) { return a.first < b.first; });
    for (auto const &[distance, cp] : candidates) {
        auto &p = villagePlan(d, cp, seed);
        if (!p.start)
            continue;
        auto b = *p.start->boundingBox;
        bool used = false;
        if (fresh)
            for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
                for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x)
                    used |= storedChunk(d, {x, z});
        if (used)
            continue;
        result = {(b.min.x + b.max.x) / 2, b.min.y, (b.min.z + b.max.z) / 2};
        return true;
    }
    return false;
}

void repairStructurePlants(Dimension &d, int x, int z, bool village, bool apply) {
    uint seed = structureSeed(d);
    ChunkPos anchor{chunk(x), chunk(z)};
    VillagePlan *vp = nullptr;
    AutoBlueprint *ip = nullptr;
    ProtectedPlants keep;
    BoundingBox b;
    if (village) {
        vp = &villagePlan(d, anchor, seed);
        if (!vp->start)
            throw std::runtime_error("Village absent");
        b = *vp->start->boundingBox;
        for (auto const &ptr : *vp->start->pieces) {
            auto &p = *static_cast<PoolElementStructurePiece *>(ptr.get());
            auto const &o = *p.mElement.mTemplate;
            if (!o)
                continue;
            auto const &v = *o->mStructureVariant;
            if (auto legacy = std::get_if<std::reference_wrapper<LegacyStructureTemplate>>(&v)) {
                if (p.mElement.getProjection() == Projection::Rigid)
                    protectTemplate(keep, legacy->get(), *p.mPosition, p.mRotation);
                else {
                    for (auto const &i : *legacy->get().mBlockInfo) {
                        auto const &name = i.mBlock.getTypeName();
                        if (name == "minecraft:air" || name == "minecraft:structure_void" ||
                            name == "minecraft:jigsaw")
                            continue;
                        auto q = LegacyStructureTemplate::transform(*i.mPos, Mirror::None, p.mRotation);
                        int xx = q.x + p.mPosition->x, zz = q.z + p.mPosition->z;
                        int ground =
                            villageGround(*vp, xx, zz, field->surface(xx + 32768, zz + 32768).height);
                        keep.insert({xx, ground + q.y, zz});
                    }
                }
            }
        }
    } else {
        ip = &autoBlueprint(d, anchor, seed);
        if (!ip->igloo)
            throw std::runtime_error("Igloo absent");
        b = ip->igloo->bounds;
        for (auto const &p : ip->igloo->parts)
            protectTemplate(keep, *p.model, p.pos, ip->igloo->rotation);
    }
    auto area = b;
    area.min.x -= 28;
    area.min.z -= 28;
    area.max.x += 28;
    area.max.z += 28;
    ensureLoaded(d, area);
    J changes = J::array();
    plantRepairChanges = &changes;
    plantRepairPreview = !apply;
    struct Reset {
        ~Reset() {
            plantRepairChanges = nullptr;
            plantRepairPreview = false;
        }
    } reset;
    auto &blocks = d.getBlockSourceFromMainChunkSource();
    auto begin = std::chrono::steady_clock::now();
    unsigned chunks = 0;
    for (int zz = chunk(area.min.z); zz <= chunk(area.max.z); ++zz)
        for (int xx = chunk(area.min.x); xx <= chunk(area.max.x); ++xx) {
            ChunkPos cp{xx, zz};
            auto n = vp ? reserveVillageTrees(blocks, cp, *vp, &keep) : reserveTrees(blocks, cp, *ip, &keep);
            if (vp)
                n += clearUnsupportedGroundPlants(blocks, cp, vp->diagnostics["groundMin"].get<int>() - 8,
                                                  vp->diagnostics["groundMax"].get<int>() + 8);
            if (n && apply) {
                auto c = d.getChunkSource().getExistingChunk(cp);
                c->setUnsaved();
                c->markSaveIfNeverSaved();
                c->markSaveNonActorDataIfDirty();
            }
            ++chunks;
        }
    // Saved-world migration only: old prototypes can contain vegetation that no
    // longer matches the current procedural model. Follow actual connected tree
    // blocks above house roofs, excluding exact native template voxels. This is
    // deliberately absent from the automatic generation hot path.
    if (vp) {
        auto tree = [](std::string const &n) {
            return n.ends_with("_wood") || n.ends_with("_log") || n.ends_with("_leaves");
        };
        std::set<std::array<int, 3>> seen, recorded;
        for (auto const &r : changes)
            recorded.insert({r[0].get<int>(), r[1].get<int>(), r[2].get<int>()});
        std::vector<BlockPos> pending;
        int top = std::min((field->generationHeight()-65), b.max.y + 64), bottom = std::max(-60, b.min.y - 8);
        auto enqueue = [&](int xx, int yy, int zz) {
            if (xx < area.min.x || xx > area.max.x || zz < area.min.z || zz > area.max.z || yy < bottom ||
                yy > top || keep.contains({xx, yy, zz}) || !seen.insert({xx, yy, zz}).second)
                return;
            BlockPos q{xx, yy, zz};
            if (tree(blocks.getBlock(q).getTypeName()))
                pending.push_back(q);
        };
        for (auto const &ptr : *vp->start->pieces) {
            auto &p = *static_cast<PoolElementStructurePiece *>(ptr.get());
            if (p.mElement.mLocation->find("houses/") == std::string::npos)
                continue;
            auto bb = *p.mBoundingBox;
            for (int zz = bb.min.z; zz <= bb.max.z; ++zz)
                for (int xx = bb.min.x; xx <= bb.max.x; ++xx)
                    for (int yy = bb.max.y + 1; yy <= top; ++yy)
                        enqueue(xx, yy, zz);
        }
        auto air = Block::tryGetFromRegistry("minecraft:air");
        std::set<std::pair<int, int>> dirty;
        for (size_t i = 0; i < pending.size(); ++i) {
            auto q = pending[i];
            if (!recorded.contains({q.x, q.y, q.z}))
                clearPlantBlock(blocks, q, *air);
            dirty.insert({chunk(q.x), chunk(q.z)});
            BlockPos up{q.x, q.y + 1, q.z};
            if (up.y < (field->generationHeight()-64) && !keep.contains({up.x, up.y, up.z}) &&
                blocks.getBlock(up).getTypeName() == "minecraft:snow_layer")
                clearPlantBlock(blocks, up, *air);
            for (auto step : {BlockPos{1, 0, 0}, BlockPos{-1, 0, 0}, BlockPos{0, 1, 0}, BlockPos{0, -1, 0},
                              BlockPos{0, 0, 1}, BlockPos{0, 0, -1}})
                enqueue(q.x + step.x, q.y + step.y, q.z + step.z);
        }
        if (apply)
            for (auto [xx, zz] : dirty) {
                auto c = d.getChunkSource().getExistingChunk({xx, zz});
                c->setUnsaved();
                c->markSaveIfNeverSaved();
                c->markSaveNonActorDataIfDirty();
            }
    }
    std::ofstream(folder / "plant-repair.json")
        << J{{"anchor", {x, z}},
             {"applied", apply},
             {"changes", changes},
             {"chunks", chunks},
             {"ms",
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()}}
               .dump(2);
}
