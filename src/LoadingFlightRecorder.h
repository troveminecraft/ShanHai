#pragma once
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif
namespace terrain_runtime {
// Producers never perform file I/O or touch native objects from the writer.
// Each world owns separate files. A heartbeat survives a blocked game tick.
class LoadingFlightRecorder {
    using Clock=std::chrono::steady_clock;
    struct Event {uint64_t id,parent,thread,start,end;char const* phase;int x,z;};
    std::mutex mutex;std::condition_variable wake;std::thread worker;
    std::atomic_bool enabled=false;bool stopping=false;
    std::map<uint64_t,Event> active;std::deque<Event> completed;
    std::string status;uint64_t statusAt=0,sequence=0;Clock::time_point epoch;
    std::filesystem::path directory; nlohmann::json metadata;
    size_t segmentLimit=16*1024*1024;
    inline static thread_local uint64_t parent=0;
    inline static std::atomic_uint64_t sessions=0;
    uint64_t micros()const{return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-epoch).count();}
    static uint64_t threadId(){
#ifdef _WIN32
        return GetCurrentThreadId();
#else
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
    }
    static uint64_t wallMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
    static nlohmann::json row(Event const&e,uint64_t now){return {{"id",e.id},{"parent",e.parent},{"thread",e.thread},{"phase",e.phase},{"chunk",{e.x,e.z}},{"startUs",e.start},{"endUs",e.end},{"durationUs",(e.end?e.end:now)-e.start}};}
    void run()noexcept{
        try{
            size_t bytes=0,segment=0;uint64_t lastStatus=0;
            std::ofstream file;
            auto open=[&]{file.close();file.open(directory/("events-"+std::to_string(segment%4)+".jsonl"),std::ios::trunc);bytes=0;};open();
            for(;;){
                std::vector<Event> live,done;std::string sample;uint64_t sampleAt=0,now=0;bool closed;
                {std::unique_lock lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(500),[&]{return stopping;});
                 closed=stopping;now=micros();for(auto const&[id,e]:active)live.push_back(e);
                 done.assign(completed.begin(),completed.end());completed.clear();
                 sampleAt=statusAt;if(sampleAt!=lastStatus){sample=status;lastStatus=sampleAt;}}
                nlohmann::json record={{"type","heartbeat"},{"wallUnixMs",wallMs()},{"elapsedUs",now},{"closed",closed},{"dropped",dropped.load()},{"ioFailures",ioFailures.load()},{"statusAgeUs",sampleAt?now-sampleAt:now},{"active",nlohmann::json::array()},{"completed",nlohmann::json::array()}};
                for(auto const&e:live)record["active"].push_back(row(e,now));
                for(auto const&e:done)record["completed"].push_back(row(e,now));
                if(!sample.empty()){record["status"]=nlohmann::json::parse(sample);std::ofstream latest(directory/"last-status.json");latest<<sample;std::ofstream legacy(directory.parent_path().parent_path()/"world-session.json");legacy<<sample;}
#ifdef _WIN32
                PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
                if(K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)))record["memory"]={{"workingSetBytes",memory.WorkingSetSize},{"privateBytes",memory.PrivateUsage},{"pageFaults",memory.PageFaultCount}};
                MEMORYSTATUSEX system{};system.dwLength=sizeof(system);if(GlobalMemoryStatusEx(&system))record["systemMemory"]={{"loadPercent",system.dwMemoryLoad},{"availablePhysicalBytes",system.ullAvailPhys},{"availableCommitBytes",system.ullAvailPageFile}};
                FILETIME c,e,k,u;if(GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))record["cpu100ns"]={{"kernel",(uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime},{"user",(uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime}};
#endif
                auto text=record.dump();if(bytes+text.size()+1>segmentLimit&&bytes){++segment;open();}
                file<<text<<'\n';file.flush();if(!file)++ioFailures;bytes+=text.size()+1;
                if(closed)break;
            }
        }catch(...){++ioFailures;enabled=false;}
    }
public:
    std::atomic_uint64_t dropped=0,ioFailures=0;
    ~LoadingFlightRecorder(){stop();}
    void start(std::filesystem::path root,nlohmann::json info,size_t limit=16*1024*1024){
        stop();std::lock_guard lock(mutex);epoch=Clock::now();active.clear();completed.clear();status.clear();statusAt=0;sequence=0;dropped=0;ioFailures=0;stopping=false;segmentLimit=limit;
        directory=root/(std::to_string(wallMs())+"-"+std::to_string(threadId())+"-"+std::to_string(++sessions));std::filesystem::create_directories(directory);
        metadata=std::move(info);metadata["schema"]=1;metadata["startWallUnixMs"]=wallMs();metadata["heartbeatMs"]=500;metadata["segments"]=4;metadata["segmentLimitBytes"]=limit;
        metadata["scope"]="native task spans and copied main-thread status; no background native object access";
        std::ofstream(directory/"session.json")<<metadata.dump(2);enabled=true;
        try{worker=std::thread([this]{run();});}catch(...){enabled=false;throw;}
    }
    void stop()noexcept{enabled=false;{std::lock_guard lock(mutex);stopping=true;}wake.notify_all();if(worker.joinable())worker.join();}
    void snapshot(nlohmann::json const&value)noexcept{if(!enabled.load())return;try{auto text=value.dump();if(text.size()>2*1024*1024){++dropped;return;}std::lock_guard lock(mutex);status=std::move(text);statusAt=micros();}catch(...){++dropped;}}
    uint64_t begin(char const*phase,int x,int z)noexcept{
        if(!enabled.load(std::memory_order_relaxed))return 0;
        try{std::lock_guard lock(mutex);if(!enabled.load()||active.size()>=512){++dropped;return 0;}
            auto id=++sequence;active.emplace(id,Event{id,parent,threadId(),micros(),0,phase,x,z});return id;
        }catch(...){++dropped;return 0;}
    }
    void end(uint64_t id)noexcept{if(!id)return;try{std::lock_guard lock(mutex);auto i=active.find(id);if(i==active.end())return;
        auto e=i->second;e.end=micros();active.erase(i);if(completed.size()>=4096){completed.pop_front();++dropped;}completed.push_back(e);
    }catch(...){++dropped;}}
    class Span {
        LoadingFlightRecorder& owner;uint64_t id,previous;
    public:
        Span(LoadingFlightRecorder&r,char const*phase,int x=0,int z=0):owner(r),id(r.begin(phase,x,z)),previous(parent){if(id)parent=id;}
        Span(Span const&)=delete;~Span(){parent=previous;owner.end(id);}
    };
};
inline LoadingFlightRecorder loadingRecorder;
using LoadingSpan=LoadingFlightRecorder::Span;
}
