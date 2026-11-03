#undef NDEBUG
#include "DerivedCache.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){try{
 assert(argc==2);std::filesystem::path root(argv[1]);assert(std::filesystem::create_directory(root));
 using namespace terrain_cache;std::vector<uint8_t> bytes(16384);for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t((i/32)%97);
 auto legacy=root/"1.bin";{std::ofstream f(legacy,std::ios::binary);f<<digest(bytes);f.write(reinterpret_cast<char*>(bytes.data()),bytes.size());}
 auto read=readDerived(legacy);assert(read.state==ReadState::Valid&&read.legacy&&read.bytes==bytes);
 writeDerived(legacy,bytes);read=readDerived(legacy);assert(read.state==ReadState::Valid&&!read.legacy&&read.bytes==bytes);
 assert(std::filesystem::file_size(legacy)<bytes.size()/2);
 {std::fstream f(legacy,std::ios::binary|std::ios::in|std::ios::out);f.seekp(4);uint32_t huge=0xffffffff;f.write(reinterpret_cast<char*>(&huge),4);}
 assert(readDerived(legacy).state==ReadState::Invalid);writeDerived(legacy,bytes);
 auto size=std::filesystem::file_size(legacy);std::filesystem::resize_file(legacy,size-2);assert(readDerived(legacy).state==ReadState::Invalid);writeDerived(legacy,bytes);
 {std::fstream f(legacy,std::ios::binary|std::ios::in|std::ios::out);f.seekp(8);f.put('z');}assert(readDerived(legacy).state==ReadState::Invalid);writeDerived(legacy,bytes);
 auto other=root/"2.bin",latest=root/"3.bin";writeDerived(other,bytes);
 auto protectedFile=root/"player-data.bin";{std::ofstream(protectedFile)<<"must survive";}
 DiskCacheBudget budget;budget.open(root,8192);assert(budget.bytes==8192);budget.touch(legacy);writeDerived(latest,bytes);budget.written(latest);
 assert(budget.bytes==8192&&budget.evicted==1&&std::filesystem::exists(legacy)&&!std::filesystem::exists(other)&&std::filesystem::exists(latest)&&std::filesystem::exists(protectedFile));
 auto outside=root.parent_path()/"outside-protected.bin";{std::ofstream(outside)<<"must survive";}budget.written(outside);assert(std::filesystem::exists(outside)&&budget.bytes==8192);
 CacheWriter writer;writer.start(root);writer.touch(legacy);writer.submit(root/"4.bin",bytes);writer.stop();assert(readDerived(root/"4.bin").bytes==bytes&&writer.failures==0);
 std::cout<<"compression, legacy read, corruption rejection, bounded eviction and writer passed\n";
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
