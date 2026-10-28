#include "BedrockAccess.h"
// One module coordinates two separately compiled host adapters.
#include <Windows.h>
#include "WindowsWorldHost.h"
#include "NativeRuntime.h"
#include "ReleaseVersions.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "mc/world/level/storage/LevelStorage.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include <array>
#include <fstream>
#include <map>
#include <mutex>
#include "mc/server/ServerLevel.h"
#include "mc/world/level/LevelSettings.h"
#include "mc/deps/core/file/PathBuffer.h"
#include "mc/resources/ResourcePackManager.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/file/PathView.h"
#include "WorldStartup.h"
#include "HookTransaction.h"
#include "CloudHeight.inc"

namespace client {
Level *current = nullptr;
ll::event::ListenerPtr listener;
std::filesystem::path diagnostics;
using Json = nlohmann::json;
unsigned sessionTicks = 0;
unsigned tickErrors = 0;
std::mutex freshWorldMutex;
std::map<std::filesystem::path, std::string> freshWorldSeeds;
void validateStoredGenerator(std::filesystem::path const &root, std::string const &seed) {
    bool fresh = false;
    {
        std::lock_guard lock(freshWorldMutex);
        auto it = freshWorldSeeds.find(std::filesystem::weakly_canonical(root));
        if (it != freshWorldSeeds.end()) {
            if (it->second != seed)
                throw std::runtime_error("Fresh world seed differs from its creation ticket");
            fresh = true;
            freshWorldSeeds.erase(it);
        }
    }
    // The saved Flat/TheVoid setting and the engine's resolved runtime generator
    // are different representations. Validate the durable world settings instead.
    auto path = root / "level.dat";
    // Only this process's successful create-world handoff may enter before the
    // first vanilla settings save. A marker file alone cannot bypass validation.
    if (!std::filesystem::exists(path) && fresh)
        return;
    auto size = std::filesystem::file_size(path);
    if (size < 8 || size > 16 * 1024 * 1024)
        throw std::runtime_error("Invalid level.dat size");
    std::string bytes(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(bytes.data(), bytes.size()))
        throw std::runtime_error("Cannot read level.dat");
    auto nbt = CompoundTag::fromBinaryNbt(std::string_view(bytes).substr(8));
    if (!nbt)
        throw std::runtime_error("Invalid level.dat NBT");
    if (int(nbt->at("Generator")) != 2 ||
        std::string_view(nbt->at("FlatWorldLayers")).find("TheVoid") == std::string_view::npos)
        throw std::runtime_error("Karma saved world generator setting was changed");
}
void writeStatus(Level &level, bool closed = false) {
    Json status = terrain::clientStatus();
    status.update(lab::clientStatus());
    status["seed"] = bedrock_compat::seed(level.getLevelData()).toUiString();
    status["ticks"] = sessionTicks;
    status["closed"] = closed;
    status["tickErrors"] = tickErrors;
    status["clouds"] = clouds::status();
    status["players"] = Json::array();
    level.forEachPlayer([&](Player &p) {
        auto pos = bedrock_compat::position(p);
        status["players"].push_back({{"pos", {pos.x, pos.y, pos.z}}, {"dimension", int(p.getDimensionId())}});
        return true;
    });
    std::ofstream(diagnostics / "world-session.json") << status.dump(2);
}

void clearWorld(Level *level) {
    if (level != current)
        return;
    writeStatus(*level, true);
    terrain::clientRelease();
    lab::clientRelease(*level);
    current = nullptr;
    terrain::log().info("Karma client: world state released");
}

void bindWorld(Level &level, std::string const &name, LevelSettings const &settings) {
    auto root = std::filesystem::u8path(level.getLevelStorage().getFullPath().value);
    // DB storage implementations may return either the world root or its db directory.
    if (root.filename() == "db")
        root = root.parent_path();
    terrain::log().info("Karma client: opening {} at {}", name, root.string());
    if (!std::filesystem::exists(root / "karma-worldgen.json"))
        return;
    if (current)
        throw std::runtime_error("Previous Karma world is still active");
    auto marker = terrain::jsonFile(root / "karma-worldgen.json");
    if (marker.value("host", "") != "windows-client")
        throw std::runtime_error("Not a client world package");
    auto seed = bedrock_compat::seed(settings).toUiString();
    if (marker.at("worldSeed").get<std::string>() != seed)
        throw std::runtime_error("World seed differs from its immutable Karma seed package");
    validateStoredGenerator(root, seed);
    auto height=terrain_runtime::BuildHeight::read(marker);
    if(height.maxYExclusive!=terrain_runtime::BuildHeight::LegacyMaxYExclusive){
        auto manager=level.getServerResourcePackManager();
        std::string definition;
        if(!manager || !manager->load(ResourceLocation(Core::PathView("dimensions/overworld.json"),ResourceFileSystem::UserPackage),definition))
            throw std::runtime_error("Height behavior pack is absent from the native server pack stack; world initialization was stopped before generation");
        startup::requireHeightResource(definition,height.MinY,height.maxYExclusive);
    }
    terrain::log().info("Karma client: world generator provenance validated; runtime generator={}",
                        int(settings.mGenerator));
    current = &level;
    terrain::clientPrepare(level, root, name, seed, marker);
    lab::clientPrepare(root, name, marker.at("worldName").get<std::string>());
    terrain::clientActivate();
    lab::clientActivate();
    sessionTicks = 0;
    tickErrors = 0;
    terrain::log().info("Karma client ready: seed={} kernel={} bounds={}", seed, NativeRuntime::KernelVersion,
                        "saved terrain profile");
    std::ofstream(diagnostics / "world-session.json") << Json{
        {"world", name},
        {"path", root.string()},
        {"seed", seed},
        {"kernel", NativeRuntime::KernelVersion},
        {"entered", true}}.dump(2);
}

using BiomeData = std::optional<std::reference_wrapper<
    std::unordered_map<std::string, std::unique_ptr<BiomeJsonDocumentGlueResolvedBiomeData>>>>;
LL_TYPE_INSTANCE_HOOK(WorldOpen, ll::memory::HookPriority::Normal, ServerLevel, &ServerLevel::$initialize,
                      bool, std::string const &name, LevelSettings const &settings,
                      Experiments const &experiments, std::string const *id, BiomeData biomes) {
    auto startupStatus=[&](char const* phase,std::string const& error=std::string{}){
        std::ofstream(diagnostics/"world-startup.json") << Json{
            {"version",shanhai_release::Windows},{"world",name},
            {"phase",phase},{"error",error},{"gameplayTested",false}}.dump(2);
    };
    startupStatus("preflight");
    try {
        bindWorld(*this, name, settings);
    } catch (std::exception const &e) {
        terrain::log().error("Karma world rejected before generation: {}", e.what());
        std::ofstream(diagnostics / "world-error.txt") << e.what();
        startupStatus("rejected-before-native-initialize",e.what());
        if (current == this)
            clearWorld(this);
        return false;
    }
    startupStatus("native-initialize");
    bool ok = origin(name, settings, experiments, id, biomes);
    if (ok && current == this) {
        try {
            auto dimension = getOrCreateDimension(0).lock();
            if (!dimension)
                throw std::runtime_error("Overworld dimension is unavailable after initialization");
            auto const &range = bedrock_compat::heightRange(*dimension);
            // Repository registration alone does not prove the pack was selected
            // by the native server. Reject a mismatched dimension before joining.
            std::ofstream(diagnostics / "world-height.json") << Json{
                {"world",name},{"minY",int(range.mMin)},
                {"maxYExclusive",int(range.mMax)},
                {"generatedMaxYExclusive",terrain::clientHeight().generatedMaxYExclusive},
                {"nativeDimensionVerified",int(range.mMin)==terrain_runtime::BuildHeight::MinY&&int(range.mMax)==terrain::clientHeight().maxYExclusive},
                {"playerPlacementTested",false}}.dump(2);
            terrain::clientHeight().requireDimension(range.mMin,range.mMax);
            // Finish native template loading on the level thread before chunk jobs can
            // mark decoration complete. Missing templates must not silently omit villages.
            lab::clientWarmTemplates(*this);
            terrain::log().info("Karma client: vanilla village and fossil templates ready");
            startupStatus("ready");
            std::error_code ignored;
            std::filesystem::remove(diagnostics/"world-error.txt",ignored);
        } catch (std::exception const &e) {
            terrain::log().error("Karma native templates unavailable: {}", e.what());
            std::ofstream(diagnostics / "world-error.txt") << e.what();
            startupStatus("rejected-after-native-initialize",e.what());
            ok = false;
        }
    }
    // A failed initialize() can still have native chunk tasks queued. The
    // engine owns their teardown; keep their session alive until WorldClose.
    // Only failures BEFORE origin() are safe to release immediately above.
    return ok;
}
LL_TYPE_INSTANCE_HOOK(WorldClose, ll::memory::HookPriority::Normal, ServerLevel, &ServerLevel::$dtor, void) {
    // ServerLevel destruction follows completion of its chunk work; release registry
    // references while dimensions still exist, before the original destructor.
    if (current == this)
        clearWorld(this);
    origin();
}

} // namespace client
#include "NewWorldCreation.inc"
namespace client {
class Mod {
  public:
    static Mod &getInstance() {
        static Mod m;
        return m;
    }
    bool load() {
        auto &m = terrain::clientMod();
        diagnostics = m.getModDir() / "diagnostics";
        std::array<wchar_t, 32768> exe{};
        auto n = GetModuleFileNameW(nullptr, exe.data(), DWORD(exe.size()));
        if (!n || n == exe.size())
            return false;
        lab::clientVanillaPath(std::filesystem::path(std::wstring(exe.data(), n)).parent_path() / "data/behavior_packs/vanilla");
        std::filesystem::create_directories(diagnostics);
        if (!creation::supportedFacetLayout()) {
            m.getLogger().error(
                "Unsupported Minecraft executable; this adapter requires Windows "
#ifdef SHANHAI_MC_26_51
                "1.26.51.01."
#else
                "1.26.20.04."
#endif
            );
            return false;
        }
        m.getLogger().info("Karma Windows client {}; local world creation enabled", shanhai_release::Windows);
        return true;
    }
    bool enable() {
        terrain_runtime::HookTransaction hooks;

        hooks.add<WorldOpen>("WorldOpen");
        hooks.add<WorldClose>("WorldClose");
        terrain::clientHooks(hooks);
        lab::clientHooks(hooks);
        clouds::install(hooks);
        creation::enable();
        hooks.onRollback([] { creation::disable(); });
        listener =
            ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto &e) {
                if (current != &e.level())
                    return;
                ++sessionTicks;
                try {
                    lab::clientTick(e.level());
                    terrain::clientTick(e.level());
                } catch (std::exception const &ex) {
                    if (++tickErrors == 1 || tickErrors % 200 == 0)
                        terrain::log().error("Karma client tick ({} errors): {}", tickErrors, ex.what());
                }
                if (sessionTicks % 100 == 0)
                    writeStatus(e.level());
            });
        hooks.commit();
        return true;
    }
    bool disable() {
        if (current)
            return false;
        creation::disable();
        ll::event::EventBus::getInstance().removeListener(listener);
        WorldOpen::unhook();
        WorldClose::unhook();
        clouds::remove();
        terrain::clientUnhook();
        lab::clientUnhook();
        return true;
    }
    bool unload() {
        return !current;
    }
};
} // namespace client
LL_REGISTER_MOD(client::Mod, client::Mod::getInstance());
