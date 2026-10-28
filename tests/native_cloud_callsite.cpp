#undef NDEBUG
#include "../ports/windows-client/NativeCloudCallsite.h"
#include "../ports/windows-client/CloudHeightPolicy.h"
#include <cassert>
#include <iostream>
namespace {
int dimension, camera;
unsigned seenStage=99,calls=0;
bool active=true;
short original(void* p){assert(p==&dimension);return 193;}
short replacement(void* p,void* c,unsigned stage){
    assert(p==&dimension&&c==&camera);seenStage=stage;++calls;
    return active?client::clouds::Height:original(p);
}
}
int main(){
    // Execute a standalone x64 fixture with the exact observed call-site ABI.
    // No Minecraft process, game object, world or runtime DLL is involved.
    auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    assert(code);
    std::array<unsigned char,32> body{0x41,0x55,0x48,0x83,0xec,0x20,0x49,0x89,0xd5,
        0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0x15,0,0,0,0,0x48,0x83,0xc4,0x20,0x41,0x5d,0xc3};
    auto getter=&original;std::memcpy(body.data()+11,&getter,8);
    int32_t slotDelta=64-25;std::memcpy(body.data()+21,&slotDelta,4);
    std::memcpy(code,body.data(),body.size());std::memcpy(code+64,&getter,8);
    DWORD ignored{};assert(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&ignored));
    FlushInstructionCache(GetCurrentProcess(),code,body.size());
    auto run=reinterpret_cast<short(*)(void*,void*)>(code);
    assert(run(&dimension,&camera)==193);
    std::array<unsigned char,6> expected{};std::memcpy(expected.data(),code+19,6);
    for(unsigned stage=0;stage<2;++stage){
        client::clouds::NativeCloudCallsite patch;
        patch.install(code+19,expected,reinterpret_cast<void*>(&replacement),stage);
        active=true;for(int i=0;i<10000;++i)assert(run(&dimension,&camera)==500);
        assert(seenStage==stage);
        active=false;assert(run(&dimension,&camera)==193);
        patch.remove();assert(run(&dimension,&camera)==193);
        assert(std::memcmp(code+19,expected.data(),6)==0);
    }
    client::clouds::NativeCloudCallsite rejected;auto bad=expected;bad[0]^=1;
    bool threw=false;try{rejected.install(code+19,bad,reinterpret_cast<void*>(&replacement),0);}catch(std::runtime_error const&){threw=true;}
    assert(threw&&run(&dimension,&camera)==193);
    assert(calls==20002);VirtualFree(code,0,MEM_RELEASE);
    std::cout<<"Native cloud call ABI, fallback, restoration and signature rejection passed\n";
}
