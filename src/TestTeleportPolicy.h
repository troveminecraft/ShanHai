#pragma once
#include "StructureTestCatalog.h"
#include <optional>
#include <string>
#include <algorithm>
#include <cstdint>

namespace test_teleport {
inline bool validY(int y,int maxYExclusive){
    return (maxYExclusive==320||maxYExclusive==512)&&y>=-58&&y<=maxYExclusive-2;
}
// Keep both player-height cells in range and strictly above terrain/structure.
// A summit at the ceiling has no safe target: skip it instead of clamping into rock.
inline std::optional<int> aboveSurface(int highest,int clearance,int maxYExclusive){
    if((maxYExclusive!=320&&maxYExclusive!=512)||highest< -64||highest>=maxYExclusive-2||clearance<1)return {};
    int64_t wanted=std::max<int64_t>(-58,int64_t(highest)+clearance);
    return int(std::min<int64_t>(wanted,maxYExclusive-2));
}
enum class Mode { Observer, Creative };
inline constexpr std::optional<Mode> mode(int value) {
    if (value == 0) return Mode::Observer;
    if (value == 1) return Mode::Creative;
    return std::nullopt;
}
// Resolve against quotas, not the number of points found so far. Missing points
// must never resolve to a different structure or disappear from the catalog.
struct Destination { bool biome; int group; int point; };
inline std::optional<Destination> destination(int id, int biomeCount) {
    if (id >= 101 && id - 101 < biomeCount) return Destination{true, id - 101, 0};
    if (id < 1 || id > structure_test::expected()) return std::nullopt;
    for (int g = 0; g < int(structure_test::Groups.size()); ++g) {
        int point = id - structure_test::firstId(g);
        if (point >= 0 && point < structure_test::Groups[g].quota) return Destination{false, g, point};
    }
    return std::nullopt;
}
struct Update { int id; char const* name; char const* content; };
inline constexpr Update updates[]{
    {115, "恶地", "提高陶瓦高原海拔，增加阶地、陡坡和悬崖"},
    {130, "恶地高原", "更高的连续高原；外围保留沙漠过渡和红沙海岸"},
    {111, "沙漠", "与恶地共同增加干湿缓冲，避免紧邻丛林、沼泽等湿润群系"},
    {116, "河流", "共线河段加入贝塞尔弯曲，保留支流汇流点"},
    {108, "平原", "水坑水面与坑底升高一格，部分形成两至三个连片水坑"}
};
inline std::string joinMessage() {
    std::string result = "§9[山&海] 最近地形更新（0.1.9，待游戏内验收）";
    for (auto const& u : updates)
        result += "\n§9[" + std::to_string(u.id) + "] " + u.name + "：" + u.content;
    return result + "\n§9/karma 查看全部群系/结构；/karma tp <序号> [模式]（1 创造 / 0 观察者；省略保持当前模式）。§r";
}
}
