#include "WorldScopedCache.h"
#include <cassert>
#include <memory>
#include <stdexcept>
int main(){
 WorldScopedCache<std::shared_ptr<int>> cache;
 int builds=0;
 auto create=[&]{++builds;return std::make_shared<int>(builds);};
 std::weak_ptr<int> first=cache.get(create);
 assert(*cache.get(create)==1&&builds==1);
 cache.reset();assert(first.expired());
 assert(*cache.get(create)==2&&builds==2);
 cache.reset();cache.reset();
 try{cache.get([]()->std::shared_ptr<int>{throw std::runtime_error("registry unavailable");});assert(false);}catch(std::runtime_error const&){}
 assert(*cache.get(create)==3&&builds==3);
}
