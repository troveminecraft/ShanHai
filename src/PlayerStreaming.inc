// Included inside namespace terrain after the engine headers and globals.
std::atomic<ChunkBuildOrderPolicyBase*> streamingPolicy=nullptr;
std::atomic<std::shared_ptr<std::vector<StreamFocus> const>> streamingFocus{std::make_shared<std::vector<StreamFocus> const>()};
std::atomic_uint64_t priorityCalls=0,priorityBoosts=0;
bool priorityHigherFirst=false;
unsigned nearPending=0,farPending=0,outsidePending=0,streamTicks=0;
uint64_t arrivalStarts=0,arrivalResends=0,arrivalRebinds=0,arrivalTimeouts=0;
uint64_t viewGapRepairs=0,viewGapResolved=0,viewGapResends=0;
struct PlayerStream {
 terrain_runtime::StreamPlanner planner;
 terrain_runtime::ArrivalRecovery arrival;
 terrain_runtime::NearChunkWatchdog watchdog;
 int cx=100000,cz=100000,ax=100000,az=100000;
 std::unique_ptr<ChunkViewSource> here,ahead;
 struct Patch {int x=0,z=0;double started=0;std::unique_ptr<ChunkViewSource> view;};
 std::array<Patch,1> patches;
 uint64_t progress=0;double nextProgressScan=0;
 terrain_runtime::ViewGapRecovery visible;
 int viewRadius=0;double nextViewScan=0,nextGapResend=0;bool gapResendPending=false;
};
std::unordered_map<std::string,PlayerStream> playerStreams;
bool arrivalReady(ChunkViewSource& source,int cx,int cz){
 for(int z=cz-1;z<=cz+1;++z)for(int x=cx-1;x<=cx+1;++x){
  auto c=source.getExistingChunk(ChunkPos{x,z});if(!c||c->getState().load()!=ChunkState::Loaded)return false;
 }
 return true;
}

void calibrateStreamingPriority(){
 ChunkBuildOrderPolicy::Influence i;
 i.position=ChunkPos{0,0};i.parameter=Vec3{0,0,0};i.type=ChunkBuildOrderPolicy::HintType::Player;
 int nearScore=i.getPriority(ChunkPos{0,0}),farScore=i.getPriority(ChunkPos{100,100});
 if(nearScore==farScore)throw std::runtime_error("Cannot establish engine chunk priority order");
 priorityHigherFirst=nearScore>farScore;
 std::vector<StreamFocus> test{{0,0,8,0,128}};
 if(!(streamingScore(0,0,test)<streamingScore(8,0,test)&&streamingScore(8,0,test)<streamingScore(-8,0,test)))throw std::runtime_error("Streaming priority invariant failed");
 log().info("[区块优先级] 原生近点={} 远点={}，已启用近处及前方优先",nearScore,farScore);
}
LL_TYPE_INSTANCE_HOOK(StreamPriority,ll::memory::HookPriority::Normal,ChunkBuildOrderPolicy,&ChunkBuildOrderPolicy::$getChunkRebuildPriority,int,ChunkPos const& cp){
 int base=origin(cp);
 if(!active||this!=streamingPolicy.load())return base;
 priorityCalls.fetch_add(1,std::memory_order_relaxed);auto f=streamingFocus.load();if(f->empty())return base;
 int score=streamingScore(cp.x,cp.z,*f);
 // Preserve native priorities elsewhere; bounded local promotion also covers
 // the neighbour stages and lighting, since BDS owns the same build policy.
 if(score>512)return base;
 priorityBoosts.fetch_add(1,std::memory_order_relaxed);
 return priorityHigherFirst?std::max(base,1000000-score):std::min(base,-1000000+score);
}
void updatePlayerStreaming(Level& level){
 if(++streamTicks%4)return;
 auto dim=level.getDimension(0).lock();if(!dim)return;
 streamingPolicy.store(&dim->getChunkBuildOrderPolicy());
 auto next=std::make_shared<std::vector<StreamFocus>>();std::unordered_set<std::string> seen;
 auto now=Clock::now();double seconds=std::chrono::duration<double>(now.time_since_epoch()).count();unsigned views=0;bool needsLaunch=false;
 static double previous=0,busyUntil=0;
 if(previous&&seconds-previous>.5)busyUntil=seconds+2.;
 previous=seconds;bool tickBusy=seconds<busyUntil;
 level.forEachPlayer([&](Player& player){
  if(static_cast<int>(player.getDimensionId())!=0)return true;
  auto name=player.getRealName();seen.insert(name);auto& s=playerStreams[name];auto p=player.getFeetPos();
  auto motion=s.planner.update(p.x,p.z,std::chrono::duration<double>(now.time_since_epoch()).count(),worldSession->waitingCalculators.load(),nearPending);
  int cx=int(std::floor(p.x/16)),cz=int(std::floor(p.z/16)),ax=int(std::floor(motion.aheadX/16)),az=int(std::floor(motion.aheadZ/16));
  if(validChunk(cx,cz))next->push_back({p.x/16.0,p.z/16.0,motion.aheadX/16.0,motion.aheadZ/16.0,motion.speed});
  bool arrival=motion.teleport||s.cx==100000;
  if(arrival){s.here.reset();s.ahead.reset();s.visible.gaps.clear();for(auto&v:s.patches)v.view.reset();s.gapResendPending=false;s.arrival.begin(seconds);++arrivalStarts;}
  else if(motion.turned)s.ahead.reset();
  // Keep a complete dependency halo. Far legacy references are released when
  // the player moves, leaves, or exceeds the finite custom test world.
  if(!validChunk(cx,cz)||views++>=4){s.here.reset();s.ahead.reset();s.visible.gaps.clear();for(auto&patch:s.patches)patch.view.reset();s.gapResendPending=false;s.arrival.active=false;return true;}
  auto move=[&](std::unique_ptr<ChunkViewSource>& v,int x,int z){
   if(!v)v=std::make_unique<ChunkViewSource>(dim->getChunkSource(),ChunkSource::LoadMode::Deferred);
   v->move(BlockPos{(x-8)*16,0,(z-8)*16},BlockPos{(x+9)*16-1,0,(z+9)*16-1},false,ChunkSourceViewGenerateMode::GenerateAll,{},nullptr);
  };
  if(!s.here||cx!=s.cx||cz!=s.cz){s.arrival.readySince=-1;move(s.here,cx,cz);s.cx=cx;s.cz=cz;}
  bool ready=arrivalReady(*s.here,cx,cz);needsLaunch|=!ready;
  // Track the entire dependency halo, not just the final central state. A
  // stationary TerrainGenerated chunk may be waiting on progressing neighbours.
  if(!ready&&seconds>=s.nextProgressScan){
   s.nextProgressScan=seconds+1.;uint64_t signature=1469598103934665603ULL;
   for(int z=cz-8;z<=cz+8;++z)for(int x=cx-8;x<=cx+8;++x){
    auto c=s.here->getExistingChunk({x,z});
    signature=(signature^uint64_t(c?int(c->getState().load())+1:0))*1099511628211ULL;
   }s.progress=signature;
  }
  // Scan the client view once/second, then request at most two small patches.
  // Readiness always comes from BDS; never mark unfinished chunks as Loaded.
  if(ready&&!tickBusy&&seconds>=s.nextViewScan){
   s.nextViewScan=seconds+2.;s.viewRadius=std::clamp(int(player.getChunkRadius())-2,2,32);
   int radius=s.viewRadius;s.visible.begin();
   for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx){
    if(dx*dx+dz*dz>radius*radius)continue;int x=cx+dx,z=cz+dz;if(!validChunk(x,z))continue;
    auto c=dim->getChunkSource().getExistingChunk({x,z});s.visible.observe(x,z,c&&c->getState().load()==ChunkState::Loaded,seconds);
   }s.visible.end();
   for(auto&patch:s.patches){
    if(patch.view){
     bool done=arrivalReady(*patch.view,patch.x,patch.z);
     if(done)++viewGapResolved;
     if(done||seconds-patch.started>=20||std::max(std::abs(patch.x-cx),std::abs(patch.z-cz))>radius)patch.view.reset();
    }
    if(!patch.view&&terrain_runtime::allowSupplementalLoading(ready,worldSession->waitingCalculators.load(),nearPending,tickBusy))if(auto gap=s.visible.next(seconds,cx,cz)){
     patch.x=gap->x;patch.z=gap->z;patch.started=seconds;gap->attempted=seconds;
     patch.view=std::make_unique<ChunkViewSource>(dim->getChunkSource(),ChunkSource::LoadMode::Deferred);
     // BDS decoration/light dependencies need the full eight-chunk halo,
     // just like the existing arrival view. A radius-four patch can stall.
     patch.view->move(BlockPos{(patch.x-8)*16,0,(patch.z-8)*16},BlockPos{(patch.x+9)*16-1,0,(patch.z+9)*16-1},false,ChunkSourceViewGenerateMode::GenerateAll,{},nullptr);++viewGapRepairs;
    }
   }
   // Native publication sends newly completed chunks. Do not resend the entire
   // view per repaired patch: that amplifies bandwidth during normal flight.
  }
  needsLaunch|=!s.visible.gaps.empty();
  if(s.watchdog.poll(cx,cz,seconds,ready,s.arrival.active)){
   s.arrival.begin(seconds);++arrivalStarts;
   log().warn("[就近加载] {} ({},{}) 停留后近邻仍未就绪，启动一次有界恢复",name,cx,cz);
  }
  using Action=terrain_runtime::ArrivalRecovery::Action;
  switch(s.arrival.poll(seconds,ready,s.progress,tickBusy)){
   case Action::Rebind:{std::unique_ptr<ChunkViewSource> renewed;move(renewed,cx,cz);s.here=std::move(renewed);++arrivalRebinds;log().warn("[传送加载] {} ({},{}) 近邻依赖连续 8 秒无阶段进展，重新请求依赖范围（仅一次）",name,cx,cz);break;}
   case Action::Resend:player.resendAllChunks();++arrivalResends;log().info("[传送加载] {} ({},{}) 近邻已就绪，已请求重发玩家区块",name,cx,cz);break;
   case Action::Timeout:++arrivalTimeouts;log().warn("[传送加载] {} ({},{}) 30 秒仍未就绪；保留正常加载，停止自动重试",name,cx,cz);break;
   default:break;
  }
  if(!unboundedWorld){ax=std::clamp(ax,-Half,Half-1);az=std::clamp(az,-Half,Half-1);}
  if(terrain_runtime::allowSupplementalLoading(ready,worldSession->waitingCalculators.load(),nearPending,tickBusy)&&!s.patches[0].view&&motion.allowAhead&&std::max(std::abs(ax-cx),std::abs(az-cz))>=3){
   if(!s.ahead||std::max(std::abs(ax-s.ax),std::abs(az-s.az))>=2){move(s.ahead,ax,az);s.ax=ax;s.az=az;}
  }else s.ahead.reset();
  return true;
 });
 for(auto it=playerStreams.begin();it!=playerStreams.end();)if(!seen.contains(it->first))it=playerStreams.erase(it);else ++it;
 if(next->empty()&&requested)next->push_back({double(probeX),double(probeZ),double(probeX),double(probeZ),0});
 streamingFocus.store(std::shared_ptr<std::vector<StreamFocus> const>(next));
 if(needsLaunch)dim->getChunkSource().checkAndLaunchChunkGenerationTasks(false);
}
