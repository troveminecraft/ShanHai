#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace terrain_snow {
struct Plant {uint8_t column;uint16_t y,block;};
inline bool plant(int block){return block==36||block==37||block==38||block==40;}
inline void append(std::vector<uint8_t>&bytes,std::vector<Plant>const&plants){
 if(plants.empty())return;if(plants.size()>256)throw std::runtime_error("Too many snow-covered plants");
 bytes[2]=51;bytes.push_back(uint8_t(plants.size()));bytes.push_back(uint8_t(plants.size()>>8));
 for(auto p:plants){bytes.push_back(p.column);bytes.push_back(uint8_t(p.y));bytes.push_back(uint8_t(p.y>>8));bytes.push_back(uint8_t(p.block));bytes.push_back(uint8_t(p.block>>8));}
}
template<class BlockAt>
std::vector<Plant> read(std::span<uint8_t const>bytes,size_t offset,int height,size_t paletteSize,BlockAt blockAt){
 if(bytes.size()<4||bytes[0]!=75||bytes[1]!=87||(bytes[2]!=50&&bytes[2]!=51)||bytes[3]!=0||offset>bytes.size())throw std::runtime_error("Invalid snow chunk header");
 if(bytes[2]==50){if(offset!=bytes.size())throw std::runtime_error("Trailing chunk data");return {};}
 if(offset+2>bytes.size())throw std::runtime_error("Missing snow layer data");
 size_t count=bytes[offset]|(size_t(bytes[offset+1])<<8);offset+=2;
 if(!count||count>256||bytes.size()-offset!=count*5)throw std::runtime_error("Invalid snow layer count");
 std::array<bool,256> seen{};std::vector<Plant> result;result.reserve(count);
 for(size_t i=0;i<count;++i,offset+=5){Plant p{bytes[offset],uint16_t(bytes[offset+1]|(unsigned(bytes[offset+2])<<8)),uint16_t(bytes[offset+3]|(unsigned(bytes[offset+4])<<8))};
  if(p.y==0||p.y>=height||p.block>=paletteSize||!plant(p.block)||seen[p.column]||blockAt(p.column,p.y)!=146)throw std::runtime_error("Invalid snow-covered plant");
  if((p.block==38||p.block==40)&&(p.y+1>=height||blockAt(p.column,p.y+1)!=p.block+1))throw std::runtime_error("Snow-covered tall plant lost its upper half");
  seen[p.column]=true;result.push_back(p);
 }return result;
}
}
