// Sparse seed-derived drainage graph; each segment is shared by adjacent chunks.
struct RiverSegment {double ax,az,bx,bz,aw,bw,ar,br;bool lakeEnd=false,terraced=false;double lakeT=1.,main=0,bend=0,runA=1000,runB=1000;bool waterfall=false;};
struct RiverTreeGuard{double x,z,r;};
std::vector<RiverTreeGuard> riverTrees;
std::unordered_map<uint64_t,std::vector<size_t>> riverTreeBins;
struct RiverJunction{double x,z,water=-32768;bool largeLake=false;};
std::vector<RiverJunction> riverJunctions;
std::unordered_map<uint64_t,std::vector<size_t>> riverJunctionBins;
std::vector<RiverSegment> riverSegments;
std::unordered_map<uint64_t,std::vector<size_t>> riverBins;
bool fittedRiverBanks=false,cutBanksEnabled=false,submergedCutBanks=false,naturalBendBanks=false,soilCutBanks=false,coherentBankProfiles=false;
bool alluvialBanks=false,matchedRiverBanks=false,localRiverBeds=false;
bool progressiveRiverBeds=false,naturalBankSlope=false,gradedJunctionBeds=false,deepMainstemBeds=false;
Cache<uint64_t,Surface> waterGroundCache{8192};
Surface waterGround(int x,int z){return waterGroundCache.get(key2(x,z),[&]{return compute(x,z,false);});}
void loadRivers(){
 if(!data.contains("rivers"))return;
 fittedRiverBanks=data.at("rivers").contains("mainstems");
 int bankVersion=data.at("rivers").value("cutBanks",0);
 cutBanksEnabled=bankVersion>=1;submergedCutBanks=bankVersion>=2;naturalBendBanks=bankVersion>=3;soilCutBanks=bankVersion>=4;
 coherentBankProfiles=bankVersion>=5;
 alluvialBanks=bankVersion>=6;matchedRiverBanks=bankVersion>=7;
 localRiverBeds=bankVersion>=8;
 progressiveRiverBeds=bankVersion>=9;
 deepMainstemBeds=bankVersion>=10;
 std::map<std::array<double,4>,double> bends;
 if(data.at("rivers").contains("mainstems"))for(auto const&m:data.at("rivers").at("mainstems"))for(auto const&r:m.at("reaches"))
  bends[{r.at("a").at(0),r.at("a").at(1),r.at("b").at(0),r.at("b").at(1)}]=r.at("bend");
 naturalBankSlope=data.at("rivers").value("naturalBankSlope",0)>=1;
 gradedJunctionBeds=data.at("rivers").value("sectionProfiles",0)>=1;
 if(data.at("rivers").contains("forestGuards"))for(auto const&g:data.at("rivers").at("forestGuards")){
  RiverTreeGuard t{g.at(1),g.at(2),double(g.at(3))+.65};size_t id=riverTrees.size();riverTrees.push_back(t);
  for(int z=int(std::floor((t.z-t.r-6)/64));z<=int(std::floor((t.z+t.r+6)/64));z++)for(int x=int(std::floor((t.x-t.r-6)/64));x<=int(std::floor((t.x+t.r+6)/64));x++)riverTreeBins[key2(x,z)].push_back(id);
 }
 for(auto const&r:data.at("rivers").at("routes"))if(r.at("receiver")=="lake"||r.at("receiver")=="ocean"){
  double x=r.at("join").at(0),z=r.at("join").at(1);auto cell=lakeAt(roundJS(x)+32768,roundJS(z)+32768);
  riverJunctions.push_back({x,z,cell?double(cell->level):-32768.,cell&&cell->flooded&&cell->sealAlcoves});
 }
 if(data.at("rivers").contains("outlets"))for(auto const&o:data.at("rivers").at("outlets"))riverJunctions.push_back({o.at("source").at(0),o.at("source").at(1)});
 // The visual influence is exactly zero beyond 56 blocks. Preserve original
 // junction order within each bin so overlapping mouths have identical ties.
 for(size_t id=0;id<riverJunctions.size();++id){auto const&j=riverJunctions[id];
  for(int z=int(std::floor((j.z-56)/64));z<=int(std::floor((j.z+56)/64));++z)
   for(int x=int(std::floor((j.x-56)/64));x<=int(std::floor((j.x+56)/64));++x)riverJunctionBins[key2(x,z)].push_back(id);
 }
 std::set<std::pair<double,double>> lakeEnds;
 for(auto const&r:data.at("rivers").at("routes"))if(r.at("receiver")=="lake")lakeEnds.emplace(double(r.at("join").at(0)),double(r.at("join").at(1)));
 for(auto const&r:data.at("rivers").at("segments")){
  RiverSegment s{r.at(0),r.at(1),r.at(2),r.at(3),r.at(4),r.at(5),r.at(6),r.at(7)};
  s.waterfall=C.value("uplandWaterways",0)>=1&&r.size()>11&&r.at(11).get<double>()>0;
  if(progressiveRiverBeds&&data.at("rivers").contains("headwaterRuns")){
   auto const&runs=data.at("rivers").at("headwaterRuns");
   if(runs.size()!=data.at("rivers").at("segments").size())throw std::runtime_error("Invalid headwater runs");
   auto const&a=runs.at(riverSegments.size());s.runA=a.at(0);s.runB=a.at(1);
   if(!std::isfinite(s.runA)||!std::isfinite(s.runB)||s.runA<0||s.runB<s.runA)throw std::runtime_error("Invalid headwater distance");
  }
  if(s.aw<s.bw||s.bw<seaLevelValue||s.aw>generatedMaxYExclusive-30||s.ar<.65||s.br<.65||s.ar>32||s.br>32||std::hypot(s.ax-s.bx,s.az-s.bz)>40)
   throw std::runtime_error("Invalid river segment");
  if(auto found=bends.find({s.ax,s.az,s.bx,s.bz});found!=bends.end()){
    s.main=smooth(4.,12.,(s.ar+s.br)*.5);s.bend=found->second;
    if(!std::isfinite(s.bend)||std::abs(s.bend)>1)throw std::runtime_error("Invalid mainstem curvature");
  }
  s.lakeEnd=lakeEnds.contains({s.bx,s.bz});
  if(s.lakeEnd){
   auto lake=lakeAt(roundJS(s.bx)+32768,roundJS(s.bz)+32768);
   if(lake&&lake->sealAlcoves){
    // The graph endpoint lies inside the lake. End the water descent at the
    // real wet rim, including the channel's sides, not at a circular radius
    // around that endpoint (which excavated a flat trench before the shore).
    double dx=s.bx-s.ax,dz=s.bz-s.az,len=std::max(1.,std::hypot(dx,dz));int steps=int(std::ceil(len*2));
    for(int k=0;k<=steps;++k){double t=double(k)/steps,x=mix(t,s.ax,s.bx),z=mix(t,s.az,s.bz);bool wet=false;
     for(double side:{-1.,0.,1.}){double r=mix(t,s.ar,s.br)*1.3;
      auto edge=lakeAt(roundJS(x-dz/len*r*side)+32768,roundJS(z+dx/len*r*side)+32768);
      wet|=edge&&edge->flooded&&edge->level==lake->level;
     }
     if(wet){s.lakeT=std::max(.08,t-1.5/len);break;}
    }
   }
  }
  double fall=s.aw-s.bw,length=std::hypot(s.bx-s.ax,s.bz-s.az);
  // Rare steep upland reaches only; leave all confluences/receivers untouched.
  s.terraced=fall>=10&&fall<=18&&length>=18&&s.bw>=70&&std::max(s.ar,s.br)<3.5&&hash01(roundJS(s.ax),roundJS(s.az),terrainSeed^2801)<.20;
  if(s.terraced)for(auto const&j:riverJunctions)if(std::min(std::hypot(s.ax-j.x,s.az-j.z),std::hypot(s.bx-j.x,s.bz-j.z))<96){s.terraced=false;break;}
  double halo=std::max(s.ar,s.br)*1.55+17+s.main*24;size_t id=riverSegments.size();riverSegments.push_back(s);
  for(int z=int(std::floor((std::min(s.az,s.bz)-halo)/64));z<=int(std::floor((std::max(s.az,s.bz)+halo)/64));z++)
   for(int x=int(std::floor((std::min(s.ax,s.bx)-halo)/64));x<=int(std::floor((std::max(s.ax,s.bx)+halo)/64));x++)riverBins[key2(x,z)].push_back(id);
 }
 if(riverSegments.size()>std::max<int64_t>(20000,int64_t(terrain_bounds::Chunks)*terrain_bounds::Chunks*20000/(1250*1250)))throw std::runtime_error("River segment budget exceeded");
}
void riverRelief(Surface&s,int wx,int wz,bool connectWater=true,bool fitWater=false){
 if(riverSegments.empty()||s.lake==1||s.stone>.01||s.volcanic>0)return;
 bool marine=s.water==seaLevelValue&&s.height<seaLevelValue;
 double x=wx-32768,z=wz-32768;auto it=riverBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));if(it==riverBins.end())return;
 auto guards=riverTreeBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));
 double treeEdge=INFINITY;
 if(guards!=riverTreeBins.end())for(auto id:guards->second){auto const&t=riverTrees[id];treeEdge=std::min(treeEdge,std::hypot(x-t.x,z-t.z)-t.r);}
 if(treeEdge<=0)return;
 double junction=0,largeMouthBlend=0,largeMouthLevel=0;
 auto mouths=riverJunctionBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));
 if(mouths!=riverJunctionBins.end())for(auto id:mouths->second){auto const&j=riverJunctions[id];
  double distance=std::hypot(x-j.x,z-j.z);junction=std::max(junction,1-smooth(8.,56.,distance));
  double blend=j.largeLake?1-smooth(18.,36.,distance):0;
  if(blend>largeMouthBlend){largeMouthBlend=blend;largeMouthLevel=j.water;}
 }
 double bankReach=9+junction*(4+1.5*matNoise.at(x/21.,z/21.));
 double best=INFINITY,radius=0,dist=0,depthSum=0,slopeMax=0,mouthBlend=0,mainSum=0,innerSum=0,bankSlopeSum=0,bankNx=0,bankNz=0;
 bool terraceReach=false,waterfallReach=false;
 // A bank's distance, water level and width must come from the same reach.
 // Combining the nearest tributary distance with a distant mainstem's level
 // cuts a deep slot just before the tributary influence expires.
 struct BankProfile {double bed,edge=0,radius=0,dist=0,nx=0,nz=0,main=0,inner=0,outer=0;int water=0;bool valid=false;};
 BankProfile bank{double(s.height)};
 double bankBlendWeight=0,bankBlendBed=0;
 double bankDetail=0,cachedDetail=0;bool detailReady=false;
 auto detail=[&](){if(!detailReady){cachedDetail=clamp(.7*matNoise.at(x/19.+81.,z/19.-137.)+.3*matNoise.at(x/47.-43.,z/47.+71.),-1.,1.);detailReady=true;}return cachedDetail;};
 double wetLevelSum=0,wetWeightSum=0,channelCut=0;
 double levelSum=0,weightSum=0,widthScale=1+.12*matNoise.at(x/37.,z/37.);
 for(auto id:it->second){auto const&a=riverSegments[id];double dx=a.bx-a.ax,dz=a.bz-a.az,len=dx*dx+dz*dz;
  double t=clamp(((x-a.ax)*dx+(z-a.az)*dz)/std::max(1.,len),0,1);
  double d=std::hypot(x-a.ax-t*dx,z-a.az-t*dz),r=mix(t,a.ar,a.br)*widthScale*(1+.28*junction),edge=d-r;
  double reach=bankReach+a.main*24;
  if(edge>=reach)continue;
  // Blend overlapping reaches, avoiding nearest-segment water seams at bends.
  double fall=a.aw-a.bw,slope=fall/std::max(1.,std::sqrt(len)),profile=t;double widthSlope=slope;
  if(a.terraced){
   // Two unevenly spaced drops and resting pools. Never raise water above
   // the previous linear profile, and preserve both network endpoint levels.
   double split=.43+.12*hash01(roundJS(a.ax),roundJS(a.az),terrainSeed^2803);
   double start=t<split?0:split,end=t<split?split:1;
   double u=(t-start)/(end-start),drop=clamp((u-.20)/.30,0,1);
   profile=start+(end-start)*std::max(u,drop);
   // Waterfall speed does not squeeze the river footprint or prescribe
   // an entire deep pool. Actual adjacent water columns determine support.
   if(edge<bankReach)terraceReach=true;
   slope=0;
  }
  if(a.lakeT<1.){profile=std::min(1.,t/a.lakeT);slope=(a.aw-a.bw)/std::max(1.,std::sqrt(len)*a.lakeT);}
  double level=mix(profile,a.aw,a.bw);
  if(a.waterfall){
   // Follow the measured ledge instead of interpolating a suspended water
   // ramp across its lower bank. No terrain is raised to hold this water.
   level=std::min(level,double(s.height-(containedWaterways?0:1)));
   if(edge<bankReach)waterfallReach=true;
  }
  r=std::max(1.2,r*(1-.40*smooth(.15,.9,widthSlope)));edge=d-r;
  if(edge>=reach)continue;
  double signedSide=(dx*(z-a.az)-dz*(x-a.ax))/std::max(1.,std::sqrt(len));
  if(matchedRiverBanks&&!marine&&a.main>.05){
   // Small coherent scallops follow the erosive outside of a real bend.
   // They widen its toe instead of manufacturing a higher bank shoulder.
   double outer=smooth(.08,.55,-a.bend*clamp(signedSide/std::max(1.,r),-1.,1.))*a.main
       *(1-smooth(.05,.20,slope))*(1-junction);
   if(outer>0){r*=1+outer*(localRiverBeds?(.025+.20*std::max(0.,detail())):(.04+.10*std::max(0.,detail())));edge=d-r;}
  }
  double weight=std::pow(1-smooth(-r,reach,edge),3);
  levelSum+=weight*level;weightSum+=weight;
  if(matchedRiverBanks){
   // Bank influence can reach far beyond the wet channel. It must not carry
   // a distant upstream pool into a lower receiving river or sea entrance.
   double wetWeight=std::pow(1-smooth(0.,r+1.,d),2);
   wetLevelSum+=wetWeight*level;wetWeightSum+=wetWeight;
  }
  mainSum+=weight*a.main;
  bankSlopeSum+=weight*slope;
  innerSum+=weight*a.main*a.bend*clamp(signedSide/std::max(1.,r),-1.,1.);
  if(coherentBankProfiles&&!marine&&edge>0){
   double localInner=a.main*a.bend*clamp(signedSide/std::max(1.,r),-1.,1.);
   double localReach=reach*(1+.55*std::max(0.,localInner)-.45*std::max(0.,-localInner));
   int localWater=int(std::floor(level+1e-6));double original=s.height;
   double bankLip=localRiverBeds?mix(a.main,.65,.05):(alluvialBanks?mix(a.main,1.5,.05):1.5);
   double localBed=std::min(original,mix(smooth(0,localReach,edge),localWater+bankLip,original));
   double outer=smooth(.10,.50,-localInner)*a.main*(1-junction)
       *(1-smooth(.08,.22,slope))*smooth(3.,7.,original-localWater);
   if(alluvialBanks){
    // Resolve the complete gentle bank before deciding whether its existing
    // terrace can erode. Including retention in the minimum would always
    // select a less-curved neighbour and erase legitimate outer banks.
    outer=0;
   }else if(outer>0){
    double shoulder=std::min(original,localWater+2.8+2.6*outer+.9*detail());
    localBed=mix(outer*(1-smooth(2.,localReach,edge)),localBed,std::max(localBed,shoulder));
   }
   localBed=mix(smooth(0,std::min(6.,edge),treeEdge),original,localBed);
   if(treeEdge<6)localBed=std::max(localBed,std::min(original,localWater+1.+edge*.85));
   // Each complete profile tends to original ground at its own boundary.
   // Their excavation union is continuous even when the winning reach changes.
   if(matchedRiverBanks){
    // Blend complete profiles locally. A remote lower reach must not win a
    // minimum and dig behind a higher channel, leaving its safety rim as a
    // free-standing embankment. Weight vanishes at each reach's own boundary.
    double influence=(1-smooth(0.,localReach,edge))/(1+edge);
    double bw=influence*influence*influence*influence;
    bankBlendWeight+=bw;bankBlendBed+=bw*localBed;
   }
   if(localBed<bank.bed)bank={localBed,edge,r,d,(x-a.ax-t*dx)/std::max(.001,d),(z-a.az-t*dz)/std::max(.001,d),a.main,localInner,outer,localWater,true};
  }
  double depth=mix(smooth(1.,4.,r),1.,2.5)+a.main*3.5;
  if(localRiverBeds)depth=mix(smooth(1.,6.,r),1.1,2.3)+a.main*3.7;
  if(progressiveRiverBeds)depth=1.+smooth(24.,180.,mix(t,a.runA,a.runB))*(depth-1.);
  if(a.lakeEnd){depth=1.;mouthBlend=std::max(mouthBlend,1-smooth(18.,42.,std::hypot(x-a.bx,z-a.bz)));}
  if(localRiverBeds&&edge<=0){
   // Union complete wet cross-sections. Mixing a tributary's radius with
   // a distant mainstem's depth made a pit followed by a sill at its mouth.
   // Each cut tends to zero at its own shoreline; shared segment endpoints
   // cannot deepen a bed just because the graph has more overlapping pieces.
   double u=d/std::max(1.,r),side=a.main*a.bend*clamp(signedSide/std::max(1.,r),-1.,1.);
   double bowl=1-smooth(0.,1.,u);
   if(gradedJunctionBeds)s.riverSection=std::max(s.riverSection,bowl);
   double cut=(depth-1)*bowl-clamp(side*8.,-2.,2.)*(1-smooth(.75,1.,u));
   if(deepMainstemBeds&&a.main>.05&&!a.lakeEnd){
    double run=smooth(24.,320.,mix(t,a.runA,a.runB));
    double offset=clamp(signedSide/std::max(1.,r),-1.,1.);
    // Shift the submerged thalweg towards the outside of the bend, leaving
    // shallow margins and the inside bar. Reuse the existing column detail.
    double thalweg=1-smooth(.05,.90,std::abs(offset+.24*a.bend));
    double scour=run*a.main*thalweg*(5.+5.*smooth(8.,24.,r)+.65*detail());
    cut=std::max(cut,scour);
   }
   channelCut=std::max(channelCut,cut);
  }
  depthSum+=weight*depth;if(edge<1)slopeMax=std::max(slopeMax,slope);
  if(edge<best){best=edge;radius=r;dist=d;bankNx=(x-a.ax-t*dx)/std::max(.001,d);bankNz=(z-a.az-t*dz)/std::max(.001,d);}
 }
 if(weightSum<1e-12)return;
 double main=mainSum/weightSum,inner=innerSum/weightSum;
 bankReach+=main*24;
 if(!coherentBankProfiles&&best>=bankReach)return;
 s.mainRiver=main;s.riverBar=std::max(0.,inner);
 if(marine&&best>0)return;
 s.riverJunction=junction;
 int water=int(std::floor(levelSum/weightSum+1e-6));double original=s.height,bed;
 // Continuous, seed-derived patches vary the banks along the bend. Neither
 // per-block randomness nor reach IDs are used, so seams stay shared.
 if(naturalBendBanks&&main>.05&&!marine&&std::abs(inner)>.05)
  bankDetail=detail();
 bool depositedBar=false;
 // A final segment already reaches lake level at the actual shoreline. Do
 // not flatten a radius of upstream water independently of that profile.
 // Rivers excavate the ground; they never build an artificial retaining levee.
 // Continue the same channel footprint across shallow seas without raising
 // sea level or filling naturally deeper seabeds. Lakes retain their mask.
 if(matchedRiverBanks&&best<=0&&wetWeightSum>1e-12)water=int(std::floor(wetLevelSum/wetWeightSum+1e-6));
 if(waterfallReach&&!marine)water=std::min(water,int(original)-(containedWaterways?0:1));
 if(marine){water=seaLevelValue;slopeMax=0;}
 if(containedWaterways&&best<=0&&!marine&&(connectWater||fitWater)){
  // The voxel shoreline, rather than the spline centre, bounds a falling
  // water column. Keep downstream wet faces open, but fit water below the
  // actual dry side banks. The matching bank pass retains this same ground.
  for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
   auto neighbour=waterGround(wx+dx,wz+dz);int ground=neighbour.height;
   riverRelief(neighbour,wx+dx,wz+dz,false);
   if(neighbour.water<=neighbour.height&&!neighbour.riverDeposit)
    water=std::min(water,ground);
  }
 }
 if(best<=0){double depth=std::max(depthSum/weightSum,1.+std::ceil(slopeMax*1.42));
  depth=std::max({2.,1.+std::ceil(slopeMax*1.42),mix(mouthBlend,depth,1.)});
  // Shoal the near-sea river floor by one block, fading upstream over five
  // blocks of water elevation. Keep naturally deeper seabeds unchanged.
  double shoal=1-smooth(seaLevelValue,seaLevelValue+5,double(water));
  bed=std::min(original,std::min(double(water-1),water-1-(depth-1)*(1-smooth(0,radius,dist))+shoal));
  // Signed, smoothed curvature controls both banks of a real bend. The
  // point bar shoals on the inside; the thalweg deepens towards the outside.
  // Preserve wet centre and one-block lateral connections at the edge.
  if(alluvialBanks&&main>0){
   // Vary depth, not a fixed eight-block bar height. The inside shallows
   // gradually; the deeper outside is a connected cross-section of the bed.
   double u=dist/std::max(1.,radius),bowl=1-smooth(0.,1.,u);
   double skew=clamp(inner*8.,-2.,2.)*(1-smooth(.75,1.,u));
   double target=water-1-(depth-1)*bowl+skew+shoal;
   bed=std::min(original,std::min(double(water-1),target));
  }else if(main>0){
   double bankFade=1-smooth(.75,1.,dist/std::max(1.,radius));
   bed=std::min(original,std::min(double(water-1),bed+inner*8*bankFade));
  }
  if(localRiverBeds){
   double cut=progressiveRiverBeds?channelCut:std::max(channelCut,std::ceil(slopeMax*1.42)*(1-smooth(0.,radius,dist)));
   bed=std::min(original,std::min(double(water-1),water-1-cut+shoal));
   // Floor converted every fractional cut at a spring into a full second
   // water block. Round only small-channel beds; deep mainstems keep their
   // accepted bed geometry. Exact falling edges are connected below.
   if(progressiveRiverBeds&&main<.05&&!marine)bed=std::min(original,double(roundJS(bed)));
  }
  if(naturalBendBanks&&!marine&&treeEdge>8){
   // Gentle inner bends may deposit one additional layer. The outer part of
   // the point bar can emerge; the centre, falls and receiving junctions keep
   // their full wet section. This is a land bar, not a raised water surface.
   double deposition=main*smooth(.10,.23,inner)
       *smooth(.25,.50,dist/std::max(1.,radius))
       *(1-smooth(.03,.12,bankSlopeSum/weightSum))*(1-junction);
   if(alluvialBanks||deposition>.55+.22*bankDetail){
    if(alluvialBanks){
     double shoaling=1.6*main*smooth(.15,.55,inner)*smooth(.55,.95,dist/std::max(1.,radius))
         *(1-smooth(.03,.12,bankSlopeSum/weightSum))*(1-junction);
     bed=std::min(original,std::min(double(water),bed+shoaling));
    }else bed=std::min(original,std::min(double(water),std::floor(bed+1e-6)+1.));
    depositedBar=bed>=water;
    // A dry bar at a rounded water-level step would expose the higher pool
    // sideways. Keep that cell wet; only emerged candidates need this check.
    if(depositedBar&&connectWater)for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
     Surface neighbour;neighbour.height=int(original);
     riverRelief(neighbour,wx+dx,wz+dz,false);
     if((neighbour.river>0||neighbour.riverDeposit)&&neighbour.water>water){bed=water-1.;depositedBar=false;break;}
    }
   }
  }
  if(submergedCutBanks&&!marine){
   // The outside thalweg must reach the cut bank instead of climbing a
   // symmetric sand ramp at both shorelines. Blend from mid-channel only
   // on the outside of a bend; do not deepen the inner point bar.
   double outer=smooth(.10,.50,-inner)*main*(1-junction)
       *(1-smooth(.08,.22,bankSlopeSum/weightSum));
   double bank=smooth(.30,.85,dist/std::max(1.,radius))*outer;
   bed=std::min(bed,mix(bank,bed,alluvialBanks?water-1.5-1.5*outer:water-3.-3.*outer));
   s.riverOuter=bank;
  }
  if((waterfallReach||terraceReach||shoal>0||largeMouthBlend>0||(fittedRiverBanks&&slopeMax>.45)||(progressiveRiverBeds&&bed>=water-1.))&&connectWater){
   // Minimum carve that keeps cardinal water columns face-connected. Probe
   // only cascades, shoals and large-lake mouths, with recursion disabled.
   for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
    Surface neighbour;neighbour.height=int(original);
    // At a sea entrance the adjacent natural column may already be marine.
    // Reusing this column's dry height would predict a higher river pool and
    // miss the actual sea-level water face. Sample the real receiving surface only
    // in this short transition; ordinary inland columns keep the fast path.
    if(containedWaterways)neighbour=waterGround(wx+dx,wz+dz);
    else if(waterfallReach||(alluvialBanks&&!marine&&shoal>0))neighbour=compute(wx+dx,wz+dz,false);
    riverRelief(neighbour,wx+dx,wz+dz,false,containedWaterways);
    if(neighbour.river>0||(alluvialBanks&&neighbour.water>neighbour.height))bed=std::min(bed,double(neighbour.water-1));
   }
  }
 }
 else {
  double nearestEdge=best;
  double blendedMain=main,blendedInner=inner;
  double outer=0;
  double reach=bankReach*(1+.55*std::max(0.,inner)-.45*std::max(0.,-inner));
  bed=std::min(original,mix(smooth(0,reach,best),water+(fittedRiverBanks?1.5:.5),original));
  // A cut bank retains a steep shoulder above the water. Merely narrowing
  // the smooth slope still produces a bowl and cannot express an overhang.
  // Curvature selects the outer bend; straight, inner and steep waterfall
  // reaches fade out. Retain existing land only, never build a river levee.
  outer=cutBanksEnabled?smooth(.10,.50,-inner)*main*(1-junction)
      *(1-smooth(.08,.22,bankSlopeSum/weightSum))*smooth(3.,7.,original-water):0.;
  if(outer>0){
   double shoulder=std::min(original,water+(naturalBendBanks?2.8+2.6*outer+.9*bankDetail:4.+6.*outer));
   bed=mix(outer*(1-smooth(2.,reach,best)),bed,std::max(bed,shoulder));
  }
  // Taper bank excavation outside the intact trunk disc. At the wet edge the
  // taper reaches full excavation, so it cannot dam or interrupt the stream.
  double bankBlend=smooth(0,std::min(6.,best),treeEdge);
  bed=mix(bankBlend,original,bed);
  // Keep a short soil shoulder beside tree roots, without raising natural
  // ground or changing the wet channel. A one-block bank replaces a raw cut.
  if(treeEdge<6)bed=std::max(bed,std::min(original,water+1.+best*.85));
  // Keep the established blended bend shoulder. Independent complete bank
  // profiles only bound excessive excavation; they must not lower a retained
  // outer bank merely because another segment has a weaker bend estimate.
  if(matchedRiverBanks&&bankBlendWeight>1e-12)bank.bed=bankBlendBed/bankBlendWeight;
  if(coherentBankProfiles&&(alluvialBanks||bank.bed>bed)){
   bed=bank.bed;outer=bank.valid?bank.outer:0.;
   if(bank.valid){
    best=bank.edge;radius=bank.radius;dist=bank.dist;bankNx=bank.nx;bankNz=bank.nz;
    main=bank.main;inner=bank.inner;water=bank.water;
    s.mainRiver=main;s.riverBar=std::max(0.,inner);
    bankDetail=main>.05&&std::abs(inner)>.05?detail():0.;
   }
  }
  if(alluvialBanks&&bank.valid){
   // No prescribed shoulder height: retain a fraction of a real low
   // terrace, and only in a slow, strong bend. High hills use the gentle
   // profile. Broad patches avoid making every outer bank an overhang.
   outer=smooth(localRiverBeds?.16:.22,.55,-blendedInner)*blendedMain*(1-junction)
       *(1-smooth(.03,.12,bankSlopeSum/weightSum))*smooth(2.,3.,original-water)
       *(1-smooth(localRiverBeds?12.:6.,localRiverBeds?20.:10.,original-water));
   if(outer>0)outer*=smooth(localRiverBeds?-.15:-.04,.18,matNoise.at(x/113.+37,z/113.-81));
   // Keep the retained fraction through the complete bank profile. Fading
   // it over only six blocks leaves an isolated lip with a ditch behind it.
   // The profile itself already returns to original ground at its outer edge.
   bed=mix(outer*(localRiverBeds?.45:(1-smooth(0.,6.,best))),bed,original);
  }
  // Bank smoothing must not cut a dry lake rim below its retained lake level.
  // The wet channel (best <= 0) remains open; this only protects dry shoulders.
  if(auto lake=lakeAt(wx,wz);lake&&!lake->flooded)
   bed=std::max(bed,std::min(original,double(lake->level)));
  if(containedWaterways){
   // Regional lakes store their exact flooded footprint, without the dry
   // collar used by the older baked lakes. Preserve the natural retaining
   // rim after bank erosion; never manufacture ground above that terrain.
   for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}})
    if(auto lake=lakeAt(wx+dx,wz+dz);lake&&lake->flooded)
     bed=std::max(bed,std::min(original,double(lake->level)));
  }
  if(fittedRiverBanks&&connectWater&&nearestEdge<2.){
   // Protect the one-block dry rim against the actual neighbouring water
   // profile, including overlapping tributaries. Never raise natural ground.
   for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
    Surface neighbour;neighbour.height=int(original);
    // A falling reach clamps water to its own ground. Reusing this bank's
    // height underestimated the higher neighbour by one or more blocks.
    if(containedWaterways)neighbour=waterGround(wx+dx,wz+dz);
    riverRelief(neighbour,wx+dx,wz+dz,false);
    // A bar may become wet after its own neighbour check at a level step.
    // Its preview still carries riverDeposit; include that retained level.
    if(neighbour.river>0||(progressiveRiverBeds&&neighbour.riverDeposit))
     bed=std::max(bed,std::min(original,double(neighbour.water)));
   }
  }
  s.riverBankCut=original-bed;
  s.riverBankWater=water;s.riverBankEdge=best;
  // A bounded, column-local notch opens towards the channel just above the
  // waterline. Its rounded rear closes within 3.5 blocks and keeps >=2 solid
  // roof blocks. The river bed/water footprint and highest solid surface
  // remain independent: this is not a lowered heightmap or a new cave field.
  if(outer>(alluvialBanks?.45:.15)&&treeEdge>8&&s.lake==0&&connectWater){
   // Fade the horizontal reach to zero as curvature weakens. A hard strength
   // threshold at a fixed depth can leave a closed slit behind the shore.
   double retreat=3.5*smooth(.15,.80,outer)*(naturalBendBanks?1.+.32*bankDetail:1.);
   if(alluvialBanks)retreat=(localRiverBeds?2.5:2.)*smooth(.45,.90,outer);
   if(best<retreat){
    // Rounded water levels can differ by one block along the shore. Use the
    // actual river-facing rim, including either side, to keep the recess
    // above neighbouring water instead of exposing a side-flow pocket.
    int rimWater=water,rimMin=water,rimFloor=-32768;bool connectedRim=true;
    for(double along:{-2.,0.,2.}){
     Surface rim;rim.height=int(original);
     riverRelief(rim,roundJS(wx-bankNx*(best+1.5)-bankNz*along),
                    roundJS(wz-bankNz*(best+1.5)+bankNx*along),false);
     if(rim.river>0){rimWater=std::max(rimWater,rim.water);rimMin=std::min(rimMin,rim.water);rimFloor=std::max(rimFloor,rim.height);}
     else connectedRim=false;
    }
    if(localRiverBeds){
     // A visible bite must share a block face with the open channel. A
     // rounded projection alone can reach across a solid low shore and
     // leave a buried pocket behind it, especially on a diagonal bank.
     bool openFace=false;int faceFloor=generatedMaxYExclusive-1;
     for(auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
      Surface face;face.height=int(original);riverRelief(face,wx+dx,wz+dz,false);
      if(face.river>0&&face.water==water){openFace=true;faceFloor=std::min(faceFloor,face.height);}
     }
     connectedRim&=openFace;rimFloor=std::max(rimFloor,faceFloor);
    }
    int lo=rimWater+1,hi=std::min(int(std::floor(bed))-2,
        int(std::floor(rimWater+1+(naturalBendBanks?1.8+.5*bankDetail:3.)*outer*std::sqrt(std::max(0.,1-best*best/(retreat*retreat))))));
    // In a level bend the recess continues below water and is filled with
    // source water, with a solid curved toe beneath. Falling reaches keep
    // the earlier dry notch to avoid projecting an upper pool sideways.
    if(submergedCutBanks&&rimMin==rimWater&&rimWater==water)
     lo=int(std::ceil(water-(2.+3.*outer)*std::sqrt(std::max(0.,1-best*best/(retreat*retreat)))));
    if(alluvialBanks){
     // Recesses open into a level channel. Version 8 can expose one block
     // above water beneath a two-block soil roof; never a detached air slit.
     lo=std::max(water-2,rimFloor+1);hi=std::min(water+(localRiverBeds?1:0),int(std::floor(bed))-2);
     if(!connectedRim||rimMin!=water||rimWater!=water)hi=lo-1;
    }
    if(hi>=lo){s.riverCutLow=lo;s.riverCutHigh=hi;s.riverCutWater=rimWater;}
   }
  }
  s.riverOuter=outer;
 }
 if(marine&&bed>=original)return;
 s.height=int(std::floor(bed+1e-6));s.continuous=(s.height-seaLevelValue)/123.;s.e=s.continuous;
 if(localRiverBeds){s.riverShore=smooth(.35,.95,dist/std::max(1.,radius));s.riverSlope=bankSlopeSum/weightSum;}
 s.cliffLow=s.cliffHigh=-32768;s.drip=0;
 if(best<=0&&s.height<water){s.water=std::max(s.water,water);s.kind=2;s.river=1;s.lake=0;s.lakeFrozen=false;}
 else if(depositedBar){s.water=std::max(s.water,water);s.kind=2;s.riverDeposit=true;}
}

int riverMaterial(Surface const&s,int x,int y,int z){
 if(y<s.height-3)return 2;
 if(localRiverBeds&&s.riverBedMaterial>=0){
  // Soil-derived edges continue down into the bed, without underwater grass
  // or copying logs/plants from mutable neighbouring chunks.
  if(s.riverBedMaterial==33||s.riverBedMaterial==85)return y==s.height?s.riverBedMaterial:3;
  return s.riverBedMaterial;
 }
 if(submergedCutBanks&&s.riverOuter>.45&&!(alluvialBanks&&s.mainRiver>.45))return 2;
 if(s.mainRiver>.45){
  if(s.riverBar>.03)return 5;
  // Broad sediment patches, not per-block gravel or regularly placed rocks.
  return matNoise.at((x-32768)/110.,(z-32768)/110.)<-.28?48:5;
 }
 double sediment=matNoise.at((x-32768)/55.,(z-32768)/55.);
 // Mountain streams expose stone/cobbles; lowland beds are sand. No gravel.
 if(s.height>90||sediment<-.22)return sediment>.10?20:2;
 return 5;
}

void riverSkin(Surface&s,int x,int z){
 if(!localRiverBeds)return;
 if(s.surf!=8){
  if(progressiveRiverBeds&&s.mainRiver>.4&&s.riverBar>.05&&s.riverBankCut>0&&s.height<=s.riverBankWater){
   double reach=1.2+3.*smooth(.05,.55,s.riverBar)+.8*matNoise.at((x-32768)/24.,(z-32768)/24.);
   if(s.riverBankEdge<reach)s.riverBedMaterial=5;
  }
  return;
 }
 double patch=matNoise.at((x-32768)/24.,(z-32768)/24.);
 double sandy=s.soilSand;
 // The inner bar's sand continues into the wet margin. Applying soil first
 // made a brown underwater ring between the dry sand and sandy channel.
 if(progressiveRiverBeds)sandy=std::max(sandy,s.mainRiver*smooth(.02,.10,s.riverBar));
 double shore=s.riverShore*(1-smooth(.65,.95,s.riverBar));
 double rock=smooth(.22,.60,s.riverSlope)*smooth(85.,155.,s.height)*(1-.85*s.mainRiver);
 // Sample once per column; the surrounding biome's soil fraction is already
 // available. Broad sediment belongs in deeper water, soil at the margins.
 if(rock>.48+.32*patch){s.riverBedMaterial=patch>.22?20:2;return;}
 if(shore>.40+.28*patch&&sandy<.62){
  s.riverBedMaterial=s.H>.66&&s.height<85?85:3;return;
 }
 if(sandy>.5||s.riverBar>.08){s.riverBedMaterial=5;return;}
 s.riverBedMaterial=patch<-.23?48:5;
}
