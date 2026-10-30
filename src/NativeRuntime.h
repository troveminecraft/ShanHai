#pragma once
#include "TerrainKernel.h"
#include "native/Noise.h"
#include <memory>
#include <mutex>
#include <atomic>
class NativeRuntime {
 std::unique_ptr<terrain_kernel::Generator> engine;
public:
 static constexpr char KernelVersion[]="karma-terrain-2.216.0-native-agent-a";
 static bool supports(std::string_view version){return version==KernelVersion||version=="karma-terrain-2.215.0-native-agent-a"||version=="karma-terrain-2.214.0-native-agent-a"||version=="karma-terrain-2.213.0-native-agent-a"||version=="karma-terrain-2.212.0-native-agent-a"||version=="karma-terrain-2.211.0-native-agent-a"||version=="karma-terrain-2.210.0-native-agent-a"||version=="karma-terrain-2.209.0-native-agent-a"||version=="karma-terrain-2.208.0-native-agent-a"||version=="karma-terrain-2.207.0-native-agent-a"||version=="karma-terrain-2.206.0-native-agent-a"||version=="karma-terrain-2.205.0-native-agent-a"||version=="karma-terrain-2.203.0-native-agent-a"||version=="karma-terrain-2.202.0-native-agent-a"||version=="karma-terrain-2.201.0-native-agent-a"||version=="karma-terrain-2.200.0-native-agent-a"||version=="karma-terrain-2.199.0-native-agent-a"||version=="karma-terrain-2.198.0-native-agent-a"||version=="karma-terrain-2.197.0-native-agent-a"||version=="karma-terrain-2.196.0-native-agent-a"||version=="karma-terrain-2.195.0-native-agent-a"||version=="karma-terrain-2.194.0-native-agent-a"||version=="karma-terrain-2.193.0-native-agent-a";}
 std::atomic_uint restarts=0;
 void start(nativegen::J const&data,terrain_kernel::ContextPtr context){engine=std::make_unique<terrain_kernel::Generator>(data,terrain_kernel::Reservations{},false,nullptr,nullptr,std::move(context));}
 std::vector<uint8_t> chunk(int x,int z){if(!engine)throw std::runtime_error("Native calculator not initialized");return engine->chunk(x,z);}
 void stop(){engine.reset();}
};
