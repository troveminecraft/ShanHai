#pragma once
#include "StructureRandom.h"
#include <algorithm>
#include <array>
#include <climits>
#include <vector>
namespace worldgen_rules {
// The identical LLBDS scratch geology on all products, without a second full
// vegetation/ore/decorations pass. Columns are [z*16+x][y+64]. Native adapters
// translate state IDs and biome IDs into their own volume representation.
struct FrozenCaptureGround {
 int height=384;
 std::vector<uint16_t>blocks=std::vector<uint16_t>(256*384);
 std::array<uint16_t,256>biomes{};std::array<int16_t,256>heights{};
};
template<class Field> FrozenCaptureGround buildFrozenCaptureGround(Field&field,ChunkPosition cp){
 int64_t x0=int64_t(cp.x)*16,z0=int64_t(cp.z)*16;
 if(x0< -30000000||z0< -30000000||x0+15>=30000000||z0+15>=30000000)throw std::runtime_error("Native capture ground outside game coordinates");
 FrozenCaptureGround out;
 if constexpr(requires{field.generationHeight();})out.height=field.generationHeight();
 if(out.height!=384&&out.height!=576)throw std::runtime_error("Unsupported native capture height");
 out.blocks.resize(256*out.height);
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){int wx=int(x0+x)+32768,wz=int(z0+z)+32768;auto s=field.surface(wx,wz);if(s.biome<0||s.biome>65535||s.height< -64||s.height>out.height-65)throw std::runtime_error("Invalid native capture surface");
  auto col=size_t(z*16+x);auto*blocks=out.blocks.data()+col*out.height;std::fill_n(blocks,4,uint16_t(1));field.materialColumn(s,wx,wz,out.height-65,blocks);out.biomes[col]=uint16_t(s.biome);out.heights[col]=int16_t(s.height+1);
 }return out;
}
}
