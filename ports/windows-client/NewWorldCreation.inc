// The OreUI create-world stage is held before any world/DB is created. Heavy
// seed preparation runs in a cancellable child process, never on the UI thread.
#include "mc/client/gui/oreui/binding/facets/vanilla/CreateNewWorldFacet_DEPRECATED.h"
#include "mc/client/gui/oreui/binding/providers_deprecated/WorldSettingsProvider_DEPRECATED.h"
#include "mc/client/gui/ProgressHandler.h"
#include "mc/client/gui/CallbackProgressHandler.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/gui/screens/models/IContentManager.h"
#include "mc/resources/IResourcePackRepository.h"
#include "mc/deps/core/file/Path.h"
#include "mc/platform/UUID.h"
#include "NewWorldFiles.h"
#include "NewWorldStatus.h"
#include "PinnedGameImage.h"

namespace client::creation {
using Facet=OreUI::CreateNewWorldFacet_DEPRECATED;
using Provider=OreUI::WorldSettingsProvider_DEPRECATED;
std::filesystem::path modFolder;
bool enabled=true;
bool hooksInstalled=false;
// LL 26.20.7 omits the nonempty FacetBase layout. This correction is local to
// the pinned 1.26.20.04 adapter; never call methods through an adjusted this.
static_assert(offsetof(Facet,mWorldSettingsProvider)==0x1e0);
static_assert(offsetof(Facet,mStatus)==0x11);
template<class T> T& facetField(Facet& f,T Facet::*member){
 return *reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&(f.*member))+0x18);
}
bool supportedFacetLayout(){
 auto base=reinterpret_cast<unsigned char const*>(GetModuleHandleW(nullptr));
 auto check=image::inspect([&](uint32_t rva,void* out,size_t size){
  if(!base||uint64_t(rva)+size>0xea9f000)return false;
  MEMORY_BASIC_INFORMATION page{};auto address=base+rva;
  if(!VirtualQuery(address,&page,sizeof(page))||page.State!=MEM_COMMIT||
     (page.Protect&(PAGE_NOACCESS|PAGE_GUARD))||
     size>size_t(static_cast<unsigned char const*>(page.BaseAddress)+page.RegionSize-address))return false;
  std::memcpy(out,address,size);return true;
 });
 std::ofstream(diagnostics/"game-image.json")<<Json{
  {"supported",check.supported},{"reason",check.reason},{"timestamp",check.timestamp},
  {"imageSize",check.imageSize},{"modVersion",shanhai_release::Windows}}.dump(2);
 if(!check.supported)terrain::log().error("Minecraft image rejected: {} (timestamp=0x{:x}, imageSize=0x{:x})",check.reason,check.timestamp,check.imageSize);
 return check.supported;
}

bool supportedSeedParser(){
 Json checks=Json::array();bool valid=true;
 for(auto const* text:{"0","-1","288493980","9223372036854775807","-9223372036854775808"}){
  auto parsed=LevelSettings::parseSeedString(text);
  auto actual=parsed?parsed->toUiString():std::string("invalid");
  bool matches=actual==text;valid=valid&&matches;
  checks.push_back({{"input",text},{"actual",actual},{"matches",matches}});
 }
 std::ofstream(modFolder/"creation-seed-check.json")<<Json{{"passed",valid},{"cases",checks}}.dump(2);
 return valid;
}

std::wstring quote(std::wstring const& value){
 std::wstring out=L"\"";size_t slashes=0;
 for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}
  out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
  if(c==L'"')out+=L'\\';out+=c;
 }out.append(slashes*2,L'\\');return out+L'"';
}
struct Job {
 std::filesystem::path path;
 std::string seed,name,phase="正在准备 Karma 世界";
 Json result;
 HANDLE group=nullptr,process=nullptr;
 bool ready=false,failed=false,cancelled=false,progressFinished=false,handedOff=false,attached=false;
 float progress=0.f;
 std::chrono::steady_clock::time_point lastPoll{};
 std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
 ~Job(){cancel();}
 void trace(char const* stage) const {
  // Only lifecycle transitions write diagnostics, never per-frame polling.
  std::ofstream(path/"handoff.jsonl",std::ios::app)
   <<Json{{"stage",stage},{"seed",seed},{"ready",ready},{"failed",failed},
    {"cancelled",cancelled},{"progressFinished",progressFinished},
    {"handedOff",handedOff},{"attached",attached},{"thread",GetCurrentThreadId()}}.dump()<<'\n';
 }
 void cancel(){
  if(group){CloseHandle(group);group=nullptr;} // Kill only this owned process tree.
  if(process){CloseHandle(process);process=nullptr;}
 }
 void cancelRequest(){
  if(cancelled)return;
  cancelled=true;phase="已取消创建";cancel();
  // The worker was killed with its owned process tree; its last running status
  // must not continue to describe a live request.
  auto tmp=path/"cancel-status.tmp";
  std::ofstream(tmp)<<Json{{"state","cancelled"},{"phase",phase},{"seed",seed}}.dump();
  MoveFileExW(tmp.c_str(),(path/"status.json").c_str(),MOVEFILE_REPLACE_EXISTING);
  trace("cancelled");
 }
 void fail(std::string message){
  failed=true;phase=std::move(message);
  cancel(); // This job owns only preparation helpers, never the game process.
  try {terrain::log().error("Create world seed={}: {}",seed,phase);} catch(...) {}
  try { std::ofstream(path/"failure.json")<<Json{{"stage","world-preparation"},{"seed",seed},
   {"version",terrain::jsonFile(modFolder/"manifest.json").value("version",std::string{})},
   {"reason",phase},{"worldCreated",false},{"action","Retry preparation after correcting the cause"}}.dump(2); } catch(std::exception const&) {}
  try {trace("failed");} catch(...) {}
 }
 void poll(){
  auto now=std::chrono::steady_clock::now();
  if(ready||failed||cancelled||now-lastPoll<std::chrono::milliseconds(200))return;
  lastPoll=now;
  if(now-started>std::chrono::minutes(30)){fail("世界准备超过 30 分钟，已停止准备进程。请检查日志后重试");return;}
  try{auto s=terrain::jsonFile(path/"status.json");phase=s.value("phase",phase);
   progress=std::max(progress,std::clamp(s.value("progress",0.f),0.f,.995f));
   if(auto message=creation_status::failure(s)){fail(*message);return;}
  }catch(std::exception const&){} // Worker may not have published status yet.
  DWORD code=STILL_ACTIVE;
  if(!GetExitCodeProcess(process,&code)){fail("无法读取准备进程状态，Windows 错误 "+std::to_string(GetLastError()));return;}
  if(code==STILL_ACTIVE)return;
  if(code!=0){fail("世界准备进程退出，代码 "+std::to_string(code)+"；详情见创建任务的 worker.log");return;}
  try{
   result=terrain::jsonFile(path/"status.json");
   if(result.at("state")!="ready"||result.at("seed")!=seed)
    throw std::runtime_error("Worker did not publish a matching seed");
   auto marker=terrain::jsonFile(path/"prepared/karma-worldgen.json");
   if(marker.at("terrainSeed")!=seed||marker.at("kernel")!=NativeRuntime::KernelVersion)
    throw std::runtime_error("Prepared package identity mismatch");
   ready=true;progress=1.f;
  }catch(std::exception const&e){fail(std::string("世界数据校验失败：")+e.what());}
 }
 void start(){
  static std::atomic_uint serial=0;
  path=modFolder/"creation-jobs"/(std::to_string(GetTickCount64())+"-"+std::to_string(++serial));
  std::filesystem::create_directories(path);
  std::ofstream(path/"request.json")<<Json{{"seed",seed},{"name",name}}.dump();
  auto runtime=modFolder/"runtime";
  auto exe=runtime/"python/python.exe";
  if(!std::filesystem::exists(exe)||!std::filesystem::exists(runtime/"bundle.json"))
   throw std::runtime_error("Karma seed worker is not installed");
  auto command=quote(exe.wstring())+L" -I -X utf8 "+quote((runtime/"ports/windows-client/seed_service.py").wstring())+L" "+quote((path/"request.json").wstring());
  group=CreateJobObjectW(nullptr,nullptr);
  if(!group)throw std::runtime_error("Cannot create worker job");
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!SetInformationJobObject(group,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw std::runtime_error("Cannot configure worker job");
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
  HANDLE log=CreateFileW((path/"worker.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&attributes,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr);
  STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
  si.hStdOutput=si.hStdError=log;si.hStdInput=input;PROCESS_INFORMATION pi{};
  auto ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,
   CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS,nullptr,runtime.c_str(),&si,&pi);
  CloseHandle(log);CloseHandle(input);
  if(!ok)throw std::runtime_error("Cannot launch Karma seed worker");
  process=pi.hProcess;
  if(!AssignProcessToJobObject(group,process)){TerminateProcess(process,1);CloseHandle(pi.hThread);throw std::runtime_error("Cannot contain Karma worker");}
  if(ResumeThread(pi.hThread)==DWORD(-1)){CloseHandle(pi.hThread);throw std::runtime_error("Cannot resume Karma worker");}
  CloseHandle(pi.hThread);
 }
};
std::map<Facet*,std::shared_ptr<Job>> jobs;
std::map<WorldCreationHelper*,std::shared_ptr<Job>> prepared;
std::map<WorldCreationHelper*,std::shared_ptr<Job>> awaitingServer;
bool alive(std::weak_ptr<bool> const& ref){auto p=ref.lock();return p&&*p;}
void resetFacet(Facet&f,std::string const&error={}){
 facetField(f,&Facet::mInWorldCreation)=false;facetField(f,&Facet::mIsDirty)=true;
 *facetField(f,&Facet::mInputError)=error;
 facetField(f,&Facet::mStatus)=error.empty()?Facet::FacetStatus::Idle:Facet::FacetStatus::InputError;
}
class Progress final:public CallbackProgressHandler {
 std::shared_ptr<Job> job;Facet* facet;std::weak_ptr<bool> life;
public:
 Progress(std::shared_ptr<Job> j,Facet&f):CallbackProgressHandler([]{return LoadingState::None;}),job(std::move(j)),facet(&f),life(facetField(f,&Facet::mIsAlive)){
  mProgressAnimation=ProgressAnimation::LoadingBar;
 }
 void onStart(MinecraftScreenModel&)override{}
 void tick(MinecraftScreenModel&)override{job->poll();}
 void onCancel(MinecraftScreenModel&)override{
  if(job->cancelled||job->progressFinished)return;
  // Closing navigation may destroy this handler, just like onComplete.
  auto j=job;auto f=facet;auto weak=life;
  j->cancelRequest();j->progressFinished=true;
  if(!alive(weak))return;
  resetFacet(*f);jobs.erase(f);
  facetField(*f,&Facet::mProgressScreenNavigation)->leaveProgressScreen();
 }
 void onExit(MinecraftScreenModel&)override{}
 void onComplete(MinecraftScreenModel&)override{
  // Already on the UI thread. ClientThreadExecutor waits for ClientLevelTickEvent,
  // which cannot fire before this world exists: scheduling there deadlocks creation.
  // Native progress completion may be requested more than once before screen removal.
  if(job->progressFinished||job->cancelled||(!job->ready&&!job->failed))return;
  auto j=job;auto f=facet;auto weak=life;
  if(!alive(weak))return;
  j->progressFinished=true;j->trace("progress_complete");
  // Navigation may destroy this handler. Use only the retained locals afterwards.
  facetField(*f,&Facet::mProgressScreenNavigation)->leaveProgressScreen();
  if(!alive(weak)||j->cancelled)return;
  j->trace("progress_closed");
  if(j->failed){resetFacet(*f,j->phase);jobs.erase(f);return;}
  f->_handleStartCreatingWorld();
 }
 LoadingState getLoadingState(MinecraftScreenModel&)const override{
  return job->ready||job->failed||job->cancelled?LoadingState::Done:
   LoadingState(int(LoadingState::BuildingTerrain)|int(LoadingState::Cancellable));
 }
 float getLoadingProgress(MinecraftScreenModel&)const override{return job->progress;}
 ProgressAnimation showLoadingBar()const override{return ProgressAnimation::LoadingBar;}
 std::string getProgressMessage(MinecraftScreenModel&)const override{
  auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-job->started).count();
  return job->phase+"\n总进度 "+std::to_string(int(job->progress*100))+"% · 用时 "+std::to_string(seconds/60)+"分"+std::to_string(seconds%60)+"秒";
 }
 std::string getTitleText()const override{return "正在创建 Karma 世界";}
 std::string getName()const override{return "karma_seed_preparation";}
};

LL_TYPE_INSTANCE_HOOK(CreateStage,ll::memory::HookPriority::Normal,Facet,&Facet::_handleStartCreatingWorld,void){
 auto& provider=*facetField(*this,&Facet::mWorldSettingsProvider);
 auto& bindings=getLevelDataBindings();
 auto& advanced=*bindings.mAdvanced;
 if(!enabled||facetField(*this,&Facet::mCreatingOnRealms)||isEditorWorld()||provider.isFromWorldTemplate())return origin();
 auto found=jobs.find(this);
 if(found==jobs.end()){
  // Explicit superflat worlds retain their selected vanilla preset.
  if(advanced.getGeneratorType()==GeneratorType::Flat)return origin();
  if(advanced.getGeneratorType()!=GeneratorType::Overworld){
   resetFacet(*this,"Karma 暂不支持这个世界类型，请使用普通世界");return;
  }
  auto j=std::make_shared<Job>();
  // The binding resolves text/numeric/random seeds with vanilla's own semantics.
  auto seedText=advanced.getWorldSeed();
  std::ofstream(modFolder/"creation-entry.json")<<Json{{"entered",true},{"enabled",enabled},{"generator",int(advanced.getGeneratorType())},{"seedText",seedText},{"name",bindings.mGeneral->getWorldName()}}.dump(2);
  // The SDK declares LevelSeed64 as trivial, but the game's Level::parse/create
  // return it via an output buffer. Calling those signatures treats a stack
  // address as a seed. optional<LevelSeed64> is 16 bytes and has matching sret.
  static_assert(sizeof(std::optional<LevelSeed64>)==16);
  if(seedText.empty()){
   LevelSeed64 random{};
   if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&random.mValue),sizeof(random.mValue),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0){
    resetFacet(*this,"无法生成随机种子，请输入种子后重试");return;
   }
   j->seed=random.toUiString();
  }else{
   auto parsed=LevelSettings::parseSeedString(seedText);
   if(!parsed){resetFacet(*this,"无法解析世界种子，请检查种子输入");return;}
   j->seed=parsed->toUiString();
   // Decimal seeds must round-trip exactly (leading zeros/plus are permitted).
   try{size_t used=0;auto numeric=std::stoll(seedText,&used);
    if(used==seedText.size()&&j->seed!=std::to_string(numeric)){
     resetFacet(*this,"Karma 种子校验失败，已停止创建");return;
    }
   }catch(std::exception const&){} // Named seeds use vanilla's string hashing.
  }
  j->name=bindings.mGeneral->getWorldName();
  // Freeze a resolved random seed before the asynchronous boundary.
  advanced.setWorldSeed(j->seed);
  try{
   j->start();jobs[this]=j;
   auto& nav=*facetField(*this,&Facet::mProgressScreenNavigation);
   auto scene=nav.mSceneFactory.createModalProgressScreen("karma_seed_preparation",std::make_unique<Progress>(j,*this),true,true,"",true,0.8f);
   (*nav.mGetSceneStack)()->pushScreen(std::move(scene),false);
   terrain::log().info("Karma new-world preparation: seed={}",j->seed);
  }catch(std::exception const&e){j->cancel();jobs.erase(this);resetFacet(*this,"Karma 创建准备无法启动，请检查模组安装");terrain::log().error("Create world: {}",e.what());}
  return;
 }
 auto j=found->second;j->poll();
 if(!j->ready||!j->progressFinished||j->failed||j->cancelled||j->handedOff)return;
 if(advanced.getWorldSeed()!=j->seed){resetFacet(*this,"准备期间种子发生变化，请重新创建");jobs.erase(this);return;}
 j->handedOff=true;
 j->trace("create_handoff");
 prepared[&provider.getWorldCreationHelper()]=j;
 advanced.setGeneratorType(GeneratorType::Flat);advanced.setFlatWorldPreset("TheVoid");
 origin();
 // The native stage only selects its next UI state; it has not started a server.
 // Replace our attachment failure with a specific input error before the next tick.
 if(j->failed){
  prepared.erase(&provider.getWorldCreationHelper());
  advanced.setGeneratorType(GeneratorType::Overworld);
  clearErrors();resetFacet(*this,j->phase);jobs.erase(this);
 }
}

LL_TYPE_INSTANCE_HOOK(CreateFiles,ll::memory::HookPriority::Normal,WorldCreationHelper,&WorldCreationHelper::createWorld,
 WorldCreationUtils::WorldCreationResult,LevelDataWrapper&data,LevelSummary&summary){
 auto found=prepared.find(this);
 if(found==prepared.end())return origin(data,summary);
 auto j=found->second;
 j->trace("create_files_enter");
 auto result=origin(data,summary);
 if(result!=WorldCreationUtils::WorldCreationResult::Join)return result;
 try{
  // _createWorldImpl copies the input summary, assigns the ID on its own copy,
  // and creates pack folders. Resolve that ID with the same native content API.
  // The input summary path and level.dat are not populated at this stage.
  auto id=*mLevelSummary->mId;
  auto root=std::filesystem::u8path(mContentManager.getWorldPath(id).value);
  std::ofstream(modFolder/"creation-path.json")<<Json{{"root",root.string()},{"id",id},{"seed",j->seed},{"levelDatExists",std::filesystem::exists(root/"level.dat")}}.dump(2);
  requireFreshWorldDirectory(root);
  auto saved=terrain::jsonFile(j->path/"prepared/karma-worldgen.json");
  if(mLevelSettings->mSeed->toUiString()!=j->seed||saved.at("worldSeed")!=j->seed)throw std::runtime_error("New world seed changed");
  if(mLevelSettings->getGenerator()!=GeneratorType::Flat||
     mLevelSettings->getFlatWorldOptions().toStyledString().find("TheVoid")==std::string::npos)
   throw std::runtime_error("Native new-world settings are not Flat/TheVoid");
  auto spawn=j->result.at("spawn");
  // Let vanilla perform the first level.dat save with its complete settings.
  *mLevelSettings->mDefaultSpawn=BlockPos{spawn.at("x").get<int>(),spawn.at("y").get<int>(),spawn.at("z").get<int>()};
  installPreparedWorld(root,j->path/"prepared");
  {std::lock_guard lock(freshWorldMutex);freshWorldSeeds[std::filesystem::weakly_canonical(root)]=j->seed;}
  j->attached=true;prepared.erase(this);awaitingServer[this]=j;
  j->trace("attached");
  terrain::log().info("Karma new world attached: {} seed={}",root.string(),j->seed);
  std::ofstream(diagnostics/"new-world-created.json")<<Json{{"world",root.string()},{"seed",j->seed},{"attached",true}}.dump(2);
 }catch(std::exception const&e){
  j->failed=true;terrain::log().error("Karma new world attachment failed: {}",e.what());
  j->phase=std::string("Karma 世界数据接入失败：")+e.what();
  j->trace("attachment_failed");
  std::ofstream(diagnostics/"new-world-error.txt")<<e.what();
  // Preserve the engine's result; CreateStage converts this job's failure into
  // an explicit Karma input error before it can advance to server startup.
  return result;
 }
 return result;
}
LL_TYPE_INSTANCE_HOOK(CreateServer,ll::memory::HookPriority::Normal,WorldCreationHelper,&WorldCreationHelper::startLocalServer,bool){
 auto found=awaitingServer.find(this);
 if(found==awaitingServer.end())return origin();
 auto j=found->second;
 if(j->failed||j->cancelled)return false;
 try{
  auto root=std::filesystem::u8path(mContentManager.getWorldPath(*mLevelSummary->mId).value);
  // This boundary is after native selected-pack copying and before native
  // startLocalServerAsync constructs its world pack stack and dimensions.
  // Re-merge here because the copy stage may have replaced the selection file.
  auto uuid=installPreparedHeightPack(root,j->path/"prepared");
  bool visibleInRepository=false;
  if(!uuid.empty()){
   auto& repository=*mResourcePackRepository;
   repository.addWorldResourcePacks(Core::Path(root));
   repository.refreshPacks();
   // This is a repository snapshot, not completion of the native world-pack
   // load. In 26.20 it can still be empty here for a newly attached pack.
   // Let native startup build the selected world stack. WorldOpen checks the
   // resulting dimension bounds and rejects it before joining if they differ.
   visibleInRepository=repository.getResourcePackByUUID(mce::UUID::fromString(uuid))!=nullptr;
  }
  j->trace("height_pack_prepared_before_server");
  std::ofstream(diagnostics/"new-world-height-pack.json")<<Json{{"world",root.string()},
   {"packUuid",uuid},{"filesAndSelectionVerified",!uuid.empty()},
   {"visibleInRepositorySnapshot",visibleInRepository},{"nativePackLoadPending",true},
   {"version",shanhai_release::Windows},{"stage","before-local-server"},
   {"runtimeHeightVerified",false}}.dump(2);
 }catch(std::exception const&e){
  j->fail(std::string("主世界高度包接入失败：")+e.what());return false;
 }
 bool started=origin();
 if(started)awaitingServer.erase(this);
 return started;
}
LL_TYPE_INSTANCE_HOOK(CreateFacetClose,ll::memory::HookPriority::Normal,Facet,&Facet::$dtor,void){
 auto it=jobs.find(this);
 if(it!=jobs.end()){
  auto j=it->second;j->cancel();std::erase_if(prepared,[&](auto const&p){return p.second==j;});
  std::erase_if(awaitingServer,[&](auto const&p){return p.second==j;});jobs.erase(it);
 }
 origin();
}
void enable(){
 modFolder=std::filesystem::absolute(terrain::Mod::getInstance().self.getModDir()).lexically_normal();
 auto config=modFolder/"client-config.json";
 if(std::filesystem::exists(config))enabled=terrain::jsonFile(config).value("newWorldsUseKarma",true);
 else std::ofstream(config)<<Json{{"newWorldsUseKarma",true}}.dump(2);
 if(!supportedFacetLayout()||!supportedSeedParser()){
  enabled=false;
  terrain::log().error("Karma new-world creation disabled: unsupported CreateNewWorldFacet ABI");
  std::ofstream(modFolder/"creation-abi-error.txt")<<"Unsupported create-world ABI; module enable aborted.";
  throw std::runtime_error("Unsupported create-world interface; module enable aborted");
 }
 std::error_code ignored;std::filesystem::remove(modFolder/"creation-abi-error.txt",ignored);
 std::ofstream(modFolder/"creation-abi.json")<<Json{{"supported",true},{"providerOffset",0x1f8},{"statusOffset",0x29}}.dump(2);
 terrain_runtime::HookTransaction hooks;
 hooks.add<CreateStage>("CreateStage");hooks.add<CreateFiles>("CreateFiles");hooks.add<CreateServer>("CreateServer");hooks.add<CreateFacetClose>("CreateFacetClose");
 hooks.commit();hooksInstalled=true;
 terrain::log().info("Karma create-world ABI verified: provider=0x1f8, status=0x29");
}
void disable(){
 for(auto const&[f,j]:jobs)j->cancel();jobs.clear();prepared.clear();awaitingServer.clear();
 if(hooksInstalled){CreateStage::unhook();CreateFiles::unhook();CreateServer::unhook();CreateFacetClose::unhook();hooksInstalled=false;}
}
}
