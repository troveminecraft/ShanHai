#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <vector>
namespace terrain_runtime {
struct LatencySnapshot {uint64_t count=0,windowCount=0,totalUs=0,maxUs=0,p50=0,p95=0,p99=0;};
class Latency {
 std::mutex mutex;std::array<uint64_t,2048> samples{};uint64_t count=0,total=0,maximum=0;
public:
 void observe(uint64_t us){std::lock_guard lock(mutex);samples[count++%samples.size()]=us;total+=us;maximum=std::max(maximum,us);}
 LatencySnapshot snapshot(){
  std::lock_guard lock(mutex);size_t n=std::min<uint64_t>(count,samples.size());std::vector<uint64_t> v(samples.begin(),samples.begin()+n);std::sort(v.begin(),v.end());
  auto q=[&](size_t percent){return n?v[std::min(n-1,(n*percent+99)/100-1)]:0;};return {count,n,total,maximum,q(50),q(95),q(99)};
 }
};
class Timer {
 Latency& latency;std::chrono::steady_clock::time_point began=std::chrono::steady_clock::now();
public:
 explicit Timer(Latency& target):latency(target){}
 ~Timer(){latency.observe(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count());}
};
}
