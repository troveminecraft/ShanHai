#undef NDEBUG
#include "../ports/windows-client/CloudNative26_51.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
struct Image {
 std::vector<unsigned char> bytes;
 uint32_t u32(size_t p)const{uint32_t v=0;if(p+4<=bytes.size())std::memcpy(&v,bytes.data()+p,4);return v;}
 uint16_t u16(size_t p)const{uint16_t v=0;if(p+2<=bytes.size())std::memcpy(&v,bytes.data()+p,2);return v;}
 bool read(uint32_t rva,void* output,size_t size)const{
  size_t offset=rva;auto nt=u32(60);
  if(rva>=0x1000){bool found=false;for(unsigned i=0;i<u16(nt+6);++i){auto s=nt+24+u16(nt+20)+40*i;
   auto va=u32(s+12),len=u32(s+16);if(rva>=va&&uint64_t(rva-va)+size<=len){offset=u32(s+20)+rva-va;found=true;break;}}
   if(!found)return false;}
  if(offset>bytes.size()||size>bytes.size()-offset)return false;std::memcpy(output,bytes.data()+offset,size);return true;
 }
};
int main(int argc,char** argv){
 using namespace client::clouds::native26;assert(argc>1);
 for(int a=1;a<argc;++a){Image image;std::ifstream file(argv[a],std::ios::binary);assert(file);
  image.bytes.assign(std::istreambuf_iterator<char>(file),{});
  auto read=[&](auto rva,auto out,auto size){return image.read(rva,out,size);};assert(inspect(read));
  for(auto rva:{MeshCallRva,ExtractCallRva,SideRva,TranslationRva,OutputRva,FogRva}){
   assert(!inspect([&](uint32_t p,void* out,size_t size){auto ok=image.read(p,out,size);if(p==rva)static_cast<unsigned char*>(out)[0]^=1;return ok;}));}
  auto nt=image.u32(60);auto stamp=image.u32(nt+8);std::memset(image.bytes.data()+nt+8,0,4);assert(!inspect(read));
  std::memcpy(image.bytes.data()+nt+8,&stamp,4);assert(inspect(read));
  std::cout<<argv[a]<<": exact cloud instruction contract and 7 rejection cases passed\n";
 }
}
