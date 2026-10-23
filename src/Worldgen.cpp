#include <Windows.h>
#include "NativeRuntime.h"
#include "PaletteContract.h"
#include "FailureRecord.h"
#include "ReleaseVersions.h"
#include "HookTransaction.h"
#include "WorldBounds.h"
#include "WorldGenerationMode.h"
#include "TerrainCacheIdentity.h"
#include "ChunkCache.h"
#include "RuntimeMetrics.h"
#include "WorkerPool.h"
#include "DerivedCache.h"
#include "ChunkCodec.h"
#include "BuildHeight.h"
#include "StreamPlanner.h"
#include "ViewGapRecovery.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "mc/server/ServerPlayer.h"
#include "mc/network/packet/LevelEventPacket.h"
#include "mc/deps/shared_types/legacy/LevelEvent.h"
#include "mc/world/level/Weather.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/actor/player/LayeredAbilities.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/GameType.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include <list>
#include <deque>
#include <thread>
#include <condition_variable>
#include "StreamingPriority.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/block/SpeleothemThickness.h"
#include "mc/world/level/block/states/BlockStateVariant.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include <unordered_set>
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/dimension/ChunkBuildOrderPolicy.h"
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <chrono>
#include <future>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/GamingStatus.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/runtime/RuntimeOverload.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "mc/world/level/storage/DBChunkStorage.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionHeightRange.h"
#include "mc/world/level/levelgen/flat/FlatWorldGenerator.h"
#include "mc/world/level/levelgen/VoidGenerator.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/BlockTickingQueue.h"
#include "mc/world/level/chunk/ChunkViewSource.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/biome/registry/BiomeRegistry.h"

namespace terrain {
using Json=nlohmann::json;
constexpr int Side=terrain_bounds::Chunks, Half=Side/2, Count=Side*Side;
#ifdef KARMA_CLIENT
std::filesystem::path Base,clientWorldPath;
#else
const std::filesystem::path Base="plugins/karma-worldgen";
#endif
using Chunk=terrain_runtime::Chunk;
#ifdef KARMA_CLIENT
constexpr unsigned Workers=2;
#else
constexpr unsigned Workers=4;
#endif
std::array<NativeRuntime,Workers> runtimes;
Json nativeData;
bool unboundedWorld=false;
terrain_runtime::WorldGenerationMode generationMode;
terrain_runtime::BuildHeight buildHeight;
bool validChunk(int x,int z){return generationMode.contains(x,z);}

std::filesystem::path cachePath;
std::atomic_uint64_t cacheMemoryHits=0,cacheDiskHits=0,cacheComputed=0,cacheInvalid=0;
std::atomic<DWORD> mainThread=0;
std::atomic_uint64_t calculationMs=0;
std::atomic_uint waitingCalculators=0;
std::vector<Block const*> palette;
std::vector<Biome const*> biomes;
std::unique_ptr<std::once_flag> registryOnce=std::make_unique<std::once_flag>();
Json metadata,activation;
std::string worldName;
std::atomic_uint generated=0,errors=0;
std::atomic_uint flatGenerated=0,voidGenerated=0;
std::atomic_int observedMinY=0,observedMaxYExclusive=0;
std::atomic_bool active=false;
bool requested=false, verifying=true;
int completed=0,waitTicks=-1,probeX=0,probeZ=0;
uint64_t checked=0;
uint64_t verificationSnowChanges=0;
Json verificationSnowExamples=Json::array();
unsigned missingSavedChunks=0;
std::unique_ptr<ChunkViewSource> view;
ll::event::ListenerPtr tickListener;
std::filesystem::path worldPath(){
#ifdef KARMA_CLIENT
 return clientWorldPath;
#else
 return std::filesystem::path("worlds")/std::filesystem::path(std::u8string(reinterpret_cast<char8_t const*>(worldName.data()),worldName.size()));
#endif
}


class Mod {
public:
 static Mod& getInstance(){static Mod m;return m;}
 ll::mod::NativeMod& self=*ll::mod::NativeMod::current();
 bool load();bool enable();bool disable();
 bool unload(){return ll::getGamingStatus()!=ll::GamingStatus::Running&&ll::getGamingStatus()!=ll::GamingStatus::Starting;}
};
auto& log(){return Mod::getInstance().self.getLogger();}
// Chunk activity diagnostics are WARN+ only, before any console or file sink.
template <typename... Args>
void logChunkDiagnostic(ll::io::LogLevel level, fmt::format_string<Args...> message, Args&&... args) {
 if(level!=ll::io::LogLevel::Warn&&level!=ll::io::LogLevel::Error&&level!=ll::io::LogLevel::Fatal)return;
 log().log(level,message,std::forward<Args>(args)...);
}
using Clock=std::chrono::steady_clock;
terrain_runtime::Latency queueLatency,computeLatency,fillLatency,cacheReadLatency,requestLatency,gateLatency,readyLatency,tickGapLatency;
std::array<terrain_runtime::Latency,14> stageLatency;
Clock::time_point lastTick{};
uint64_t observedDetached=0,observedCoalesced=0;std::atomic_uint64_t observationDrops=0;
uint64_t oldestPendingMs=0;std::array<unsigned,15> pendingByState{};

#include "PlayerStreaming.inc"
struct LoadEvent {int x,z;bool saved;Clock::time_point started;};
struct PendingLoad {LoadEvent event;int state=-1;Clock::time_point changed=Clock::now();};
std::mutex loadMutex;
std::vector<LoadEvent> loadEvents;
std::unordered_map<uint64_t,PendingLoad> pendingLoads;
uint64_t loadRequests=0,loadReady=0,loadReleased=0;
unsigned logTicks=0;
char const* loadStateName(int state){
 static constexpr std::array names={"未加载","生成中","地形已生成","结构处理中","结构完成","装饰处理中","装饰完成","检查存档替换数据","等待邻区升级","邻区升级中","等待光照","计算光照","光照完成","就绪"};
 return state>=0&&state<int(names.size())?names[state]:"等待区块引用";
}
uint64_t loadKey(int x,int z){return (uint64_t(uint32_t(x))<<32)|uint32_t(z);}
// Observe actual DB load requests, including saved chunks outside pre-generation.
// Generator calls alone also include BDS boundary reconstruction and cannot
// distinguish reading disk from creating a missing chunk.
LL_TYPE_INSTANCE_HOOK(ObserveLoad,ll::memory::HookPriority::Normal,DBChunkStorage,&DBChunkStorage::$loadChunk,void,LevelChunk& lc,bool force){
 if(active&&static_cast<int>(lc.getDimension().getDimensionId())==0&&mLevel->getLevelData().getLevelName()==worldName){
  auto const p=lc.getPosition();LoadEvent e{p.x,p.z,isChunkSaved(p),Clock::now()};
  std::lock_guard guard(loadMutex);if(loadEvents.size()<8192)loadEvents.push_back(e);else ++observationDrops;
 }
 origin(lc,force);
}
void pollLoads(Level& level){
 if(++logTicks%10)return;
 std::vector<LoadEvent> incoming;{std::lock_guard guard(loadMutex);incoming.swap(loadEvents);}
 auto dim=level.getDimension(0).lock();if(!dim)return;
 nearPending=farPending=outsidePending=0;pendingByState.fill(0);oldestPendingMs=0;auto focus=streamingFocus.load();auto now=Clock::now();
 static unsigned disk=0,fresh=0,ready=0,released=0,detached=0;
 unsigned stalled=0;std::vector<std::pair<uint64_t,std::string>> examples;
 for(auto const&e:incoming){e.saved?++disk:++fresh;++loadRequests;
  if(pendingLoads.contains(loadKey(e.x,e.z))){++observedCoalesced;continue;}
  if(pendingLoads.size()>=16384){++observationDrops;continue;}
  pendingLoads.emplace(loadKey(e.x,e.z),PendingLoad{e,-1,now});
 }
 for(auto it=pendingLoads.begin();it!=pendingLoads.end();){
  auto& p=it->second;auto c=dim->getChunkSource().getExistingChunk(ChunkPos{p.event.x,p.event.z});
  auto age=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(now-p.event.started).count());int state=c?int(c->getState().load()):-1;
  if(state!=p.state){if(p.state>=0&&p.state<14)stageLatency[p.state].observe(std::chrono::duration_cast<std::chrono::microseconds>(now-p.changed).count());p.state=state;p.changed=now;}
  if(c&&c->getState().load()==ChunkState::Loaded){++ready;++loadReady;readyLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(now-p.event.started).count());it=pendingLoads.erase(it);continue;}
  if(!c&&age>5000){++released;++loadReleased;it=pendingLoads.erase(it);continue;}
  // Only stop observing abandoned far-away requests; never pretend they
  // finished, cancel engine jobs or force a chunk to a ready state.
  if(age>15000&&(focus->empty()||streamingScore(p.event.x,p.event.z,*focus)>512)){++detached;++observedDetached;it=pendingLoads.erase(it);continue;}
  if(!validChunk(p.event.x,p.event.z))++outsidePending;
  else if(!focus->empty()&&streamingScore(p.event.x,p.event.z,*focus)<=224)++nearPending;else ++farPending;
  ++pendingByState[state>=0&&state<14?state:14];oldestPendingMs=std::max(oldestPendingMs,age);if(age>=60000)++stalled;
  if(logTicks%200==0)examples.emplace_back(age,fmt::format(" ({},{}:{})",p.event.x,p.event.z,loadStateName(state)));
  ++it;
 }
 if(logTicks%200==0&&(disk||fresh||ready||released||detached||!pendingLoads.empty())){
  std::sort(examples.begin(),examples.end(),[](auto const&a,auto const&b){return a.first>b.first;});std::string text;for(size_t i=0;i<std::min<size_t>(4,examples.size());i++)text+=examples[i].second;
  logChunkDiagnostic(ll::io::LogLevel::Debug,"[区块加载] 读取={} 新建={} 就绪={} 等待={} 近处={} 远处={} 引用释放={} 离开观察范围={} 最长={}ms 超60秒={}{}",disk,fresh,ready,pendingLoads.size(),nearPending,farPending,released,detached,oldestPendingMs,stalled,text);
  disk=fresh=ready=released=detached=0;
 }
}
Json jsonFile(std::filesystem::path const& p){std::ifstream in(p);if(!in)throw std::runtime_error("Missing "+p.string());Json j;in>>j;return j;}
std::string sha256(std::vector<uint8_t> const& data){
 std::array<uint8_t,32> digest{};
 if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(data.data()),static_cast<ULONG>(data.size()),digest.data(),32)<0)throw std::runtime_error("SHA256 failed");
 std::string s;for(auto c:digest){s+="0123456789abcdef"[c>>4];s+="0123456789abcdef"[c&15];}return s;
}
#include "NativeCache.inc"
void readData(){
 metadata=jsonFile(Base/"terrain.json");terrain_runtime::validatePaletteContract(metadata);activation=jsonFile(Base/"active-world.json");worldName=activation.at("worldName").get<std::string>();
 buildHeight=terrain_runtime::BuildHeight::read(metadata);
 flatGenerated=0;voidGenerated=0;observedMinY=0;observedMaxYExclusive=0;
 std::ifstream profileFile(Base/"profile.json",std::ios::binary);std::vector<uint8_t> profile((std::istreambuf_iterator<char>(profileFile)),{});
 auto expectedProfile=metadata;expectedProfile.erase("sha256");
 if(sha256(profile)!=metadata.at("sha256").get<std::string>()||Json::parse(profile)!=expectedProfile)throw std::runtime_error("Runtime metadata checksum mismatch");
 if(metadata.at("format")!=2||metadata.at("mode")!="on-demand"||metadata.at("terrainSeed")!=activation.at("terrainSeed")||metadata.value("backend","")!="native-cpp"||!NativeRuntime::supports(metadata.at("version").get<std::string>())||activation.at("sha256")!=metadata.at("sha256"))throw std::runtime_error("Frozen runtime profile mismatch");
 if(jsonFile(worldPath()/"karma-worldgen.json")!=activation)throw std::runtime_error("World activation marker mismatch");
 for(auto const& [name,digest]:metadata.at("runtimeFiles").items()){
  auto path=std::filesystem::path(name);
  if(path.is_absolute()||name.find("..")!=std::string::npos||!name.starts_with("native/"))throw std::runtime_error("Unsafe runtime path");
  std::ifstream in(Base/path,std::ios::binary);if(!in)throw std::runtime_error("Missing runtime file: "+name);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
  if(sha256(bytes)!=digest.get<std::string>())throw std::runtime_error("Runtime checksum mismatch: "+name);
 }
 log().info("正在初始化 Epic 种子驱动地形内核；不会预生成整张地图");
 // Cache identity describes terrain output, independent of service scheduling.
 auto text=terrain_cache::identityText(metadata);cachePath=Base/"cache"/sha256(std::vector<uint8_t>(text.begin(),text.end()));
 std::error_code ec;std::filesystem::create_directories(cachePath,ec);
 if(ec)log().warn("无法创建地形缓存目录，本次仅用内存缓存：{}",ec.message());
 nativeData=jsonFile(Base/"native/native-input.json");
 buildHeight.requireNativeRecipe(nativeData,metadata);
 generationMode=terrain_runtime::WorldGenerationMode::read(metadata,nativeData);
 unboundedWorld=generationMode.unbounded;
 nativegen::regional::storage(nativeData,Base/"regional-cache");
 if(!NativeRuntime::supports(nativeData.at("version").get<std::string>())||nativeData.at("version")!=metadata.at("version")||!nativeData.at("seedManifest").value("ready",false)||!nativeData.at("seedManifest").value("riversReady",false)||nativeData.at("seedManifest").at("terrainSeed")!=metadata.at("terrainSeed"))throw std::runtime_error("Native data identity mismatch");
 std::array<std::future<void>,Workers> starts;
 for(unsigned i=0;i<Workers;i++)starts[i]=std::async(std::launch::async,[i]{runtimes[i].start(nativeData);});
 for(auto& start:starts)start.get();
 cacheWriter.start();
 log().info("{} 个计算器已就绪，seed={}，内存及磁盘缓存启用",Workers,metadata.at("terrainSeed").get<std::string>());
}
Chunk decodeChunk(std::vector<uint8_t>const&bytes){return terrain_runtime::decodeChunk(bytes,metadata.at("palette").size(),metadata.at("biomeNames").size(),buildHeight.generatedHeight());}
std::shared_ptr<Chunk const> getChunk(int x,int z){
 terrain_runtime::Timer overall(requestLatency);
 if(!validChunk(x,z))throw std::runtime_error("Chunk outside generation bounds");
 uint64_t key=generationMode.cacheKey(x,z);
 if(auto hit=memoryGet(key)){++cacheMemoryHits;return hit;}
 auto gateStart=Clock::now();std::lock_guard gate(chunkGates[terrain_cache::MixedShard{}(key)%chunkGates.size()]);gateLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-gateStart).count());
 if(auto hit=memoryGet(key)){++cacheMemoryHits;return hit;}
 auto file=cachePath/(std::to_string(key)+".bin");
 {
  terrain_runtime::Timer reading(cacheReadLatency);auto result=terrain_runtime::readChunk(file,metadata.at("palette").size(),metadata.at("biomeNames").size(),buildHeight.generatedHeight());
  if(result.state==terrain_cache::ReadState::Valid){memoryPut(key,result.value);++cacheDiskHits;return result.value;}
  else if(result.state==terrain_cache::ReadState::Invalid)++cacheInvalid;
 }
 auto began=Clock::now();std::vector<uint8_t>bytes;
 {CalculatorLease lease;auto computeStart=Clock::now();bytes=runtimes[lease.i].chunk(x,z);auto us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-computeStart).count();computeLatency.observe(us);computeMicros+=us;recordMax(maxComputeMicros,us);}
 auto value=std::make_shared<Chunk const>(decodeChunk(bytes));memoryPut(key,value);++cacheComputed;
 calculationMs+=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-began).count();
 cacheWriter.submit(file,std::move(bytes));return value;
}
[[noreturn]] void calculatorFailure(std::exception const& e,std::string_view phase="initialization",std::optional<std::array<int,2>> chunk={}){
 // Only the first failing worker may write and stop. A second worker must
 // not truncate the JSON while the first is about to terminate the process.
 static std::mutex failureStopMutex;std::lock_guard failureStopLock(failureStopMutex);
 ++errors;log().fatal("地形计算失败，立即停止以防保存虚空区块：{}",e.what());
 // Never return successful empty terrain or throw through an unknown BDS task
 // boundary. A failed generator must not advance to Generated and be saved.
 try {
#ifdef KARMA_CLIENT
 constexpr auto version=shanhai_release::Windows;
#else
 constexpr auto version=shanhai_release::Terrain;
#endif
 terrain_runtime::writeFailure(Base/"fatal-runtime.json",terrain_runtime::failureRecord(
   "terrain",version,phase,metadata.value("terrainSeed",Json(nullptr)),worldName,chunk,e.what(),
   "Process stopped to prevent incomplete chunk saves. Back up the world and retain diagnostics before retrying."));
 std::ofstream out(Base/"fatal-runtime.txt");out<<e.what()<<'\n';out.flush();
 } catch(...) {} // Reporting failure must not bypass the stop-before-save protection.
 TerminateProcess(GetCurrentProcess(),70);std::terminate();
}
void resolveRegistry(Dimension& dim){
 std::call_once(*registryOnce,[&]{
  palette.clear();biomes.clear();
  if(metadata.at("palette").size()>65536)throw std::runtime_error("Palette exceeds uint16");
  for(auto const& entry:metadata.at("palette")){
   if(entry.is_object()){
    auto base=Block::tryGetFromRegistry(entry.at("name").get<std::string>());
    if(!base)throw std::runtime_error("Unregistered custom block "+entry.dump());
    auto parsed=CompoundTag::fromSnbt(entry.at("states").dump());
    if(!parsed)throw std::runtime_error("Invalid block states "+entry.dump());
    CompoundTag tag=base->getSerializationId();tag["states"]=std::move(*parsed);
    auto block=Block::tryGetFromRegistry(tag);
    if(!block)throw std::runtime_error("Missing custom block permutation "+entry.dump());
    for(size_t i=71;i<palette.size();i++)if(*palette[i]==*block)throw std::runtime_error("Duplicate custom permutation "+entry.dump()+" actual="+block->toDebugString());
    palette.push_back(&*block);continue;
   }
   std::string name=entry.get<std::string>();
   if(name=="kelp"){
    auto block=Block::tryGetFromRegistry("minecraft:kelp");
    if(block)block=block->setState(VanillaStates::KelpAge(),25);
    if(!block||block->getState<int>(VanillaStates::KelpAge())!=25)throw std::runtime_error("Missing mature kelp state");
    palette.push_back(&*block);continue;
   }
   if(name=="tall_grass_lower"||name=="tall_grass_upper"||name=="large_fern_lower"||name=="large_fern_upper"){
    bool upper=name.ends_with("_upper");auto id=name.starts_with("tall_grass")?"minecraft:tall_grass":"minecraft:large_fern";
    auto block=Block::tryGetFromRegistry(id);if(block)block=block->setState(VanillaStates::UpperBlockBit(),upper);
    if(!block||block->getState<bool>(VanillaStates::UpperBlockBit())!=upper)throw std::runtime_error("Missing plant half state "+name);
    palette.push_back(&*block);continue;
   }

   if(name.starts_with("pointed_dripstone_up_")||name.starts_with("pointed_dripstone_down_")){
    bool hanging=name.starts_with("pointed_dripstone_down_");
    auto suffix=name.substr(std::string(hanging?"pointed_dripstone_down_":"pointed_dripstone_up_").size());
    auto thickness=suffix=="base"?SpeleothemThickness::Base:suffix=="middle"?SpeleothemThickness::Middle:suffix=="frustum"?SpeleothemThickness::Frustum:SpeleothemThickness::Tip;
    auto block=Block::tryGetFromRegistry("minecraft:pointed_dripstone");
    if(block)block=block->setState(VanillaStates::HangingBit(),hanging);
    if(block)block=block->setState(VanillaStates::SpeleothemThickness(),static_cast<int>(thickness));
    if(!block||block->getState<bool>(VanillaStates::HangingBit())!=hanging||block->getState<SpeleothemThickness>(VanillaStates::SpeleothemThickness())!=thickness)throw std::runtime_error("Missing oriented dripstone state "+suffix);
    palette.push_back(&*block);continue;
   }
   if(name=="oak_log_x"||name=="oak_log_z"||name=="spruce_log_x"||name=="spruce_log_z"){
    auto base=Block::tryGetFromRegistry("minecraft:"+name.substr(0,name.size()-2));
    if(!base)throw std::runtime_error("Missing oak log");
    auto axis=name.ends_with("_x")?PillarAxis::X:PillarAxis::Z;
    auto block=base->setState(VanillaStates::PillarAxis(),static_cast<int>(axis));
    if(!block||block->getState<PillarAxis>(VanillaStates::PillarAxis())!=axis)throw std::runtime_error("Missing oriented log state");palette.push_back(&*block);continue;
   }
   auto id="minecraft:"+name;
   auto b=Block::tryGetFromRegistry(id);
   if(b&&name.ends_with("_leaves")){b=b->setState(VanillaStates::PersistentBit(),true);if(b)b=b->setState(VanillaStates::UpdateBit(),false);
    if(!b||b->getState<bool>(VanillaStates::PersistentBit())!=true)throw std::runtime_error("Missing persistent leaves state");}
   if(!b)throw std::runtime_error("Unknown block "+id);palette.push_back(&*b);
  }
  for(std::string name:metadata.at("biomeNames")){auto b=dim.getBiomeRegistry().lookupByName(name);if(!b)b=dim.getBiomeRegistry().lookupByName("minecraft:"+name);if(!b)throw std::runtime_error("Unknown biome "+name);biomes.push_back(b);}
  log().info("Resolved {} blocks and {} biomes",palette.size(),biomes.size());
 });
}
int index(ChunkPos const& p){return !validChunk(p.x,p.z)?-1:unboundedWorld?0:(p.z+Half)*Side+p.x+Half;}
template<class F>void column(Chunk const& c,int col,F fn){size_t p=c.columns[col];int n=c.runs[p]|(c.runs[p+1]<<8);p+=2;int y=0;for(int r=0;r<n;r++){int len=c.runs[p]|(c.runs[p+1]<<8);int b=c.runs[p+2]|(c.runs[p+3]<<8);p+=4;fn(y,len,b);y+=len;}}
void fill(LevelChunk& lc,Chunk const& c){
 auto fillStarted=Clock::now();
 resolveRegistry(lc.getDimension());
 buildHeight.requireDimension(lc.getHeightRange().mMin,lc.getHeightRange().mMax);
 thread_local std::vector<Block const*> blocks;
 blocks.resize(buildHeight.blocks());
 BlockVolume volume({blocks.data(),blocks.data()+blocks.size()},16,buildHeight.height(),16,*palette[0],buildHeight.MinY);
 terrain_runtime::fillGeneratedBlockVolume(std::span<Block const*>{blocks},std::span<Block const* const>{palette},c,buildHeight,
  [&](int x,int y,int z){return volume.indexNoBoundsCheck(BlockPos{x,y,z});});
 // A void chunk reconstructed by DBChunkStorage can have an empty/moved biome
 // vector while retaining a nonzero stack size. _setBiome(fillY=true) assumes
 // that vector already exists and dereferences it. Replace the whole volume
 // through the initialization API instead of patching uninitialized storage.
 ChunkVolume::VolumeOf<Biome> biomeVolume;
 biomeVolume.mNumSubChunks=buildHeight.subchunks();
 thread_local std::vector<Biome const*> biomeSubchunk(4096);
 terrain_runtime::fillBiomeSubchunk(std::span<Biome const*>{biomeSubchunk},std::span<Biome const* const>{biomes},c);
 biomeVolume.mSubChunkVectors.assign(buildHeight.subchunks(),biomeSubchunk);
 lc.setBiomeFromVolume(biomeVolume);
 lc.setBlockVolume(volume,0);
 // Aquatic plants replace the primary water block but retain source water in
 // Bedrock's secondary layer. Write directly during generation, without ticks.
 int lavaTicks=0;
 for(int col=0;col<256;col++)column(c,col,[&](int y,int len,int b){
  // At most eight once-only vanilla queue entries per generated chunk.
  if(b==145&&lavaTicks<8){BlockPos p{lc.getPosition().x*16+col%16,y+len-1-64,lc.getPosition().z*16+col/16};lc.getTickQueue().addTickToLevelChunk(lc,p,*palette[b],40+lavaTicks*10,0);++lavaTicks;}

  if(b!=49&&b!=50&&(b<56||b>60))return;
  for(int j=y;j<y+len;j++)lc.setExtraBlock(ChunkBlockPos{BlockPos{col%16,j-64,col/16},-64},*palette[8],nullptr);
 });
 lc.setPreWorldGenHeightMap(volume.computeHeightMap());lc.recomputeHeightMap(true);lc.setUnsaved();
 observedMinY=lc.getHeightRange().mMin;observedMaxYExclusive=lc.getHeightRange().mMax;
 auto fillUs=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-fillStarted).count();fillLatency.observe(fillUs);fillMicros+=fillUs;recordMax(maxFillMicros,fillUs);
 unsigned n=++generated;if(n<=3||n%256==0)logChunkDiagnostic(ll::io::LogLevel::Info,"地形生成调用={}, chunk=({},{}), state={}, readOnly={}",n,lc.getPosition().x,lc.getPosition().z,int(lc.getState().load()),lc.isReadOnly());
}
LL_TYPE_INSTANCE_HOOK(Generate,ll::memory::HookPriority::Normal,FlatWorldGenerator,&FlatWorldGenerator::$loadChunk,void,LevelChunk& lc,bool force){
 if(!active||static_cast<int>(lc.getDimension().getDimensionId())!=0||mLevel->getLevelData().getLevelName()!=worldName||index(lc.getPosition())<0){origin(lc,force);return;}
 auto p=lc.getPosition();std::string_view phase="validate-generator";
 try{
  if(!mIsVoidWorld)throw std::runtime_error("Target is not a void flat world");
  buildHeight.requireDimension(lc.getHeightRange().mMin,lc.getHeightRange().mMax);
  phase="calculate-or-decode";auto const& c=getChunk(p.x,p.z);
  if(generated.load()<3)log().info("[生成线程] thread={} 主线程={} chunk=({},{})",GetCurrentThreadId(),GetCurrentThreadId()==mainThread,p.x,p.z);
  phase="initialize-chunk";origin(lc,force);phase="fill-and-prefilter";fill(lc,*c);++flatGenerated;
 }catch(std::exception const& e){calculatorFailure(e,phase,std::array<int,2>{p.x,p.z});}
}
// A data-driven minecraft:generation=void can select the independent native
// VoidGenerator. Keep the legacy flat path for saved worlds and cover both.
LL_TYPE_INSTANCE_HOOK(GenerateVoid,ll::memory::HookPriority::Normal,VoidGenerator,&VoidGenerator::$loadChunk,void,LevelChunk& lc,bool force){
 if(!active||static_cast<int>(lc.getDimension().getDimensionId())!=0||mLevel->getLevelData().getLevelName()!=worldName||index(lc.getPosition())<0){origin(lc,force);return;}
 auto p=lc.getPosition();std::string_view phase="validate-void-generator";
 try{
  buildHeight.requireDimension(lc.getHeightRange().mMin,lc.getHeightRange().mMax);
  phase="calculate-or-decode";auto const& c=getChunk(p.x,p.z);
  phase="initialize-chunk";origin(lc,force);phase="fill-and-prefilter";fill(lc,*c);++voidGenerated;
 }catch(std::exception const& e){calculatorFailure(e,phase,std::array<int,2>{p.x,p.z});}
}
#include "GlacierWeather.inc"
Json latencyJson(terrain_runtime::Latency& latency){auto s=latency.snapshot();Json q={{"count",s.count},{"windowCount",s.windowCount},{"windowLimit",2048},{"totalMs",s.totalUs/1000.},{"maxMs",s.maxUs/1000.}};if(s.windowCount){q["p50Ms"]=s.p50/1000.;q["p95Ms"]=s.p95/1000.;q["p99Ms"]=s.p99/1000.;}else{q["p50Ms"]=nullptr;q["p95Ms"]=nullptr;q["p99Ms"]=nullptr;}return q;}
void writeReport(){
 unsigned restarts=0;for(auto& runtime:runtimes)restarts+=runtime.restarts.load();
 Json report={{"worldName",worldName},{"terrainSeed",metadata.at("terrainSeed")},{"totalChunks",Count},{"pendingLoads",pendingLoads.size()},{"loadRequests",loadRequests},{"loadsReady",loadReady},{"sha256",metadata.at("sha256")},{"generatedThisSession",generated.load()},{"calculationWaitMs",calculationMs.load()},{"calculatorRestarts",restarts},{"calculatorWorkers",Workers},{"cacheMemoryHits",cacheMemoryHits.load()},{"cacheDiskHits",cacheDiskHits.load()},{"cacheComputed",cacheComputed.load()},{"cacheInvalid",cacheInvalid.load()},{"missingSavedChunks",missingSavedChunks},{"completedChunks",completed},{"checkedBlocks",checked},{"errors",errors.load()},{"mode","on-demand"},{"ready",active.load()},{"automaticPregen",false},{"probeRunning",requested}};
 report["backend"]="native-cpp";
 report["height"]={{"minY",buildHeight.MinY},{"buildMaxYExclusive",buildHeight.maxYExclusive},{"generatedMaxYExclusive",buildHeight.generatedMaxYExclusive},{"subchunks",buildHeight.subchunks()},{"observedMinY",observedMinY.load()},{"observedMaxYExclusive",observedMaxYExclusive.load()},{"flatGeneratorChunks",flatGenerated.load()},{"voidGeneratorChunks",voidGenerated.load()}};
 report["arrivalRecovery"]={{"started",arrivalStarts},{"resends",arrivalResends},{"rebinds",arrivalRebinds},{"timeouts",arrivalTimeouts}};
 report["viewGapRecovery"]={{"repairs",viewGapRepairs},{"resolved",viewGapResolved},{"resends",viewGapResends},{"patchesPerPlayer",2},{"patchRadius",8},{"scanSeconds",1}};
 report["playerViews"]=Json::array();for(auto const&[name,s]:playerStreams)report["playerViews"].push_back({{"name",name},{"chunk",{s.cx,s.cz}},{"radius",s.viewRadius},{"unfinished",s.visible.gaps.size()}});
 report["latency"]={{"request",latencyJson(requestLatency)},{"sameChunkWait",latencyJson(gateLatency)},{"calculatorQueue",latencyJson(queueLatency)},{"compute",latencyJson(computeLatency)},{"derivedCacheRead",latencyJson(cacheReadLatency)},{"fill",latencyJson(fillLatency)},{"observedReady",latencyJson(readyLatency)},{"tickInterval",latencyJson(tickGapLatency)}};
 report["stageObservation"]={{"pollEveryTicks",10},{"oldestPendingMs",oldestPendingMs},{"detached",observedDetached},{"coalesced",observedCoalesced},{"dropped",observationDrops.load()},{"networkSendMeasured",false},{"note","State residence is sampled wall time; skipped states have no sample, not zero CPU time"}};
 report["stageObservation"]["states"]=Json::array();for(int i=0;i<14;i++)report["stageObservation"]["states"].push_back({{"name",loadStateName(i)},{"pending",pendingByState[i]},{"residence",latencyJson(stageLatency[i])}});
 report["nativeMemoryResidentChunks"]=memoryCache.size();
 report["cacheWriter"]={{"queued",cacheWriter.pending()},{"limit",terrain_cache::CacheWriter::QueueLimit},{"coalesced",cacheWriter.coalesced.load()}};

 report["native"]={{"regionBlocks",16},{"memoryChunkLimit",MemoryChunks},{"memoryEvictions",memoryEvictions.load()},{"busy",busyCalculators.load()},{"waiting",waitingCalculators.load()},{"queueMs",queueMicros.load()/1000.},{"computeMs",computeMicros.load()/1000.},{"maxComputeMs",maxComputeMicros.load()/1000.},{"fillMs",fillMicros.load()/1000.},{"maxFillMs",maxFillMicros.load()/1000.},{"cacheWrites",cacheWriter.completed.load()},{"cacheWriteDrops",cacheWriter.dropped.load()},{"cacheWriteFailures",cacheWriter.failures.load()}};
 report["verificationSnowChanges"]={{"supportedVariations",verificationSnowChanges},{"examples",verificationSnowExamples}};
 report["glacierWeather"]={{"trackedPlayers",glacierWeatherPlayers.size()},{"updates",glacierWeatherPackets},{"restores",glacierWeatherRestores},{"clientVisualOnly",true},{"strength",glacier_weather::Strength},{"superGlacierOnly",true}};
 if(unboundedWorld){report["totalChunks"]=nullptr;report["boundsMode"]="infinite";}
 report["generationMode"]=generationMode.regional?"regional-on-demand":"frozen";
 report["streaming"]={{"priorityCalls",priorityCalls.load()},{"priorityBoosts",priorityBoosts.load()},{"trackedPlayers",playerStreams.size()},{"nearPending",nearPending},{"farPending",farPending},{"outsideTestBounds",outsidePending},{"lookaheadMaxBlocks",128},{"dependencyRadiusChunks",8}};
 auto tmp=Base/"last-run.tmp";{std::ofstream out(tmp);out<<report.dump(2)<<'\n';}
 MoveFileExW(tmp.c_str(),(Base/"last-run.json").c_str(),MOVEFILE_REPLACE_EXISTING);
}
bool supportedSnowVariation(LevelChunk&lc,Chunk const&c,int col,int j,int expected,Block const&actual){
 bool wasSnow=expected>=146&&expected<=148;
 bool isSnow=actual.getTypeName()=="minecraft:snow_layer";
 if(j<1||!((wasSnow&&(isSnow||actual.getTypeName()=="minecraft:air"))||(expected==0&&isSnow)))return false;
 int biome=c.biomes[col];if(biome!=5&&biome!=19&&biome!=24)return false;
 int under=0;column(c,col,[&](int y,int len,int b){if(j-1>=y&&j-1<y+len)under=b;});
 bool terrain=(under>=1&&under<=7)||under==9||under==20||under==23||under==27||under==33||under==42
     ||(under>=44&&under<=48)||under==80||under==85||(under>=94&&under<=99)||under==143||under==144;
 if(!terrain)return false;
 auto const&support=lc.getBlock(ChunkBlockPos{BlockPos{col%16,j-65,col/16},-64});
 if(!(support==*palette[under]))return false;
 ++verificationSnowChanges;
 if(verificationSnowExamples.size()<8)verificationSnowExamples.push_back({{"x",lc.getPosition().x*16+col%16},{"y",j-64},{"z",lc.getPosition().z*16+col/16},{"expected",palette[expected]->toDebugString()},{"actual",actual.toDebugString()},{"support",support.toDebugString()}});
 return true;
}
bool check(LevelChunk& lc){
 int ci=index(lc.getPosition());if(ci<0)return true;auto p=lc.getPosition();auto hold=getChunk(p.x,p.z);auto const& c=*hold;
 if(!lc.mLevelChunkHasHadOriginalLighting){++errors;log().error("Chunk has no original lighting");return false;}
 for(int col:{51,119,187,255})if(&lc.getBiome(ChunkBlockPos{BlockPos{col%16,99,col/16},-64})!=biomes[c.biomes[col]]){++errors;log().error("Biome mismatch in chunk {},{}",lc.getPosition().x,lc.getPosition().z);return false;}
 // Eight complete columns per chunk check volume order, caves, water and negative Y.
 for(int col:{0,15,51,85,128,170,240,255}){
  bool ok=true;column(c,col,[&](int y,int len,int b){for(int j=y;j<y+len;j++){
   auto const& actual=lc.getBlock(ChunkBlockPos{BlockPos{col%16,j-64,col/16},-64});
   ++checked;if(actual.getTypeName()!=palette[b]->getTypeName()){if(supportedSnowVariation(lc,c,col,j,b,actual))continue;if(errors++<8)log().error("Mismatch at {},{},{}: {} != {}",lc.getPosition().x*16+col%16,j-64,lc.getPosition().z*16+col/16,actual.getTypeName(),palette[b]->getTypeName());ok=false;return;}
  }});if(!ok)return false;
 }
 // Check every oriented log, since eight sampled columns can miss a short log.
 for(int col=0;col<256;col++){
  bool ok=true;column(c,col,[&](int y,int len,int b){if(b<25)return;for(int j=y;j<y+len;j++){
   auto const& actual=lc.getBlock(ChunkBlockPos{BlockPos{col%16,j-64,col/16},-64});
   if(!(actual==*palette[b])){if(supportedSnowVariation(lc,c,col,j,b,actual))continue;++errors;ok=false;log().error("Block state mismatch at {},{},{}: {} expected {}",lc.getPosition().x*16+col%16,j-64,lc.getPosition().z*16+col/16,actual.toDebugString(),palette[b]->toDebugString());}
   if((b==49||b==50||(b>=56&&b<=60))&&!(lc.getExtraBlock(ChunkBlockPos{BlockPos{col%16,j-64,col/16},-64})==*palette[8])){
    ++errors;ok=false;log().error("Aquatic plant missing source-water layer at {},{},{}: {} expected {}",lc.getPosition().x*16+col%16,j-64,lc.getPosition().z*16+col/16,lc.getExtraBlock(ChunkBlockPos{BlockPos{col%16,j-64,col/16},-64}).toDebugString(),palette[8]->toDebugString());
   }
  }});if(!ok)return false;
 }
 return true;
}
#include "TestAssist.inc"
void tick(Level& level){
 auto tickAt=Clock::now();if(lastTick!=Clock::time_point{})tickGapLatency.observe(std::chrono::duration_cast<std::chrono::microseconds>(tickAt-lastTick).count());lastTick=tickAt;
 mainThread=GetCurrentThreadId();updatePlayerStreaming(level);pollLoads(level);testAssistTick(level);glacierWeatherTick(level);
 if(logTicks%100==0)writeReport();
 if(!requested||!active)return;
 auto dim=level.getOrCreateDimension(0).lock();if(!dim)return;
 resolveRegistry(*dim);
 if(!view)view=std::make_unique<ChunkViewSource>(dim->getChunkSource(),ChunkSource::LoadMode::Deferred);
 if(waitTicks<0){
  auto pos=ChunkPos{probeX,probeZ};missingSavedChunks=dim->getChunkSource().isChunkSaved(pos)?0:1;
  view->move(BlockPos{(probeX-8)*16,0,(probeZ-8)*16},BlockPos{(probeX+9)*16-1,0,(probeZ+9)*16-1},false,ChunkSourceViewGenerateMode::GenerateAll,{},nullptr);waitTicks=0;
 }
 dim->getChunkSource().checkAndLaunchChunkGenerationTasks(false);
 auto c=view->getExistingChunk(ChunkPos{probeX,probeZ});
 if(!c||c->getState().load()!=ChunkState::Loaded){
  if(++waitTicks%200==0)log().info("[按需探测] 等待 ({},{}) state={}",probeX,probeZ,c?int(c->getState().load()):-1);
  if(waitTicks>6000){++errors;requested=false;writeReport();log().error("Probe timed out");}return;
 }
 if(!check(*c)){requested=false;writeReport();return;}
 c->setUnsaved();c->markSaveIfNeverSaved();c->markSaveNonActorDataIfDirty();
 if(!dim->getChunkSource().saveLiveChunk(*c)){++errors;log().error("Probe persistence refused");}
 dim->getChunkSource().flushThreadBatch();dim->getChunkSource().flushPendingDiscardedChunkWrites();
 completed=1;requested=false;writeReport();
 log().info("[按需探测完成] chunk=({},{}) 原来未保存={} 检查方块={} 错误={}；terrain release 释放测试加载范围",probeX,probeZ,missingSavedChunks,checked,errors.load());
}
// Apply to the plugin logger before initialization, covering console and file sinks.
bool Mod::load() {
    log().setLevel(ll::io::LogLevel::Warn);
    try {
        readData();
        mainThread = GetCurrentThreadId();
        terrain_runtime::HookTransaction hooks;
        hooks.add<Generate>("terrain::Generate");
        hooks.add<GenerateVoid>("terrain::GenerateVoid");
        hooks.add<ObserveLoad>("terrain::ObserveLoad");
        active = true;
        hooks.commit();
        return true;
    } catch (std::exception const& e) {
        calculatorFailure(e);
    }
}
bool Mod::enable(){
 try {
     calibrateStreamingPriority();
     terrain_runtime::HookTransaction hooks;
     hooks.add<StreamPriority>("terrain::StreamPriority");
     hooks.commit();
 } catch (std::exception const& e) {
     calculatorFailure(e);
 }
 registerTestAssist();
 auto& cmd=ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("terrain","Fixed runtime terrain",CommandPermissionLevel::Owner);
 cmd.runtimeOverload().text("status").execute([](CommandOrigin const&,CommandOutput& out,ll::command::RuntimeCommand const&){writeReport();out.success("world={} mode=on-demand active={} seed={} 生成调用={} 自动全图预生成=false 加载请求={} 就绪={} 等待={} errors={}",worldName,active.load(),metadata.at("terrainSeed").get<std::string>(),generated.load(),loadRequests,loadReady,pendingLoads.size(),errors.load());});
 cmd.runtimeOverload().text("checkblocks").execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const&){
  if(origin.getOriginType()!=CommandOriginType::DedicatedServer){out.error("Console only");return;}
  try{
   auto level=origin.getLevel();if(!level)throw std::runtime_error("Level not ready");
   auto dim=level->getOrCreateDimension(0).lock();if(!dim)throw std::runtime_error("Overworld not ready");
   resolveRegistry(*dim);
   Json states=Json::array();for(size_t i=27;i<palette.size();i++)states.push_back(palette[i]->toDebugString());
   std::ofstream file(Base/"block-state-check.json");file<<Json{{"worldName",worldName},{"sha256",metadata.at("sha256")},{"ok",true},{"states",states}}.dump(2);
   out.success("Validated {} block states without loading chunks",palette.size());
  }catch(std::exception const& e){out.error("Block-state validation failed: {}",e.what());}
 });
 cmd.runtimeOverload().text("probe").required("x",ll::command::ParamKind::Int).required("z",ll::command::ParamKind::Int).execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const& command){
  if(origin.getOriginType()!=CommandOriginType::DedicatedServer){out.error("Console only");return;}
  if(requested||errors||!active){out.error("Busy or failed; inspect server log");return;}
  int x=std::get<int>(*command["x"].as_ptr()),z=std::get<int>(*command["z"].as_ptr());
  if(index(ChunkPos{x,z})<0){out.error("Chunk coordinates must be -256..255");return;}
  probeX=x;probeZ=z;completed=0;waitTicks=-1;checked=0;verificationSnowChanges=0;verificationSnowExamples=Json::array();requested=true;writeReport();out.success("Started explicit development probe ({},{}); geometry check with supported snow variations recorded",x,z);
 });
 cmd.runtimeOverload().text("release").execute([](CommandOrigin const& origin,CommandOutput& out,ll::command::RuntimeCommand const&){
  if(origin.getOriginType()!=CommandOriginType::DedicatedServer){out.error("Console only");return;}
  requested=false;if(view){view->clear();view.reset();}writeReport();out.success("Released development probe");
 });
 tickListener=ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto& e){try{tick(e.level());}catch(std::exception const& ex){++errors;requested=false;writeReport();log().error("Terrain tick failed: {}",ex.what());}});
 requested=false;writeReport();
 if(generationMode.regional)log().info("Ready: {} / 按区域生成 / 使用游戏坐标范围 / terrain status",worldName);
 else log().info("Ready: {} / Epic 样条地形 / 方块 [{},{}] / 随加载生成，无全图预生成 / terrain status",worldName,terrain_bounds::MinBlock,terrain_bounds::MaxBlock);
 return true;
}
bool Mod::disable(){
 if(ll::getGamingStatus()==ll::GamingStatus::Running||ll::getGamingStatus()==ll::GamingStatus::Starting){log().error("按需地形生成器不能在运行中禁用，请正常停服后更新");return false;}
 if(testJoin)ll::event::EventBus::getInstance().removeListener(testJoin);if(testLeave)ll::event::EventBus::getInstance().removeListener(testLeave);
 requested=false;if(tickListener)ll::event::EventBus::getInstance().removeListener(tickListener);glacierWeatherPlayers.clear();playerStreams.clear();streamingPolicy=nullptr;StreamPriority::unhook();view.reset();ObserveLoad::unhook();GenerateVoid::unhook();Generate::unhook();active=false;cacheWriter.stop();for(auto& runtime:runtimes)runtime.stop();return true;
}
}
#ifndef KARMA_CLIENT
LL_REGISTER_MOD(terrain::Mod,terrain::Mod::getInstance());
#endif
