#undef NDEBUG
#include "../ports/windows-client/PinnedGameImage.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
using client::image::inspect;
// Translate RVA reads through the file's section table; no game process needed.
struct Image {
    std::vector<uint8_t> bytes;
    uint32_t u32(size_t offset) const { uint32_t v=0; if(offset+4<=bytes.size())std::memcpy(&v,bytes.data()+offset,4);return v; }
    uint16_t u16(size_t offset) const { uint16_t v=0; if(offset+2<=bytes.size())std::memcpy(&v,bytes.data()+offset,2);return v; }
    bool read(uint32_t rva,void* out,size_t size) const {
        size_t offset=rva;auto nt=u32(60);
        if(rva>=0x1000){bool found=false;
            for(unsigned i=0;i<u16(nt+6);++i){size_t s=nt+24+u16(nt+20)+40*i;
                auto va=u32(s+12),len=u32(s+16);
                if(rva>=va&&uint64_t(rva-va)+size<=len){offset=u32(s+20)+(rva-va);found=true;break;}}
            if(!found)return false;
        }
        if(offset>bytes.size()||size>bytes.size()-offset)return false;
        std::memcpy(out,bytes.data()+offset,size);return true;
    }
    auto check() const {return inspect([&](auto rva,auto out,auto size){return read(rva,out,size);});}
    void set32(size_t offset,uint32_t v){std::memcpy(bytes.data()+offset,&v,4);}
};
int main(int argc,char**argv){
    assert(argc>=2);nlohmann::json report=nlohmann::json::array();
    for(int arg=1;arg<argc;++arg){Image image;std::ifstream f(argv[arg],std::ios::binary);
        assert(f);image.bytes.assign(std::istreambuf_iterator<char>(f),{});
        auto result=image.check();assert(result.supported);auto nt=image.u32(60);
        image.set32(nt+8,0);assert(!image.check().supported);image.set32(nt+8,result.timestamp);
        image.set32(nt+24+56,0xea9d000);assert(!image.check().supported);image.set32(nt+24+56,result.imageSize);
        auto badSignature=inspect([&](uint32_t rva,void*out,size_t size){bool ok=image.read(rva,out,size);if(rva==client::image::FacetRva)static_cast<uint8_t*>(out)[0]^=1;return ok;});
        assert(!badSignature.supported);
        auto section=nt+24+image.u16(nt+20);auto old=image.u32(section+8);image.set32(section+8,old-1);assert(!image.check().supported);image.set32(section+8,old);
        assert(image.check().supported);
        report.push_back({{"path",argv[arg]},{"supported",true},{"timestamp",result.timestamp},{"imageSize",result.imageSize},{"negativeCases",4}});
    }std::cout<<report.dump(2)<<std::endl;
}
