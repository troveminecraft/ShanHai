#undef NDEBUG
#include "runtime/NativePortalGround26_51.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <Windows.h>
static void verifyUnwind(std::vector<unsigned char> const& bytes){
 auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(memory);
 std::memcpy(memory,bytes.data(),bytes.size());
 auto* function=reinterpret_cast<RUNTIME_FUNCTION*>(memory+0x300);
 *function={0,static_cast<DWORD>(bytes.size()),0x320};
 const unsigned char unwind[]{1,7,2,0,7,1,23,0};std::memcpy(memory+0x320,unwind,sizeof(unwind));
 assert(RtlAddFunctionTable(function,1,reinterpret_cast<DWORD64>(memory)));
 alignas(16) std::array<unsigned char,1024> stack{};
 auto entry=reinterpret_cast<DWORD64>(stack.data()+512+8);
 *reinterpret_cast<DWORD64*>(entry)=0x12345678;
 auto check=[&](size_t offset,bool allocated){
  CONTEXT context{};context.ContextFlags=CONTEXT_FULL;context.Rip=reinterpret_cast<DWORD64>(memory+offset);
  context.Rsp=entry-(allocated?184:0);context.Rbx=0x1234;context.Rbp=0x5678;context.Rsi=0x9012;context.R12=0x3456;
  PVOID handler{};DWORD64 frame{};
  RtlVirtualUnwind(UNW_FLAG_NHANDLER,reinterpret_cast<DWORD64>(memory),context.Rip,function,&context,&handler,&frame,nullptr);
  assert(context.Rip==0x12345678&&context.Rsp==entry+8);
  assert(context.Rbx==0x1234&&context.Rbp==0x5678&&context.Rsi==0x9012&&context.R12==0x3456);
 };
 check(0,false);check(7,true);check(100,true);
 std::array<unsigned char,8> epilogue{0x48,0x81,0xc4,0xb8,0,0,0,0xc3};unsigned count=0;
 for(size_t i=0;i+8<=bytes.size();++i)if(!std::memcmp(bytes.data()+i,epilogue.data(),8)){
  check(i,true);check(i+7,false);++count;
 }
 assert(count==2);assert(RtlDeleteFunctionTable(function));assert(VirtualFree(memory,0,MEM_RELEASE));
 std::cout<<"Windows unwind: entry, body, both branch epilogues passed\n";
}
int main(int argc,char**argv){
 using namespace terrain_runtime::portal26;assert(argc==4);
 for(int n=1;n<=2;++n){
  std::ifstream f(argv[n],std::ios::binary);assert(f);std::vector<unsigned char>b{std::istreambuf_iterator<char>(f),{}};
  auto u16=[&](size_t p){uint16_t v{};std::memcpy(&v,b.data()+p,2);return v;};
  auto u32=[&](size_t p){uint32_t v{};std::memcpy(&v,b.data()+p,4);return v;};
  auto nt=u32(60);
  auto matches=[&](uint32_t rva,auto const& expected){
   for(unsigned i=0;i<u16(nt+6);++i){auto s=nt+24+u16(nt+20)+40*i;auto va=u32(s+12),size=u32(s+16);
    if(rva>=va&&uint64_t(rva-va)+expected.size()<=size)return !std::memcmp(b.data()+u32(s+20)+rva-va,expected.data(),expected.size());
   }return false;
  };
  assert(matches(n==1?ServerSite:ClientSite,Original));assert(matches(n==1?ServerResume:ClientResume,Resume));
 }
 auto code=relay(0x4000000,0x5000000);verifyUnwind(code);std::ofstream f(argv[3],std::ios::binary);f.write(reinterpret_cast<char const*>(code.data()),code.size());assert(f);
 std::cout<<"Both native portal boundaries verified; relay fixture emitted\n";
}
