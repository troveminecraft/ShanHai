#pragma once
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>
namespace terrain_runtime {
template<size_t N>class WorkerPool {
 std::mutex mutex;std::condition_variable ready;std::array<bool,N> busy{};uint64_t next=0,serving=0;
 void release(size_t i){{std::lock_guard lock(mutex);busy[i]=false;}ready.notify_all();}
public:
 class Lease {
  WorkerPool* pool;friend class WorkerPool;
  Lease(WorkerPool* p,size_t index):pool(p),i(index){}
 public:
  size_t i;Lease(Lease const&)=delete;Lease& operator=(Lease const&)=delete;
  Lease(Lease&& rhs)noexcept:pool(std::exchange(rhs.pool,nullptr)),i(rhs.i){}
  ~Lease(){if(pool)pool->release(i);}
 };
 Lease acquire(){
  std::unique_lock lock(mutex);auto ticket=next++;
  ready.wait(lock,[&]{return ticket==serving&&std::find(busy.begin(),busy.end(),false)!=busy.end();});
  auto i=size_t(std::find(busy.begin(),busy.end(),false)-busy.begin());busy[i]=true;++serving;ready.notify_all();return Lease(this,i);
 }
};
}
