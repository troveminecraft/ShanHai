#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
namespace creation_status {
inline std::optional<std::string> failure(nlohmann::json const& status) {
    if(status.value("state",std::string{})!="error")return {};
    auto message=status.value("phase",std::string("世界准备失败"));
    auto detail=status.value("detail",std::string{});
    if(!detail.empty())message+="："+detail;
    return message;
}
}
