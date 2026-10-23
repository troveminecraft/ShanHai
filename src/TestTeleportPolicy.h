#pragma once
#include "StructureTestCatalog.h"
#include <optional>
#include <string>

namespace test_teleport {
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
    {128, "蘑菇岛", "补全菌丝地表与巨型蘑菇；海中生成，与陆地群系隔开"},
    {115, "恶地", "补全红沙、陶瓦分层及恶地生态"},
    {129, "侵蚀恶地", "补全侵蚀石柱地形"},
    {130, "恶地高原", "补全高原台地与陶瓦层"},
    {131, "繁茂恶地高原", "补全高原草土表层及稀疏植被"}
};
inline std::string joinMessage() {
    std::string result = "§9[山&海] 最近地形更新（0.1.5，待游戏内验收）";
    for (auto const& u : updates)
        result += "\n§9[" + std::to_string(u.id) + "] " + u.name + "：" + u.content;
    return result + "\n§9/karma 查看全部群系/结构；/karma tp <序号> <模式>（1 创造 / 0 观察者）；/karma back 返回。§r";
}
}
