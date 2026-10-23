#pragma once
#include <set>
#include "../WorldBounds.h"
#include "Noise.h"
#include "CaveNetwork.h"
#include "FiniteCaves.h"
#include "CavernGroups.h"
#include "RegionalPlans.h"
namespace nativegen {
inline double num(J const&j,char const*k){return j.at(k).get<double>();}
struct Surface {bool mushroom=false,mushroomZone=false;int badlands=0;int16_t seaCutLow=-32768,seaCutHigh=-32768;float seaBank=0,volcanicDeposit=1;double glacier=0,superGlacier=0,iceHabitat=0;int iceGround=-1;int iceThickness=0;bool iceSnow=false;int cavePoolBed=320,caveSealFloor=320;int aridType=0,aridTop=-1;double sandCover=1,desert=0,desertSand=0,desertCrest=0,desertPatch=0,oasis=0;int fluvialType=0,fluvialTop=-1;double riverShore=0,riverSlope=0,riverSection=0;int riverBedMaterial=-1;int riverBankWater=-32768;double riverBankEdge=INFINITY;bool riverDeposit=false,scarpWarped=false;int scarpCaveCeiling=319;int riverCutLow=-32768,riverCutHigh=-32768,riverCutWater=-32768;double riverOuter=0,scarp=0;int preRiverHeight=0,height=0,water=-32768,cliffLow=-32768,cliffHigh=-32768,drip=0,biome=0,kind=0,surf=0,treeK=1,rock=0,lake=0,aquifer=-1,lava=-32768;bool vent=false,lakeFrozen=false,volcanicMountainRock=false;double wetland=0,wetlandMoisture=0;int wetlandLevel=-32768;double mainRiver=0,riverBar=0,riverJunction=0,riverBankCut=0,river=0,soilSand=0,volcanicHot=0,estuary=0,volcanic=0,barren=0,ocean=0,coast=0,raw=0,T=0,H=0,ridge=0,e=0,continuous=0,mount=0,plateau=0,basin=0,stone=0,pillar=0,giant=0,treeD=0,rockCap=0;};
struct ReliefNode{double v,relief,pos,corr;};
struct Cliff{bool valid=false;double x=0,z=0,h=0,slope=0,nx=0,nz=0,crest=0;};
class Field {
 friend struct CompletedBiomesTest;
 // Opt-in for the mobile runtime; desktop/BDS retain their existing defaults.
 bool localPlanningCulling=false;
 J const&data;J const&C;uint32_t terrainSeed;Density graph,caves;
 std::function<double(double,double,double)> offset,erosion,ridge,temp,humidity,cave;
 Perlin low,high,secondary,warpX,warpZ,stoneNoise,giantNoise,matNoise,rockNoise,treeNoise,biomeNoise;
 std::array<Cache<uint64_t,double>,2> rawNodes{{Cache<uint64_t,double>(8192),Cache<uint64_t,double>(8192)}};
 std::array<Cache<uint64_t,ReliefNode>,2> nodes{{Cache<uint64_t,ReliefNode>(8192),Cache<uint64_t,ReliefNode>(8192)}};
 Cache<uint64_t,std::array<double,2>> climateNodes{8192};
 Cache<uint64_t,Surface> baseCache{32768},skinCache{32768};double caveRoof,caveFade,caveScale,waterline;Cache<uint64_t,Cliff> cliffs{4096};
 struct Pillar{double x,z,r,phase,outer,h;};std::vector<Pillar> pillars;std::unordered_map<uint64_t,std::vector<size_t>> pillarBins;
 #include "Caves.inc"
 #include "RegionalTerrain.inc"
 #include "Deserts.inc"
 #include "Landform.inc"
 #include "SmallStoneForests.inc"
 #include "Hydrology.inc"
 #include "Volcano.inc"
 #include "Ocean.inc"
 #include "Rivers.inc"
 #include "JunctionBeds.inc"
 #include "FluvialLandforms.inc"
 #include "AridLandforms.inc"
 #include "BirchHabitat.inc"
 #include "Wetlands.inc"
 #include "OverhangScarps.inc"
 #include "IceClimate.inc"
 #include "StreamingRegions.inc"
 #include "CompletedBiomes.inc"
 double v0(int level,int x,int z){auto const&q=C.at("relief").at("levels")[level];return rawNodes[level].get(key2(x,z),[&]{return vcont(x*num(q,"pitch"),z*num(q,"pitch"));});}
 ReliefNode node(int level,int x,int z){return nodes[level].get(key2(x,z),[&]{auto const&q=C.at("relief").at("levels")[level];double v=v0(level,x,z),lo=v,hi=v,sum=0;for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dz)continue;double a=v0(level,x+dx,z+dz);lo=std::min(lo,a);hi=std::max(hi,a);sum+=a;}double r=hi-lo,p=r>1e-6?(v-lo)/r:.5,w=smooth(0,num(q,"edgeBand"),p)*smooth(0,num(q,"edgeBand"),1-p),cap=std::min(num(q,"cap"),num(q,"reliefCap")*r);return ReliefNode{v,r,p,clamp(-num(q,"gain")*(v-sum/8)*w*smooth(num(q,"reliefLo"),num(q,"reliefHi"),r),-cap,cap)};});}
 double shaped(double x,double z,double v,bool applyRegional=true){auto const&q=C.at("relief");double corr=0,rel=0,pos=.5,regional=v;for(int i=0;i<2;i++){double R=num(q.at("levels")[i],"pitch"),gx=x/R,gz=z/R;int ix=int(std::floor(gx)),iz=int(std::floor(gz));auto a=node(i,ix,iz),b=node(i,ix+1,iz),c=node(i,ix,iz+1),d=node(i,ix+1,iz+1);double u=smooth(0,1,gx-ix),w=smooth(0,1,gz-iz);auto bl=[&](double A,double B,double D,double E){return A*(1-u)*(1-w)+B*u*(1-w)+D*(1-u)*w+E*u*w;};corr+=bl(a.corr,b.corr,c.corr,d.corr);regional=bl(a.v,b.v,c.v,d.v);if(!i){rel=bl(a.relief,b.relief,c.relief,d.relief);pos=bl(a.pos,b.pos,c.pos,d.pos);}}
 corr=clamp(corr,-num(q,"totalCap"),num(q,"totalCap"));if(corr>0)corr*=smooth(31+num(q,"waterFadeLo"),31+num(q,"waterFadeHi"),v);
 double v1=v+corr,detail=0,land=smooth(31+num(q,"landFadeLo"),31+num(q,"landFadeHi"),v1),flat=1-smooth(num(q,"flatLo"),num(q,"flatHi"),rel);
 if(land>0&&flat>0)detail+=land*flat*num(q,"lowAmplitude")*low.fbm(x/num(q,"lowWavelength"),z/num(q,"lowWavelength"),int(num(q,"lowOctaves")));
 double summit=smooth(num(q,"highReliefLo"),num(q,"highReliefHi"),rel)*smooth(num(q,"highPosLo"),num(q,"highPosHi"),pos);
 if(summit>0)detail+=summit*num(q,"highAmplitude")*(high.fbm(x/num(q,"highWavelength")+3.7,z/num(q,"highWavelength")+11.3,int(num(q,"highOctaves")),true)-num(q,"highRidgeMean"));
 auto const&s=q.at("secondary");double mask=smooth(num(s,"mountainLow"),num(s,"mountainHigh"),v);if(mask>0){double px=x-32768,pz=z-32768,dx=num(s,"warpBlocks")*warpX.at(px/num(s,"warpWavelength"),pz/num(s,"warpWavelength")),dz=num(s,"warpBlocks")*warpZ.at(px/num(s,"warpWavelength"),pz/num(s,"warpWavelength"));double u=(num(s,"rotationCos")*px-num(s,"rotationSin")*pz+num(s,"offsetX")+dx)/num(s,"wavelength"),w=(num(s,"rotationSin")*px+num(s,"rotationCos")*pz+num(s,"offsetZ")+dz)/num(s,"wavelength");detail+=mask*num(s,"amplitude")*(secondary.fbm(u,w,int(num(s,"octaves")),true)-num(s,"ridgeMean"));}
 auto const&sp=q.at("spike");double result=v1+detail+landformCorrection(x,z)-num(sp,"maxCut")*smooth(num(sp,"prominenceLo"),num(sp,"prominenceHi"),v-regional)*smooth(6,16,rel);return applyRegional?desertHeight(x,z,regionalHeight(x,z,result)):result;
 }
 double uncliff(double x,double z){return shaped(x,z,vcont(x,z));}
 Cliff patch(int gx,int gz){return cliffs.get(key2(gx,gz),[&]{Cliff a;auto const&q=C.at("cliffs");auto const&s=C.at("stoneForest");if(hash01(gx,gz,terrainSeed^int(num(q,"seed")))>=num(q,"chance"))return a;double cx=(gx+.5)*num(q,"grid"),cz=(gz+.5)*num(q,"grid");if(largeStoneForestEnabled()&&std::hypot(cx-32768-num(s,"centerX"),cz-32768-num(s,"centerZ"))<=num(s,"outerRadius")+num(s,"edgeWarp")+num(q,"grid"))return a;for(int dz:{-12,0,12})for(int dx:{-12,0,12}){double x=cx+dx,z=cz+dz,h=uncliff(x,z);if(h<num(q,"minHeight")||h>num(q,"maxHeight"))continue;double ex=(uncliff(x+8,z)-uncliff(x-8,z))/16,ez=(uncliff(x,z+8)-uncliff(x,z-8))/16,sl=std::hypot(ex,ez);if(sl<num(q,"minSlope")||sl>num(q,"maxSlope")||(a.valid&&sl<=a.slope))continue;a={true,x,z,h,sl,ex/sl,ez/sl,0};}if(a.valid)a.crest=std::max(a.h,uncliff(a.x+a.nx*24,a.z+a.nz*24));return a;});}
 Surface compute(int wx,int wz,bool drainage=true){double x=(wx-32768)*num(C.at("epic"),"horizontalScale"),z=(wz-32768)*num(C.at("epic"),"horizontalScale");Surface s;double er=erosion(x,0,z);s.ridge=ridge(x,0,z);s.raw=224+192*offset(x,0,z);double v=shaped(wx,wz,-64+(s.raw+64)*.75);
 smallStoneRelief(s,v,wx,wz);
 auto const&q=C.at("stoneForest");double px=wx-32768-num(q,"centerX"),pz=wz-32768-num(q,"centerZ"),d=std::hypot(px,pz);if(largeStoneForestEnabled()&&d<num(q,"outerRadius")+num(q,"edgeWarp")){
 s.stone=1-smooth(num(q,"innerRadius"),num(q,"outerRadius"),d+num(q,"edgeWarp")*stoneNoise.at(px/320,pz/320));if(s.stone>0){double best=-INFINITY,ph=0;Pillar const* site=nullptr;double rad=num(q,"largeRadiusMax")*1.165;for(int bz=int(std::floor((pz-rad)/128));bz<=int(std::floor((pz+rad)/128));bz++)for(int bx=int(std::floor((px-rad)/128));bx<=int(std::floor((px+rad)/128));bx++){auto it=pillarBins.find(key2(bx,bz));if(it==pillarBins.end())continue;for(auto i:it->second){auto const&a=pillars[i];double dx=px-a.x,dz=pz-a.z,r=std::hypot(dx,dz);if(r>a.outer)continue;double t=std::atan2(dz,dx),R=a.r*(1+.07*std::cos(2*t+a.phase)+.06*std::sin(3*t+a.phase*1.7)+.035*std::cos(5*t-a.phase)),gap=R-r;if(gap>best){best=gap;site=&a;}}}
 double mask=0;if(site&&best>0){mask=smooth(0,num(q,"wallBevel"),best);ph=mask*(site->h+num(q,"crownBroad")*stoneNoise.at(px/24+7,pz/24-9)+num(q,"crownDetail")*stoneNoise.at(px/9,pz/9));}
 double clear=smooth(num(q,"spawnClearRadius"),num(q,"spawnFadeRadius"),d),ground=num(q,"valleyY")+clear*(num(q,"groundAmplitude")*stoneNoise.fbm(px/36+7.3,pz/36-2.1,2)+num(q,"groundDetail")*stoneNoise.at(px/11,pz/11));s.pillar=mask*clear;v+=(ground+(ph+(num(q,"valleyY")-ground)*mask)*clear-v)*s.stone;
 if(s.stone>=.85&&s.pillar<=0&&d>=num(q,"spawnFadeRadius")&&hash01(wx,wz,terrainSeed^917)<=num(q,"dripstoneChance"))s.drip=2+int(hash01(wx,wz,terrainSeed^919)*5);
 }}
 // Continuous patch support: a patch may cross the selection grid without being cut off.
 // Shape the slope; sparse ellipsoidal alcoves are restricted to the cliff waist.
 if(s.stone==0){
  auto const& c=C.at("cliffs");double grid=num(c,"grid"),shift=0;
  int gx=int(std::floor(wx/grid)),gz=int(std::floor(wz/grid));
  for(int iz=gz-1;iz<=gz+1;++iz)for(int ix=gx-1;ix<=gx+1;++ix){
   double cx=(ix+.5)*grid,cz=(iz+.5)*grid;
   if(std::abs(wx-cx)>64||std::abs(wz-cz)>64)continue;
   auto a=patch(ix,iz);if(!a.valid)continue;
   double dx=wx-a.x,dz=wz-a.z,w=-dx*a.nz+dz*a.nx;
   double u=dx*a.nx+dz*a.nz-1.3*std::sin(w/13+a.h),front=num(c,"front"),len=num(c,"halfLength");
   if(std::abs(u)>=front||std::abs(w)>=len)continue;
   double strength=(1-smooth(front-16,front,std::abs(u)))*(1-smooth(len-14,len,std::abs(w)));
   double rise=std::min(num(c,"maxShift"),a.slope*6)*strength;
   // Nonzero transition width retains steep faces, without a discontinuous height step.
   shift+=rise*(2*smooth(-1.5,1.5,u)-1);
   if(C.contains("alcoves")){
    auto const& q=C.at("alcoves");
    if(q.value("enabled",false)&&a.h>=num(q,"minHeight")&&hash01(ix,iz,terrainSeed^607)<num(q,"chance")){
     double width=num(q,"halfWidth")*(.85+.3*hash01(ix,iz,terrainSeed^609)),depth=num(q,"depth");
     double shape=1-(w*w)/(width*width)-((u-2)*(u-2))/(depth*depth);
     if(shape>0){double mid=a.h-num(q,"midDepth"),span=num(q,"halfHeight")*std::sqrt(shape);
      int low=int(std::ceil(mid-span)),high=int(std::floor(std::min(mid+span,v+shift-num(q,"roof"))));
      if(high-low>=2&&low>40){s.cliffLow=low;s.cliffHigh=high;}
     }
    }
   }
  }
  v+=clamp(shift,-num(c,"maxShift"),num(c,"maxShift"));
 }

 s.coast=s.stone>0?0:coastWeight(wx,wz,v);
 v+=s.coast*24*smooth(29,45,v)*(1-smooth(52,96,v));
 oceanRelief(s,v,wx,wz);
 volcanicRelief(s,v,wx,wz);
 s.height=int(clamp(std::floor(v),-61,319));s.continuous=(v-31)/123;s.e=(s.height-31)/123.;auto const&tr=C.at("transition");double highW=smooth(145-num(tr,"domHeightWidth"),145+num(tr,"domHeightWidth"),s.raw);s.plateau=highW*smooth(.65-num(tr,"domErosionWidth"),.65+num(tr,"domErosionWidth"),er);s.mount=highW-s.plateau;s.basin=1-smooth(67-num(tr,"domHeightWidth"),67+num(tr,"domHeightWidth"),s.raw);
 double temperatureNoise=temp(x,0,z);s.T=(1-s.stone)*clamp(.62+temperatureNoise*.45-std::max(0.,s.raw-85)*.003,0,1)+s.stone*.67;s.H=(1-s.stone)*clamp(.52+humidity(x,0,z)*.45,.05,.95)+s.stone*.72;s.water=s.height<31?31:-32768;s.kind=s.height<31?(s.ridge<.1?1:3):0;waterSurface(s,wx,wz);s.preRiverHeight=s.height;if(drainage){riverRelief(s,wx,wz);junctionBed(s,wx,wz);}finalTerrainClimate(s,temperatureNoise,er);desertClimate(s,wx,wz);return s;
 }
public:
 J inspectMushroomIsland(int gx,int gz){auto a=mushroomIsland(gx,gz);return J{{"valid",a.valid},{"x",a.x-32768},{"z",a.z-32768},{"radius",a.r}};}
 J inspectRegionalDrainage(int gx,int gz){
  if(!streamingRegions)throw std::runtime_error("Regional recipe required");
  return *regionalWaterPlan(gx,gz);
 }
 J inspectSmallStoneForests(int lo,int hi){
  J rows=J::array();if(!rareStoneForests)return rows;auto const&q=C.at("stoneForest");double grid=num(q,"smallRegionGrid");
  if(hi<lo||int64_t(hi)-lo>100000)throw std::runtime_error("Stone-forest inventory range too wide");
  for(int z=int(std::floor(lo/grid))-1;z<=int(std::floor(hi/grid))+1;++z)for(int x=int(std::floor(lo/grid))-1;x<=int(std::floor(hi/grid))+1;++x){
   auto r=smallStoneRegion(x,z);if(!r.valid||r.x<lo||r.x>hi||r.z<lo||r.z>hi)continue;
   rows.push_back({{"x",r.x},{"z",r.z},{"floor",r.floor},{"radius",num(q,"smallRegionOuter")},{"cell",{x,z}}});
  }return rows;
 }
 bool streamed()const{return streamingRegions;}
 bool unbounded()const{return unboundedRegions;}
 Surface bootstrapSurface(int x,int z){return streamingRegions?naturalSamplingField().surface(x,z,false):surface(x,z,false);}
 // Read-only coarse habitat queries for searches. They do not prepare water
 // regions, change terrain, or substitute for the final surface/structure fit.
 double desertRegionWeight(int x,int z){return desertWeight(x,z);}
 double minimumNaturalTemperature(int x,int z){
  if(!streamingRegions)return base(x,z).T;
  auto& natural=naturalSamplingField();double result=natural.base(x,z).T;
  int gx=int(std::floor(x/64.)),gz=int(std::floor(z/64.));
  for(int dz=0;dz<2;++dz)for(int dx=0;dx<2;++dx)result=std::min(result,natural.base((gx+dx)*64,(gz+dz)*64).T);
  return result;
 }
 J regionalStatistics()const{return {{"enabled",streamingRegions},{"regionsPrepared",preparedRegions.size()},{"riverSegments",riverSegments.size()},{"iceRegions",iceAtlas.size()}};}
 J inspectLocalVolcanoes(int x,int z){J rows=J::array();if(!volcanoEnabled)return rows;int gx=int(std::floor(x/volcanoGrid)),gz=int(std::floor(z/volcanoGrid));
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto v=volcanoRegion(gx+dx,gz+dz);if(!v.valid)continue;auto a=v.cone;rows.push_back({{"x",a.x},{"z",a.z},{"outer",v.outer},{"crater",a.crater}});}return rows;
 }
 J inspectSmallPools(int lo,int hi){J rows=J::array();for(int gz=int(std::floor((lo+32768)/160.));gz<=int(std::floor((hi+32768)/160.));++gz)for(int gx=int(std::floor((lo+32768)/160.));gx<=int(std::floor((hi+32768)/160.));++gx){auto p=smallPool(gx,gz);if(!p.valid)continue;J shore=J::array();for(int i=0;i<p.shoreCount;++i)shore.push_back({p.shore[i][0]-32768,p.shore[i][1]-32768});rows.push_back({{"x",p.x-32768},{"z",p.z-32768},{"level",p.level},{"radius",p.radius},{"phase",p.phase},{"lava",p.lava},{"shore",shore}});}return rows;}
 J inspectIceRegions(){J out=J::array();if(!snowUpdateEnabled)return out;
  if(iceAtlasReady){for(auto const&r:iceAtlas)out.push_back({{"x",r.x},{"z",r.z},{"radiusX",r.rx},{"radiusZ",r.rz},{"cellX",r.gx},{"cellZ",r.gz}});return out;}
  for(int gz=terrain_bounds::MinBlock/4608-1;gz<=terrain_bounds::MaxBlock/4608+1;++gz)
for(int gx=terrain_bounds::MinBlock/4608-1;gx<=terrain_bounds::MaxBlock/4608+1;++gx){auto r=iceRegion(gx,gz);if(r.valid&&r.x-r.rx<terrain_bounds::MaxBlock&&r.x+r.rx>terrain_bounds::MinBlock&&r.z-r.rz<terrain_bounds::MaxBlock&&r.z+r.rz>terrain_bounds::MinBlock)out.push_back({{"x",r.x},{"z",r.z},{"radiusX",r.rx},{"radiusZ",r.rz},{"cellX",r.gx},{"cellZ",r.gz}});}
  return out;
 }
 J inspectFiniteCaves(int lo=terrain_bounds::MinBlock,int hi=terrain_bounds::MaxBlock){return finiteCaves.inventory(lo,hi,[&](int x,int z){return regionalHeight(x+32768,z+32768,vcont(x+32768,z+32768));});}
 J inspectCavernGroups(int lo=terrain_bounds::MinBlock,int hi=terrain_bounds::MaxBlock,bool geometry=false){return cavernGroups.inventory(lo,hi,[&](int x,int z){return cavernHeight(x,z);},geometry);}
 J bakeCavernPlan(){CavernGroups planner(terrainSeed,terrainRevision(data)>=176||C.at("cave").value("irregularMegaHalls",false),terrainRevision(data)>=186,terrainRevision(data)>=189,terrainRevision(data)>=193);std::map<std::pair<int,int>,int> samples;
  planner.inventory(terrain_bounds::MinBlock,terrain_bounds::MaxBlock,[&](int x,int z){int h=base(x+32768,z+32768).height;samples[{x,z}]=h;return double(h);});
  J rows=J::array();for(auto const&[p,h]:samples)rows.push_back({p.first,p.second,h});return {{"algorithm","cavern-height-controls-v1"},{"seed",terrainSeed},{"min",terrain_bounds::MinBlock},{"max",terrain_bounds::MaxBlock},{"samples",rows}};
 }
 J inspectScarpCandidates(){return scarpCandidates();}
 J inspectWetlands(int lo=terrain_bounds::MinBlock,int hi=terrain_bounds::MaxBlock){return wetlandInventory(lo,hi);}
 J inspectCascades(){J out=J::array();for(auto const&s:riverSegments)if(s.terraced)out.push_back({s.ax,s.az,s.bx,s.bz,s.aw,s.bw,s.ar,s.br});return out;}
 J inspectJunctionBeds(){return junctionReport;}
 J inspectVolcanoes(int min=terrain_bounds::MinBlock,int max=terrain_bounds::MaxBlock){return volcanoInventory(min,max);}
 explicit Field(J const&d,bool cullLocalPlans=false):data(d),C(d.at("config")),terrainSeed(C.at("seed").get<uint32_t>()),graph(d,true),caves(d,false),low(seedHash(terrainSeed,201)),high(seedHash(terrainSeed,211)),secondary(seedHash(terrainSeed,237)),warpX(seedHash(terrainSeed,238)),warpZ(seedHash(terrainSeed,239)),stoneNoise(seedHash(terrainSeed,308)),giantNoise(seedHash(terrainSeed,503)),matNoise(seedHash(terrainSeed,221)),rockNoise(seedHash(terrainSeed,222)),treeNoise(seedHash(terrainSeed,223)),biomeNoise(seedHash(terrainSeed,224)){
 localPlanningCulling=cullLocalPlans;
 if(!d.contains("seedManifest")||d.at("seedManifest").at("runtimeSeed")!=terrainSeed)
  throw std::runtime_error("Seed package mismatch; rebuild with tools/build_seed.py");
 if(streamingRegions){if(!regional::supportedAlgorithm(d.at("regionalGeneration").at("algorithm")))throw std::runtime_error("Unsupported regional terrain recipe");streamingIdentity=regional::identity(d);}
 loadCaves();
 offset=graph.ref("etn:overworld/continents/c2");erosion=graph.ref("etn:overworld/erosion/e");ridge=graph.ref("etn:overworld/ridge/1");temp=graph.ref("minecraft:overworld/temperature");humidity=graph.ref("minecraft:overworld/vegetation");
 loadRegionalTerrain();loadDeserts();loadLakes();loadVolcano();loadOcean();loadRivers();
 snowUpdateEnabled=C.contains("snowUpdate")&&C.at("snowUpdate").value("enabled",false);
 loadIceAtlas();

 for(auto const&a:d.at("pillars")){size_t i=pillars.size();pillars.push_back({a.at("x"),a.at("z"),a.at("radius"),a.at("phase"),a.at("outer"),a.at("height")});pillarBins[key2(int(std::floor(pillars.back().x/128)),int(std::floor(pillars.back().z/128)))].push_back(i);}
 buildJunctionBeds();loadScarps();loadFluvialLandforms();loadAridLandforms();
 }
 double vcont(double x,double z){return-64+(224+192*offset((x-32768)*4/3.,0,(z-32768)*4/3.)+64)*.75;}
 Surface base(int x,int z){ensureStreamingRegions(x,z);return baseCache.get(key2(x,z),[&]{return compute(x,z);});}
 double giantWeight(int x,int z,Surface const&s)const{if(s.lake==1||s.river>0)return 0;auto const&q=C.at("giantForest");double dx=x-32768-num(q,"centerX"),dz=z-32768-num(q,"centerZ"),d=std::hypot(dx,dz)+24*giantNoise.at(dx/96,dz/96);if(d>=num(q,"outerRadius")||s.stone>.01)return 0;return std::pow(1-s.barren,3)*(1-smooth(num(q,"innerRadius"),num(q,"outerRadius"),d))*smooth(35,47,s.height)*(1-smooth(240,270,s.height));}
 // Cache only coarse climate samples, never a pre-generated terrain map.
 std::array<double,2> regionalClimate(int x,int z){
  constexpr double pitch=64.;int gx=int(std::floor(x/pitch)),gz=int(std::floor(z/pitch));
  auto at=[&](int a,int b){return climateNodes.get(key2(a,b),[&]{auto s=base(a*64,b*64);return std::array<double,2>{s.T,s.H};});};
  auto a=at(gx,gz),b=at(gx+1,gz),c=at(gx,gz+1),d=at(gx+1,gz+1);
  double u=smooth(0,1,x/pitch-gx),v=smooth(0,1,z/pitch-gz);
  std::array<double,2> out{};for(int i=0;i<2;++i)out[i]=(a[i]*(1-u)+b[i]*u)*(1-v)+(c[i]*(1-u)+d[i]*u)*v;
  return out;
 }
 Surface computeSurface(int x,int z,bool slope=false){ensureStreamingIce(x,z);auto s=base(x,z);if(mushroomSurface(s,x,z))return s;wetlandRelief(s,x,z);scarpRelief(s,x,z);fluvialRelief(s,x,z);auto const&q=C.at("transition");double bw=num(q,"climateWidth"),ew=num(q,"elevationWidth"),T=s.T,H=s.H,e=s.continuous;auto climate=regionalClimate(x,z);if(s.stone==0){T=.1*T+.9*climate[0];H=.1*H+.9*climate[1];}auto above=[](double v,double t,double w){return smooth(t-w,t+w,v);};auto below=[&](double v,double t,double w){return 1-above(v,t,w);};std::array<double,17>W{};double water=below(e,0,ew),dry=1-water,open=water*above(T,.22,bw),shore=dry*below(e,.02,ew),land=dry*above(e,.02,ew);W[2]=water*below(T,.22,bw);W[0]=open*below(e,-.42,ew*6);W[1]=open*above(e,-.42,ew*6);W[4]=shore*below(T,.22,bw);W[3]=shore*above(T,.22,bw);W[5]=land*below(T,.20,bw);double cold=land*above(T,.20,bw)*below(T,.34,bw),temperate=land*above(T,.34,bw)*below(T,.64,bw),wet=temperate*above(H,.66,bw),warm=land*above(T,.64,bw);W[7]+=cold*below(H,.28,bw);W[6]+=cold*above(H,.28,bw);W[7]+=temperate*below(H,.28,bw);W[8]+=temperate*above(H,.28,bw)*below(H,.48,bw);W[9]+=temperate*above(H,.48,bw)*below(H,.66,bw)+wet*above(e,.06,ew*6);W[10]+=wet*below(e,.06,ew*6);W[11]+=warm*below(H,.30,bw);W[12]+=warm*above(H,.30,bw)*below(H,.58,bw);W[13]+=warm*above(H,.58,bw);
 // Only qualified large regions may be deserts; warm dry microclimates
 // elsewhere remain savanna, while beaches and river/lake sand retain their roles.
 if(largeDesertOnly){W[12]+=W[11];W[11]=0;}
 for(int b:{8,9,6,7}){double mv=W[b]*s.mount;W[b]-=mv;W[14]+=mv;}for(int b:{8,7,12}){double mv=W[b]*s.plateau*(completedBiomes?above(T,.48,bw)*below(H,.58,bw):1.);W[b]-=mv;W[15]+=mv;}double take=s.basin*below(H,.32,bw);for(int b=5;b<16;b++){double mv=W[b]*take;W[b]-=mv;W[16]+=mv;}double sum=0;for(double w:W)sum+=w;if(sum>0)for(auto&w:W)w/=sum;
 constexpr int species[17]={1,1,1,1,1,0,0,1,1,1,4,5,3,2,1,1,1};std::array<double,6>sw{};int best=0;double sand=0,snow=0,excluded=0;for(int b=0;b<17;b++){double v=W[b];if(v<=0)continue;if(v>W[best])best=b;s.treeD+=v*C.at("treeDensityByBiome")[b].get<double>();if(b==3||b==4||b==11)sand+=v;if(b==5)snow+=v;if(b==3||b==4||b==5)excluded+=v;sw[species[b]]+=v;}
 // A biome describes a region, not a 20-block lottery. Density still uses all
 // climate weights; the dominant region determines compatible tree species.
 auto dith=[&](Perlin const& p){return clamp(.5+.95*p.at(x/num(q,"ditherWavelength"),z/num(q,"ditherWavelength")),0,.999999);};
 s.biome=best;s.treeK=species[best];
 // Continuous soil composition, with restrained regional variation. Only the
 // narrow edges of each soil stage use grain; no full grass/sand column lottery.
 double soilDetail=.12*matNoise.at(x/160.,z/160.)+.035*matNoise.at(x/40.,z/40.);
 s.soilSand=clamp(sand+4*sand*(1-sand)*soilDetail,0,1);
 // Low sandy coast is a shore, not a desert/grass ecotone. W[3]+W[4]
 // loses half its weight exactly at sea level, so use actual coast elevation.
 double beach=(1-smooth(33,39,s.height))*(1-s.coast);
 if(!s.lake&&s.stone==0&&s.volcanic==0&&s.height>=29)
  s.soilSand=std::max(s.soilSand,beach);
 if(snow>.5)s.surf=2;
 else if(s.soilSand>0)s.surf=7;
 if(s.treeK==5)s.treeD*=smooth(.55,.85,s.soilSand);
 else s.treeD*=1-smooth(.45,.85,s.soilSand);
  if(s.stone>0){s.surf=0;if(snow>0||sand>0){double v=dith(matNoise);s.surf=v<snow?2:v<snow+sand?1:0;}if(s.treeD>0){double v=dith(treeNoise),cum=0;for(int k=0;k<6;k++){cum+=sw[k];if(v<cum){s.treeK=k;break;}}}s.biome=best;if(W[best]<.999999){double threshold=clamp(.5+1.2*biomeNoise.at(x/num(q,"biomeWavelength"),z/num(q,"biomeWavelength")),0,.999999),cum=0;for(int b=0;b<17;b++){cum+=W[b];if(threshold<cum){s.biome=b;break;}}}}
 s.rockCap=1-std::min(1.,excluded);s.treeD*=1-s.stone;s.giant=giantWeight(x,z,s);if(s.giant>0){s.treeD=s.treeD*(1-s.giant)+.003*s.giant;double v=dith(matNoise);if(v<s.giant){s.biome=17;s.treeK=0;s.surf=v<s.giant*.7?3:4;}}
 // A shallow forest bank keeps its soil; the submerged bed must not make
 // the adjacent one-block dry shoulder look like a bare mountain cliff.
 if(s.riverBankCut>0&&s.riverBankCut<=3&&s.giant>.5)s.rockCap=0;
 // A shallow river recess retains the existing soil cap. A slope measured
 // across its submerged bed describes erosion, not a change of bedrock:
 // otherwise the outermost grass/dirt column turns into a stone lip.
 // Only suppress slope-selected rock; explicit rocky coasts, stone forests
 // and scarps still keep their geology, and deeper material remains stone.
 // Keep sand-rich banks rocky: replacing their roof with falling sand would
 // change the bank after block updates instead of merely restoring its skin.
 if(soilCutBanks&&s.riverCutLow>-32768&&s.soilSand<=.55&&s.surf!=1)s.rockCap=0;
 // A lowland river cutting soil does not turn that soil into a stone rim.
 // Explicit rock coasts, scarps and stone forests still override rockCap.
 if(alluvialBanks&&s.mainRiver>.45&&s.riverBankCut>0&&s.riverBankWater<100)s.rockCap=0;
 // Small streams share the surrounding biome's soil palette too. Reuse the
 // computed soil/snow/wetland fields; no neighbour-block or noise lookup.
 // The river-created slope alone must not draw a continuous stone outline.
 if(matchedRiverBanks&&s.riverBankWater>31&&std::isfinite(s.riverBankEdge))
  s.rockCap*=1-(1-smooth(110.,165.,s.height))*(1-smooth(2.,6.,s.riverBankEdge));
 if(s.lake){s.surf=5;if(s.lake==1){s.treeD=0;s.giant=0;s.biome=s.lakeFrozen?19:18;}}
 if(s.volcanic>0){s.treeD*=1-s.volcanic;if(s.volcanic>.35){s.treeD=0;s.giant=0;s.biome=20;s.kind=4;}}
 // Open-water strength controls ocean ecology, not whether a submerged inlet
 // has a seabed. Otherwise narrow sea-level channels retain underwater grass.
 if((s.ocean>0||(coastalBeds&&s.stone<=.01&&s.volcanic==0))&&s.water==31&&s.height<31&&!s.lake){s.treeD=0;s.giant=0;s.kind=3;s.surf=6;s.biome=oceanBiome(s,x,z);}
 if((s.river>0||s.riverDeposit)&&!s.lake){s.treeD=0;s.giant=0;s.biome=18;s.kind=2;s.surf=8;}
 riverSkin(s,x,z);
 if(s.barren>0){s.treeD*=std::pow(1-s.barren,3);if(s.height>=31&&dith(matNoise)<s.barren)s.biome=20;}
 // Independent broadleaf subregion; does not modify height or drainage.
 if(s.biome==9&&s.T>=.42&&s.giant<.01&&s.stone<.01&&!s.kind&&!s.lake&&!s.river&&s.barren<.01&&s.coast<.1&&s.cliffLow==-32768&&biomeNoise.at(x/650.,z/650.)>.08&&birchSuitability(x,z)>.55){s.biome=23;s.treeK=2;}
 if(s.wetland>0){s.rockCap*=1-s.wetland;s.soilSand*=1-s.wetland;if(s.wetland>.40){s.biome=10;s.treeK=4;s.treeD=.006*s.wetland;}if(s.water>=s.height)s.treeD=0;}
 if(s.scarp>.05)s.treeD=0;
 desertSkin(s,x,z);
 fluvialSkin(s);
 aridSurface(s,x,z);
 glacierSkin(s,x,z);
 shallowSeaBed(s,x,z);
 shallowSeaShore(s,x,z);
 smallSurfacePool(s,x,z);
 badlandsSurface(s,x,z);
 if(slope)surfaceRock(s,x,z);
 return s;
 }
 #include "SmallPools.inc"
 void surfaceRock(Surface&s,int x,int z){
  if(s.mushroomZone||s.badlands){s.rock=0;return;}
  auto const&q=C.at("transition");
  auto at=[&](int X,int Z){return (s.wetland>0||s.scarpWarped)?surface(X,Z,false):base(X,Z);};
  auto east=at(x+1,z),west=at(x-1,z),south=at(x,z+1),north=at(x,z-1);
  double slope=std::hypot(east.height-west.height,south.height-north.height)/2.;
  // A channel's cut is not evidence of a rocky parent hillside. Retain the
  // pre-drainage heights in the same cache entries already used for slope.
  // Include the outside edge's one-column halo, where rounding a tiny cut
  // to a block used to turn intact grass into isolated stone patches.
  bool riverEdge=s.riverBankWater>31||east.riverBankWater>31||west.riverBankWater>31
      ||south.riverBankWater>31||north.riverBankWater>31;
  if(naturalBankSlope&&riverEdge&&!s.scarpWarped){
   double natural=std::hypot(east.preRiverHeight-west.preRiverHeight,south.preRiverHeight-north.preRiverHeight)/2.;
   slope=std::min(slope,natural);
  }
  double p=smooth(num(q,"rockSlopeLo"),num(q,"rockSlopeHi"),slope)*float(s.rockCap);
  s.rock=s.scarp>.05||s.coast>.45||s.stone>=.5||s.cliffLow>-32768||p>=1
      ||(p>0&&clamp(.5+.95*rockNoise.at(x/num(q,"ditherWavelength"),z/num(q,"ditherWavelength")),0,.999999)<p);
 }
 Surface surface(int x,int z,bool slope=true){if(evaluatingSmallPool){auto s=computeSurface(x,z,false);if(slope)surfaceRock(s,x,z);return s;}auto s=skinCache.get(key2(x,z),[&]{return computeSurface(x,z,false);});if(slope)surfaceRock(s,x,z);return s;}
 std::vector<std::array<int,3>> caveFalls(int cx,int cz){return cavernGroups.waterfallCandidates(cx,cz,[&](int X,int Z){return cavernHeight(X,Z);});}
 void materialColumn(Surface const&s,int x,int z,int top,uint16_t*col){
 if(top < -64 || top > 319 || s.height < -64 || s.height > 319)throw std::out_of_range("Material column outside world height");
  int next=-60;
  if(groupedCaverns&&boundedAquifers&&waterEnabled&&s.volcanic<=0){
   // Deep uncut rock is uniform before the geology pass. Fill it once, then
   // visit only actual cave bits. Surface, cliff and river cuts retain the
   // authoritative material path instead of being approximated here.
   int end=std::min(top,s.height-5);
   if(s.badlands)end=std::min(end,24);
   if(s.seaCutLow>-32768)end=std::min(end,s.seaCutLow-1);
   if(s.riverCutLow>-32768)end=std::min(end,int(s.riverCutLow)-1);
   if(s.cliffLow>-32768)end=std::min(end,int(s.cliffLow)-1);
   if(end>=-60){
    std::fill(col+4,col+end+65,uint16_t(2));next=end+1;
    int cap=std::min({end,int(s.scarpCaveCeiling),int(std::ceil(s.height-caveRoof-4))-1,319});
    if(cap>=-56){auto const&bits=cavernGroups.column(x-32768,z-32768,[&](int X,int Z){return cavernHeight(X,Z);});
     bool lavaFloor=caveDesign&&(cavernGroups.thermalColumn()||(((bits[0]>>12)&1)&&((bits[0]>>16)&1)));
     for(int w=0;w<=(cap+64)/64;++w){uint64_t mask=bits[w];
      while(mask){int y=w*64+int(std::countr_zero(mask))-64;mask&=mask-1;
       if(y>=-56&&y<=cap)col[y+64]=uint16_t(caveWaterMaterial(s,y,lavaFloor));
      }
     }
    }
   }
  }
  for(int y=next;y<=top;++y)col[y+64]=material(s,x,y,z);
 }
 uint8_t material(Surface const&s,int x,int y,int z){if(y<-64||y>319)return 0;if(y<-60)return 1;if(connectedCaves&&y<=-57)return 2;if(int v=volcanicMaterial(s,x,y,z);v>=0)return uint8_t(v);if(s.seaCutLow>-32768&&y>=s.seaCutLow&&y<=s.seaCutHigh)return y<=31?8:0;if(s.riverCutLow>-32768&&y>=s.riverCutLow&&y<=s.riverCutHigh)return y<=s.riverCutWater?8:0;if(s.cliffLow>-32768&&y>=s.cliffLow&&y<=s.cliffHigh)return 0;if(y>s.height){if(s.lava>=y)return 43;int dy=y-s.height;if(s.drip&&dy<=s.drip)return dy==s.drip?31:dy==s.drip-1?30:dy==1?28:29;return s.water>-32768&&y<=s.water?((s.river>0?false:s.lake==1?s.lakeFrozen:float(s.T)<.22)&&y==s.water?9:8):0;}if(y<=s.scarpCaveCeiling&&y<=s.height-caveRoof&&isCave(x,y,z,s.height)){if(boundedAquifers&&waterEnabled)return caveWaterMaterial(s,y,caveLavaFloor(x,z));if(s.aquifer<0)return y<waterline?8:0;if(s.aquifer==1&&y<=waterline+4)return 2;return s.aquifer==2&&y<waterline?8:0;}if(int b=badlandsMaterial(s,x,y,z);b>=0)return uint8_t(b);if(s.mushroom&&y>=s.height-4)return y==s.height?168:3;if(y<s.height-4)return 2;if(s.lava>=s.height&&s.lava>-32768)return 2;if(s.drip&&y==s.height)return 27;if(s.aridType&&y>=s.height-4)return y==s.height?s.aridTop:(s.aridType==3?48:(s.aridTop==2?2:(s.aridTop==3||s.aridTop==33)?3:(s.height-y<3?5:6)));if(glacierWetlands&&s.fluvialType&&s.iceGround>=0&&y>=s.height-3)return y==s.height?uint8_t(s.iceGround):2;if(s.fluvialType&&y>=s.height-3)return y==s.height?s.fluvialTop:(s.fluvialTop==5?5:3);if(s.surf==5){if(y<s.height-2)return 2;double pick=hash01(x,z,terrainSeed^9191^((y+64)*7381));return pick<.5?42:(s.lake==1?5:20);}if(s.surf==6)return oceanMaterial(s,x,y,z);if(s.surf==8)return riverMaterial(s,x,y,z);if(s.barren>0&&hash01(x,z,volcanicSeed^1409)<s.barren)return hash01(x,z,volcanicSeed^1407^((y+64)*7919))<.65?2:42;if(progressiveRiverBeds&&s.riverBedMaterial==5&&s.riverBankCut>0&&y>=s.height-2)return 5;if(!progressiveRiverBeds&&s.mainRiver>.4&&s.riverBar>.05&&s.riverBankCut>0&&y>=s.height-2&&(!alluvialBanks||(s.riverBankEdge<3&&s.height<=s.riverBankWater)))return 5;if(int desert=desertMaterial(s,x,y,z);desert>=0)return uint8_t(desert);if(s.iceGround>=0)return y==s.height?uint8_t(s.iceGround):(s.iceHabitat>=.10?2:3);if(s.rock)return 2;if(s.wetland>0&&s.surf!=5&&s.surf!=6&&s.surf!=8){double coverage=s.wetland*(s.water>=s.height?1.:.35+.65*s.wetlandMoisture);double patch=.48+.20*matNoise.at(x/18.,z/18.);if(coverage>patch)return y==s.height?85:3;}if(s.surf==7){
  int depth=s.height-y;
  double grain=(hash01(x,z,terrainSeed^1229)-.5)*.05;
  double mix=s.soilSand+grain;
  if(depth==0)return mix>.68?5:mix>.24?33:4;
  // Sand wedges thicken towards the desert interior instead of replacing the
  // entire soil column at the same boundary; sandstone only under deep sand.
  double sandDepth=3*smooth(.58,1,s.soilSand);
  if(depth<=sandDepth)return 5;
  if(s.soilSand>.94&&depth>=3)return 6;
  return 3;
 }if(s.surf==1)return y<s.height-2?6:5;if(s.surf==2)return y==s.height?7:3;if(s.surf==3||s.surf==4)return y==s.height?(s.surf==3?32:33):3;return y==s.height?4:3;}
};
}
