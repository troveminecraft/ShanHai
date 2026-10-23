#pragma once
#include "DerivedCache.h"
#include <array>
#include <memory>
namespace terrain_runtime {
// KW2 stores column-relative runs. Height comes from the immutable world profile
// (legacy 384 or expanded 576), and every decoded column must match it exactly.
inline bool supportedChunkHeight(int height){return height==384||height==576;}
inline void validateColumnRun(int y,int len,int block,size_t paletteSize,int height=384){
 if(!supportedChunkHeight(height)||y<0||y>=height||len<=0||len>height-y||block<0||size_t(block)>=paletteSize)
  throw std::runtime_error("Invalid decoded block column");
}
struct Chunk {std::array<uint8_t,256> biomes;std::array<uint32_t,256> columns;std::vector<uint8_t> runs;int height=384;};
inline Chunk decodeChunk(std::vector<uint8_t>const&bytes,size_t paletteSize,size_t biomeSize,int expectedHeight=384){
 Chunk c{};if(!supportedChunkHeight(expectedHeight))throw std::runtime_error("Unsupported decoded chunk height");c.height=expectedHeight;if(paletteSize>65536)throw std::runtime_error("Palette exceeds uint16");if(bytes.size()<260||bytes[0]!=75||bytes[1]!=87||bytes[2]!=50||bytes[3]!=0)throw std::runtime_error("Unsupported chunk codec");if(bytes.size()<260)throw std::runtime_error("Missing biome data");
 for(int i=0;i<256;i++){c.biomes[i]=bytes[i+4];if(bytes[i+4]>=biomeSize)throw std::runtime_error("Invalid biome ID");}
 c.runs.assign(bytes.begin()+260,bytes.end());size_t q=0;
 for(auto&col:c.columns){col=uint32_t(q);if(q+2>c.runs.size())throw std::runtime_error("Missing column");int n=c.runs[q]|(c.runs[q+1]<<8);q+=2;int y=0;
  for(int i=0;i<n;i++){if(q+4>c.runs.size())throw std::runtime_error("Missing run");int len=c.runs[q]|(c.runs[q+1]<<8);int block=c.runs[q+2]|(c.runs[q+3]<<8);validateColumnRun(y,len,block,paletteSize,expectedHeight);y+=len;q+=4;}
  if(y!=expectedHeight)throw std::runtime_error("Invalid column height");
 }
 if(q!=c.runs.size())throw std::runtime_error("Trailing chunk data");return c;
}
struct CachedChunk {terrain_cache::ReadState state;std::shared_ptr<Chunk const> value;};
inline CachedChunk readChunk(std::filesystem::path const&file,size_t paletteSize,size_t biomeSize,int expectedHeight=384){
 auto read=terrain_cache::readDerived(file);if(read.state!=terrain_cache::ReadState::Valid)return {read.state,{}};
 try{return {read.state,std::make_shared<Chunk const>(decodeChunk(read.bytes,paletteSize,biomeSize,expectedHeight))};}catch(std::exception const&){return {terrain_cache::ReadState::Invalid,{}};}
}
}
