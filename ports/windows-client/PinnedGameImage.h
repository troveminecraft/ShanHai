#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

namespace client::image {
inline constexpr uint32_t FacetRva = 0x928b910;
inline constexpr std::array<uint8_t, 24> FacetBytes{
    0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x83,
    0xec,0x20,0x48,0x89,0xce,0x48,0x8b,0x89,0xf8,0x01,0,0};
struct Result {
    bool supported = false;
    uint32_t timestamp = 0, imageSize = 0;
    std::string reason;
};
// Reader addresses a mapped PE image by RVA. Keeping validation independent of
// the process lets the same checks run against the installed executable offline.
template<class Reader> Result inspect(Reader read) {
    Result result;
    auto reject = [&](char const* reason) { result.reason = reason; return result; };
    auto u16 = [&](uint32_t rva) { uint16_t v = 0; read(rva, &v, sizeof(v)); return v; };
    auto u32 = [&](uint32_t rva) { uint32_t v = 0; read(rva, &v, sizeof(v)); return v; };
    if (u16(0) != 0x5a4d) return reject("Invalid DOS header");
    auto nt = u32(0x3c);
    if (nt < 0x40 || nt > 0x1000 || u32(nt) != 0x4550) return reject("Invalid PE header");
    if (u16(nt+4) != 0x8664 || u16(nt+24) != 0x20b) return reject("Not an x64 PE image");
    result.timestamp = u32(nt+8);
    result.imageSize = u32(nt+24+56);
    if (result.timestamp != 0x69e8f5a1) return reject("Different Minecraft build timestamp");
    // Stock GDK image, PreLoader import patch, and LeviLauncher loader section.
    // Only these measured layouts are supported; never accept an arbitrary size.
    if (result.imageSize != 0xea93000 && result.imageSize != 0xea9e000 && result.imageSize != 0xea9f000)
        return reject("Unrecognized Minecraft/loader image size");
    auto count = u16(nt+6), optionalSize = u16(nt+20);
    if (count < 9 || count > 11 || optionalSize != 0xf0) return reject("Unrecognized PE section table");
    bool text = false;
    for (uint32_t i=0; i<count; ++i) {
        auto section = nt+24+optionalSize+40*i;
        std::array<char,8> name{};
        if (!read(section,name.data(),name.size())) return reject("Unreadable PE section table");
        if (name != std::array<char,8>{'.','t','e','x','t',0,0,0}) continue;
        text = u32(section+8)==0xac5c656 && u32(section+12)==0x1000 &&
               u32(section+16)==0xac5c800 && (u32(section+36)&0x60000020)==0x60000020;
    }
    if (!text) return reject("Different game code section layout");
    std::array<uint8_t,FacetBytes.size()> bytes{};
    if (!read(FacetRva,bytes.data(),bytes.size()) || bytes != FacetBytes)
        return reject("CreateNewWorldFacet instruction signature mismatch");
    result.supported = true;
    result.reason = "Pinned 1.26.20.04 code and facet layout verified";
    return result;
}
}
