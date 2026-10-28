#pragma once
#include "runtime/NativePortalGround26_51.h"
#include "NativeGeneration26_51.h"
#include "ll/api/thread/GlobalThreadPauser.h"
#include <Windows.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bedrock_compat {
class NativePortalPatch {
 unsigned char* site{};unsigned char* code{};
 std::array<unsigned char,10> replacement{};
 bool installed=false;
 static unsigned char* allocateNear(uintptr_t target){
  SYSTEM_INFO info{};GetSystemInfo(&info);auto gran=uintptr_t(info.dwAllocationGranularity);
  auto low=target>0x7fff0000?target-0x7fff0000:gran;
  auto high=(std::min)(target+0x7fff0000,uintptr_t(info.lpMaximumApplicationAddress));
  for(auto address=(low+gran-1)&~(gran-1);address<high;){
   MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)))break;
   auto next=uintptr_t(region.BaseAddress)+region.RegionSize;
   if(region.State==MEM_FREE&&next-address>=4096)
    if(auto p=VirtualAlloc(reinterpret_cast<void*>(address),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))return static_cast<unsigned char*>(p);
   if(next<=address)break;address=(next+gran-1)&~(gran-1);
  }
  throw std::runtime_error("Cannot allocate native portal relay");
 }
 void write(void const* data){
  DWORD old{},ignored{};if(!VirtualProtect(site,10,PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("Cannot patch portal height boundary");
  std::memcpy(site,data,10);FlushInstructionCache(GetCurrentProcess(),site,10);
  if(!VirtualProtect(site,10,old,&ignored))throw std::runtime_error("Cannot restore portal code protection");
 }
public:
 NativePortalPatch()=default;NativePortalPatch(NativePortalPatch const&)=delete;
 void install(void* callback){
  if(installed)throw std::runtime_error("Portal boundary already patched");
  ll::thread::GlobalThreadPauser pause;
  generationTaskEntry(0); // Check the same exact executable version and ABI.
  auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
#ifdef KARMA_CLIENT
  constexpr auto at=terrain_runtime::portal26::ClientSite,to=terrain_runtime::portal26::ClientResume;
#else
  constexpr auto at=terrain_runtime::portal26::ServerSite,to=terrain_runtime::portal26::ServerResume;
#endif
  site=base+at;
  if(std::memcmp(site,terrain_runtime::portal26::Original.data(),10)||
     std::memcmp(base+to,terrain_runtime::portal26::Resume.data(),terrain_runtime::portal26::Resume.size()))
   throw std::runtime_error("Native portal instruction boundary mismatch");
  auto bytes=terrain_runtime::portal26::relay(reinterpret_cast<uintptr_t>(callback),reinterpret_cast<uintptr_t>(base+to));
  code=allocateNear(reinterpret_cast<uintptr_t>(site));std::memcpy(code,bytes.data(),bytes.size());
  // Unwind across the callback's 184-byte stack allocation; volatile register
  // saves need no individual unwind records. The mapping lasts until exit.
  auto* function=reinterpret_cast<RUNTIME_FUNCTION*>(code+0x300);
  *function={0,static_cast<DWORD>(bytes.size()),0x320};
  const unsigned char unwind[]{1,7,2,0,7,1,23,0};std::memcpy(code+0x320,unwind,sizeof(unwind));
  std::memcpy(code+0x340,&code,sizeof(code));DWORD ignored{};
  if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&ignored))throw std::runtime_error("Cannot protect native portal relay");
  if(!RtlAddFunctionTable(function,1,reinterpret_cast<DWORD64>(code)))throw std::runtime_error("Cannot register portal relay unwind data");
  FlushInstructionCache(GetCurrentProcess(),code,bytes.size());
  auto delta=int64_t(reinterpret_cast<uintptr_t>(code+0x340))-int64_t(reinterpret_cast<uintptr_t>(site+6));
  if(delta<std::numeric_limits<int32_t>::min()||delta>std::numeric_limits<int32_t>::max())throw std::runtime_error("Portal relay displacement overflow");
  replacement={0xff,0x15,0,0,0,0,0x90,0x90,0x90,0x90};auto relative=int32_t(delta);std::memcpy(replacement.data()+2,&relative,4);
  installed=true;try{write(replacement.data());}catch(...){if(!std::memcmp(site,replacement.data(),10))remove();else installed=false;throw;}
 }
 void remove(){
  if(!installed)return;ll::thread::GlobalThreadPauser pause;
  if(std::memcmp(site,replacement.data(),10))throw std::runtime_error("Portal boundary changed by another module");
  write(terrain_runtime::portal26::Original.data());installed=false;
  // Unload occurs after structure workers are quiescent. Keep the page mapped
  // with only original instructions, so no module callback remains reachable.
  DWORD old{},ignored{};if(!VirtualProtect(code,4096,PAGE_READWRITE,&old))throw std::runtime_error("Cannot retire portal relay");
  RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(code+0x300));
  std::memcpy(code,terrain_runtime::portal26::Original.data(),10);code[10]=0xc3;
  if(!VirtualProtect(code,4096,old,&ignored))throw std::runtime_error("Cannot protect retired portal relay");
  FlushInstructionCache(GetCurrentProcess(),code,11);
 }
};
}
