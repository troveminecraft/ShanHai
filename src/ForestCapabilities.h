#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>

namespace forest_capabilities {
enum class Profile { Legacy, Bedrock2620, Bedrock2651 };
inline constexpr std::string_view Common="bedrock-26.20-forests-v1";
inline constexpr std::string_view Modern="bedrock-26.51-forests-v1";
inline Profile profile(nlohmann::json const& data) {
    auto name=data.value("capabilityProfile",std::string{});
    if(name.empty())return Profile::Legacy;
    if(name==Common)return Profile::Bedrock2620;
    if(name==Modern)return Profile::Bedrock2651;
    throw std::runtime_error("Unsupported forest capability profile: "+name);
}
inline Profile recipe(nlohmann::json const& data) {
    auto p=profile(data);int revision=data.at("config").value("forestEcologyRevision",0);
    if((p==Profile::Legacy&&revision!=0)||(p!=Profile::Legacy&&(revision<1||revision>2)))
        throw std::runtime_error("Forest recipe and capability profile disagree");
    return p;
}
inline void requireHost(nlohmann::json const& data,bool modern) {
    if(profile(data)==Profile::Bedrock2651&&!modern)
        throw std::runtime_error("This world requires Minecraft 26.51 forest blocks; it cannot open on 26.20");
}
inline void requireMatch(nlohmann::json const& data,nlohmann::json const& metadata) {
    if(recipe(data)!=profile(metadata))throw std::runtime_error("Forest metadata and recipe capabilities disagree");
}
inline constexpr bool enabled(Profile p){return p!=Profile::Legacy;}
inline constexpr bool dappled(Profile p){return p==Profile::Bedrock2651;}
inline constexpr int Dark=58,Pale=59,Cherry=60,Mangrove=61,Dappled=62;
inline constexpr bool biome(int id){return id>=Dark&&id<=Dappled;}
}
