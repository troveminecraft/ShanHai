#include "TerrainKernel.h"
#include "native/Engine.h" // Independent legacy entry point, only in tests.
#include <cassert>
#include <fstream>
#include <future>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    nativegen::J data;
    std::ifstream(argv[1]) >> data;
    nativegen::Engine reference(data);
    auto expected = reference.chunk(0, 0);
    // Neither caller JSON mutation nor destruction may invalidate the kernel.
    auto owned = [&] {
        auto input = data;
        auto result = std::make_unique<terrain_kernel::Generator>(input);
        input.clear();
        return result;
    }();
    assert(owned->chunk(0, 0) == expected);
    // A borrowed field handle may die before a generator that retains its data.
    auto field = std::make_unique<terrain_kernel::Field>(data);
    terrain_kernel::Generator shared(data, {}, false, field.get());
    field.reset();
    assert(shared.chunk(0, 0) == expected);
    assert(shared.terrain().generationHeight() == reference.terrain().generationHeight());
    auto s = shared.terrain().surface(32768, 32768);
    assert(s.height == reference.terrain().surface(32768, 32768).height);
    // Separate owned instances preserve output under concurrent use.
    std::vector<std::future<std::vector<uint8_t>>> jobs;
    for (int i = 0; i < 4; ++i)
        jobs.push_back(std::async(std::launch::async, [data] {
            terrain_kernel::Generator worker(data);
            return worker.chunk(0, 0);
        }));
    for (auto& job : jobs) assert(job.get() == expected);
    terrain_kernel::Field heights(data);
    terrain_kernel::Reservations areas{{32764, 32764, 32780, 32780}};
    terrain_kernel::Generator vegetation(data, {}, true, &heights);
    terrain_kernel::Generator reserved(data, areas, true, &heights, &vegetation);
    nativegen::Engine oldReserved(data, areas, true);
    assert(reserved.vegetationChunk(0, 0) == oldReserved.vegetationChunk(0, 0));
    bool rejected = false;
    try { owned->chunk(INT_MAX, INT_MIN); }
    catch (std::out_of_range const&) { rejected = true; }
    assert(rejected);
    std::cout << "kernel facade: recipe/field lifetime, output, vegetation, concurrency and bounds passed\n";
}
