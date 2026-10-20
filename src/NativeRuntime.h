#pragma once
#include "native/Engine.h"
#include <memory>
#include <mutex>
#include <atomic>
class NativeRuntime {
 std::unique_ptr<nativegen::Engine> engine;
public:
 static constexpr char KernelVersion[]="karma-terrain-2.194.0-native-agent-a";
 static bool supports(std::string_view version){return version==KernelVersion||version=="karma-terrain-2.193.0-native-agent-a";}
 std::atomic_uint restarts=0;
 void start(nativegen::J const&data){engine=std::make_unique<nativegen::Engine>(data);}
 std::vector<uint8_t> chunk(int x,int z){if(!engine)throw std::runtime_error("Native calculator not initialized");return engine->chunk(x,z);}
 void stop(){engine.reset();}
};
