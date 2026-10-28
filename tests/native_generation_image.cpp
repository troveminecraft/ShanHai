#undef NDEBUG
#include "runtime/NativeGenerationImage26_51.h"
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
 using namespace terrain_runtime::native26;assert(argc>2);
 for(int a=1;a<argc;++a){Image image;std::ifstream file(argv[a],std::ios::binary);assert(file);
  image.bytes.assign(std::istreambuf_iterator<char>(file),{});
  auto platform=a==1?Platform::Server:Platform::Client;
  auto read=[&](auto rva,auto out,auto size){return image.read(rva,out,size);};assert(inspect(platform,read));
  assert(!inspect(platform==Platform::Client?Platform::Server:Platform::Client,read));
  unsigned rejected=1;
  for(auto const& entry:tasks(platform))for(unsigned n=0;n<entry.body.size();++n){
   assert(!inspect(platform,[&](uint32_t p,void* out,size_t size){auto ok=image.read(p,out,size);if(p==entry.rva+32)static_cast<unsigned char*>(out)[n]^=1;return ok;}));++rejected;
  }
  auto nt=image.u32(60);auto stamp=image.u32(nt+8);std::memset(image.bytes.data()+nt+8,0,4);assert(!inspect(platform,read));++rejected;
  std::memcpy(image.bytes.data()+nt+8,&stamp,4);assert(inspect(platform,read));
  std::cout<<argv[a]<<": native task bodies verified, "<<rejected<<" rejection cases passed\n";
 }
 struct Chunk{int id;};struct View{int id;};Chunk a{1},b{2};View v{3};
 std::array<unsigned char,0x30> closure{};auto* owner=&a;auto* view=&v;
 std::memcpy(closure.data()+0x10,&owner,sizeof(owner));std::memcpy(closure.data()+0x20,&view,sizeof(view));
 assert(taskOwner<Chunk>(closure.data())==&a&&taskView<View>(closure.data())==&v);
 owner=&b;std::memcpy(closure.data()+0x10,&owner,sizeof(owner));assert(taskOwner<Chunk>(closure.data())==&b);
}
