// Read-only startup probe. This plugin does not generate chunks, run commands,
// place blocks, teleport players or change any dimension's height.
#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/Versions.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionHeightRange.h"
#include "ll/api/memory/Hook.h"
#include "mc/server/ServerInstance.h"
#include <fstream>

namespace {
bool recorded = false;
LL_TYPE_INSTANCE_HOOK(HeightProbePump, ll::memory::HookPriority::Normal, ServerInstance, &ServerInstance::_update, void) {
    origin();
    if (recorded) return;
            auto level = ll::service::getLevel();
            if (!level) return;
            auto dimension = level->getOrCreateDimension(0).lock();
            if (!dimension) return;
            auto const& range = dimension->getHeightRange();
            std::ofstream out("height-range-result.json");
            out << "{\"gameVersion\":\"" << ll::getGameVersion().to_string()
                << "\",\"min\":" << int(range.mMin) << ",\"max\":" << int(range.mMax)
                << ",\"subchunks\":" << dimension->getHeightInSubchunks() << ",\"within\":{";
            bool first = true;
            for (short y : {-65, -64, 319, 320, 383, 511, 512}) {
                if (!first) out << ',';
                first = false;
                out << '\"' << y << "\":" << (dimension->isHeightWithinRange(y) ? "true" : "false");
            }
            out << "},\"blocksPlaced\":0,\"gameplayTested\":false}";
            out.close();
            ll::mod::NativeMod::current()->getLogger().info("Read-only height range: min={} max={} subchunks={}", int(range.mMin), int(range.mMax), dimension->getHeightInSubchunks());
            recorded = true;
}
}
class HeightRangeProbe {
public:
    static HeightRangeProbe& getInstance() { static HeightRangeProbe instance; return instance; }
    bool load() { return true; }
    bool enable() { recorded = false; HeightProbePump::hook(); return true; }
    bool disable() { HeightProbePump::unhook(); return true; }
    bool unload() { return true; }
};
LL_REGISTER_MOD(HeightRangeProbe, HeightRangeProbe::getInstance());
