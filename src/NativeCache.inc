struct CalculatorLease {
 terrain_runtime::WorkerPool<Workers>::Lease lease;unsigned i;
 static auto obtain(){auto began=Clock::now();++worldSession->waitingCalculators;auto value=worldSession->calculatorPool.acquire();--worldSession->waitingCalculators;++worldSession->busyCalculators;auto us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-began).count();worldSession->queueMicros+=us;worldSession->queueLatency.observe(us);return value;}
 CalculatorLease():lease(obtain()),i(unsigned(lease.i)){}
 ~CalculatorLease(){--worldSession->busyCalculators;}
};
void recordMax(std::atomic_uint64_t& value,uint64_t sample){auto old=value.load(std::memory_order_relaxed);while(old<sample&&!value.compare_exchange_weak(old,sample,std::memory_order_relaxed)){};}
std::shared_ptr<Chunk const> memoryGet(uint64_t key){return worldSession->memoryCache.get(uint64_t(key));}
void memoryPut(uint64_t key,std::shared_ptr<Chunk const> value){worldSession->memoryCache.put(uint64_t(key),std::move(value));}
