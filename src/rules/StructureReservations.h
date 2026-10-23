#pragma once
#include <string_view>
#include <span>
#include <cstdint>
#include <stdexcept>
namespace worldgen_rules {
inline bool structureReservationPlant(std::string_view n){return n.ends_with("_log")||n.ends_with("_wood")||n.ends_with("_leaves")||n=="minecraft:vine"||n=="minecraft:bamboo"||n=="minecraft:cocoa"||n=="minecraft:short_grass"||n=="minecraft:tall_grass"||n=="minecraft:fern"||n=="minecraft:large_fern"||n=="minecraft:deadbush";}
inline bool structureReservationCandidate(int before,int after){
 if(before==after)return false;
 // ID 2 is also geological stone. Only a disappeared procedural feature is
 // eligible; unchanged substrate must never enter the removal path.
 if(before==2)return after==0;
 return (before>=10&&before<=17)||before==25||before==26||before==37||before==40||before==41||before==61||before==65||before==66||before==69||before==70||(before>=75&&before<=80)||(before>=82&&before<=84)||(before>=89&&before<=93)||before==120||(before>=149&&before<=167);
}
inline bool structureReservationRemoves(int before,int after,std::string_view actual){
 if(!structureReservationCandidate(before,after))return false;
 if(before==2)return actual=="minecraft:stone";
 return structureReservationPlant(actual);
}
inline int vegetationColumnHeight(std::span<uint16_t const> before,std::span<uint16_t const> after){
 if(before.size()!=after.size()||(before.size()!=256*384&&before.size()!=256*576))
  throw std::runtime_error("Structure vegetation column height mismatch");
 return int(before.size()/256);
}
// Engine vegetation buffers are X/Z columns with Y contiguous. Use the same
// validated traversal in the native-volume prefill and live-world repair paths.
template<class Visit> void visitVegetationReservations(std::span<uint16_t const> before,
                                                     std::span<uint16_t const> after,Visit visit){
 int height=vegetationColumnHeight(before,after);
 for(int z=0;z<16;++z)for(int x=0;x<16;++x)for(int y=4;y<height;++y){
  size_t i=size_t(z*16+x)*height+y;
  if(structureReservationCandidate(before[i],after[i]))visit(x,y,z,before[i],after[i]);
 }
}
}
