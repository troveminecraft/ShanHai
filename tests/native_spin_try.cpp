#include "../src/runtime/NativeSpinTry.h"
#include <thread>
#include <vector>
#include <stdexcept>
#include <iostream>
using namespace terrain_runtime;
void check(bool ok){if(!ok)throw std::runtime_error("Native spin try-lock contract failed");}
int main(){
    constexpr auto me=nativeThreadOwner(0x1234);
    static_assert(me==5103915217088890363ULL);
    constexpr uint64_t vacant=0x8877665544332211ULL;
    std::atomic<uint64_t> owner{vacant};unsigned refs=0;
    check(tryNativeSpinLock(owner,refs,vacant,me)&&owner==me&&refs==1);
    check(tryNativeSpinLock(owner,refs,vacant,me)&&refs==2);
    check(!tryNativeSpinLock(owner,refs,vacant,nativeThreadOwner(0x5678))&&refs==2);
    refs=std::numeric_limits<unsigned>::max()-1;
    check(!tryNativeSpinLock(owner,refs,vacant,me)&&refs==std::numeric_limits<unsigned>::max()-1);
    refs=0;owner=vacant;
    uint64_t protectedCounter=0;
    std::vector<std::thread> workers;
    for(unsigned id=1;id<=8;++id)workers.emplace_back([&,id]{
        for(int i=0;i<20000;){
            if(!tryNativeSpinLock(owner,refs,vacant,nativeThreadOwner(id))){std::this_thread::yield();continue;}
            check(refs==1);++protectedCounter;++i;
            refs=0;owner.store(vacant);
        }
    });
    for(auto& worker:workers)worker.join();
    check(protectedCounter==160000&&owner==vacant&&refs==0);
    std::cout<<"native spin try-lock: ownership, recursion, contention, overflow and 160000 guarded writes passed\n";
}
