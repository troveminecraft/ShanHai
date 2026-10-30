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
    // Reproduce the initial-view backlog: old raw loads must not outrank
    // closer chunks that have just become eligible for their next stage.
    for(int tick=0;tick<=200;++tick){
        auto credits=ages.update(queue,tick*.2);oldCredit=credits.at(key).ageCredit;
        assert(oldCredit<=96);
        assert(scheduledStreamingScore(176,0,false,4)<scheduledStreamingScore(800,oldCredit,false,0));
    }
    queue[0].state=4;assert(ages.update(queue,40.2).at(key).ageCredit==oldCredit);
    auto duplicate=queue;duplicate.push_back({48,0,2});
    auto merged=ages.update(duplicate,40.3).at(key);assert(merged.state==4&&merged.ageCredit==oldCredit);
    ages.update({},40.4);assert(ages.update(queue,43).at(key).ageCredit==oldCredit);
    queue[0].state=0;assert(ages.update(queue,43.2).at(key).ageCredit==0);
    ages.update({},43.4);assert(ages.update(queue,74).at(key).ageCredit==0);
    ages.clear();assert(ages.update(queue,1).at(key).ageCredit==0);
    for(int d=0;d<=2048;d+=16)for(int age=0;age<=3840;age+=128)
        assert(scheduledStreamingScore(256,0,true)<scheduledStreamingScore(d,age,false));
    assert(scheduledStreamingScore(1024,3840,false)>scheduledStreamingScore(256,0,false));
    assert(scheduledStreamingScore(320,0,false,4)<scheduledStreamingScore(320,96,false,0));
    // The dependency window is centered on floor(position), including both
    // opposite edges and negative coordinates; player fractions cannot trim it.
    for(double x:{4.03125,-4.03125})for(double z:{44.03125,-44.03125}){
        int cx=int(std::floor(x)),cz=int(std::floor(z));
        for(int dz=-8;dz<=8;++dz)for(int dx=-8;dx<=8;++dx)assert(terrain::inDependencyHalo(cx+dx,cz+dz,x,z,8));
        assert(!terrain::inDependencyHalo(cx-9,cz,x,z,8));assert(!terrain::inDependencyHalo(cx+9,cz,x,z,8));
    }
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
    std::cout<<"PASS bounded fairness, cross-stage age, complete integer halo, pipeline hysteresis, pause clock, bounded concurrent trace\n";
}
