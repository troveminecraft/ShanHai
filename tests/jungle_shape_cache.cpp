#include "native/JungleShapeCache.h"
#include <atomic>
#include <barrier>
#include <chrono>
#include <iostream>
#include <thread>
using Cache=nativegen::JungleShapes;
using namespace std::chrono_literals;
void require(bool ok,char const*message){if(!ok)throw std::runtime_error(message);}
auto tree(int x,size_t cells=0){auto p=std::make_shared<nativegen::JungleTree>();p->site.x=x;p->cells.resize(cells);return p;}
void concurrent(bool failure){
 Cache cache;std::atomic_int builds=0;std::barrier enter(8);std::promise<void> release;auto gate=release.get_future().share();
 std::vector<std::future<Cache::Value>> tasks;
 for(int i=0;i<8;++i)tasks.push_back(std::async(std::launch::async,[&]{enter.arrive_and_wait();return cache.get(91,[&]{++builds;gate.wait();if(failure)throw std::runtime_error("intentional build error");return tree(731,2048);});}));
 auto deadline=std::chrono::steady_clock::now()+5s;while(cache.statistics().waits!=7&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
 auto admitted=cache.statistics();release.set_value();std::vector<Cache::Value> results;int errors=0;
 for(auto&task:tasks)try{results.push_back(task.get());}catch(std::runtime_error const&e){require(std::string(e.what())=="intentional build error","unexpected exception");++errors;}
 require(admitted.waits==7&&builds==1,"concurrent shape was rebuilt or waiters not admitted");
 require(cache.statistics().pending==0,"pending entry leaked");
 if(failure){require(errors==8&&cache.statistics().entries==0,"build error not delivered to all waiters");auto retry=cache.get(91,[&]{++builds;return tree(17);});require(builds==2&&retry->site.x==17,"failed key cannot retry");}
 else {require(errors==0&&results.size()==8,"missing result");for(auto const&p:results)require(p==results[0]&&p->cells.size()==2048,"workers received different or partial trees");}
}
int main(){try{
 concurrent(false);concurrent(true);
 Cache cache(2);int builds=0;auto get=[&](int k){return cache.get(k,[&]{++builds;return tree(k);});};
 auto first=get(1);get(2);require(get(1)==first,"hit changed tree");get(3);require(get(1)==first&&builds==3,"recently used tree evicted");get(2);require(builds==4&&cache.statistics().entries==2,"entry bound not applied");require(first->site.x==1,"eviction invalidates a retained reader");
 Cache bytes(100,4096);for(int i=0;i<20;++i){auto p=bytes.get(i,[&]{return tree(i,80);});auto s=bytes.statistics();require(p->site.x==i&&s.bytes<=4096,"byte limit exceeded");}
 auto large=bytes.get(300,[]{return tree(300,4096);});require(large->cells.size()==4096&&bytes.statistics().bytes<=4096,"oversize shape must still return without resident storage");
 std::weak_ptr<nativegen::JungleTree const> weak;
 {Cache world;weak=world.get(0,[]{return tree(0);});require(!weak.expired(),"world dropped retained value");}require(weak.expired(),"tree outlived cache without a reader");
 std::cout<<"PASS concurrent single build, immutable publication, exception fan-out/retry, LRU, entry/byte limits, retained readers, world cleanup\n";
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
