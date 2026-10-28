#pragma once
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace client::clouds {
// The pinned call sites already have a normal x64 call frame: RCX=Dimension*,
// R13=LevelRendererPlayer*, RAX=native getter. Only volatile argument registers
// change. The leaf relay does not alter the stack or nonvolatile registers.
class NativeCloudCallsite {
    unsigned char* site{};
    unsigned char* relay{};
    std::array<unsigned char,6> original{}, replacement{};
    bool installed=false;
    static unsigned char* allocateNear(uintptr_t target) {
        SYSTEM_INFO info{}; GetSystemInfo(&info);
        auto gran=uintptr_t(info.dwAllocationGranularity);
        auto low=target>0x7fff0000 ? target-0x7fff0000 : gran;
        auto high=(std::min)(target+0x7fff0000,uintptr_t(info.lpMaximumApplicationAddress));
        for(auto address=(low+gran-1)&~(gran-1);address<high;) {
            MEMORY_BASIC_INFORMATION region{};
            if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)))break;
            auto next=uintptr_t(region.BaseAddress)+region.RegionSize;
            if(region.State==MEM_FREE) {
                auto candidate=(address+gran-1)&~(gran-1);
                if(candidate+4096<=next && candidate+4096<high)
                    if(auto p=VirtualAlloc(reinterpret_cast<void*>(candidate),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))
                        return static_cast<unsigned char*>(p);
            }
            if(next<=address)break;
            address=(next+gran-1)&~(gran-1);
        }
        throw std::runtime_error("No nearby page for the pinned cloud call site");
    }
    void write(std::array<unsigned char,6> const& bytes) {
        DWORD protection{};
        if(!VirtualProtect(site,bytes.size(),PAGE_EXECUTE_READWRITE,&protection))
            throw std::runtime_error("Cannot write the pinned cloud call site");
        std::memcpy(site,bytes.data(),bytes.size());
        FlushInstructionCache(GetCurrentProcess(),site,bytes.size());
        DWORD ignored{};
        if(!VirtualProtect(site,bytes.size(),protection,&ignored))
            throw std::runtime_error("Cannot restore cloud code protection");
    }
public:
    NativeCloudCallsite()=default;
    NativeCloudCallsite(NativeCloudCallsite const&)=delete;
    void install(void* address,std::array<unsigned char,6> expected,void* callback,unsigned stage) {
        if(installed)throw std::runtime_error("Cloud call site already installed");
        site=static_cast<unsigned char*>(address);original=expected;
        if(std::memcmp(site,original.data(),original.size())!=0)
            throw std::runtime_error("Cloud call site was changed by another module");
        relay=allocateNear(uintptr_t(site));
        // mov rdx,r13; mov r8d,stage; mov rax,callback; jmp rax.
        std::array<unsigned char,21> code{0x4c,0x89,0xea,0x41,0xb8,0,0,0,0,0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
        std::memcpy(code.data()+5,&stage,4);std::memcpy(code.data()+11,&callback,8);
        std::memcpy(relay,code.data(),code.size());
        std::memcpy(relay+32,&relay,8);
        DWORD ignored{};
        if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&ignored)) {
            VirtualFree(relay,0,MEM_RELEASE);relay=nullptr;
            throw std::runtime_error("Cannot protect the cloud relay");
        }
        FlushInstructionCache(GetCurrentProcess(),relay,code.size());
        auto delta=int64_t(uintptr_t(relay+32))-int64_t(uintptr_t(site+6));
        if(delta<std::numeric_limits<int32_t>::min()||delta>std::numeric_limits<int32_t>::max())
            throw std::runtime_error("Cloud relay exceeds relative call range");
        replacement={0xff,0x15,0,0,0,0};auto displacement=int32_t(delta);
        std::memcpy(replacement.data()+2,&displacement,4);
        installed=true; // Removal can roll back even if protection restoration fails.
        try {write(replacement);} catch(...) {
            if(std::memcmp(site,replacement.data(),replacement.size())==0)remove();
            else installed=false;
            throw;
        }
    }
    void remove() {
        if(!installed)return;
        if(std::memcmp(site,replacement.data(),replacement.size())!=0)
            throw std::runtime_error("Cloud call site changed while installed; refusing to overwrite it");
        write(original);installed=false;
        // Called only after the world has closed, with render threads paused.
        // Keep the relay mapped until process exit and remove its module pointer.
        DWORD protection{},ignored{};
        if(!VirtualProtect(relay,4096,PAGE_READWRITE,&protection))
            throw std::runtime_error("Cannot retire cloud relay");
        relay[0]=0xff;relay[1]=0xe0; // jmp original getter still in RAX
        std::memset(relay+2,0xcc,30);
        if(!VirtualProtect(relay,4096,protection,&ignored))
            throw std::runtime_error("Cannot protect retired cloud relay");
        FlushInstructionCache(GetCurrentProcess(),relay,32);
    }
};
}
