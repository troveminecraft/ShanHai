J structureLatency(terrain_runtime::Latency& value){
 auto s=value.snapshot();return {{"count",s.count},{"windowCount",s.windowCount},{"totalMs",s.totalUs/1000.},
  {"maxMs",s.maxUs/1000.},{"p50Ms",s.p50/1000.},{"p95Ms",s.p95/1000.},{"p99Ms",s.p99/1000.}};
}
J structurePerformance(){
 J result{{"decorationSnapshotSkips",decorationSnapshotSkips.load()},{"prefillSnapshotHits",prefillSnapshotHits.load()},{"prefillSnapshotMisses",prefillSnapshotMisses.load()},{"lockWait",structureLatency(stageMutex.waitLatency)},{"lockHold",structureLatency(stageMutex.holdLatency)},
  {"adapterTotalMs",adapterNanos.load()/1000000.},{"adapterMaxMs",adapterPeakNanos.load()/1000000.},
  {"prefillJobs",prefillJobs.load()},{"prefillComputeMs",prefillComputeNanos.load()/1000000.},
  {"regionalWarmOutsideLock",structureLatency(regionalWarmLatency)},{"regionalWarmJobs",regionalWarmJobs.load()}};
 auto sites=[&](auto const& rows){J out=J::array();for(auto const&s:rows){
  static constexpr char const* phases[]={"prefill","decoration","populationRecovery","cacheCollection"};
  out.push_back({{"phase",s[0]>=0&&s[0]<4?phases[s[0]]:"unknown"},{"chunk",{s[1],s[2]}},{"waitMs",s[3]/1000.},{"holdMs",s[4]/1000.}});
 }return out;};
 result["slowLocks"]=sites(stageMutex.slowSnapshot());result["longestHolds"]=sites(stageMutex.longestSnapshot());
 if(kernelContext)result["regionalPlans"]=kernelContext->statistics();return result;
}
void writeStructurePerformance(){
 static unsigned ticks=0;if(++ticks%100)return;
 // Small fixed-size diagnostics; no new native planning or blocking stage lock.
 J result{{"version",shanhai_release::Structures},{"world",activeWorld},{"performance",structurePerformance()}};
 std::ofstream(folder/"performance.json")<<result.dump(2);
}
