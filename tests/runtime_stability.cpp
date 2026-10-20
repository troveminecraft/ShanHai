#include "ChunkCodec.h"
#include "RuntimeMetrics.h"
#include "WorkerPool.h"
#include "StreamPlanner.h"
#include "StreamingPriority.h"
#include <future>
#include <iostream>
#include <chrono>
#include <set>
void check(bool b,char const*message){if(!b)throw std::runtime_error(message);}
using namespace std::chrono_literals;
int main(int argc,char**argv){try{
 check(argc==2,"Use a new scratch directory");std::filesystem::path root(argv[1]);check(std::filesystem::create_directory(root),"Scratch directory must be new");
 std::vector<uint8_t>good(260,0);good[0]=75;good[1]=87;good[2]=50;for(int i=0;i<256;i++)good.insert(good.end(),{1,0,128,1,15,1});
 auto decoded=terrain_runtime::decodeChunk(good,272,24);check(decoded.runs[4]==15&&decoded.runs[5]==1,"Wide index 271 roundtrip");
 auto file=root/"sample.bin";using terrain_cache::ReadState;
 check(terrain_runtime::readChunk(file,272,24).state==ReadState::Missing,"Missing cache");
 terrain_cache::writeDerived(file,good);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Valid,"Roundtrip");
 {std::ofstream out(file,std::ios::binary|std::ios::trunc);out<<"partial";}check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Truncated cache");
 terrain_cache::writeDerived(file,good);{std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);out.seekp(68);out.put(char(8));}check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Checksum corruption");
 auto bad=good;bad[4]=255;terrain_cache::writeDerived(file,bad);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Malformed biome with valid digest");
 bad=good;bad[265]=255;terrain_cache::writeDerived(file,bad);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Malformed block with valid digest");
 bad=good;bad[262]=0;bad[263]=0;terrain_cache::writeDerived(file,bad);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Zero run");
 bad=good;bad.push_back(0);terrain_cache::writeDerived(file,bad);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Invalid,"Trailing bytes");
 terrain_cache::writeDerived(file,good);check(terrain_runtime::readChunk(file,272,24).state==ReadState::Valid,"Repair roundtrip");
 std::promise<void>entered,unlock;auto release=unlock.get_future().share();std::atomic_uint calls=0;
 terrain_cache::CacheWriter writer([&](auto const&p,auto const&v){if(calls++==0){entered.set_value();release.wait();}if(p.filename()=="fail.bin")throw std::runtime_error("Injected IO error");terrain_cache::writeDerived(p,v);});
 writer.start();writer.submit(root/"first.bin",good);check(entered.get_future().wait_for(2s)==std::future_status::ready,"Writer launch");
 for(int i=0;i<64;i++)writer.submit(root/(i==63?"fail.bin":std::to_string(i)+".bin"),good);
 writer.submit(root/"0.bin",good);writer.submit(root/"overflow.bin",good);check(writer.pending()==64&&writer.coalesced==1&&writer.dropped==1,"Bounded/coalesced writer");
 unlock.set_value();writer.stop();check(writer.completed==64&&writer.failures==1,"Flush and failure isolation");writer.start();writer.submit(root/"restart.bin",good);writer.stop();check(terrain_runtime::readChunk(root/"restart.bin",272,24).state==ReadState::Valid,"Writer restart");
 terrain_runtime::Latency latency;for(uint64_t i=1;i<=3000;i++)latency.observe(i);auto snap=latency.snapshot();check(snap.count==3000&&snap.windowCount==2048&&snap.p50==1976&&snap.p95==2898&&snap.p99==2980,"Rolling quantiles");
 terrain_runtime::WorkerPool<4> pool;std::array<std::atomic_int,4>inUse{};std::vector<std::future<void>>jobs;
 for(int w=0;w<12;w++)jobs.push_back(std::async(std::launch::async,[&]{for(int j=0;j<500;j++){auto lease=pool.acquire();check(inUse[lease.i].fetch_add(1)==0,"Shared worker concurrent use");std::this_thread::yield();check(inUse[lease.i].fetch_sub(1)==1,"Lease release");}}));
 for(auto&job:jobs)job.get();try{auto lease=pool.acquire();throw 1;}catch(int){}for(int i=0;i<20;i++){auto lease=pool.acquire();}
 terrain_runtime::StreamPlanner motion;motion.update(0,0,1,0,0);auto straight=motion.update(32,0,1.2,0,0);check(straight.allowAhead&&straight.aheadX>32,"Forward lookahead");
 auto turn=motion.update(0,0,1.4,0,0);check(turn.turned&&turn.aheadX<0,"Immediate reversal");
 auto warp=motion.update(1000,1000,1.6,0,0);check(warp.teleport&&!warp.allowAhead&&warp.aheadX==1000,"Teleport suppression");
 auto jam=motion.update(1032,1000,2,8,0);check(!jam.allowAhead,"Congestion pause");check(!motion.update(1064,1000,2.2,0,0).allowAhead,"Cooldown hysteresis");check(motion.update(1096,1000,3.3,0,0).allowAhead,"Cooldown recovery");
 check(motion.update(-2000,-2000,6.5,0,0).teleport,"Teleport after delayed tick");
 using Recovery=terrain_runtime::ArrivalRecovery;using Action=Recovery::Action;
 Recovery recovery;recovery.begin(0);
 check(recovery.poll(7,false)==Action::None,"Do not churn active generation");
 check(recovery.poll(8,false)==Action::Rebind&&recovery.poll(9,false)==Action::None,"Only one dependency retry");
 check(recovery.poll(10,true)==Action::None&&recovery.poll(10.5,true)==Action::Resend,"Wait for stable actual readiness");
 check(recovery.poll(11,true)==Action::None,"Only one resend per arrival");
 recovery.begin(11);check(recovery.poll(11,true)==Action::None&&recovery.poll(11.6,true)==Action::None,"Rapid teleport resend budget");
 check(recovery.poll(12.6,true)==Action::Resend,"Deferred newest destination resend");
 recovery.begin(20);check(recovery.poll(20,true)==Action::None&&recovery.poll(20.3,false)==Action::None&&recovery.poll(20.8,true)==Action::None,"Readiness loss restarts settling");
 check(recovery.poll(21.3,true)==Action::Resend,"Readiness restored");
 recovery.begin(30);check(recovery.poll(60,false)==Action::Timeout&&!recovery.active,"Bounded timeout leaves normal streaming intact");
 terrain_runtime::NearChunkWatchdog watch;
 check(!watch.poll(0,0,0,false,false)&&!watch.poll(0,0,3.9,false,false),"Watchdog grace period");
 check(watch.poll(0,0,4,false,false)&&!watch.poll(0,0,80,false,false),"One watchdog attempt per location");
 check(!watch.poll(0,0,81,true,false)&&!watch.poll(0,0,82,false,false)&&watch.poll(0,0,86,false,false),"New readiness loss may recover");
 check(!watch.poll(1,0,87,false,true)&&!watch.poll(1,0,200,false,false),"No duplicate timed-out teleport recovery");
 for(int i=0;i<10000;i++)check(!watch.poll(i+2,i/13,201+i*.2,false,false),"Moving flight must not churn watchdog retries");
 std::vector<terrain::StreamFocus> players{{0,0,8,0,128},{80,80,72,80,128},{-80,60,-80,52,128},{20,-80,28,-80,128}};
 for(auto const&p:players)check(terrain::streamingScore(int(p.x),int(p.z),players)<terrain::streamingScore(120,-120,players),"Multi-player near priority");
 std::cout<<"{\"cacheFaultCases\":6,\"writerQueueBound\":64,\"writerFailureIsolated\":true,\"writerRestart\":true,\"workerLeases\":6000,\"workerThreads\":12,\"motionCases\":7,\"arrivalRecoveryCases\":10,\"watchdogCases\":5,\"movingWatchdogSamples\":10000,\"playerFoci\":4,\"rollingQuantiles\":true}"<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
