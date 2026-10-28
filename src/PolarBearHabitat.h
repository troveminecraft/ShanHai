#pragma once
namespace glacier_ecology {
// Frozen rivers also represent the generator's frozen lakes. A snowy biome
// alone is insufficient: ordinary glaciers must have dense glacier ice below.
constexpr bool polarBearHabitat(bool frozenOcean, bool frozenRiver,
                                bool glacierBiome, bool denseIce) {
    return !frozenRiver && (frozenOcean || (glacierBiome && denseIce));
}
constexpr bool restrictPolarBear(bool ownWorld, bool overworld, bool natural, bool spawner) {
    return ownWorld && overworld && natural && !spawner;
}
}
