#pragma once
#include "NativeRuntime.h"
#include "ChunkCodec.h"
#include "ChunkCache.h"
#include "DerivedCache.h"
#include "RuntimeMetrics.h"
#include "WorkerPool.h"
#include "WorldTasks.h"
#include "ChunkPipelineTrace.h"
#include <future>

namespace terrain_runtime {
// Host-owned resources for one world. Native registry pointers stay in the host
// adapter and may only be cleared after tasks.closeAndWait().
template<unsigned Workers, size_t MemoryChunks> struct TerrainSession {
    static constexpr size_t CacheShards = 16, ChunkGateCount = 4096;
    WorldTasks tasks;
    std::atomic<void const*> levelIdentity=nullptr;
    nlohmann::json nativeData;
    std::filesystem::path cachePath;
    terrain_kernel::ContextPtr kernelContext;
    std::array<NativeRuntime, Workers> runtimes;
    WorkerPool<Workers> calculatorPool;
    terrain_cache::ChunkCache<Chunk, MemoryChunks, CacheShards, terrain_cache::MixedShard, uint64_t> memoryCache;
    std::array<std::mutex, ChunkGateCount> chunkGates;
    terrain_cache::CacheWriter cacheWriter;
    std::atomic_uint64_t cacheMemoryHits=0,cacheDiskHits=0,cacheComputed=0,cacheInvalid=0;
    std::atomic_uint64_t queueMicros=0,computeMicros=0,fillMicros=0,maxComputeMicros=0,maxFillMicros=0;
    std::atomic_uint64_t calculationMs=0;
    std::atomic_uint busyCalculators=0,waitingCalculators=0;
    Latency queueLatency,computeLatency,fillLatency,cacheReadLatency,requestLatency,gateLatency,readyLatency,tickGapLatency;
    std::array<Latency,14> stageLatency;
    ChunkPipelineTrace pipelineTrace;
    std::atomic_uint64_t streamActiveMicros=0;
    // Actual function durations, distinct from coarse polling of state residence.
    std::array<Latency,5> nativeWorkLatency;
    struct SlowWork {int stage,x,z;uint64_t us;};
    std::mutex slowWorkMutex;
    std::deque<SlowWork> slowWork;
    void recordWork(int stage,int x,int z,uint64_t us){
        nativeWorkLatency[stage].observe(us);
        if(us<100000)return;
        std::lock_guard lock(slowWorkMutex);
        if(slowWork.size()==32)slowWork.pop_front();
        slowWork.push_back({stage,x,z,us});
    }

    void bind(void const* level) {
        void const* expected=nullptr;
        if(!levelIdentity.compare_exchange_strong(expected,level) && expected!=level)
            throw std::runtime_error("Terrain session belongs to another native level");
    }
    bool belongsTo(void const* level) const { return level && levelIdentity.load()==level; }

    void start(std::filesystem::path const& regionalDirectory) {
        kernelContext = std::make_shared<terrain_kernel::Context>();
        kernelContext->storage(nativeData, regionalDirectory);
        std::array<std::future<void>, Workers> starts;
        for (unsigned i=0; i<Workers; ++i)
            starts[i] = std::async(std::launch::async,[this,i]{runtimes[i].start(nativeData,kernelContext);});
        for (auto& job : starts) job.get();
        cacheWriter.start();
        tasks.open();
    }
    void close() {
        tasks.closeAndWait();
        cacheWriter.stop(); // Flush already accepted derived-cache writes.
        for (auto& runtime : runtimes) runtime.stop();
        memoryCache.clear();
        kernelContext.reset();
    }
    ~TerrainSession() { close(); }
};
}
