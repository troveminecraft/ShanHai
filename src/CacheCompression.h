#pragma once
#include <Windows.h>
#include <compressapi.h>
#include <cstdint>
#include <stdexcept>
#include <vector>
#pragma comment(lib,"cabinet.lib")
namespace terrain_cache {
inline constexpr size_t MaxDerivedBytes=1024*1024-64;
struct CompressionHandles {
 COMPRESSOR_HANDLE compressor=nullptr;DECOMPRESSOR_HANDLE decompressor=nullptr;
 ~CompressionHandles(){if(compressor)CloseCompressor(compressor);if(decompressor)CloseDecompressor(decompressor);}
};
inline CompressionHandles& compressionHandles(){thread_local CompressionHandles handles;return handles;}
inline std::vector<uint8_t> compressDerived(std::vector<uint8_t>const&bytes){
 if(bytes.size()>MaxDerivedBytes)throw std::runtime_error("Derived cache too large");
 auto&h=compressionHandles();if(!h.compressor&&!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&h.compressor))return {};
 std::vector<uint8_t> out(bytes.size());SIZE_T size=0;
 if(!Compress(h.compressor,bytes.data(),bytes.size(),out.data(),out.size(),&size)||size+8>=bytes.size())return {};
 out.resize(size);return out;
}
inline std::vector<uint8_t> decompressDerived(std::vector<uint8_t>const&packed,size_t size){
 if(size<256||size>MaxDerivedBytes)throw std::runtime_error("Invalid derived cache length");
 auto&h=compressionHandles();if(!h.decompressor&&!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&h.decompressor))throw std::runtime_error("Cache decompressor unavailable");
 std::vector<uint8_t> out(size);SIZE_T actual=0;
 if(!Decompress(h.decompressor,packed.data(),packed.size(),out.data(),out.size(),&actual)||actual!=size)throw std::runtime_error("Corrupt compressed terrain cache");
 return out;
}
}
