// Included inside terrain after Chunk, Clock, cachePath and sha256 exist.
#ifdef KARMA_CLIENT
constexpr size_t MemoryChunks=256,CacheShards=16,ChunkGateCount=4096;
#else
constexpr size_t MemoryChunks=1024,CacheShards=16,ChunkGateCount=4096;
#endif
// Both hosts can use signed X/Z coordinate keys. Truncating to 32 bits aliases distant chunks.
using ChunkKey=uint64_t;
terrain_cache::ChunkCache<Chunk,MemoryChunks,CacheShards,terrain_cache::MixedShard,ChunkKey> memoryCache;
auto& memoryEvictions=memoryCache.evictions;
std::array<std::mutex,ChunkGateCount> chunkGates;
std::atomic_uint64_t queueMicros=0,computeMicros=0,fillMicros=0,maxComputeMicros=0,maxFillMicros=0;
std::atomic_uint busyCalculators=0;
terrain_runtime::WorkerPool<Workers> calculatorPool;
struct CalculatorLease {
 terrain_runtime::WorkerPool<Workers>::Lease lease;unsigned i;
 static auto obtain(){auto began=Clock::now();++waitingCalculators;auto value=calculatorPool.acquire();--waitingCalculators;++busyCalculators;auto us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-began).count();queueMicros+=us;queueLatency.observe(us);return value;}
 CalculatorLease():lease(obtain()),i(unsigned(lease.i)){}
 ~CalculatorLease(){--busyCalculators;}
};
void recordMax(std::atomic_uint64_t& value,uint64_t sample){auto old=value.load(std::memory_order_relaxed);while(old<sample&&!value.compare_exchange_weak(old,sample,std::memory_order_relaxed)){};}
std::shared_ptr<Chunk const> memoryGet(uint64_t key){return memoryCache.get(ChunkKey(key));}
void memoryPut(uint64_t key,std::shared_ptr<Chunk const> value){memoryCache.put(ChunkKey(key),std::move(value));}
terrain_cache::CacheWriter cacheWriter;
