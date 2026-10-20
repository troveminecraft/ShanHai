// One module owns terrain and structures, so their world lifetimes cannot diverge.
#include "../../src/Worldgen.cpp"
#include "../../experiments/vanilla-structure-runtime/lab.cpp"
#include "mc/server/ServerLevel.h"
#include "mc/world/level/LevelSettings.h"
#include "mc/deps/core/file/PathBuffer.h"

namespace client {
Level* current=nullptr;
ll::event::ListenerPtr listener;
std::filesystem::path diagnostics;
using Json=nlohmann::json;
unsigned sessionTicks=0;
unsigned tickErrors=0;
std::mutex freshWorldMutex;
std::map<std::filesystem::path,std::string> freshWorldSeeds;
void validateStoredGenerator(std::filesystem::path const& root,std::string const& seed){
 bool fresh=false;
 {
  std::lock_guard lock(freshWorldMutex);
  auto it=freshWorldSeeds.find(std::filesystem::weakly_canonical(root));
  if(it!=freshWorldSeeds.end()){
   if(it->second!=seed)throw std::runtime_error("Fresh world seed differs from its creation ticket");
   fresh=true;freshWorldSeeds.erase(it);
  }
 }
 // The saved Flat/TheVoid setting and the engine's resolved runtime generator
 // are different representations. Validate the durable world settings instead.
 auto path=root/"level.dat";
 // Only this process's successful create-world handoff may enter before the
 // first vanilla settings save. A marker file alone cannot bypass validation.
 if(!std::filesystem::exists(path)&&fresh)return;
 auto size=std::filesystem::file_size(path);
 if(size<8||size>16*1024*1024)throw std::runtime_error("Invalid level.dat size");
 std::string bytes(size,'\0');
 std::ifstream input(path,std::ios::binary);
 if(!input.read(bytes.data(),bytes.size()))throw std::runtime_error("Cannot read level.dat");
 auto nbt=CompoundTag::fromBinaryNbt(std::string_view(bytes).substr(8));
 if(!nbt)throw std::runtime_error("Invalid level.dat NBT");
 if(int(nbt->at("Generator"))!=2||
    std::string_view(nbt->at("FlatWorldLayers")).find("TheVoid")==std::string_view::npos)
  throw std::runtime_error("Karma saved world generator setting was changed");
}
void writeStatus(Level&level,bool closed=false){
 Json status{{"world",terrain::worldName},{"seed",level.getLevelData().getSeed().toUiString()},
  {"kernel",NativeRuntime::KernelVersion},{"generatedChunks",terrain::generated.load()},
  {"terrainErrors",terrain::errors.load()},{"structureCalls",lab::stageCalls.load()},
  {"structureErrors",lab::stageErrors.load()},{"entityLedgerReady",lab::entityLedgerReady.load()},
  {"ticks",sessionTicks},{"closed",closed},{"worldPath",terrain::worldPath().string()}};
 status["tickErrors"]=tickErrors;
 status["villageTemplatesReady"]=lab::villageTemplatesReady;
 status["fossilTemplatesReady"]=lab::fossilTemplatesReady;
 status["players"]=Json::array();
 level.forEachPlayer([&](Player&p){auto pos=p.getPosition();status["players"].push_back({{"pos",{pos.x,pos.y,pos.z}},{"dimension",int(p.getDimensionId())}});return true;});
 std::ofstream(diagnostics/"world-session.json")<<status.dump(2);
}

void clearWorld(Level* level){
 if(level!=current)return;
 writeStatus(*level,true);
 terrain::active=false;lab::automaticStructures=false;lab::smallFeaturesEnabled=false;
 terrain::unboundedWorld=false;
 terrain::requested=false;terrain::view.reset();terrain::playerStreams.clear();
 terrain::glacierWeatherPlayers.clear();terrain::streamingPolicy=nullptr;
 terrain::cacheWriter.stop();for(auto&r:terrain::runtimes)r.stop();
 terrain::memoryCache.clear();terrain::palette.clear();terrain::biomes.clear();
 terrain::registryOnce=std::make_unique<std::once_flag>();
 terrain::loadEvents.clear();terrain::pendingLoads.clear();
 lab::atlasCatalog.active=false;
 std::lock_guard lock(lab::stageMutex);
 lab::structurePalette.reset();lab::cityNaturalBlockCache.clear();
 lab::extraPlans.clear();lab::extraDelegates.clear();lab::prefilteredChunks.clear();
 lab::cityNeighbourCache.clear();lab::cityLinks.clear();
 lab::villages.clear();lab::iglooPopulationPlans.clear();lab::automaticBlueprints.clear();lab::vegetationPairs.clear();
 lab::pending.reset();lab::pendingIgloo.reset();lab::nativeEntityJobs.clear();
 lab::queuedEntityKeys.clear();lab::villageSpawns.clear();lab::villagePopulationPlans.clear();
 lab::scatteredFeatureRecord=nullptr;lab::villageFeatureRecord=nullptr;lab::extraFeatureRecords.clear();
 if(auto dim=level->getDimension(0).lock()){
  auto* generator=dim->getWorldGenerator();
  if(generator&&generator->mStructureFeatureRegistry){
   auto&entries=*generator->mStructureFeatureRegistry->mStructureFeatures;
   std::erase_if(entries,[](auto const&p){return lab::ownedNativeFeatures.contains(p.get());});
  }
 }
 lab::ownedNativeFeatures.clear();lab::fossilTemplatesReady=false;
 lab::frozenFossils.clear();lab::fossilJournalRows.clear();lab::rejectedFossils.clear();
 lab::rejectedFossilOrder.clear();lab::recentFossilFragments.clear();lab::recentFossilFragmentOrder.clear();
 lab::nativeSmallDefinitions.clear();lab::completedSmallChunks.clear();
 lab::recentSmallChunks.clear();lab::recentSmallOrder.clear();
 lab::mansionTemplatesReady=false;lab::villageTemplatesReady=false;
 lab::entityLedgerReady=false;lab::entityLedger=Json::object();lab::field.reset();
 current=nullptr;terrain::log().info("Karma client: world state released");
}

void bindWorld(Level&level,std::string const&name,LevelSettings const&settings){
 auto root=std::filesystem::u8path(level.getLevelStorage().getFullPath().value);
 // DB storage implementations may return either the world root or its db directory.
 if(root.filename()=="db")root=root.parent_path();
 terrain::log().info("Karma client: opening {} at {}",name,root.string());
 if(!std::filesystem::exists(root/"karma-worldgen.json"))return;
 if(current)throw std::runtime_error("Previous Karma world is still active");
 auto marker=terrain::jsonFile(root/"karma-worldgen.json");
 if(marker.value("host","")!="windows-client")throw std::runtime_error("Not a client world package");
 auto seed=settings.getSeed().toUiString();
 if(marker.at("worldSeed").get<std::string>()!=seed)throw std::runtime_error("World seed differs from its immutable Karma seed package");
 validateStoredGenerator(root,seed);
 terrain::log().info("Karma client: world generator provenance validated; runtime generator={}",int(settings.getGenerator()));
 current=&level;
 terrain::clientWorldPath=root;terrain::Base=root/"karma";
 terrain::readData();
 if(terrain::nativeData.at("seedManifest").at("terrainSeed").get<std::string>()!=seed)
  throw std::runtime_error("Terrain was baked for a different world seed");
 // Identity is the actual world directory and seed. Display names may be renamed.
 terrain::worldName=name;lab::activeWorld=name;
 lab::clientWorldIdentity=marker.at("worldName").get<std::string>();
 lab::clientTerrainFolder=terrain::Base;lab::clientWorldFolder=root;
 lab::folder=terrain::Base/"structures";std::filesystem::create_directories(lab::folder);
 lab::initialize();
 {std::lock_guard lock(lab::stageMutex);lab::initializeSmallHistory();lab::initializeFossilJournal();lab::initializeEntityLedger();}
 current=&level;terrain::active=true;lab::automaticStructures=true;lab::smallFeaturesEnabled=true;
 terrain::generated=0;terrain::errors=0;terrain::mainThread=GetCurrentThreadId();
 sessionTicks=0;tickErrors=0;lab::stageCalls=0;lab::stageErrors=0;
 terrain::log().info("Karma client ready: seed={} kernel={} bounds={}",seed,NativeRuntime::KernelVersion,terrain::unboundedWorld?"infinite":"saved world bounds");
 std::ofstream(diagnostics/"world-session.json")<<Json{{"world",name},{"path",root.string()},{"seed",seed},{"kernel",NativeRuntime::KernelVersion},{"entered",true}}.dump(2);
}

using BiomeData=std::optional<std::reference_wrapper<std::unordered_map<std::string,std::unique_ptr<BiomeJsonDocumentGlueResolvedBiomeData>>>>;
LL_TYPE_INSTANCE_HOOK(WorldOpen,ll::memory::HookPriority::Normal,ServerLevel,&ServerLevel::$initialize,bool,
 std::string const&name,LevelSettings const&settings,Experiments const&experiments,std::string const*id,BiomeData biomes){
 try{bindWorld(*this,name,settings);}
 catch(std::exception const&e){
  terrain::log().error("Karma world rejected before generation: {}",e.what());
  std::ofstream(diagnostics/"world-error.txt")<<e.what();
  if(current==this)clearWorld(this);
  return false;
 }
 bool ok=origin(name,settings,experiments,id,biomes);
 if(ok&&current==this){
  try{
   // Finish native template loading on the level thread before chunk jobs can
   // mark decoration complete. Missing templates must not silently omit villages.
   lab::warmVillageTemplates(*this);lab::warmFossilTemplates(*this);
   terrain::log().info("Karma client: vanilla village and fossil templates ready");
  }catch(std::exception const&e){
   terrain::log().error("Karma native templates unavailable: {}",e.what());
   std::ofstream(diagnostics/"world-error.txt")<<e.what();ok=false;
  }
 }
 if(!ok&&current==this)clearWorld(this);
 return ok;
}
LL_TYPE_INSTANCE_HOOK(WorldClose,ll::memory::HookPriority::Normal,ServerLevel,&ServerLevel::$dtor,void){
 // ServerLevel destruction follows completion of its chunk work; release registry
 // references while dimensions still exist, before the original destructor.
 if(current==this)clearWorld(this);
 origin();
}

} // namespace client
#include "NewWorldCreation.inc"
namespace client {
class Mod {
public:
 static Mod&getInstance(){static Mod m;return m;}
 bool load(){
  auto&m=terrain::Mod::getInstance().self;diagnostics=m.getModDir()/"diagnostics";
  std::array<wchar_t,32768> exe{};
  auto n=GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size()));
  if(!n||n==exe.size())return false;
  lab::clientVanillaFolder=std::filesystem::path(std::wstring(exe.data(),n)).parent_path()/"data/behavior_packs/vanilla";
  std::filesystem::create_directories(diagnostics);
  m.getLogger().info("Karma Windows client 0.1.0; local world creation enabled");
  return true;
 }
 bool enable(){
  creation::enable();
  WorldOpen::hook();WorldClose::hook();terrain::Generate::hook();
  lab::SurfaceMapProbe::hook();lab::VillageHeightProbe::hook();lab::ModernGround::hook();
  lab::VillageBuilderDepth::hook();lab::VillageBuilderBudget::hook();
  lab::ShipGround::hook();lab::ShipGroundLegacy::hook();lab::PortalGround::hook();lab::RuinGround::hook();
  lab::MineSupport::hook();lab::MineWeb::hook();lab::SeededStructureRead::hook();lab::SeededStructureLayer::hook();
  lab::SeededStructureSolid::hook();lab::FragmentChunkWrite::hook();lab::FragmentChunkFastWrite::hook();
  lab::StructurePrefill::hook();lab::AutoStage::hook();lab::NativeLocate::hook();
  lab::SmallWrite::hook();lab::SmallWriteSimple::hook();lab::SmallWriteFast::hook();
  lab::FossilHeightXZ::hook();lab::FossilHeightPos::hook();lab::FossilCorners::hook();
  listener=ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto&e){
   if(current!=&e.level())return;
   ++sessionTicks;
   try{
    lab::tickIglooEntities(e.level());lab::tickStructureEntities(e.level());
    terrain::glacierWeatherTick(e.level());
   }catch(std::exception const&ex){if(++tickErrors==1||tickErrors%200==0)terrain::log().error("Karma client tick ({} errors): {}",tickErrors,ex.what());}
   if(sessionTicks%100==0)writeStatus(e.level());
  });
  return true;
 }
 bool disable(){
  if(current)return false;
  creation::disable();
  ll::event::EventBus::getInstance().removeListener(listener);
  WorldOpen::unhook();WorldClose::unhook();terrain::Generate::unhook();
  lab::Mod::getInstance().disable();return true;
 }
 bool unload(){return !current;}
};
}
LL_REGISTER_MOD(client::Mod,client::Mod::getInstance());
