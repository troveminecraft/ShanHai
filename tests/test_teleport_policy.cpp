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
    struct Land {int biome=11,height=70,water=-32768,lava=-32768,lake=0;bool badlandsDesert=false;double desert=1,river=0,iceHabitat=0,wetland=0,stone=0,volcanic=0;};
    Land good;assert(desertDestination(0,0,[&](int,int){return good;}));
    auto separator=good;separator.badlandsDesert=true;assert(!desertInterior(separator));
    auto narrow=good;narrow.desert=.01;assert(!desertInterior(narrow));
    assert(!desertDestination(0,0,[&](int x,int z){return x==64&&z==-64?separator:good;}));
    assert(mode(0) == Mode::Observer && mode(1) == Mode::Creative);
    for (int i : {-1, 2, 6, INT_MIN, INT_MAX}) assert(!mode(i));
    assert(aboveSurface(480,8,512)==488);
    assert(aboveSurface(509,8,512)==510);
    assert(aboveSurface(317,8,320)==318);
    assert(aboveSurface(100,8,320)==108);
    for(auto pair:{std::pair{510,512},std::pair{511,512},std::pair{318,320},std::pair{319,320},std::pair{INT_MAX,512}})
        assert(!aboveSurface(pair.first,8,pair.second));
    assert(validY(488,512)&&!validY(488,320)&&!validY(511,512));
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
    assert(message.find("back")==std::string::npos && message.find("[模式]")!=std::string::npos);
    assert(message.starts_with("§9"));
    for (size_t i = message.find('\n'); i != std::string::npos; i = message.find('\n', i + 1))
        assert(message.compare(i + 1, std::string("§9").size(), "§9") == 0);
    for (auto const& u : updates) {
        assert(destination(u.id, 31));
        assert(message.find("[" + std::to_string(u.id) + "] " + u.name) != std::string::npos);
        assert(message.find(u.content) != std::string::npos);
    }
    std::cout << "76 stable destinations; safe above-summit targets for both heights; modes and blue updates verified\n";
}
