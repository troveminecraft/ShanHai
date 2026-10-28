#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

namespace terrain_runtime {
// Pinned Win64 SpinLockImpl uses FNV-1a over the four little-endian bytes of
// GetCurrentThreadId (not the numeric thread ID or std::hash chosen by us).
constexpr uint64_t nativeThreadOwner(uint32_t id) {
    uint64_t hash=0xcbf29ce484222325ULL;
    for(unsigned shift=0;shift<32;shift+=8)hash=(hash^uint8_t(id>>shift))*0x100000001b3ULL;
    return hash;
}
template<class Owner>
bool tryNativeSpinLock(Owner& owner,unsigned& refs,uint64_t vacant,uint64_t thread) {
    auto expected=static_cast<typename Owner::value_type>(vacant);
    if(owner.compare_exchange_strong(expected,static_cast<typename Owner::value_type>(thread))) {
        refs=1;return true;
    }
    if(expected!=thread)return false;
    auto next=refs+1;
    if(next==std::numeric_limits<unsigned>::max())return false;
    refs=next;return true;
}
}
