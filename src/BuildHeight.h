#pragma once
#include "ChunkCodec.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <span>
#include <stdexcept>

namespace terrain_runtime {
// Persisted per world. Missing fields identify the original -64..319 profile;
// loading a newer binary must never silently expand an older world.
struct BuildHeight {
 static constexpr int MinY=-64,LegacyMaxYExclusive=320,ExpandedMaxYExclusive=512;
 int maxYExclusive=LegacyMaxYExclusive;
 int generatedMaxYExclusive=LegacyMaxYExclusive;
 int height() const {return maxYExclusive-MinY;}
 int generatedHeight() const {return generatedMaxYExclusive-MinY;}
 int subchunks() const {return height()/16;}
 size_t blocks() const {return size_t(height())*256;}
 bool contains(int y) const {return y>=MinY&&y<maxYExclusive;}
 static BuildHeight read(nlohmann::json const& profile){
  auto readMax=[&](char const* field){
   if(!profile.contains(field))return LegacyMaxYExclusive;
   auto const& value=profile.at(field);
   // Compare before narrowing: reject floats, unsigned overflow and strings.
   if(!value.is_number_integer()||(value!=LegacyMaxYExclusive&&value!=ExpandedMaxYExclusive))
    throw std::runtime_error(std::string("Unsupported world height field: ")+field);
   return value.get<int>();
  };
  BuildHeight result{readMax("buildMaxYExclusive"),readMax("generatedMaxYExclusive")};
  if(result.generatedMaxYExclusive>result.maxYExclusive)
   throw std::runtime_error("Generated height exceeds world build height");
  return result;
 }
 void requireDimension(int minY,int maxY) const {
  if(minY!=MinY||maxY!=maxYExclusive)
   throw std::runtime_error("Dimension height disagrees with saved world profile");
 }
 void requireNativeRecipe(nlohmann::json const& data,nlohmann::json const& profile) const {
  auto const& config=data.at("config");
  nlohmann::json heights={{"buildMaxYExclusive",maxYExclusive}};
  if(config.contains("generatedMaxYExclusive"))heights["generatedMaxYExclusive"]=config.at("generatedMaxYExclusive");
  if(read(heights).generatedMaxYExclusive!=generatedMaxYExclusive)
   throw std::runtime_error("Native recipe generated height disagrees with world profile");
  if(profile.contains("maxY")&&(!profile.at("maxY").is_number_integer()||profile.at("maxY")!=generatedMaxYExclusive-1))
   throw std::runtime_error("World profile maxY disagrees with generated height");
 }
};

// Fill precisely the native volume supplied by the caller. The index callback
// uses the native BlockVolume layout in production; tests cover both layouts.
// All extra construction space is initialized to air before decoded runs copy.
template<class Value,class Index>
void fillGeneratedBlockVolume(std::span<Value> blocks,std::span<Value const> palette,
                              Chunk const& chunk,BuildHeight const& bounds,Index index){
 if(blocks.size()!=bounds.blocks()||palette.empty()||chunk.height!=bounds.generatedHeight())
  throw std::runtime_error("Block volume/profile height mismatch");
 std::fill(blocks.begin(),blocks.end(),palette[0]);
 auto base=size_t(index(0,0,0)),next=size_t(index(0,1,0));
 if(next<=base)throw std::runtime_error("Invalid block volume Y stride");
 auto stride=next-base;
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  size_t p=chunk.columns[z*16+x];
  if(p>chunk.runs.size()||chunk.runs.size()-p<2)throw std::runtime_error("Missing decoded column");
  int count=chunk.runs[p]|(chunk.runs[p+1]<<8);p+=2;int y=0;
  for(int r=0;r<count;r++){
   if(p>chunk.runs.size()||chunk.runs.size()-p<4)throw std::runtime_error("Missing decoded run");
   int len=chunk.runs[p]|(chunk.runs[p+1]<<8),block=chunk.runs[p+2]|(chunk.runs[p+3]<<8);p+=4;
   validateColumnRun(y,len,block,palette.size(),chunk.height);
   auto first=size_t(index(x,y,z));
   if(first>=blocks.size()||size_t(len-1)>(blocks.size()-1-first)/stride)
    throw std::runtime_error("Block volume index out of bounds");
   if(stride==1)std::fill_n(blocks.begin()+first,len,palette[block]);
   else for(int j=0;j<len;j++)blocks[first+size_t(j)*stride]=palette[block];
   y+=len;
  }
  if(y!=chunk.height)throw std::runtime_error("Incomplete decoded column");
 }
}

template<class Value>
void fillBiomeSubchunk(std::span<Value> destination,std::span<Value const> palette,Chunk const& chunk){
 if(destination.size()!=4096)throw std::runtime_error("Invalid biome subchunk size");
 for(int x=0;x<16;x++)for(int z=0;z<16;z++){
  auto id=chunk.biomes[z*16+x];
  if(id>=palette.size())throw std::runtime_error("Biome index out of bounds");
  for(int y=0;y<16;y++)destination[(x<<8)|(z<<4)|y]=palette[id];
 }
}
}
