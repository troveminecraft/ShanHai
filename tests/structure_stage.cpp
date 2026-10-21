#include "rules/StructureStage.h"
#include <iostream>
#include <thread>
#include <vector>
#include <map>

struct Chunk { int x, z; };
struct View {
    std::map<std::pair<int,int>,Chunk> chunks;
    // A random availability query need not expose a processing chunk.
    Chunk *randomAvailableChunk() const { return nullptr; }
    Chunk *lookup(int x,int z) { auto it=chunks.find({x,z}); return it==chunks.end()?nullptr:&it->second; }
};
using Task = worldgen_rules::DecorationTaskScope<View,Chunk>;
Chunk &ownerFor(View &view) {
    return worldgen_rules::decorationTaskOwner<View,Chunk>(view,
        [](Chunk const &c) { return std::pair{c.x,c.z}; },
        [&](int x,int z) { return view.lookup(x,z); });
}

void check(bool ok, char const *reason) {
    if (!ok)
        throw std::runtime_error(reason);
}
int main() {
    try {
        auto rejected=[](auto action) { try {action();} catch(std::runtime_error const&) {return true;} return false; };
        View view;
        for(int z=-5;z<=-3;++z)for(int x=-7;x<=-5;++x)view.chunks.emplace(std::pair{x,z},Chunk{x,z});
        auto &owner=*view.lookup(-6,-4);
        check(view.randomAvailableChunk()==nullptr,"fixture must reproduce unavailable random probe");
        check(rejected([&]{ownerFor(view);}),"missing task accepted");
        {
            Task scope(view,owner);
            check(&ownerFor(view)==&owner,"explicit processing owner lost when random probe is empty");
            View other;
            check(rejected([&]{ownerFor(other);}),"wrong view accepted task context");
            auto nested=[&]{Task inner(other,owner);throw std::runtime_error("unwind");};
            check(rejected(nested),"nested exception missing");
            check(&ownerFor(view)==&owner,"nested exception did not restore task");
            bool isolated=false;
            std::thread worker([&]{isolated=rejected([&]{Task::ownerFor(view);});});worker.join();
            check(isolated,"task context leaked across workers");
            auto saved=view.chunks.extract({-7,-5});
            check(rejected([&]{ownerFor(view);}),"incomplete neighbourhood accepted");
            view.chunks.insert(std::move(saved));
            Chunk impostor{-6,-4};
            {Task wrong(view,impostor);check(rejected([&]{ownerFor(view);}),"wrong owner identity accepted");}
        }
        check(rejected([&]{ownerFor(view);}),"completed task context leaked");
        // Every possible native probe, including negative chunk coordinates.
        for (auto [cx, cz] : {std::pair{0, 0}, {-300, 70}, {1000000, -1000000}}) {
            for (int z = cz - 1; z <= cz + 1; ++z)
                for (int x = cx - 1; x <= cx + 1; ++x)
                    check(worldgen_rules::decorationCenter(x, z,
                                                           [&](int px, int pz) {
                                                               return px >= cx - 1 && px <= cx + 1 &&
                                                                      pz >= cz - 1 && pz <= cz + 1;
                                                           }) == std::pair{cx, cz},
                          "wrong decoration owner");
        }
        for (int missing = 0; missing < 9; ++missing) {
            bool failed = false;
            try {
                worldgen_rules::decorationCenter(0, 0, [&](int x, int z) {
                    return x >= -1 && x <= 1 && z >= -1 && z <= 1 && (z + 1) * 3 + x + 1 != missing;
                });
            } catch (std::runtime_error const &) {
                failed = true;
            }
            check(failed, "incomplete view allowed structure writes");
        }
        worldgen_rules::BoundedHints<std::pair<int, int>, 64> hints;
        for (int i = 0; i < 100000; ++i) {
            hints.insert({i, -i});
            hints.insert({i, -i});
            check(hints.size() <= 64, "hint cache leaked");
            if (i % 3 == 0)
                hints.erase({i, -i});
        }
        hints.clear();
        hints.insert({1, 2});
        hints.erase({1, 2});
        hints.insert({1, 2});
        for (int i = 0; i < 63; ++i)
            hints.insert({3, i});
        check(hints.contains({1, 2}), "stale FIFO entry evicted a newer hint");
        worldgen_rules::StructureStageMutex mutex;
        int counter = 0;
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i)
            workers.emplace_back([&] {
                for (int j = 0; j < 10000; ++j) {
                    std::lock_guard lock(mutex);
                    ++counter;
                }
            });
        for (auto &worker : workers)
            worker.join();
        check(counter == 40000 && mutex.acquisitions == 40000, "native lock lost synchronization");
        std::cout << "structure stage: ownership, incomplete views, bounded hints, lock accounting passed\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
