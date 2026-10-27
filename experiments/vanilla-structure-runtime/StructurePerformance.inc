J structureLatency(terrain_runtime::Latency& value){
 auto s=value.snapshot();return {{"count",s.count},{"windowCount",s.windowCount},{"totalMs",s.totalUs/1000.},
  {"maxMs",s.maxUs/1000.},{"p50Ms",s.p50/1000.},{"p95Ms",s.p95/1000.},{"p99Ms",s.p99/1000.}};
}
J structurePerformance(){
 J result{{"lockWait",structureLatency(stageMutex.waitLatency)},{"lockHold",structureLatency(stageMutex.holdLatency)},
  {"adapterTotalMs",adapterNanos.load()/1000000.},{"adapterMaxMs",adapterPeakNanos.load()/1000000.},
  {"prefillJobs",prefillJobs.load()},{"prefillComputeMs",prefillComputeNanos.load()/1000000.}};
 if(kernelContext)result["regionalPlans"]=kernelContext->statistics();return result;
}
void writeStructurePerformance(){
 static unsigned ticks=0;if(++ticks%100)return;
 // Small fixed-size diagnostics; no new native planning or blocking stage lock.
 J result{{"version",shanhai_release::Structures},{"world",activeWorld},{"performance",structurePerformance()}};
 std::ofstream(folder/"performance.json")<<result.dump(2);
}
