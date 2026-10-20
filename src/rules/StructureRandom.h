#pragma once
#include "StructureRules.h"
#include <cstdint>
#include <random>
#include <stdexcept>
namespace worldgen_rules {
struct ChunkPosition {int x,z;bool operator==(ChunkPosition const&)const=default;};
inline uint32_t structureSeed32(uint64_t worldSeed){return uint32_t(worldSeed);}
// Pinned Bedrock Random semantics: bound uses all 32 bits; unbounded int uses
// the top 31 bits, double consumes one unsigned word scaled by 2^-32.
class StructureRandom {
 std::mt19937 engine;
public:
 explicit StructureRandom(uint32_t seed):engine(seed){}
 int nextInt(){return int(engine()>>1);}
 int nextInt(int bound){if(bound<0)throw std::invalid_argument("Negative structure random bound");return bound?int(engine()%uint32_t(bound)):0;}
 double nextDouble(){return double(engine())/4294967296.;}
};
inline uint32_t placementSeed(int x,int z,int salt,uint32_t seed){return uint32_t(x)*0x9939f508u+uint32_t(z)*0xf1565bd5u+seed+uint32_t(salt);}
inline int spacingCell(int value,int spacing){return value>=0?value/spacing:(value-spacing+1)/spacing;}
// LLBDS uses getChunkPosInSpace(..., tiltedSpacing=false). MT19937 and
// unsigned modulo match its Random::nextInt(n), including negative cells.
inline ChunkPosition candidateForCell(int x,int z,uint32_t seed,int spacing,int separation,int salt){
 if(spacing<1||separation<0||separation>=spacing)throw std::invalid_argument("Invalid structure spacing");
 std::mt19937 random(placementSeed(x,z,salt,seed));auto bound=uint32_t(spacing-separation);
 int dx=int(random()%bound),dz=int(random()%bound);return {x*spacing+dx,z*spacing+dz};
}
inline ChunkPosition candidateForChunk(int x,int z,uint32_t seed,int spacing,int separation,int salt){return candidateForCell(spacingCell(x,spacing),spacingCell(z,spacing),seed,spacing,separation,salt);}
inline std::array<uint32_t,624> initialRandomState(uint32_t seed){
 std::array<uint32_t,624> state;state[0]=seed;
 for(uint32_t i=1;i<624;++i)state[i]=1812433253u*(state[i-1]^(state[i-1]>>30))+i;
 return state;
}
}
