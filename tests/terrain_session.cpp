#include "TerrainSession.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <latch>
using namespace std::chrono_literals;
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    nlohmann::json recipe; std::ifstream(argv[1])>>recipe;
    auto output=std::filesystem::path(argv[2]);
    std::filesystem::create_directories(output);
    auto owned=std::make_unique<terrain_runtime::TerrainSession<2,32>>();
    auto& session=*owned;
    session.nativeData=recipe;
    session.start(output/"regional-cache");
    std::weak_ptr<terrain_kernel::Context> lifetime=session.kernelContext;
    std::latch admitted(6);
    std::promise<void> release;auto gate=release.get_future().share();
    std::vector<std::future<std::vector<uint8_t>>> work;
    for(int i=0;i<6;++i)work.push_back(std::async(std::launch::async,[&,i] {
        auto job=session.tasks.enter(); admitted.count_down(); gate.wait();
        auto slot=session.calculatorPool.acquire();
        auto bytes=session.runtimes[slot.i].chunk(0,0);
        session.cacheWriter.submit(output/(std::to_string(i)+".bin"),bytes);
        auto chunk=terrain_runtime::decodeChunk(bytes,256,256,recipe.at("config").value("generatedMaxYExclusive",320)+64);
        session.memoryCache.put(uint64_t(i),std::make_shared<terrain_runtime::Chunk>(std::move(chunk)));
        return bytes;
    }));
    admitted.wait();
    auto closing=std::async(std::launch::async,[&]{session.close();});
    assert(closing.wait_for(20ms)==std::future_status::timeout);
    while(session.tasks.snapshot().accepting)std::this_thread::yield();
    assert(!session.tasks.tryEnter());
    release.set_value();
    auto expected=work.front().get();
    for(size_t i=1;i<work.size();++i)assert(work[i].get()==expected);
    closing.get();
    assert(session.tasks.snapshot().running==0);
    assert(session.memoryCache.size()==0 && session.cacheWriter.pending()==0);
    assert(session.cacheWriter.completed==6 && session.cacheWriter.failures==0);
    assert(lifetime.expired());
    for(int i=0;i<6;++i)assert(terrain_cache::readDerived(output/(std::to_string(i)+".bin")).bytes==expected);
    auto nextOwned=std::make_unique<terrain_runtime::TerrainSession<2,32>>();
    auto& next=*nextOwned;
    int firstLevel=0,secondLevel=0;
    session.bind(&firstLevel);
    assert(session.belongsTo(&firstLevel) && !session.belongsTo(&secondLevel));
    bool wrongLevel=false;
    try{session.bind(&secondLevel);}catch(std::runtime_error const&){wrongLevel=true;}
    assert(wrongLevel);
    assert(next.computeMicros==0 && next.requestLatency.snapshot().count==0);
    assert(next.memoryCache.size()==0 && !next.tasks.snapshot().accepting);
    std::cout<<"terrain session: six admitted jobs/two workers drained; disk writes flushed; resources released; new metrics empty\n";
}
