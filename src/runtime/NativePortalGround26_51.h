#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace terrain_runtime::portal26 {
// Both pinned builds keep the piece in RBX at this inlined helper boundary.
// Skip exactly height selection/support descent, before template placement.
inline constexpr uint32_t ServerSite=0x835d169,ServerResume=0x835d5ca;
inline constexpr uint32_t ClientSite=0x6a90fd9,ClientResume=0x6a9143a;
inline constexpr std::array<unsigned char,10> Original{0x48,0x8b,0x01,0x48,0x8b,0x80,0x78,0x01,0x00,0x00};
inline constexpr std::array<unsigned char,30> Resume{
 0x44,0x0f,0x2e,0xc6,0x0f,0x97,0x85,0x38,0x04,0x00,0x00,0x44,0x8b,0x72,0x4c,
 0x8b,0x5a,0x40,0x8b,0x72,0x48,0x8b,0x7a,0x54,0x44,0x89,0xf1,0x29,0xd9,0x29};
// Callback is noexcept bool(piece): false keeps native selection; true moves
// the piece bounds and jumps over the native helper without extra RNG draws.
inline std::vector<unsigned char> relay(uintptr_t callback,uintptr_t resume){
 std::vector<unsigned char> b;
 auto emit=[&](std::initializer_list<unsigned char> v){b.insert(b.end(),v);};
 auto u64=[&](uintptr_t v){for(unsigned i=0;i<8;++i)b.push_back(static_cast<unsigned char>(v>>(i*8)));};
 // One fixed stack frame keeps Windows unwind metadata valid throughout.
 emit({0x48,0x81,0xec,0xb8,0,0,0});
 auto registers=[&](bool load){
  // RAX, RCX, RDX, R8, R9, R10, R11. XMM0..5 and shadow space precede them.
  for(unsigned i=0;i<7;++i){unsigned reg=i<3?i:i+5;
   emit({static_cast<unsigned char>(reg>=8?0x4c:0x48),static_cast<unsigned char>(load?0x8b:0x89),static_cast<unsigned char>(0x84+(reg%8)*8),0x24});
   uint32_t offset=0x80+i*8;for(unsigned j=0;j<4;++j)b.push_back(static_cast<unsigned char>(offset>>(j*8)));
  }
 };
 registers(false);
 for(unsigned i=0;i<6;++i)emit({0xf3,0x0f,0x7f,static_cast<unsigned char>(0x44+i*8),0x24,static_cast<unsigned char>(0x20+i*16)});
 emit({0x48,0x89,0xd9,0x48,0xb8});u64(callback);emit({0xff,0xd0,0x84,0xc0,0x0f,0x84});
 auto branch=b.size();emit({0,0,0,0});
 auto restore=[&]{
  for(unsigned i=0;i<6;++i)emit({0xf3,0x0f,0x6f,static_cast<unsigned char>(0x44+i*8),0x24,static_cast<unsigned char>(0x20+i*16)});
  registers(true);
 };
 // Redirect the return address before the conventional add-rsp/ret epilogue.
 emit({0x48,0xb8});u64(resume);emit({0x48,0x89,0x84,0x24,0xb8,0,0,0});
 restore();emit({0x48,0x89,0xda,0x48,0x81,0xc4,0xb8,0,0,0,0xc3});
 auto displacement=static_cast<int32_t>(b.size()-branch-4);std::memcpy(b.data()+branch,&displacement,4);
 restore();b.insert(b.end(),Original.begin(),Original.end());emit({0x48,0x81,0xc4,0xb8,0,0,0,0xc3});
 return b;
}
}
