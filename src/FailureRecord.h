#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <array>
#include <string_view>
namespace terrain_runtime {
inline nlohmann::json failureRecord(std::string_view component,std::string_view version,
    std::string_view phase,nlohmann::json const& seed,std::string_view world,
    std::optional<std::array<int,2>> chunk,std::string_view reason,std::string_view action) {
    return {{"schema",1},{"component",component},{"version",version},{"phase",phase},
        {"seed",seed},{"world",world},{"chunk",chunk?nlohmann::json(*chunk):nlohmann::json(nullptr)},
        {"reason",reason},{"action",action},{"gameplayValidated",false},
        {"unixMilliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}};
}
inline bool writeFailure(std::filesystem::path const& path,nlohmann::json const& record) noexcept {
    try {
        static std::mutex mutex;std::lock_guard lock(mutex);
        std::ofstream out(path,std::ios::trunc);out<<record.dump(2)<<'\n';out.flush();
        return bool(out);
    } catch(...) {return false;}
}
}
