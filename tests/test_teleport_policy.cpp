#ifdef NDEBUG
#undef NDEBUG
#endif
#include "TestTeleportPolicy.h"
#include <cassert>
#include <climits>
#include <set>
#include <iostream>
int main() {
    using namespace test_teleport;
    assert(mode(0) == Mode::Observer && mode(1) == Mode::Creative);
    for (int i : {-1, 2, 6, INT_MIN, INT_MAX}) assert(!mode(i));
    std::set<int> ids;
    for (int g = 0; g < int(structure_test::Groups.size()); ++g)
        for (int i = 0; i < structure_test::Groups[g].quota; ++i) {
            int id = structure_test::firstId(g) + i;
            auto d = destination(id, 31);
            assert(d && !d->biome && d->group == g && d->point == i);
            assert(ids.insert(id).second);
        }
    for (int i = 0; i < 31; ++i) {
        auto d = destination(101 + i, 31);
        assert(d && d->biome && d->group == i && d->point == 0);
        assert(ids.insert(101 + i).second);
    }
    assert(ids.size() == 76);
    for (int i : {INT_MIN, -1, 0, 46, 100, 132, INT_MAX}) assert(!destination(i, 31));
    auto message = joinMessage();
    assert(message.starts_with("§9"));
    for (size_t i = message.find('\n'); i != std::string::npos; i = message.find('\n', i + 1))
        assert(message.compare(i + 1, std::string("§9").size(), "§9") == 0);
    for (auto const& u : updates) {
        assert(destination(u.id, 31));
        assert(message.find("[" + std::to_string(u.id) + "] " + u.name) != std::string::npos);
        assert(message.find(u.content) != std::string::npos);
    }
    std::cout << "76 stable destinations; modes and all blue update lines verified\n";
}
