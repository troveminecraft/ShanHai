#pragma once
namespace terrain_runtime {
// Keep SDK-free queue/recovery policies in the 26.20 semantic layout.
// 26.51 removed the two neighbour-upgrade stages before lighting.
template<int Loaded> struct ChunkStateLayout {
    static_assert(Loaded == 11 || Loaded == 13, "Review the native chunk lifecycle for this SDK");
    static constexpr int canonical(int raw) {
        if constexpr (Loaded == 11) return raw >= 8 && raw <= 11 ? raw + 2 : raw;
        return raw;
    }
};
}
