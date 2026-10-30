#include "LoadingFlightRecorder.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace std::chrono_literals;
int main(int argc,char**argv){
 assert(argc==2);std::filesystem::path root=argv[1];assert(!std::filesystem::exists(root));
 terrain_runtime::LoadingFlightRecorder recorder;
 recorder.start(root/"sessions",{{"seed","-5873418742035687502"},{"test",true}},4096);
 std::promise<void> started,release;auto gate=release.get_future();
 auto blocked=std::async(std::launch::async,[&]{terrain_runtime::LoadingFlightRecorder::Span task(recorder,"blocked.native",7,-4);started.set_value();gate.wait();});
 started.get_future().wait();std::this_thread::sleep_for(1100ms);
 bool live=false;for(auto const&entry:std::filesystem::recursive_directory_iterator(root))if(entry.path().extension()==".jsonl"){
  std::ifstream f(entry.path());std::string line;while(std::getline(f,line)){auto j=nlohmann::json::parse(line);for(auto const&e:j["active"])if(e["phase"]=="blocked.native"&&e["durationUs"].get<uint64_t>()>=400000)live=true;}
 }assert(live); // background evidence exists without any main-thread snapshot
 release.set_value();blocked.get();recorder.snapshot({{"closed",true},{"seed","-5873418742035687502"}});
 for(int i=0;i<9000;++i){terrain_runtime::LoadingFlightRecorder::Span s(recorder,"bounded",i,0);}
 assert(recorder.dropped>0);recorder.stop();assert(recorder.ioFailures==0);
 bool closed=false,completed=false;int segments=0;
 for(auto const&entry:std::filesystem::recursive_directory_iterator(root))if(entry.path().extension()==".jsonl"){
  ++segments;std::ifstream f(entry.path());std::string line;while(std::getline(f,line)){auto j=nlohmann::json::parse(line);closed|=j["closed"].get<bool>();completed|=!j["completed"].empty();}
 }assert(closed&&completed&&segments<=4);
 recorder.start(root/"sessions",{{"world","second"}});recorder.stop();
 int sessions=0;for(auto const&e:std::filesystem::directory_iterator(root/"sessions"))if(e.is_directory())++sessions;assert(sessions==2);
 std::cout<<"PASS independent heartbeat, blocking-task evidence, overflow accounting, shutdown flush, separate world sessions\n";
}
