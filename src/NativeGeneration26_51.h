#pragma once
#include "runtime/NativeGenerationImage26_51.h"
#include <Windows.h>
#include <stdexcept>
namespace bedrock_compat {
inline uintptr_t generationTaskEntry(unsigned stage){
#ifdef KARMA_CLIENT
 constexpr auto platform=terrain_runtime::native26::Platform::Client;
#else
 constexpr auto platform=terrain_runtime::native26::Platform::Server;
#endif
 auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 auto read=[&](uint32_t rva,void* out,size_t size){
  SIZE_T count{};return rva<0x13000000&&size<=0x13000000-rva&&
   ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void const*>(base+rva),out,size,&count)&&count==size;
 };
 if(stage>=4||!terrain_runtime::native26::inspect(platform,read))
  throw std::runtime_error("Unsupported native chunk task image/layout (expected 26.51)");
 return base+terrain_runtime::native26::tasks(platform)[stage].rva;
}
}
