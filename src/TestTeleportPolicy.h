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
    {115, "恶地", "扩大为连续区域，外围以沙漠过渡，仅与沙漠或海洋接壤"},
    {129, "风蚀恶地", "纳入大型恶地区域，保留侵蚀柱与彩色陶瓦"},
    {130, "恶地高原", "扩大高原区域，修复探索时结构高度校验导致的退出"},
    {131, "疏林恶地高原", "保留稀疏林冠与大型恶地区域的过渡"},
    {102, "海洋", "与恶地接壤的海岸及近岸海床改用红沙"}
};
inline std::string joinMessage() {
    std::string result = "§9[山&海] 最近地形更新（0.1.8，待游戏内验收）";
    for (auto const& u : updates)
        result += "\n§9[" + std::to_string(u.id) + "] " + u.name + "：" + u.content;
    return result + "\n§9/karma 查看全部群系/结构；/karma tp <序号> <模式>（1 创造 / 0 观察者）；/karma back 返回。§r";
}
}
