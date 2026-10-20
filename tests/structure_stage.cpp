#include "rules/StructureStage.h"
#include <iostream>
#include <thread>
#include <vector>

void check(bool ok, char const* reason) {
    if (!ok) throw std::runtime_error(reason);
}
int main() {
    try {
        // Every possible native probe, including negative chunk coordinates.
        for (auto [cx, cz] : {std::pair{0, 0}, {-300, 70}, {1000000, -1000000}}) {
            for (int z = cz - 1; z <= cz + 1; ++z)
                for (int x = cx - 1; x <= cx + 1; ++x)
                    check(worldgen_rules::decorationCenter(x, z, [&](int px, int pz) {
                        return px >= cx - 1 && px <= cx + 1 && pz >= cz - 1 && pz <= cz + 1;
                    }) == std::pair{cx, cz}, "wrong decoration owner");
        }
        for (int missing = 0; missing < 9; ++missing) {
            bool failed = false;
            try {
                worldgen_rules::decorationCenter(0, 0, [&](int x, int z) {
                    return x >= -1 && x <= 1 && z >= -1 && z <= 1
                        && (z + 1) * 3 + x + 1 != missing;
                });
            } catch (std::runtime_error const&) { failed = true; }
            check(failed, "incomplete view allowed structure writes");
        }
        worldgen_rules::BoundedHints<std::pair<int, int>, 64> hints;
        for (int i = 0; i < 100000; ++i) {
            hints.insert({i, -i});
            hints.insert({i, -i});
            check(hints.size() <= 64, "hint cache leaked");
            if (i % 3 == 0) hints.erase({i, -i});
        }
        hints.clear();
        hints.insert({1, 2}); hints.erase({1, 2}); hints.insert({1, 2});
        for (int i = 0; i < 63; ++i) hints.insert({3, i});
        check(hints.contains({1, 2}), "stale FIFO entry evicted a newer hint");
        worldgen_rules::StructureStageMutex mutex;
        int counter = 0;
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) workers.emplace_back([&] {
            for (int j = 0; j < 10000; ++j) { std::lock_guard lock(mutex); ++counter; }
        });
        for (auto& worker : workers) worker.join();
        check(counter == 40000 && mutex.acquisitions == 40000, "native lock lost synchronization");
        std::cout << "structure stage: ownership, incomplete views, bounded hints, lock accounting passed\n";
        return 0;
    } catch (std::exception const& e) { std::cerr << e.what() << '\n'; return 1; }
}
