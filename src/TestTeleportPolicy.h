#pragma once
#include "StructureTestCatalog.h"
#include <optional>
#include <string>
#include <algorithm>
#include <cstdint>

namespace test_teleport {
// A sand separator around a mesa is not a representative desert destination.
template<class S> bool desertInterior(S const&s){
    return s.biome==11 && !s.badlandsDesert && s.desert>=.5 &&
        s.water<s.height && s.lava<s.height && !s.lake && s.river==0 &&
        s.iceHabitat<=.01 && s.wetland<=.01 && s.stone<=.01 && s.volcanic<=.01;
}
template<class Sample> bool desertDestination(int x,int z,Sample sample){
    for(int dz:{-64,0,64})for(int dx:{-64,0,64})
        if(!desertInterior(sample(x+dx,z+dz)))return false;
    return true;
}
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
    {116, "河流", "逐格校验两岸，支流寻找更低汇流点，修复高出岸边的水带"},
    {115, "恶地", "统一干旱气候与地形高度，修复冰川插入和群系边缘切面"},
    {129, "侵蚀恶地", "石柱先于河流规划生成，避免末段突然加高"},
    {111, "沙漠", "传送点检查周围大片沙漠，排除恶地边缘沙带"}
};
inline std::string joinMessage() {
    std::string result = "§9[山&海] 最近地形更新（0.1.10，待游戏内验收）";
    for (auto const& u : updates)
        result += "\n§9[" + std::to_string(u.id) + "] " + u.name + "：" + u.content;
    return result + "\n§9/karma 查看全部群系/结构；/karma tp <序号> [模式]（1 创造 / 0 观察者；省略保持当前模式）。§r";
}
}
