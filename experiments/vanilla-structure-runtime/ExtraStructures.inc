// Native structure compatibility probes. Unverified families stay out of generation.
std::map<int, std::unique_ptr<StructureFeature>> extraDelegates;
inline constexpr auto const &extraNames = worldgen_rules::ExtraNames;
#include "ModernStructures.inc"
#include "StructureProjection.inc"
#include "FrozenShip.inc"
IFeature &smallDefinition(Level &, std::string const &);
class CityEcologyTarget final : public WorldBlockTarget {
    BoundingBox clip;
    std::vector<BlockDataFetchResult<Block>> fetched;

  public:
    unsigned writes = 0;
    CityEcologyTarget(BlockSource &blocks, WorldGenContext const &context, BoundingBox b)
        : WorldBlockTarget(blocks, context), clip(b) {}
    bool canGetChunk() const override {
        return false;
    }
    LevelChunk *getChunk(ChunkPos const &) override {
        return nullptr;
    }
    Block const &getBlock(BlockPos const &p) const override {
        return mBlockSource.getBlock(p);
    }
    Block const &getBlockNoBoundsCheck(BlockPos const &p) const override {
        return mBlockSource.getBlock(p);
    }
    Block const &getExtraBlock(BlockPos const &p) const override {
        return mBlockSource.getBlock(p, 1);
    }
    gsl::span<BlockDataFetchResult<Block> const>
    fetchBlocksInBox(BoundingBox const &box, std::function<bool(Block const &)> predicate) override {
        fetched.clear();
        worldgen_rules::fetchCityEcologyBlocks(
            sharedBox(box), [&](auto p) { return &getBlock({p.x, p.y, p.z}); },
            [&](auto block) { return predicate(*block); },
            [&](auto block, auto p, uint32_t distance) {
                fetched.push_back({std::cref(*block), {p.x, p.y, p.z}, distance});
            });
        return {fetched.data(), fetched.size()};
    }
    bool allows(BlockPos const &p) const {
        return worldgen_rules::cityEcologyWriteAllowed(sharedBox(clip), {p.x, p.y, p.z}, false) &&
               mBlockSource.getBlock(p).getTypeName() != "minecraft:bedrock";
    }
    bool setBlock(BlockPos const &p, Block const &b, int flags) override {
        if (!allows(p))
            return false;
        bool done = WorldBlockTarget::setBlock(p, b, flags);
        writes += done;
        return done;
    }
    bool setBlockSimple(BlockPos const &p, Block const &b) override {
        if (!allows(p))
            return false;
        bool done = WorldBlockTarget::setBlockSimple(p, b);
        writes += done;
        return done;
    }
    short getHeightmap(int x, int z) override {
        return short(field->surface(x + 32768, z + 32768).height + 1);
    }
};
bool placeCityEcology(Dimension &d, BlockSource &blocks, BoundingBox const &clip, BlockPos pos, Random &rng,
                      std::string const &name) {
    // Native nodes can sit several blocks above the surrounding cave floor.
    // Find actual dry support; never manufacture a pedestal for a patch.
    auto supported = worldgen_rules::cityEcologySupport(
        worldgen_rules::StructurePoint{pos.x, pos.y, pos.z},
        [&](auto p) { return blocks.getBlock({p.x, p.y, p.z}).getTypeName() == "minecraft:air"; },
        [&](auto p) { return blocks.isSolidBlockingBlock({p.x, p.y, p.z}); });
    if (!supported)
        return false;
    pos = {supported->x, supported->y, supported->z};
    // A border patch must not depend on whether its neighbour's city fragment
    // already exists. Only read the live fragment; outside it use seeded rock.
    MineNeighbourSource localView(blocks, clip);
    auto *previousSource = seededReadSource;
    auto *previousClip = seededReadClip;
    seededReadSource = &localView;
    seededReadClip = &clip;
    struct ReadReset {
        BlockSource *source;
        BoundingBox const *clip;
        ~ReadReset() {
            seededReadSource = source;
            seededReadClip = clip;
        }
    } readReset{previousSource, previousClip};
    WorldGenContext worldContext{};
    worldContext.mPreliminarySurfaceProvider = &provider;
    CityEcologyTarget target(localView, worldContext, clip);
    RenderParams params;
    params.mLevel = &d.getLevel();
    params.mBlockSource = &localView;
    params.mBlockPlacementTarget = &target;
    FeaturePlacementFailureLog logger;
    FeaturePlacementFailureLogProxy proxy(logger);
    struct Context {
        IBlockWorldGenAPI &target;
        BlockPos pos;
        Random &random;
        RenderParams &params;
        FeaturePlacementFailureLogProxy logger;
    };
    static_assert(sizeof(Context) == sizeof(IFeature::PlacementContext));
    static_assert(offsetof(Context, logger) == offsetof(IFeature::PlacementContext, mLogger));
    Context storage{target, pos, rng, params, std::move(proxy)};
    bool placed = smallDefinition(d.getLevel(), name)
                      .place(reinterpret_cast<IFeature::PlacementContext const &>(storage))
                      .has_value();
    return target.writes > 0;
}
// Smooth world-space fields shared by cavern boundaries and deep-dark cover.
// Evaluate per column/surface, never run 3-D noise over the cavity volume.
double cityNoise(int x, int z, uint seed, double scale) {
    return worldgen_rules::cityNoise(x, z, seed, scale);
}
// Whole-city neighbour plans are seed-derived and cached before chunk carving.
// Never derive exclusions from the subset of candidates visible to one chunk.
std::map<std::tuple<int, int, uint>, std::vector<BoundingBox>> cityNeighbourCache;
std::vector<BoundingBox> const &cityNeighbours(Dimension &, StructureStart const &, uint);
using CityLink = worldgen_rules::CityLink;
std::map<std::tuple<int, int, uint>, std::vector<CityLink>> cityLinks;
std::vector<CityLink> const &cityConnections(StructureStart const &start, uint seed,
                                             std::vector<BoundingBox> const &neighbours) {
    auto key = std::tuple{int(start.chunkX), int(start.chunkZ), seed};
    if (auto it = cityLinks.find(key); it != cityLinks.end())
        return it->second;
    if (cityLinks.size() > 64)
        cityLinks.erase(cityLinks.begin());
    auto &links = cityLinks[key];
    std::vector<worldgen_rules::StructureBox> boxes;
    for (auto const &b : neighbours)
        boxes.push_back(sharedBox(b));
    auto planned = worldgen_rules::cityConnections(
        sharedBox(*start.boundingBox), {int(start.chunkX), int(start.chunkZ)}, seed, boxes,
        [](int x, int z) { return field->surface(x + 32768, z + 32768); },
        [](auto const &s, int x, int y, int z) { return field->material(s, x + 32768, y, z + 32768); });
    links = std::move(planned);
    return links;
}
using CityColumn = worldgen_rules::CityColumn;
using CityShape = worldgen_rules::CityShape;
std::vector<worldgen_rules::StructureBlueprintPiece> cityBlueprintPieces(StructureStart const &start) {
    std::vector<worldgen_rules::StructureBlueprintPiece> pieces;
    pieces.reserve(start.pieces->size());
    for (auto const &ptr : *start.pieces) {
        auto &p = *static_cast<PoolElementStructurePiece *>(ptr.get());
        worldgen_rules::StructureBlueprintPiece b;
        b.box = sharedBox(*p.mBoundingBox);
        b.kind = p.mElement.type() == StructurePoolElementType::Feature
                     ? worldgen_rules::StructurePieceKind::Feature
                     : worldgen_rules::StructurePieceKind::Template;
        auto q = *p.mPosition;
        b.origin = {q.x, q.y, q.z};
        b.location = *p.mElement.mLocation;
        pieces.push_back(std::move(b));
    }
    return pieces;
}
std::vector<CityShape> cityShapes(StructureStart const &start, BoundingBox const &clip) {
    return worldgen_rules::cityShapes(cityBlueprintPieces(start), sharedBox(clip));
}
CityColumn cityColumn(std::vector<CityShape> const &shapes, std::vector<CityLink> const &links,
                      std::vector<BoundingBox> const &neighbours, int x, int z, uint seed) {
    // Conversion belongs to the adapter; cache it per actual neighbour vector so
    // the 256 column loop does not allocate a new geometry array each time.
    static thread_local std::vector<BoundingBox> old;
    static thread_local std::vector<worldgen_rules::StructureBox> boxes;
    bool changed = old.size() != neighbours.size();
    if (!changed)
        for (size_t i = 0; i < old.size(); ++i)
            if (sharedBox(old[i]).min != sharedBox(neighbours[i]).min ||
                sharedBox(old[i]).max != sharedBox(neighbours[i]).max) {
                changed = true;
                break;
            }
    if (changed) {
        old = neighbours;
        boxes.clear();
        for (auto const &b : neighbours)
            boxes.push_back(sharedBox(b));
    }
    return worldgen_rules::cityColumn(shapes, links, boxes, x, z, seed);
}
std::unordered_map<Block const *, bool> cityNaturalBlockCache;
bool cityNaturalBlock(Block const &block) {
    // Only pre-existing natural terrain/ecology is removed. Cache by native
    // immutable state so the volume loop does not repeat string classification.
    auto &cache = cityNaturalBlockCache;
    if (auto it = cache.find(&block); it != cache.end())
        return it->second;
    auto const &n = block.getTypeName();
    bool value = worldgen_rules::cityNatural(n);
    if (cache.size() > 2048)
        cache.clear();
    cache.emplace(&block, value);
    return value;
}
// Platform translation only; traversal, masks, terrain decisions and ordering
// are shared with client ports. Cache classification by immutable native state.
struct NativeCityEnvironment {
    BlockSource &blocks;
    Block const *airBlock = Block::tryGetFromRegistry("minecraft:air");
    Block const *sculkBlock = Block::tryGetFromRegistry("minecraft:sculk");
    Block const *veinBlock = Block::tryGetFromRegistry("minecraft:sculk_vein");
    std::array<int, 6> masks = {MultifaceBlock::MULTIFACE_DOWN(),  MultifaceBlock::MULTIFACE_UP(),
                                MultifaceBlock::MULTIFACE_NORTH(), MultifaceBlock::MULTIFACE_SOUTH(),
                                MultifaceBlock::MULTIFACE_WEST(),  MultifaceBlock::MULTIFACE_EAST()};
    Block const *get(worldgen_rules::StructurePoint p) {
        return &blocks.getBlock({p.x, p.y, p.z});
    }
    std::string_view name(Block const *b) {
        return b->getTypeName();
    }
    bool natural(Block const *b) {
        return cityNaturalBlock(*b);
    }
    bool solid(Block const *b) {
        return b->isSolidBlockingBlock();
    }
    bool isAir(Block const *b) {
        return b->isAir();
    }
    bool survives(Block const *b, worldgen_rules::StructurePoint p) {
        return b->canSurvive(blocks, {p.x, p.y, p.z});
    }
    int veinBits(Block const *b) {
        return b->getState<int>(VanillaStates::MultiFaceDirectionBits()).value_or(0);
    }
    int faceMask(size_t i) {
        return masks[i];
    }
    void air(worldgen_rules::StructurePoint p) {
        blocks.setBlockNoUpdate({p.x, p.y, p.z}, *airBlock);
    }
    void sculk(worldgen_rules::StructurePoint p) {
        blocks.setBlockNoUpdate({p.x, p.y, p.z}, *sculkBlock);
    }
    void vein(worldgen_rules::StructurePoint p, Block const *b, int bits) {
        auto state = b->setState(VanillaStates::MultiFaceDirectionBits(), bits);
        if (state)
            blocks.setBlockNoUpdate({p.x, p.y, p.z}, *state);
    }
    void fringe(worldgen_rules::StructurePoint p, int bits) {
        vein(p, veinBlock, bits);
    }
};
std::vector<worldgen_rules::StructureBox> sharedCityNeighbours(std::vector<BoundingBox> const &native) {
    std::vector<worldgen_rules::StructureBox> out;
    out.reserve(native.size());
    for (auto const &b : native)
        out.push_back(sharedBox(b));
    return out;
}
unsigned prepareCityCavern(BlockSource &blocks, StructureStart const &start, BoundingBox const &clip,
                           uint seed, std::vector<BoundingBox> const &neighbours) {
    auto pieces = cityBlueprintPieces(start);
    auto boxes = sharedCityNeighbours(neighbours);
    auto const &links = cityConnections(start, seed, neighbours);
    NativeCityEnvironment env{blocks};
    return worldgen_rules::carveCityEnvironment(pieces, links, boxes, sharedBox(clip), seed, env);
}
void citySurfaceEcology(BlockSource &blocks, BoundingBox const &clip, StructureStart const &start, uint seed,
                        std::vector<BoundingBox> const &neighbours) {
    auto pieces = cityBlueprintPieces(start);
    auto boxes = sharedCityNeighbours(neighbours);
    auto const &links = cityConnections(start, seed, neighbours);
    NativeCityEnvironment env{blocks};
    worldgen_rules::finishCityEnvironment(
        sharedBox(*start.boundingBox), pieces, links, boxes, sharedBox(clip), seed,
        [](int x, int z) { return field->surface(x + 32768, z + 32768); },
        [](auto const &s, int x, int y, int z) { return field->material(s, x + 32768, y, z + 32768); }, env);
}

// No dark-forest habitat exists in the current terrain catalog.
constexpr bool mansionHabitatAvailable = worldgen_rules::MansionHabitatAvailable;
bool mansionTemplatesReady = false;
void warmMansionTemplates(Level &level) {
    if (!mansionHabitatAvailable || mansionTemplatesReady ||
        level.getLevelData().getLevelName() != activeWorld)
        return;
    std::lock_guard guard(stageMutex);
    if (mansionTemplatesReady)
        return;
    auto began = std::chrono::steady_clock::now();
    // This version's archive index contains fixed 256-byte filename records.
    // Loading on the level thread lets native legacy-NBT upgrades finish before
    // a generation worker selects a previously unused mansion room.
    std::ifstream f(vanillaFolder() / "__brarchive/structures/mansion.brarchive", std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    uint32_t count = 0;
    if (bytes.size() < 16)
        throw std::runtime_error("Mansion archive index unavailable");
    std::memcpy(&count, bytes.data() + 8, 4);
    if (count == 0 || count > 256 || bytes.size() != 16 + size_t(count) * 256)
        throw std::runtime_error("Mansion archive index version mismatch");
    auto manager = level.getStructureManager();
    J names = J::array();
    for (uint32_t i = 0; i < count; ++i) {
        auto offset = 16 + size_t(i) * 256;
        auto len = uint8_t(bytes[offset]);
        if (len == 0 || len >= 128)
            throw std::runtime_error("Mansion archive name invalid");
        std::string name(bytes.data() + offset + 1, len);
        if (!name.ends_with(".nbt") ||
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
                std::string::npos)
            throw std::runtime_error("Mansion archive entry invalid");
        name = "mansion/" + name.substr(0, name.size() - 4);
        auto size = manager->getOrCreateLegacy(name).rawSize();
        if (size.x <= 0 || size.y <= 0 || size.z <= 0)
            throw std::runtime_error("Mansion native template missing: " + name);
        names.push_back(name);
    }
    mansionTemplatesReady = true;
    std::ofstream(folder / "mansion-template-warmup.json")
        << J{{"world", activeWorld},
             {"count", count},
             {"names", names},
             {"ms",
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count()},
             {"terrainChunksGenerated", 0}}
               .dump(2);
}
// Keep native mansion room layouts/templates while binding the origin to our
// immutable terrain field, without asking the vanilla start to preview terrain.
class FrozenMansionStart final : public StructureStart {
  public:
    FrozenMansionStart(Dimension &d, Random &random, ChunkPos cp) {
        chunkX = cp.x;
        chunkZ = cp.z;
        maxYSpawnOffset = 0;
        auto rotation = static_cast<Rotation>(random.nextInt(4));
        BlockPos pos{cp.x * 16 + 8, field->surface(cp.x * 16 + 32776, cp.z * 16 + 32776).height + 1,
                     cp.z * 16 + 8};
        WoodlandMansionPieces::generateMansion(d.getLevel().getStructureManager(), pos, rotation, *pieces,
                                               random);
        calculateBoundingBox();
    }
    std::string_view getStructureName() const override {
        return "minecraft:mansion";
    }
};
StructureFeature &extraDelegate(Dimension &d, int type) {
    auto it = extraDelegates.find(type);
    if (it != extraDelegates.end())
        return *it->second;
    uint seed = structureSeed(d);
    auto &v = d.getLevel().getLevelData().getBaseGameVersion();
    std::unique_ptr<StructureFeature> f;
    switch (type) {
    case 0:
        f = std::make_unique<PillagerOutpostFeature>(seed);
        break;
    case 1:
        f = std::make_unique<RuinedPortalFeature>(seed, false);
        break;
    case 2:
        f = std::make_unique<ShipwreckFeature>(static_cast<OceanMonumentFeature &>(extraDelegate(d, 5)), seed,
                                               v);
        break;
    case 3:
        f = std::make_unique<OceanRuinFeature>(static_cast<OceanMonumentFeature &>(extraDelegate(d, 5)), seed,
                                               v);
        break;
    case 4:
        f = std::make_unique<BuriedTreasureFeature>(seed);
        break;
    case 5:
        f = std::make_unique<OceanMonumentFeature>(seed);
        break;
    case 6:
        f = std::make_unique<StrongholdFeature>(
            auditVillageDelegate ? auditVillageDelegate : &registeredVillage(d), seed);
        break;
    case 7:
        f = std::make_unique<WoodlandMansionFeature>(seed);
        break;
    case 8:
        f = std::make_unique<AncientCityFeature>(seed, v);
        break;
    default:
        throw std::runtime_error("Unknown native feature family");
    }
    auto *ptr = f.get();
    extraDelegates.emplace(type, std::move(f));
    return *ptr;
}
std::unique_ptr<StructureStart> createExtra(Dimension &d, int type, ChunkPos cp) {
    initialize();
    auto s = field->surface(cp.x * 16 + 32768, cp.z * 16 + 32768);
    static std::vector<std::string> biomeNames;
    if (biomeNames.empty()) {
        J metadata;
        std::ifstream((terrainFolder() / "terrain.json")) >> metadata;
        biomeNames = metadata["biomeNames"].get<std::vector<std::string>>();
    }
    auto const &name = biomeNames.at(s.biome);
    auto b = d.getBiomeRegistry().lookupByName("minecraft:" + name);
    if (!b)
        throw std::runtime_error("Missing native biome");
    FixedBiomeSource bs(*b);
    uint seed = structureSeed(d);
    Random rng(seed, true);
    StructureFeature::setRandomSeedFor(rng, cp.x, cp.z, 165745296 + type, seed);
    std::map<std::pair<int, int>, std::vector<short>> heights;
    planningHeights = &heights;
    adaptHeightMap = true;
    struct Reset {
        ~Reset() {
            adaptHeightMap = false;
            planningHeights = nullptr;
        }
    } reset;
    if (type >= 10)
        return createModern(d, type, cp);
    if (type == 2)
        return std::make_unique<FrozenShipStart>(
            d, rng, cp, seed,
            worldgen_rules::sharedStructurePlan(
                input.at("config").value("sharedRules", J::object()).value("revision", std::string{})));
    if (type == 7)
        return std::make_unique<FrozenMansionStart>(d, rng, cp);
    if (type == 9)
        return std::make_unique<MineshaftStart>(d, bs, rng, cp, provider);
    return extraDelegate(d, type).createStructureStart(d, bs, rng, cp, provider);
}
void extraProbe(Dimension &d, int type, int x, int z) {
    if (type < 0 || type >= int(extraNames.size()))
        throw std::runtime_error("family must be 0..11");
    auto begin = std::chrono::steady_clock::now();
    auto start = createExtra(d, type, {chunk(x), chunk(z)});
    J j = {{"family", extraNames[type]}, {"anchor", {x, z}}, {"valid", bool(start) && start->isValid()}};
    if (start && type == 0) {
        j["details"] = J::array();
        for (auto const &ptr : *start->pieces) {
            auto &p = *static_cast<PillagerOutpostPieces::PillagerOutpostPiece *>(ptr.get());
            j["details"].push_back(
                {{"template", *p.mTemplateName},
                 {"pos", {p.mPosition->x, p.mPosition->y, p.mPosition->z}},
                 {"templatePos", {p.mTemplatePosition->x, p.mTemplatePosition->y, p.mTemplatePosition->z}},
                 {"heightSet", bool(p.mHeightSet)},
                 {"box", boxJson(*p.mBoundingBox)}});
        }
    }
    if (start) {
        j["box"] = boxJson(*start->boundingBox);
        j["pieces"] = start->pieces->size();
        j["name"] = start->getStructureName();
    }
    j["ms"] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    std::ofstream(folder / "extra-probe.json") << j.dump(2);
}

// Enabled: underground blueprints, height-frozen outposts and seabed ruins.
// Remaining families stay as probes until their projections are adapted.
struct ExtraPlan {
    StructureStart *start = nullptr;
    J diagnostics;
    VillagePlan terrain;
    int floor = -32768;
    std::set<std::array<int, 3>> wallPositions;
    bool portalReady = false;
    std::map<std::array<int, 3>, std::pair<Block const *, Block const *>> portalWrites;
    std::map<std::array<int, 3>, std::pair<std::string, int>> portalChests;
    std::shared_ptr<worldgen_rules::FrozenRuinProgram const> ruins;
    std::shared_ptr<worldgen_rules::FrozenCaptureProgram const> portal;
};
void placeFrozenPortal(Dimension &, BlockSource &, ChunkPos, ExtraPlan &, ChunkPos, uint);
std::map<std::tuple<int, int, int>, ExtraPlan> extraPlans;
using ExtraSpec = worldgen_rules::ExtraSpec;
inline constexpr auto const &extraSpecs = worldgen_rules::ExtraSpecs;
bool extraSurface(int t) {
    return worldgen_rules::extraSurface(t);
}
bool extraMarine(int t) {
    return worldgen_rules::extraMarine(t);
}
ExtraSpec const *extraSpec(int type) {
    return worldgen_rules::extraSpec(type);
}
int spacingCell(int v, int spacing) {
    return int(std::floor(double(v) / spacing));
}
ChunkPos extraCandidate(ExtraSpec const &s, int x, int z, uint seed) {
    auto c = worldgen_rules::candidateForCell(x, z, seed, s.spacing, s.separation, s.salt);
    return {c.x, c.z};
}
ExtraPlan &extraPlan(Dimension &, int, ChunkPos);
class TerrainExtraFeature final : public StructureFeature {
  public:
    int type;
    TerrainExtraFeature(uint seed, int t)
        : StructureFeature(seed, HashedString(std::string("minecraft:") + extraNames[t])), type(t) {}
    bool isFeatureChunk(BiomeSource const &, Random &, ChunkPos const &, uint,
                        IPreliminarySurfaceProvider const &, Dimension const &) override {
        return false;
    }
    std::unique_ptr<StructureStart> createStructureStart(Dimension &, BiomeSource const &, Random &,
                                                         ChunkPos const &,
                                                         IPreliminarySurfaceProvider const &) override {
        return {};
    }
    bool getNearestGeneratedFeature(Dimension &d, BiomeSource const &, BlockPos const &from, BlockPos &result,
                                    IPreliminarySurfaceProvider const &, bool fresh,
                                    std::optional<HashedString> const &) override {
        if (type == 7 && !mansionHabitatAvailable)
            return false;
        auto &s = *extraSpec(type);
        uint seed = structureSeed(d);
        std::vector<std::pair<int64_t, ChunkPos>> candidates;
        if (worldgen_rules::sharedStructurePlan(
                input.at("config").value("sharedRules", J::object()).value("revision", std::string{}))) {
            auto x = spacingCell(from.x / 16, s.spacing), z = spacingCell(from.z / 16, s.spacing);
            auto ordered = worldgen_rules::orderedExtraCandidates(
                s, from.x, from.z, seed,
                structureInfinite() ? x - 32 : spacingCell(-terrain_bounds::HalfChunks, s.spacing),
                structureInfinite() ? x + 32 : spacingCell(terrain_bounds::HalfChunks - 1, s.spacing),
                structureInfinite() ? z - 32 : spacingCell(-terrain_bounds::HalfChunks, s.spacing),
                structureInfinite() ? z + 32 : spacingCell(terrain_bounds::HalfChunks - 1, s.spacing));
            for (auto const &c : ordered)
                candidates.push_back({c.distance, {c.anchor.x, c.anchor.z}});
        } else {
            for (int z = structureInfinite() ? spacingCell(from.z / 16, s.spacing) - 32
                                             : spacingCell(-terrain_bounds::HalfChunks, s.spacing);
                 z <= (structureInfinite() ? spacingCell(from.z / 16, s.spacing) + 32
                                           : spacingCell(terrain_bounds::HalfChunks - 1, s.spacing));
                 ++z)
                for (int x = structureInfinite() ? spacingCell(from.x / 16, s.spacing) - 32
                                                 : spacingCell(-terrain_bounds::HalfChunks, s.spacing);
                     x <= (structureInfinite() ? spacingCell(from.x / 16, s.spacing) + 32
                                               : spacingCell(terrain_bounds::HalfChunks - 1, s.spacing));
                     ++x) {
                    auto c = extraCandidate(s, x, z, seed);
                    int64_t dx = c.x * 16LL - from.x, dz = c.z * 16LL - from.z;
                    candidates.push_back({dx * dx + dz * dz, c});
                }
            std::sort(candidates.begin(), candidates.end(),
                      [](auto const &a, auto const &b) { return a.first < b.first; });
        }
        for (auto const &candidate : candidates) {
            auto &p = extraPlan(d, type, candidate.second);
            if (!p.start)
                continue;
            auto b = *p.start->boundingBox;
            bool used = false;
            if (fresh)
                for (int z = chunk(b.min.z); z <= chunk(b.max.z); ++z)
                    for (int x = chunk(b.min.x); x <= chunk(b.max.x); ++x)
                        used |= storedChunk(d, {x, z});
            if (!used) {
                result = {(b.min.x + b.max.x) / 2, b.min.y, (b.min.z + b.max.z) / 2};
                return true;
            }
        }
        return false;
    }
};
std::map<int, TerrainExtraFeature *> extraFeatureRecords;
TerrainExtraFeature &registeredExtra(Dimension &d, int type) {
    if (auto it = extraFeatureRecords.find(type); it != extraFeatureRecords.end())
        return *it->second;
    auto &entries = *d.getWorldGenerator()->mStructureFeatureRegistry->mStructureFeatures;
    auto f = std::make_unique<TerrainExtraFeature>(structureSeed(d), type);
    auto *ptr = f.get();
    extraFeatureRecords.emplace(type, ptr);
    ownedNativeFeatures.insert(ptr);
    entries.push_back(std::move(f));
    return *ptr;
}
bool fitSharedExtra(StructureStart &start, ExtraPlan &result, int type, ChunkPos cp, uint seed) {
    worldgen_rules::ExtraBlueprint blueprint{type, {cp.x, cp.z}, sharedBox(*start.boundingBox), {}};
    // Modern structures have native StructureSections, not legacy pieces. An
    // empty legacy vector is normal and must not make their shared plan invalid.
    if (type >= 10)
        for (auto const &section : *static_cast<ModernStart &>(start).instance) {
            worldgen_rules::StructureBlueprintPiece p;
            p.box = sharedBox(section->boundingBox());
            p.origin = p.box.min;
            blueprint.pieces.push_back(std::move(p));
        }
    for (auto const &ptr : *start.pieces) {
        worldgen_rules::StructureBlueprintPiece p;
        p.box = sharedBox(*ptr->mBoundingBox);
        p.origin = p.box.min;
        if (type == 0) {
            auto &part = *static_cast<PillagerOutpostPieces::PillagerOutpostPiece *>(ptr.get());
            p.origin = {part.mPosition->x, part.mPosition->y, part.mPosition->z};
        }
        if (type == 3) {
            auto &part = *static_cast<OceanRuinPieces::OceanRuinPiece *>(ptr.get());
            p.origin = {part.mPosition->x, part.mPosition->y, part.mPosition->z};
        }
        blueprint.pieces.push_back(std::move(p));
    }
    auto plan = worldgen_rules::planExtraLayout(
        blueprint, [](int x, int z) { return field->surface(x + 32768, z + 32768); }, field->generationHeight()-65);
    if (!plan.accepted)
        return false;
    result.floor = plan.floor;
    result.terrain.fittedGround = std::move(plan.fittedGround);
    for (auto const &p : plan.pads)
        result.terrain.pads.push_back({nativeBox(p.box), p.height});
    for (size_t i = 0; i < start.pieces->size(); ++i) {
        auto &piece = *(*start.pieces)[i];
        int dy = plan.verticalMoves[i];
        if (type == 0) {
            auto &p = static_cast<PillagerOutpostPieces::PillagerOutpostPiece &>(piece);
            p.mPosition->y += dy;
            p.mTemplatePosition->y += dy;
            p.mHeightSet = true;
            p._setBoundingBoxFromTemplate();
        } else if (type == 3) {
            auto &p = static_cast<OceanRuinPieces::OceanRuinPiece &>(piece);
            p.mPosition->y += dy;
            p.mTemplatePosition->y += dy;
            p._setBoundingBoxFromTemplate();
        } else if (type == 4)
            *piece.mBoundingBox = nativeBox(plan.pieceBounds[i]);
        else if (dy) {
            piece.moveBoundingBox(0, dy, 0);
            if (type == 5)
                for (auto &child : *static_cast<MonumentBuilding &>(piece).mChildPieces)
                    child->moveBoundingBox(0, dy, 0);
        }
    }
    *start.boundingBox = nativeBox(plan.bounds);
    if (type == 2)
        for (auto const &piece : *start.pieces)
            static_cast<FrozenShipPiece &>(*piece).syncSharedGeometry();
    if (type == 3) {
        std::vector<worldgen_rules::FrozenRuinPart> parts;
        for (auto const &ptr : *start.pieces) {
            auto const &p = *static_cast<OceanRuinPieces::OceanRuinPiece const *>(ptr.get());
            parts.push_back({*p.mTemplateName,
                             {p.mPosition->x, p.mPosition->y, p.mPosition->z},
                             sharedBox(*p.mBoundingBox),
                             int(p.mRotation),
                             nativeLegacyDataMarkers(*p.mTemplate)});
        }
        result.ruins = std::make_shared<worldgen_rules::FrozenRuinProgram>(
            worldgen_rules::compileFrozenRuins({cp.x, cp.z}, seed, std::move(parts)));
    }
    return true;
}
ExtraPlan &extraPlan(Dimension &d, int type, ChunkPos cp) {
    if (type == 7 && (!mansionHabitatAvailable || !mansionTemplatesReady)) {
        static ExtraPlan pending;
        return pending;
    }
    auto key = std::tuple{type, cp.x, cp.z};
    if (auto it = extraPlans.find(key); it != extraPlans.end())
        return it->second;
    auto &r = extraPlans[key];
    auto *s = extraSpec(type);
    if (!s)
        return r;
    int wx = cp.x * 16, wz = cp.z * 16;
    if (!structureBoxInside(wx - s->radius * 16, wz - s->radius * 16, wx + s->radius * 16,
                            wz + s->radius * 16))
        return r;
    auto surf = field->surface(wx + 32768, wz + 32768);
    if (!structureIncluded(extraNames[type]) ||
        !worldgen_rules::extraAnchorAllowed(type, surf, mansionHabitatAvailable))
        return r;
    if (type == 0) {
        // Do not let outposts occupy village sites, independent of load order.
        uint seed = structureSeed(d);
        for (int z = villageCell(cp.z - 20); z <= villageCell(cp.z + 20); ++z)
            for (int x = villageCell(cp.x - 20); x <= villageCell(cp.x + 20); ++x) {
                auto c = villageCandidate(x, z, seed);
                if (std::abs(c.x - cp.x) <= 20 && std::abs(c.z - cp.z) <= 20 && villagePlan(d, c, seed).start)
                    return r;
            }
    }
    auto began = std::chrono::steady_clock::now();
    auto start = createExtra(d, type, cp);
    if (!start || !start->isValid())
        return r;
    bool unified = worldgen_rules::sharedStructurePlan(
        input.at("config").value("sharedRules", J::object()).value("revision", std::string{}));
    if (unified) {
        if (!fitSharedExtra(*start, r, type, cp, structureSeed(d)))
            return r;
    } else {
        if (type == 1 || type == 2 || type == 4 || type == 5 || type == 7 || type == 10) {
            auto b = *start->boundingBox;
            int lo = INT_MAX, hi = -64;
            for (int z = b.min.z; z <= b.max.z; z += 3)
                for (int x = b.min.x; x <= b.max.x; x += 3) {
                    auto t = field->surface(x + 32768, z + 32768);
                    if (!worldgen_rules::extraFootprintAllowed(type, t))
                        return r;
                    lo = std::min(lo, t.height);
                    hi = std::max(hi, t.height);
                    if (hi - lo > worldgen_rules::extraMaxSlope(type))
                        return r;
                }
            if (type == 5 && hi > 6)
                return r;
            r.floor = (lo + hi) / 2 + 1;
            if (type == 1) {
                b.min.y = r.floor - 4;
                b.max.y = r.floor + 35;
                *start->boundingBox = b;
            } else if (type == 4) {
                b.min.y = lo - 4;
                b.max.y = hi;
                *start->boundingBox = b;
                for (auto &ptr : *start->pieces)
                    *ptr->mBoundingBox = b;
            } else if (type == 2) {
                int dy = r.floor - b.min.y;
                start->moveBoundingBoxes(dy);
            } else if (type == 5) {
                int dy = (hi + 1) - b.min.y;
                start->moveBoundingBoxes(dy);
                for (auto &ptr : *start->pieces)
                    for (auto &child : *static_cast<MonumentBuilding *>(ptr.get())->mChildPieces)
                        child->moveBoundingBox(0, dy, 0);
            } else if (type == 7) {
                int dy = r.floor - b.min.y;
                start->moveBoundingBoxes(dy);
            }
            if (extraSurface(type) && type != 10 && type != 1 && type != 7)
                r.terrain.pads.push_back({*start->boundingBox, r.floor - 1});
            if (type == 7)
                for (auto const &ptr : *start->pieces) {
                    auto &part = *static_cast<WoodlandMansionPieces::WoodlandMansionPiece *>(ptr.get());
                    for (auto const &info : *part.mTemplate->mBlockInfo) {
                        auto const &n = info.mBlock.getTypeName();
                        if (info.mBlock.isAir() || n == "minecraft:structure_void" ||
                            n == "minecraft:structure_block")
                            continue;
                        auto q = LegacyStructureTemplate::transform(*info.mPos, part.mSettings->getMirror(),
                                                                    part.mSettings->getRotation());
                        q.x += part.mTemplatePosition->x;
                        q.y += part.mTemplatePosition->y;
                        q.z += part.mTemplatePosition->z;
                        if (q.y < r.floor - 1 || q.y > r.floor + 1)
                            continue;
                        auto key = std::pair{q.x, q.z};
                        auto it = r.terrain.fittedGround.find(key);
                        if (it == r.terrain.fittedGround.end())
                            r.terrain.fittedGround[key] = q.y - 1;
                        else
                            it->second = std::min(it->second, q.y - 1);
                    }
                }
        }
        if (!unified && type == 0) {
            for (auto const &ptr : *start->pieces) {
                auto &p = *static_cast<PillagerOutpostPieces::PillagerOutpostPiece *>(ptr.get());
                auto b = *p.mBoundingBox;
                int lo = INT_MAX, hi = -64;
                for (int z = b.min.z; z <= b.max.z; ++z)
                    for (int x = b.min.x; x <= b.max.x; ++x) {
                        auto t = field->surface(x + 32768, z + 32768);
                        if (t.water >= t.height || t.river || t.lake || t.volcanic > .01 ||
                            t.iceHabitat > .01 || villageStyle(t.biome) < 0)
                            return r;
                        lo = std::min(lo, t.height);
                        hi = std::max(hi, t.height);
                    }
                if (hi - lo > 4)
                    return r;
                // Field heights identify the top solid soil block. Native template
                // local Y=0 is the first structure layer ABOVE it, including cage fences.
                int ground = (lo + hi) / 2;
                p.mPosition->y = ground + 1;
                p.mTemplatePosition->y = ground + 1;
                p.mHeightSet = true;
                p._setBoundingBoxFromTemplate();
                r.terrain.pads.push_back({*p.mBoundingBox, ground});
            }
            start->calculateBoundingBox();
        }
        if (!unified && type == 3) {
            // Freeze native ruins against immutable seabed, not the chunk-order-dependent
            // live heightmap. Overlapping cold-ruin layers share the same ground base.
            for (auto const &ptr : *start->pieces) {
                auto &p = *static_cast<OceanRuinPieces::OceanRuinPiece *>(ptr.get());
                auto b = *p.mBoundingBox;
                int lo = INT_MAX, hi = -64;
                for (int z = b.min.z; z <= b.max.z; ++z)
                    for (int x = b.min.x; x <= b.max.x; ++x) {
                        auto t = field->surface(x + 32768, z + 32768);
                        if (t.ocean < .5 || t.water < 30 || t.height > 25 || t.height < -40 || t.lake ||
                            t.river || t.iceHabitat > .01)
                            return r;
                        lo = std::min(lo, t.height);
                        hi = std::max(hi, t.height);
                    }
                if (hi - lo > 3)
                    return r;
                p.mPosition->y = lo;
                p.mTemplatePosition->y = lo;
                p._setBoundingBoxFromTemplate();
            }
            start->calculateBoundingBox();
        }
        auto b = *start->boundingBox;
        bool underground = worldgen_rules::extraUnderground(type);
        if (b.min.y < -58 || b.max.y > (field->generationHeight()-74) || (underground && b.max.y > surf.height - 10) ||
            b.min.x < wx - s->radius * 16 || b.max.x > wx + s->radius * 16 || b.min.z < wz - s->radius * 16 ||
            b.max.z > wz + s->radius * 16)
            return r;
        for (int z = b.min.z; z <= b.max.z; z += 16)
            for (int x = b.min.x; x <= b.max.x; x += 16) {
                auto t = field->surface(x + 32768, z + 32768);
                if ((underground && t.height < b.max.y + 8) ||
                    (!extraMarine(type) && type != 4 && t.ocean > .1) || t.volcanic > .01)
                    return r;
            }
    } // legacy saved recipes
    auto b = *start->boundingBox;
    if (extraSurface(type)) {
        constexpr int reach = worldgen_rules::VillageReach + 28;
        uint seed = structureSeed(d);
        for (int z = villageCell(chunk(b.min.z - reach)); z <= villageCell(chunk(b.max.z + reach)); ++z)
            for (int x = villageCell(chunk(b.min.x - reach)); x <= villageCell(chunk(b.max.x + reach));
                 ++x) {
                auto c = villageCandidate(x, z, seed);
                auto &v = villagePlan(d, c, seed);
                if (v.start &&
                    worldgen_rules::villageSurfaceConflict(sharedBox(b), sharedBox(*v.start->boundingBox)))
                    return r;
            }
    }
    r.diagnostics = {
        {"family", extraNames[type]},
        {"anchor", {wx, wz}},
        {"box", boxJson(b)},
        {"pieces", start->pieces->size()},
        {"placementDistribution", "seeded terrain-adapted spacing; not identical vanilla frequency"},
        {"planMs",
         std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count()}};
    if (type == 0) {
        r.diagnostics["groundDatum"] = "template base = soil top + 1";
        r.diagnostics["templates"] = J::array();
        for (auto const &ptr : *start->pieces) {
            auto &part = *static_cast<PillagerOutpostPieces::PillagerOutpostPiece *>(ptr.get());
            J bottom = J::object();
            for (auto const &info : *part.mTemplate->mBlockInfo)
                if (info.mPos->y == 0) {
                    auto const &name = info.mBlock.getTypeName();
                    bottom[name] = bottom.value(name, 0) + 1;
                }
            r.diagnostics["templates"].push_back(
                {{"name", *part.mTemplateName},
                 {"origin",
                  {part.mTemplatePosition->x, part.mTemplatePosition->y, part.mTemplatePosition->z}},
                 {"box", boxJson(*part.mBoundingBox)},
                 {"bottomBlocks", bottom}});
        }
    }
    if (type == 3) {
        r.diagnostics["templates"] = J::array();
        for (auto const &ptr : *start->pieces) {
            auto &p = *static_cast<OceanRuinPieces::OceanRuinPiece *>(ptr.get());
            J markers = J::array();
            for (auto const &[pos, label] : (*p.mTemplate).getMarkers(*p.mTemplatePosition, *p.mSettings))
                markers.push_back({{"label", label}, {"pos", {pos.x, pos.y, pos.z}}});
            r.diagnostics["templates"].push_back(
                {{"name", *p.mTemplateName}, {"integrity", float(p.mIntegrity)}, {"markers", markers}});
        }
    }
    J checkpoint = {{"floor", r.floor}};
    if (r.diagnostics.contains("templates"))
        checkpoint["templates"] = r.diagnostics.at("templates");
    checkpointStructure(*start, std::string(extraNames[type]), std::move(checkpoint));
    r.start = start.get();
    if (auditPlanningStarts) {
        auditPlanningStarts->push_back(std::move(start));
        return r;
    }
    auto &record = registeredExtra(d, type);
    std::unique_lock lock(*record.mCacheMutex);
    if (record.mCachedStructures->contains(cp)) {
        r.start = nullptr;
        throw std::runtime_error("Native structure cache owner collision");
    }
    record.mCachedStructures->emplace(cp, std::move(start));
    return r;
}
std::vector<std::pair<int, ChunkPos>> nearbyExtra(ChunkPos cp, uint seed) {
    std::vector<std::pair<int, ChunkPos>> out;
    worldgen_rules::forNearbyExtraCandidate(
        {cp.x, cp.z}, seed, [](int) { return true; },
        [&](auto const &s, auto c) { out.push_back({s.type, {c.x, c.z}}); });
    return out;
}
std::vector<BoundingBox> const &cityNeighbours(Dimension &d, StructureStart const &start, uint seed) {
    auto &cache = cityNeighbourCache;
    auto key = std::tuple{int(start.chunkX), int(start.chunkZ), seed};
    if (auto it = cache.find(key); it != cache.end())
        return it->second;
    if (cache.size() > 64)
        cache.erase(cache.begin());
    auto &out = cache[key];
    auto city = *start.boundingBox;
    for (auto const &candidate : worldgen_rules::cityNeighbourCandidates(sharedBox(city), seed)) {
        auto &q = extraPlan(d, candidate.type, {candidate.anchor.x, candidate.anchor.z});
        if (!q.start)
            continue;
        auto b = *q.start->boundingBox;
        if (worldgen_rules::cityNeighbourIncluded(sharedBox(city), sharedBox(b)))
            out.push_back(b);
    }
    return out;
}
VegetationPair &extraVegetationPair(ExtraPlan const &p) {
    auto b = *p.start->boundingBox;
    auto key = std::pair{b.min.x, b.min.z};
    auto it = vegetationPairs.find(key);
    if (it == vegetationPairs.end()) {
        if (vegetationPairs.size() >= 2)
            vegetationPairs.erase(vegetationPairs.begin());
        VegetationPair pair;
        pair.normal =
            std::make_unique<nativegen::Engine>(input, std::vector<std::array<int, 4>>{}, true, field.get());
        std::vector<std::array<int, 4>> occupied;
        for (auto const &piece : *p.start->pieces) {
            auto bb = *piece->mBoundingBox;
            if (auto area = worldgen_rules::structurePlantReservation(sharedBox(bb)))
                occupied.push_back(*area);
        }
        pair.reserved = std::make_unique<nativegen::Engine>(input, occupied, true, field.get());
        it = vegetationPairs.emplace(key, std::move(pair)).first;
    }
    return it->second;
}
void placeExtras(Dimension &d, BlockSource &blocks, ChunkPos cp,
                 std::vector<std::pair<int, ChunkPos>> const &candidates, uint seed, bool prefiltered) {
    BoundingBox clip{{cp.x * 16, -64, cp.z * 16}, {cp.x * 16 + 15, (field->generationHeight()-65), cp.z * 16 + 15}};
    for (auto const &[type, anchor] : candidates) {
        auto &p = extraPlan(d, type, anchor);
        if (!p.start)
            continue;
        auto b = *p.start->boundingBox;
        if (extraSurface(type) && !prefiltered &&
            !(b.max.x + 28 < clip.min.x || b.min.x - 28 > clip.max.x || b.max.z + 28 < clip.min.z ||
              b.min.z - 28 > clip.max.z))
            clearReservedPlants(blocks, cp, extraVegetationPair(p));
        int margin = type == 8 ? worldgen_rules::CityFragmentReach : type == 1 ? 16 : 6;
        if (b.max.x + margin < clip.min.x || b.min.x - margin > clip.max.x || b.max.z + margin < clip.min.z ||
            b.min.z - margin > clip.max.z)
            continue;
        if (extraSurface(type))
            villageTerrain(blocks, cp, p.terrain);
        unsigned count = 0;
        auto t = std::chrono::steady_clock::now();
        std::vector<BoundingBox> emptyNeighbours;
        auto const &neighbours = type == 8 ? cityNeighbours(d, *p.start, seed) : emptyNeighbours;
        unsigned cavernCarved = 0, ecologyAttempts = 0, ecologyPlaced = 0;
        shipFloor = type == 2 ? p.floor : -32768;
        portalFloor = type == 1 ? p.floor : -32768;
        portalWriteSource = (type == 1 || type == 9) ? &blocks : nullptr;
        portalWriteClip = (type == 1 || type == 9) ? &clip : nullptr;
        structureWallWrites = &p.wallPositions;
        mineshaftQueryClip = type == 9 ? &clip : nullptr;
        seededReadSource = type == 9 ? &blocks : nullptr;
        seededReadClip = type == 9 ? &clip : nullptr;
        struct ProjectionReset {
            ~ProjectionReset() {
                shipFloor = portalFloor = ruinFloor = -32768;
                portalWriteSource = nullptr;
                portalWriteClip = nullptr;
                structureWallWrites = nullptr;
                mineshaftQueryClip = nullptr;
                seededReadSource = nullptr;
                seededReadClip = nullptr;
            }
        } projectionReset;
        if (type == 1) {
            placeFrozenPortal(d, blocks, cp, p, anchor, seed);
            ++count;
        }
        if (type == 8) {
            auto pieces = cityBlueprintPieces(*p.start);
            auto stats = worldgen_rules::executeCityFragment(
                pieces, {anchor.x, anchor.z}, sharedBox(clip), seed,
                [&] { return prepareCityCavern(blocks, *p.start, clip, seed, neighbours); },
                [&](size_t i, uint32_t pieceSeed, auto const &) {
                    Random rng(pieceSeed, true);
                    (*p.start->pieces)[i]->postProcess(blocks, rng, clip);
                },
                [&](auto const &node, auto const &) {
                    Random rng(node.seed, true);
                    return placeCityEcology(d, blocks, clip,
                                            {node.position.x, node.position.y, node.position.z}, rng,
                                            std::string(node.feature));
                },
                [&] { citySurfaceEcology(blocks, clip, *p.start, seed, neighbours); });
            count += stats.templates;
            cavernCarved = stats.carved;
            ecologyAttempts = stats.ecologyAttempts;
            ecologyPlaced = stats.ecologyPlaced;
        }
        if (type >= 10) {
            Random rng(seed, true);
            StructureFeature::setRandomSeedFor(rng, cp.x, cp.z, extraSpec(type)->salt, seed);
            p.start->postProcess(blocks, rng, clip);
            ++count;
        }
        if (type == 3 && p.ruins)
            worldgen_rules::executeFrozenRuins(
                *p.ruins, sharedBox(clip),
                [&](size_t i, auto const &part, uint32_t partSeed, auto const &owner) {
                    ruinFloor = part.origin.y;
                    Random rng(partSeed, true);
                    (*p.start->pieces)[i]->postProcess(blocks, rng, nativeBox(owner));
                    ++count;
                },
                [&](worldgen_rules::StructurePoint q) {
                    return blocks.getBlock({q.x, q.y, q.z}).getTypeName() == "minecraft:chest";
                },
                [&](auto const &marker, uint32_t markerSeed, auto const &owner) {
                    auto &part = *static_cast<OceanRuinPieces::OceanRuinPiece *>(
                        (*p.start->pieces)[marker.part].get());
                    Random rng(markerSeed, true);
                    part._handleDataMarker(marker.label,
                                           {marker.position.x, marker.position.y, marker.position.z}, blocks,
                                           rng, nativeBox(owner));
                });
        if (type != 1 && type != 8 && !(type == 3 && p.ruins))
            count += worldgen_rules::executeExtraPieces(
                type, {anchor.x, anchor.z}, sharedBox(clip), seed, p.start->pieces->size(),
                [&](size_t i) { return sharedBox(*(*p.start->pieces)[i]->mBoundingBox); },
                [&](size_t i, uint32_t pieceSeed, auto const &) {
                    auto &piece = (*p.start->pieces)[i];
                    if (type == 9 && piece->getType() == StructurePieceType::MineshaftCorridor)
                        static_cast<MineshaftCorridor *>(piece.get())->hasPlacedSpider = false;
                    Random rng(pieceSeed, true);
                    if (type == 3)
                        ruinFloor = static_cast<OceanRuinPieces::OceanRuinPiece *>(piece.get())->mPosition->y;
                    piece->postProcess(blocks, rng, clip);
                });
        // Feature pool elements have an empty native bounding box (max < min).
        // Route by their original origin, after this chunk's template floors exist;
        // the normal piece intersection path otherwise drops these ecology nodes.
        // Cold ruins overlay brick/cracked/mossy templates. A later layer can cover
        // an earlier chest marker; finalize native chest markers after all layers.
        // Only the owning fresh chunk runs this, and existing chests keep their loot.
        if (type == 3 && !p.ruins) {
            std::set<std::tuple<int, int, int>> done;
            for (auto const &ptr : *p.start->pieces) {
                auto &part = *static_cast<OceanRuinPieces::OceanRuinPiece *>(ptr.get());
                for (auto const &[pos, label] :
                     (*part.mTemplate).getMarkers(*part.mTemplatePosition, *part.mSettings)) {
                    if (label != "chest" || !done.emplace(pos.x, pos.y, pos.z).second)
                        continue;
                    worldgen_rules::StructureLootAssignment assignment{
                        {pos.x, pos.y, pos.z},
                        {},
                        worldgen_rules::structureMarkerLootSeed(type, {pos.x, pos.y, pos.z}, seed)};
                    if (!worldgen_rules::structureLootMarkerOwned(assignment, sharedBox(clip), [&] {
                            return blocks.getBlock(pos).getTypeName() == "minecraft:chest";
                        }))
                        continue;
                    Random rng(assignment.seed, true);
                    part._handleDataMarker(label, pos, blocks, rng, clip);
                }
            }
        }
        structureWallWrites = nullptr;
        for (int pass = 0; pass < 2; ++pass)
            for (auto it = p.wallPositions.rbegin(); it != p.wallPositions.rend(); ++it) {
                auto const &pos = *it;
                if (pos[0] < clip.min.x - 1 || pos[0] > clip.max.x + 1 || pos[2] < clip.min.z - 1 ||
                    pos[2] > clip.max.z + 1)
                    continue;
                BlockPos q{pos[0], pos[1], pos[2]};
                auto const &block = blocks.getBlock(q);
                if (block.getTypeName().ends_with("_wall"))
                    static_cast<WallBlock const &>(block.getBlockType()).tryFixWallStates(blocks, q, 2);
            }
        if (extraSurface(type))
            clearUnsupportedGroundPlants(blocks, cp, b.min.y - 10, b.max.y + 2);
        // The void world's native sea level is lower than our frozen ocean. Keep
        // monument rooms submerged using only their own fresh chunk's air cells.
        if (type == 2 || type == 3 || type == 5) {
            struct WaterEnvironment {
                BlockSource &source;
                Block const *waterBlock = Block::tryGetFromRegistry("minecraft:water");
                Block const *get(worldgen_rules::StructurePoint p) {
                    return &source.getBlock({p.x, p.y, p.z});
                }
                bool air(Block const *b) {
                    return b->isAir();
                }
                bool canContainWater(Block const *b) {
                    return BlockLiquidDetectionComponent::canContainLiquid(*b);
                }
                void water(worldgen_rules::StructurePoint p, int layer) {
                    BlockPos q{p.x, p.y, p.z};
                    if (layer)
                        source.setExtraBlockSimple(q, *waterBlock);
                    else
                        source.setBlockNoUpdate(q, *waterBlock);
                }
            } env{blocks};
            worldgen_rules::submergeStructure(
                sharedBox(b), sharedBox(clip),
                [](int x, int z) { return field->surface(x + 32768, z + 32768).water; }, env);
        }
        if (!auditPlanningStarts && worldgen_rules::extraPopulationRequired(type))
            queueStructureEntities(p.start, clip, extraNames[type], anchor, seed);
        auto ch = blocks.getChunkAt({cp.x * 16, 0, cp.z * 16});
        for (auto const &piece : *p.start->pieces)
            piece->addHardcodedSpawnAreas(*ch);
        ch->setUnsaved();
        ch->markSaveIfNeverSaved();
        ch->markSaveNonActorDataIfDirty();
        if (!auditPlanningStarts)
            std::ofstream(folder / "extra-placements.jsonl", std::ios::app)
                << J{{"family", extraNames[type]},
                     {"anchor", {anchor.x * 16, anchor.z * 16}},
                     {"chunk", {cp.x, cp.z}},
                     {"pieces", count},
                     {"cavernCarved", cavernCarved},
                     {"ecologyAttempts", ecologyAttempts},
                     {"ecologyPlaced", ecologyPlaced},
                     {"ms", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t)
                                .count()}}
                       .dump()
                << '\n';
    }
}
