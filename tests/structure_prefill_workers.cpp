#include "native/Engine.h"
#include <fstream>
#include <future>
#include <iostream>

int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("expected native-input.json");
        nativegen::J data;
        std::ifstream file(argv[1]);
        file >> data;
        using Areas = std::vector<std::array<int, 4>>;
        std::vector<std::pair<int, int>> chunks{{0, 0}, {-1, -1}, {-594, -360}, {22, 177}};
        Areas a{{32768 - 9515, 32768 - 5780, 32768 - 9440, 32768 - 5710}};
        Areas b{{32768 + 330, 32768 + 2820, 32768 + 390, 32768 + 2890}};
        auto sample = [&](bool independent) {
            nativegen::Field field(data);
            nativegen::Engine normal(data, {}, true, &field);
            std::vector<std::vector<uint16_t>> result;
            std::unique_ptr<nativegen::Engine> reserved;
            std::map<Areas, std::unique_ptr<nativegen::Engine>> previousCache;
            for (auto areas : {a, b, a}) {
                // The reserved engine is recreated when the snapshot changes, exactly
                // like the worker; its Field remains local to the worker.
                nativegen::Engine *active;
                if (independent) {
                    reserved = std::make_unique<nativegen::Engine>(data, areas, true, &field);
                    active = reserved.get();
                } else {
                    auto &cached = previousCache[areas];
                    if (!cached)
                        cached = std::make_unique<nativegen::Engine>(data, areas, true, &field);
                    active = cached.get();
                }
                for (auto [x, z] : chunks) {
                    result.push_back(normal.vegetationChunk(x, z));
                    result.push_back(active->vegetationChunk(x, z));
                }
            }
            return result;
        };
        auto expected = sample(false);
        auto first = std::async(std::launch::async, [&] { return sample(true); });
        auto second = std::async(std::launch::async, [&] { return sample(true); });
        if (first.get() != expected || second.get() != expected)
            throw std::runtime_error("independent prefill workers changed vegetation masks");
        std::cout << "two independent vegetation workers match sequential masks across reservation changes\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
