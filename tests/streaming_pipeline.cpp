#undef NDEBUG
#include "StreamingBudget.h"
#include "StreamingPriority.h"
#include "ChunkPipelineTrace.h"
#include <cassert>
#include <iostream>
#include <future>
using namespace terrain_runtime;
int main(){
    ReadyAges ages;
    std::vector<ReadyChunk> queue{{48,0,2},{0,0,0}};
    auto key=streamingKey(48,0);
    int oldCredit=0;
    // A continuous stream of fresh nearby work cannot permanently beat a
    // distant dependency-ready task, even while near tasks never run out.
    for(int tick=0;tick<=200;++tick){
        auto credits=ages.update(queue,tick*.2);oldCredit=credits.at(key);
        if(tick>=80)assert(fairStreamingScore(1024,oldCredit)<fairStreamingScore(0,0));
    }
    queue[0].state=4;assert(ages.update(queue,40.2).at(key)==0);
    ages.update({},40.4);assert(ages.update(queue,43).at(key)==0);
    ages.clear();assert(ages.update(queue,1).at(key)==0);
    // Reproduce 0.2.9 inversion: a 30-second-old view-edge job beats the
    // newly eligible centre even though the player's initial halo is blocked.
    assert(fairStreamingScore(1024,3840)<fairStreamingScore(0,0));
    for(int d=0;d<=2048;d+=16)for(int age=0;age<=3840;age+=128)
        assert(scheduledStreamingScore(256,0,true)<scheduledStreamingScore(d,age,false));
    assert(scheduledStreamingScore(1024,3840,false)<scheduledStreamingScore(256,0,false));
    IdleQueuePump pump;
    assert(!pump.poll(0,true,8,0));assert(!pump.poll(.4,true,8,0));
    assert(pump.poll(.6,true,8,0));assert(!pump.poll(.8,true,8,0));
    assert(!pump.poll(1.8,true,8,1));assert(!pump.poll(2,true,8,0));
    assert(pump.poll(2.6,true,8,0));assert(!pump.poll(4,false,8,0));
    assert(!pump.poll(5,true,0,0));assert(!pump.poll(6,true,8,0));
    SupplementalBudget budget;PipelinePressure p{0,0,0,true};
    assert(budget.update(p,0,false));
    p.postprocess=200;assert(!budget.update(p,0,false)); // CPU workers idle, decoration backed up
    p.postprocess=60;assert(!budget.update(p,0,false)); // stay closed until drained
    p.postprocess=20;assert(budget.update(p,0,false));
    p.queued=300;assert(!budget.update(p,0,false));
    p.queued=0;p.queueSampleValid=false;assert(!budget.update(p,0,false));
    p.queueSampleValid=true;assert(budget.update(p,0,false));
    assert(!budget.update(p,4,false));assert(!budget.update(p,0,true));
    ActiveStreamingClock clock;clock.tick(100);clock.tick(100.05);clock.tick(700);clock.tick(700.05);
    assert(clock.seconds<.36&&clock.excluded>599);
    ChunkPipelineTrace trace;trace.watch({streamingKey(-4,5)});
    auto writer=[&]{for(int i=0;i<1000;++i){trace.record({0,-4,5,2});trace.record({0,0,0,2});}};
    auto a=std::async(std::launch::async,writer),b=std::async(std::launch::async,writer);a.get();b.get();
    auto events=trace.snapshot();assert(events.size()==ChunkPipelineTrace::Limit);
    for(auto e:events)assert(e.x==-4&&e.z==5);
    trace.watch({});trace.record({0,-4,5,2});assert(trace.snapshot().size()==events.size());
    std::cout<<"PASS continuous-flight fairness, stage/queue reset, whole-pipeline hysteresis, pause clock, bounded concurrent trace\n";
}
