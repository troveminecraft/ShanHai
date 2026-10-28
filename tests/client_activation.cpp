#undef NDEBUG
#include "../ports/windows-client/ClientActivation.h"
#include "../ports/windows-client/PinnedGameImage26_51.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

struct Image {
    std::vector<unsigned char> data;
    uint32_t u32(size_t p) const { uint32_t v{};std::memcpy(&v,data.data()+p,4);return v; }
    uint16_t u16(size_t p) const { uint16_t v{};std::memcpy(&v,data.data()+p,2);return v; }
    size_t offset(uint32_t rva,size_t count) const {
        if(rva<0x1000)return rva;
        auto nt=u32(60);
        for(unsigned i=0;i<u16(nt+6);++i){auto s=nt+24+u16(nt+20)+i*40;
            auto va=u32(s+12),size=u32(s+16);
            if(rva>=va&&uint64_t(rva-va)+count<=size)return u32(s+20)+rva-va;
        }
        return data.size();
    }
    bool valid() const {
        return client::image::inspect([&](uint32_t rva,void* out,size_t count){
            auto p=offset(rva,count);if(p>data.size()||count>data.size()-p)return false;
            std::memcpy(out,data.data()+p,count);return true;
        }).supported;
    }
    // Model the changed rel32 displacement in both actual cloud call sites.
    // No executable or player world is modified or executed by this test.
    void toggleCloudCalls(){
        for(auto rva:{client::clouds::native26::MeshCallRva,client::clouds::native26::ExtractCallRva})
            data.at(offset(rva,6)+5)^=0x80;
    }
};
int main(int argc,char** argv){
    assert(argc>1);
    for(int n=1;n<argc;++n){
        Image image;std::ifstream file(argv[n],std::ios::binary);assert(file);
        image.data.assign(std::istreambuf_iterator<char>(file),{});assert(image.valid());
        image.toggleCloudCalls();assert(!image.valid());image.toggleCloudCalls(); // 0.3.0 failure
        bool creation=false,patched=false;int checks=0;
        auto start=[&]{++checks;if(!image.valid())throw std::runtime_error("image rejected");creation=true;};
        auto stop=[&]{creation=false;};
        auto runtime=[&](auto& hooks){
            assert(creation);hooks.onRollback([&]{image.toggleCloudCalls();patched=false;});
            image.toggleCloudCalls();patched=true;
        };
        {
            terrain_runtime::HookTransaction hooks;
            client::activation::install(hooks,start,stop,runtime);
            assert(creation&&patched&&checks==1&&!image.valid());hooks.commit();
        }
        assert(creation&&patched);image.toggleCloudCalls();patched=false;stop();assert(image.valid());
        bool failed=false;
        try {
            terrain_runtime::HookTransaction hooks;
            client::activation::install(hooks,start,stop,[&](auto& h){runtime(h);throw std::runtime_error("later hook failed");});
        }catch(std::runtime_error const&){failed=true;}
        assert(failed&&!creation&&!patched&&image.valid());
        auto stamp=image.u32(60)+8;image.data[stamp]^=1;failed=false;
        try {
            terrain_runtime::HookTransaction hooks;
            client::activation::install(hooks,start,stop,[&](auto&){assert(false&&"runtime ran after invalid image");});
        }catch(std::runtime_error const&){failed=true;}
        assert(failed&&!creation&&!patched);image.data[stamp]^=1;assert(image.valid());
        std::cout<<argv[n]<<": old-order failure reproduced; activation, rollback and unsupported-image rejection passed\n";
    }
}
