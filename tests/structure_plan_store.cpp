#include "rules/StructurePlanStore.h"
#include <iostream>

void check(bool value, char const *reason) {
    if (!value)
        throw std::runtime_error(reason);
}
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("expected temporary output directory");
        namespace fs = std::filesystem;
        using J = nlohmann::json;
        auto dir = fs::path(argv[1]) / std::to_string(worldgen_persistence::diagnosticStamp());
        auto file = worldgen_rules::plan_store::checkpointPath(dir, "village", -17, 32);
        fs::create_directories(file.parent_path());
        J plan = {{"world", "test"},
                  {"seed", "-9223372036854775808"},
                  {"floor", 62},
                  {"pieces", J::array({{1, 62, 2, 15, 70, 16}})}};
        worldgen_rules::plan_store::verifyOrCreate(file, plan);
        worldgen_rules::plan_store::verifyOrCreate(file, plan);
        auto altered = plan;
        altered["floor"] = 63;
        bool rejected = false;
        try {
            worldgen_rules::plan_store::verifyOrCreate(file, altered);
        } catch (std::runtime_error const &) {
            rejected = true;
        }
        check(rejected && worldgen_persistence::read(file) == plan,
              "changed plan silently replaced saved layout");
        {
            std::ofstream broken(file);
            broken << "{";
        }
        worldgen_rules::plan_store::verifyOrCreate(file, plan);
        check(worldgen_persistence::read(file) == plan, "backup not recovered");
        {
            std::ofstream broken(file);
            broken << "{";
        }
        {
            std::ofstream broken(file.string() + ".bak");
            broken << "{";
        }
        rejected = false;
        try {
            worldgen_rules::plan_store::verifyOrCreate(file, plan);
        } catch (std::runtime_error const &) {
            rejected = true;
        }
        check(rejected && fs::file_size(file) == 1, "corrupt checkpoints reset as new world");
        rejected = false;
        try {
            worldgen_rules::plan_store::checkpointPath(dir, "../other", 0, 0);
        } catch (std::runtime_error const &) {
            rejected = true;
        }
        check(rejected, "checkpoint path escaped structure directory");
        std::cout
            << "structure checkpoints: reload, drift rejection, backup, corruption, path checks passed\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
