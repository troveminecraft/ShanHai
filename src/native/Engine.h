#pragma once
#include "Field.h"
#include "OreCluster.h"
#include "../WorldBounds.h"
#include "../rules/HighMountainUnderground.h"
#include <chrono>
#include <unordered_set>
namespace nativegen {
struct TreeRun{int16_t x,z,lo,n;uint8_t color;};
struct CrownTier {double base,top,radius,dx,dz,phase;};
struct Limb {double angle,base,reach,rise;bool bare;};
struct Giant {
 int id=0,x=0,z=0,base=0,h=0,extent=0,footLo=0,footHi=0,shape=0,voxels=0;
 double radius=0,angle=0,lean=0,crownRadius=0,crownBase=0,aspect=1;
 std::vector<CrownTier> tiers;std::vector<Limb> limbs;std::vector<TreeRun> runs;
};
struct Structure {int x=0,z=0,type=-1;double e=0;};
struct SmallTree {int x,z,h,type;double radius,ground;};
class Engine {
 friend struct LandscapeRedoTest;
 friend struct ElevationVariantsTest;
 friend struct IceSpireRegression;
 friend struct GlacierEcologyRegression;
 friend struct EngineBoundaryTest; // Offline regression exercises the production passes.
 std::vector<std::array<int,4>> nativeReservations;
 bool forestDesign=false,sealedFluidsEnabled=false,gradualIceEcotone=false;
 double iceVegetation(Surface const&s)const{return gradualIceEcotone?1-smooth(.06,.60,s.iceHabitat):1.;}
 bool forestTreeKept(int x,int z)const{return !forestDesign||hash01(x,z,terrainSeed^51971)<.75;}
 // One lottery at the feature anchor; never clip individual tree/shrub voxels.
 bool glacierPlantKept(Surface const&s,int x,int z)const{return s.glacierVegetation>=1||hash01(x,z,terrainSeed^73129)<s.glacierVegetation;}
 bool nativeReserved(int x,int z)const{for(auto const& r:nativeReservations)if(x>=r[0]&&x<=r[2]&&z>=r[1]&&z<=r[3])return true;return false;}
 J const&C;uint32_t terrainSeed;std::unique_ptr<Field> ownedField;Field& field;Perlin forestNoise,understoryNoise;
 bool legacyStructures=true;
 bool treeSubstrates=false,glacierEcologyEnabled=false,rootedTreeSoil=false;
 Cache<uint64_t,bool> rootedSoilCache{8192};
 KARMA_NOINLINE bool finalPlantingSoil(Surface const&s,int x,int z){
  return rootedSoilCache.get(key2(x,z),[&]{
   auto actual=s;
   if(s.scarp>.05||s.coast>.45||s.stone>=.5||s.cliffLow>-32768)actual.rock=1;
   else if(s.rockCap==0)actual.rock=0;
   else field.surfaceRock(actual,x,z);
   auto soil=[](int b){return b==3||b==4||b==32||b==33||b==80||b==85;};
   int block=field.material(actual,x,actual.height,z);
   return soil(block)||(block==7&&soil(field.material(actual,x,actual.height-1,z)));
  });
 }
 // Climate/biome eligibility is not planting ground: river deposition can
 // replace forest soil with bare sand while the climate remains unchanged.
 // Check final terrain material at cached tree anchors/footprints, before
 // spacing reservations or voxel placement. Keep existing rocky-slope policy.
 bool treeGroundAllowed(Surface const&s,int x,int z,bool reservations=true){
  if(reservations&&nativeReserved(x,z))return false;
  if(s.fluvialType)return false;
  if(s.iceHabitat>0){
   // Footprint checks test soil, not 25 independent climate lotteries.
   // Ecology applies one density fade at each feature anchor instead.
   if(gradualIceEcotone){if(s.iceHabitat>=.60)return false;}
   else if(s.iceHabitat>=.10||hash01(x,z,terrainSeed^51601)<smooth(.005,.10,s.iceHabitat))return false;
   if(s.iceGround==2||s.iceGround==42||s.iceGround==143)return false;
  }
  if(!treeSubstrates)return true;
  if(s.river||s.riverDeposit||s.kind==2||s.kind==3||s.water>=s.height||s.lava>=s.height)return false;
  if(rootedTreeSoil)return finalPlantingSoil(s,x,z);
   // Candidate queries often omit slope for speed; rock is then still zero.
   // Resolve the same final rock skin used by chunk generation before testing
   // the substrate. Reuse this result across nearby tree footprints/candidates.
  int block=field.material(s,x,s.height,z);
  return block!=5&&block!=6&&block!=42&&block!=8&&block!=9&&block!=43;
 }
 struct Site{int id,x,z;double radius;};std::unordered_map<uint64_t,std::vector<Site>> sites;
 Cache<int,std::shared_ptr<Giant>> giantCache{128};Cache<uint64_t,Structure> structureCache{512};
 static constexpr int Halo=28,Side=2*Halo+1;
 const int Height=field.generationHeight();
 std::vector<uint8_t> scratch=std::vector<uint8_t>(Side*Side*Height);
 std::array<int,Side*Side> ground{};
 double rand(int id,int k)const{return hash01(id,0,terrainSeed^(503+k));}
 Structure structure(int gx,int gz){if(!legacyStructures)return {};return structureCache.get(key2(gx,gz),[&]{Structure out;auto const&q=C.at("structure");if(hash01(gx,gz,terrainSeed^101)>=num(q,"chance"))return out;int x=roundJS((gx+.5+(hash01(gx,gz,terrainSeed^103)-.5)*.6)*num(q,"grid")),z=roundJS((gz+.5+(hash01(gx,gz,terrainSeed^107)-.5)*.6)*num(q,"grid"));if(x<=80||z<=80||x>=65456||z>=65456)return out;auto s=field.surface(x,z,false);if(hash01(gx,gz,terrainSeed^1511)<s.barren||s.desert>.15)return out;int type=-1;if(s.biome==11)type=1;else if(s.biome==13)type=2;else if(s.biome==5)type=3;else if(s.biome==10)type=5;else if(s.kind==3&&s.e>-.42&&s.e<-.02)type=4;else if((s.biome==8||s.biome==12||s.biome==7||s.biome==6)&&!s.kind&&s.e>.03&&s.e<.38)type=0;int span=type==0?40:12,lo=s.height,hi=s.height;for(auto [dx,dz]:{std::pair{-span,0},{span,0},{0,-span},{0,span}}){int h=field.base(x+dx,z+dz).height;lo=std::min(lo,h);hi=std::max(hi,h);}if(type>=0&&(type==4||hi-lo<=span*num(C.at("slope"),"structure")))out={x,z,type,s.e};return out;});}
 std::vector<Structure> structures(int x,int z,int size,int halo=96){std::vector<Structure>out;double grid=num(C.at("structure"),"grid");for(int gz=int(std::floor((z-halo)/grid));gz<=int(std::floor((z+size+halo)/grid));gz++)for(int gx=int(std::floor((x-halo)/grid));gx<=int(std::floor((x+size+halo)/grid));gx++){auto a=structure(gx,gz);if(a.type>=0)out.push_back(a);}return out;}
 bool blocked(int x,int z,bool reservations=true){if(reservations&&nativeReserved(x,z))return true;for(auto const&a:structures(x,z,0))if(std::abs(a.x-x)<80&&std::abs(a.z-z)<80)return true;return false;}
 #include "GiantForest.inc"
 #include "Understory.inc"
 #include "OceanFeatures.inc"
 #include "ForestEdges.inc"
 #include "ConiferEcology.inc"
 #include "PlainsEcology.inc"
 #include "MountainEcology.inc"
 #include "RiparianEcology.inc"
 #include "BroadleafForest.inc"
 #include "BirchForest.inc"
 #include "BirchCompanions.inc"
 #include "JungleEcology.inc"
 #include "WetDryEcology.inc"
 #include "ForestMushrooms.inc"
 #include "DesertEcology.inc"
 #include "UndergroundEcology.inc"
 #include "SurfaceFlora.inc"
 #include "UndergroundPockets.inc"
 #include "SealedFluidCaves.inc"
 #include "StoneGrotto.inc"
 #include "SnowRifts.inc"
 #include "IceFractures.inc"
 #include "ClosedIceRifts.inc"
 #include "IceSpires.inc"
 #include "VariantSnowCover.inc"
 #include "SmallIceSpikes.inc"
 #include "IceFeatures.inc"
 #include "LavaFlows.inc"
 #include "LandscapeDetails.inc"
 #include "FinalTerrainOres.inc"
 #include "VanillaOres.inc"
 std::vector<SmallTree> smallTrees(int x0,int z0,int size,std::vector<Structure>const&ss){std::vector<SmallTree>out;for(int z=int(std::floor((z0-16)/4.))*4;z<z0+size+16;z+=4)for(int x=int(std::floor((x0-16)/4.))*4;x<x0+size+16;x+=4){if(C.at("stoneForest").value("largeEnabled",true)&&std::hypot(x-32768-num(C.at("stoneForest"),"centerX"),z-32768-num(C.at("stoneForest"),"centerZ"))<12)continue;auto s=field.surface(x,z,false);if(s.treeD==0||s.kind||s.e<=.02||coniferHabitat(s)||broadleafHabitat(s)||birchHabitat(s)||jungleHabitat(s)||wetDryHabitat(s)||(s.treeK!=5&&!treeGroundAllowed(s,x,z)))continue;if(hash01(x,z,terrainSeed)>=float(s.treeD)*16*field.ecologyTreeWeight(s)*std::max(0.,.55+.9*forestNoise.fbm(x*9/65536.,z*9/65536.,2)))continue;bool skip=false;for(auto const&a:ss)if(std::abs(x-a.x)<45&&std::abs(z-a.z)<45){skip=true;break;}if(skip)continue;double slope=std::hypot(field.base(x+1,z).height-field.base(x-1,z).height,field.base(x,z+1).height-field.base(x,z-1).height)/2.;if(slope>num(C.at("slope"),"tree"))continue;for(auto const&a:giants(x,z,1))if(std::hypot(x-a->x,z-a->z)<=a->radius+5){skip=true;break;}if(skip)continue;double scale=float(.75+.55*hash01(x,z,terrainSeed^37));int t=s.treeK,h=std::max(2,roundJS(C.at("treeModels").at("trunkHeights")[t].get<double>()*scale));out.push_back({x,z,h,t,C.at("treeModels").at("canopyRadii")[t].get<double>()*scale,double(s.height)});}return out;}
public:
 explicit Engine(J const&data,std::vector<std::array<int,4>> reservations={},bool vegetationOnly=false,Field* sharedField=nullptr,Engine const* vegetationSource=nullptr):nativeReservations(std::move(reservations)),C(data.at("config")),terrainSeed(C.at("seed").get<uint32_t>()),ownedField(sharedField?nullptr:std::make_unique<Field>(data)),field(sharedField?*sharedField:*ownedField),forestNoise(seedHash(terrainSeed,41)),understoryNoise(seedHash(terrainSeed,809)){
  int revision=terrainRevision(data);gradualIceEcotone=revision>=193;forestDesign=revision>=186;sealedFluidsEnabled=revision>=190;
  if(!vegetationOnly)loadLavaFlows(data);
  legacyStructures=revision<183;
  rootedTreeSoil=revision>=179;vanillaOres=revision>=178;finalTerrainOres=revision>=177;
  landscapeDetails=revision>=176;
  if(!vegetationOnly&&landscapeDetails&&!field.streamed())for(auto const&v:field.inspectVolcanoes())detailVolcanoes.push_back({v.at("x"),v.at("z"),v.at("crater"),v.at("outer")});
  oreBoundaryAir=revision>=175;bedrockIceRifts=revision>=174;closedIceRifts=revision>=173;
  thinSnowRifts=revision>=172;snowRifts=revision>=171;glacierEcologyEnabled=revision>=167;
  continuousGlacierBodies=revision>=166;smallIceEnabled=revision>=165;slantedGlaciers=revision>=164;
  refinedGlaciers=revision>=163;fracturedGlaciers=revision>=162;
 treeSubstrates=data.contains("rivers")&&data.at("rivers").value("treeSubstrates",false);
 undergroundEcologyEnabled=C.at("cave").value("undergroundEcology",0)==1;
 caveBiomesEnabled=C.at("cave").value("caveBiomes",0)==1;
 caveHabitatRefinement=C.at("cave").value("habitatRefinement",0)==1;
 if(!vegetationOnly)loadStoneGrotto(data);
 oreProfilesEnabled=C.at("cave").value("oreProfiles",0)==1;
 // Optional platform adapter: reuse already planned giant anchors without
 // performing global layout work in every lightweight footprint engine.
 if(vegetationOnly){if(vegetationSource){sites=vegetationSource->sites;
  // Shared Fields are already restricted to serial use. Do not share mutable
  // caches across independent workers or mismatched recipes.
  if(&field==&vegetationSource->field&&C==vegetationSource->C)jungleShapes=vegetationSource->jungleShapes;
 }return;}
 auto const&q=C.at("giantForest");double reach=num(q,"outerRadius")+24,cx=32768+num(q,"centerX"),cz=32768+num(q,"centerZ"),grid=num(q,"grid");for(int i=0;i<num(q,"layoutAttempts");i++){double angle=rand(i,1)*6.283185307179586,d=std::sqrt(rand(i,2))*reach,r=num(q,"radiusMin")+rand(i,5)*(num(q,"radiusMax")-num(q,"radiusMin"));int x=int(std::floor(cx+std::cos(angle)*d)),z=int(std::floor(cz+std::sin(angle)*d)),gx=int(std::floor(x/grid)),gz=int(std::floor(z/grid));bool clear=true;for(int dz=-1;dz<=1&&clear;dz++)for(int dx=-1;dx<=1&&clear;dx++){auto it=sites.find(key2(gx+dx,gz+dz));if(it!=sites.end())for(auto const&b:it->second)if(std::hypot(x-b.x,z-b.z)<r+b.radius+num(q,"minGap")){clear=false;break;}}if(clear)sites[key2(gx,gz)].push_back({i,x,z,r});}
 }
 void setSealedFluidDiagnostics(bool enabled){sealedFluidsEnabled=enabled;}
 J inspectFeatureFlags()const{return J{{"forestDesign",forestDesign},{"legacyStructures",legacyStructures},{"vanillaOres",vanillaOres},{"finalTerrainOres",finalTerrainOres},{"glacierEcology",glacierEcologyEnabled},{"sealedFluids",sealedFluidsEnabled}};}
 J inspectSealedFluids(int min,int max){J rows=J::array();if(!sealedFluidsEnabled)return rows;
  for(int gz=int(std::floor((min+32768)/128.));gz<=int(std::floor((max+32768)/128.));++gz)for(int gx=int(std::floor((min+32768)/128.));gx<=int(std::floor((max+32768)/128.));++gx)for(bool lava:{false,true}){
   auto p=sealedFluidPlan(gx,gz,lava);if(p->columns.empty())continue;J columns=J::array();
   for(auto const&c:p->columns)columns.push_back({c.x-32768,c.z-32768,c.lo,c.hi});
   rows.push_back({{"centre",{p->x-32768,p->y,p->z-32768}},{"ground",p->ground},{"fluid",p->fluid},{"columns",columns}});
  }return rows;
 }
 std::array<double,4> forestMs{};
 double surfaceMs=0,columnMs=0,featureMs=0,undergroundMs=0;uint64_t columnNoise=0;
 J inspectForest(bool includeRuns=false,int limit=0,bool footprintsOnly=false) {
  J rows=J::array();std::vector<Site> ordered;for(auto const&[key,list]:sites)ordered.insert(ordered.end(),list.begin(),list.end());
  std::sort(ordered.begin(),ordered.end(),[](auto const&a,auto const&b){return a.id<b.id;});
  for(auto const&site:ordered)if(auto a=giant(site)){
   if(!footprintsOnly)model(*a);int bare=0;for(auto const&b:a->limbs)bare+=b.bare;
   J row={{"id",a->id},{"x",a->x-32768},{"z",a->z-32768},{"base",a->base},{"height",a->h},{"groundRange",a->footHi-a->footLo},{"radius",a->radius},{"crownRadius",a->crownRadius},{"crownBase",a->crownBase},{"shape",a->shape},{"tiers",a->tiers.size()},{"bareLimbs",bare},{"voxels",a->voxels},{"runCount",a->runs.size()}};
   if(includeRuns){row["runs"]=J::array();for(auto const&r:a->runs)row["runs"].push_back({r.x,r.z,r.lo,r.n,r.color});}
   rows.push_back(std::move(row));
   if(limit>0&&int(rows.size())>=limit)break;
  }
  return rows;
 }
 J inspectBroadleaves(int x,int z,int size){J a=J::array();for(auto const&t:broadleaves(x+32768,z+32768,size)){J spine=J::array();for(auto p:oakSpine(t))spine.push_back({p.x-32768,p.y,p.z-32768});a.push_back({{"kept",forestTreeKept(t.x,t.z)},{"x",t.x-32768},{"z",t.z-32768},{"height",t.h},{"ground",t.ground},{"species",t.species},{"spine",spine}});}return a;}
 J inspectConifers(int x,int z,int size){
  J rows=J::array();for(auto const&a:conifers(x+32768,z+32768,size))rows.push_back({{"kept",forestTreeKept(a.x,a.z)},{"x",a.x-32768},{"z",a.z-32768},{"height",a.h},{"ground",a.ground},{"radius",a.radius},{"shape",a.shape}});return rows;
 }
 J inspectBirches(int x,int z,int size){J rows=J::array();for(auto const&t:birches(x+32768,z+32768,size)){J cells=J::array();for(auto const&p:t->cells)cells.push_back({p.x-32768,p.y,p.z-32768,p.block});rows.push_back({{"x",t->x-32768},{"z",t->z-32768},{"height",t->height},{"ground",t->ground},{"cells",cells}});}return rows;}
 J inspectJungles(int x,int z,int size){J rows=J::array();for(auto const&t:jungles(x+32768,z+32768,size)){J cells=J::array();for(auto p:t->cells)cells.push_back({p.x-32768,p.y,p.z-32768,p.block});auto const&a=t->site;J stem=J::array();for(auto v:t->stem)stem.push_back({v[0]-32768,v[1],v[2]-32768,v[3]});rows.push_back({{"x",a.x-32768},{"z",a.z-32768},{"height",a.h},{"ground",a.ground},{"layer",a.layer},{"width",a.width},{"style",a.style},{"primaryBranches",t->primaryBranches},{"secondaryBranches",t->secondaryBranches},{"brokenBranches",t->brokenBranches},{"attachedVines",t->attachedVines},{"epiphytes",t->epiphytes},{"stem",stem},{"cells",cells}});}return rows;}
 J inspectSnowRift(int x,int z){auto r=riftAt(x,z);auto s=field.surface(x+32768,z+32768,false);return J{{"x",x},{"z",z},{"cut",r.cut},{"lip",r.lip},{"glacier",s.glacier},{"super",s.superGlacier},{"ground",s.height},{"interior",closedIceRifts?closedRiftInterior(x,z):0.},{"width",r.width},{"distance",std::isfinite(r.distance)?r.distance:-1.},{"end",r.end},{"protected",bool(s.kind||s.river||s.lake||s.fluvialType||s.water>=s.height)}};}
 J landscapeSites(int lo,int hi){return inspectLandscapeSites(lo,hi);}
 Field& terrain(){return field;}
 J inspectWetDry(int x,int z,int size){J rows=J::array();for(int gz=(z+32768)/16;gz<=(z+32768+size)/16;gz++)for(int gx=(x+32768)/16;gx<=(x+32768+size)/16;gx++){
  auto a=wetDrySite(gx,gz);auto s=field.surface(a.x,a.z);rows.push_back({{"x",a.x-32768},{"z",a.z-32768},{"height",a.h},{"ground",s.height},{"biome",s.biome},{"rock",s.rock},{"blocked",blocked(a.x,a.z)},{"material",field.material(s,a.x,s.height,a.z)},{"accepted",wetDryAccepted(gx,gz,a)}});
 }return rows;}
 J inspectStandEdge(int x,int z){auto s=field.surface(x+32768,z+32768,false);int f=forestFamily(s);return J{{"family",f},{"interior",f?forestInterior(x+32768,z+32768,f):0.}};}
 J inspectBirchOaks(int x,int z,int size){J rows=J::array();for(auto const&t:birchOaks(x+32768,z+32768,size)){J cells=J::array();for(auto const&p:t->cells)cells.push_back({p.x-32768,p.y,p.z-32768,p.block});auto const&a=t->site;rows.push_back({{"x",a.x-32768},{"z",a.z-32768},{"height",a.height},{"ground",a.ground},{"cells",cells}});}return rows;}
 // Native structure reservations need only the existing forest model and its
 // surface substrate. Do not repeat caves, ores, fluids or terrain serialization.
 template<class Put,class Structures,class Trees> void naturalFeatures(int x0,int z0,Put&& put,Structures const& ss,Trees const& ts){
 // Natural small features use their own world anchors and bounded point count.
 int grid=int(num(C.at("natural"),"grid"));for(int gz=int(std::floor((z0-8)/double(grid)));gz<=int(std::floor((z0+24)/double(grid)));gz++)for(int gx=int(std::floor((x0-8)/double(grid)));gx<=int(std::floor((x0+24)/double(grid)));gx++){
 auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};int x=int(std::floor((gx+.2+r(502)*.6)*grid)),z=int(std::floor((gz+.2+r(503)*.6)*grid));if(C.at("stoneForest").value("largeEnabled",true)&&std::hypot(x-32768-num(C.at("stoneForest"),"centerX"),z-32768-num(C.at("stoneForest"),"centerZ"))<12)continue;auto s=field.surface(x,z,false);int h=s.height;if(s.mushroomZone||s.badlands||s.iceHabitat>=.10||s.desert>.15||coniferHabitat(s)||broadleafHabitat(s)||birchHabitat(s)||jungleHabitat(s)||wetDryHabitat(s)||r(501)>num(C.at("natural"),"chance")+s.giant*.25||s.stone>.5||s.kind||h<=33||h>Height-72)continue;bool skip=false;for(auto const&a:giants(x,z,1))if(std::hypot(x-a->x,z-a->z)<a->radius+5){skip=true;break;}for(auto const&a:ss)if(std::abs(x-a.x)<48&&std::abs(z-a.z)<48)skip=true;for(auto const&t:ts)if(std::hypot(x-t.x,z-t.z)<7)skip=true;if(skip||nativeReserved(x,z))continue;int kind=r(504)<.4?0:r(504)<.75?1:2;if(kind&&r(1513)>std::pow(1-s.barren,3))continue;
 if(kind&&(s.biome==3||s.biome==4||s.biome==5||s.biome==11||s.biome==14||s.biome==15||s.biome==16))continue;
 if(!kind){double rx=1.3+r(505)*1.6,rz=1.1+r(506)*1.9,phase=r(509)*6.283185307;int lo=h,hi=h;
  for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){if(dx*dx/(rx*rx)+dz*dz/(rz*rz)>1.4)continue;auto b=field.surface(x+dx,z+dz,false);if(b.water>b.height||b.lava>b.height){skip=true;break;}lo=std::min(lo,b.height);hi=std::max(hi,b.height);}
  if(skip||hi-lo>3)continue;
  for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){
   double angle=std::atan2(double(dz),double(dx)),edge=1+.23*std::sin(3*angle+phase)+.12*std::cos(5*angle-phase),q=(dx*dx/(rx*rx)+dz*dz/(rz*rz))/(edge*edge);if(q>=1)continue;
   auto b=field.surface(x+dx,z+dz,false);int top=roundJS(h+.3+(1.1+r(510)*1.4)*std::sqrt(1-q)+.4*std::sin(dx*1.6+dz+phase));
   for(int y=b.height+1;y<=top;y++)put(x+dx,y,z+dz,2);
  }
 }else if(kind==1){if(!treeGroundAllowed(s,x,z))continue;if(std::abs(field.surface(x+2,z,false).height-field.surface(x-2,z,false).height)>2||std::abs(field.surface(x,z+2,false).height-field.surface(x,z-2,false).height)>2)continue;for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)for(int dy=1;dy<=3;dy++)if(dx*dx+dz*dz+(dy-2)*(dy-2)*2<=4.5&&h+dy>field.surface(x+dx,z+dz,false).height)put(x+dx,h+dy,z+dz,11);put(x,h+1,z,10);
 }else{int axis=r(507)<.5?0:1,len=3+int(r(508)*4),lo=1000,hi=-1000;for(int i=0;i<len;i++){int g=field.surface(x+(axis==0?i:0),z+(axis==1?i:0),false).height;lo=std::min(lo,g);hi=std::max(hi,g);}if(hi-lo>1)continue;for(int i=0;i<len;i++)put(x+(axis==0?i:0),hi+1,z+(axis==1?i:0),axis==0?25:26);}
 }
 }
 std::vector<uint16_t> vegetationChunk(int cx,int cz,bool includeGiants=false){
  if(!(field.unbounded()?terrain_bounds::gameContainsChunk(cx,cz):terrain_bounds::containsChunk(cx,cz)))throw std::out_of_range("Native vegetation bounds");
  int x0=32768+cx*16,z0=32768+cz*16;std::array<Surface,256> surfaces;
  std::vector<uint16_t> raw(256*Height);
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){auto s=field.surface(x0+x,z0+z);surfaces[z*16+x]=s;
   if(s.height>=-64&&s.height<=Height-65)raw[(z*16+x)*Height+s.height+64]=field.material(s,x0+x,s.height,z0+z);
  }
 auto put=[&](double X,double Y,double Z,int id){int x=int(std::floor(X))-x0,y=int(std::floor(Y)),z=int(std::floor(Z))-z0;if(x>=0&&x<16&&z>=0&&z<16&&y>=-64&&y<=Height-65){auto const&s=surfaces[z*16+x];if(y>s.height&&(y<=s.water||y<=s.lava))return;raw[(z*16+x)*Height+y+64]=uint8_t(id);}};
 auto ss=structures(x0,z0,16);auto ts=smallTrees(x0,z0,16,ss);auto gs=giants(x0,z0,16);
 for(auto const&t:ts){if(t.type!=5&&!forestTreeKept(t.x,t.z))continue;int x=t.x,z=t.z,h=t.h,type=t.type;double g=t.ground,r=t.radius;if(type==5){for(int y=1;y<=h;y++)put(x,g+y,z,18);continue;}constexpr int logs[5]={12,10,14,16,10},leaves[5]={13,11,15,17,11};put(x,g,z,3);double ry=type==0?h*.65:type==3?1.2:r*.8,cy=g+h;for(int dz=-int(std::ceil(r));dz<=int(std::ceil(r));dz++)for(int dx=-int(std::ceil(r));dx<=int(std::ceil(r));dx++)for(int dy=-int(std::ceil(ry));dy<=int(std::ceil(ry));dy++){double cr=type==0?r*(1-(dy+ry)/(2*ry))+.3:r;if((dx*dx+dz*dz)/(cr*cr)+(type==0?0:dy*dy/(ry*ry))<=1)put(x+dx,cy+dy,z+dz,leaves[type]);}for(int y=1;y<=h;y++)put(x,g+y,z,logs[type]);}

  // Android native villages also reserve giant-tree anchors. Keep the existing
  // server diff path unchanged unless its caller explicitly requests giants.
  if(includeGiants){
   constexpr int ids[19]={19,20,6,21,22,20,7,24,19,2,11,25,26,10,13,34,35,12,61};
   for(auto&a:gs)if(forestTreeKept(a->x,a->z))model(*a);
   for(bool leaves:{true,false})for(auto const&a:gs){if(!forestTreeKept(a->x,a->z))continue;for(auto const&r:a->runs){
    if((r.color==14)!=leaves)continue;
    for(int y=r.lo;y<r.lo+r.n;++y)put(a->x+r.x,y,a->z+r.z,ids[r.color]);
   }}
  }
  naturalFeatures(x0,z0,put,ss,ts);
  coniferEcology(x0,z0,raw,surfaces,ss,ts);broadleafForest(x0,z0,raw,surfaces);birchForest(x0,z0,raw,surfaces);
  jungleEcology(x0,z0,raw,surfaces);wetDryEcology(x0,z0,raw,surfaces);forestMushrooms(x0,z0,raw,surfaces);surfaceFlora(x0,z0,raw,surfaces,true);return raw;
 }
 void caveWaterfalls(int x0,int z0,std::vector<uint16_t>&raw){
  if(!forestDesign)return;auto candidates=field.caveFalls((x0-32768)/16,(z0-32768)/16);
  auto solid=[](int b){return b==2||(b>=94&&b<=99)||b==48;};
  for(auto p:candidates){int X=p[0]+32768,Z=p[2]+32768,y=p[1],x=X-x0,z=Z-z0;auto col=raw.data()+(z*16+x)*Height;
   if(y<-45||y>200||col[y+64]!=0)continue;bool support=false;
   for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){int ax=x+dx,az=z+dz;if(ax>=0&&ax<16&&az>=0&&az<16)support|=solid(raw[(az*16+ax)*Height+y+64]);else{auto s=field.surface(X+dx,Z+dz);support|=solid(field.material(s,X+dx,y,Z+dz));}}
   if(!support)continue;int bottom=y;while(bottom>-51&&col[bottom-1+64]==0)--bottom;
   if(y-bottom<8||!solid(col[bottom-1+64]))continue;
   col[y+64]=8;for(int h=bottom;h<y;++h)col[h+64]=126;
   // Deposit a shallow splash pool only in a sealed existing rock floor.
   if(x>1&&x<14&&z>1&&z<14)for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto c=raw.data()+((z+dz)*16+x+dx)*Height;if(c[bottom+64]==0&&solid(c[bottom-1+64])&&solid(c[bottom-2+64]))c[bottom-1+64]=8;}
  }
 }
 std::vector<uint8_t> chunk(int cx,int cz){if(!(field.unbounded()?terrain_bounds::gameContainsChunk(cx,cz):terrain_bounds::containsChunk(cx,cz)))throw std::out_of_range("Native chunk bounds");surfaceMs=columnMs=featureMs=0;forestMs.fill(0);columnNoise=0;closedRiftColumns.fill({});if(landscapeDetails&&field.streamed()){detailVolcanoes.clear();for(auto const&v:field.inspectLocalVolcanoes(cx*16,cz*16))detailVolcanoes.push_back({v.at("x"),v.at("z"),v.at("crater"),v.at("outer")});}auto clock=[](){return std::chrono::steady_clock::now();};auto chunkBegin=clock();int x0=32768+cx*16,z0=32768+cz*16;std::array<Surface,256> surfaces;std::vector<uint16_t>raw(256*Height);std::vector<uint8_t>out{75,87,50,0};out.reserve(16384);for(int z=0;z<16;z++)for(int x=0;x<16;x++){auto began=clock();auto s=field.surface(x0+x,z0+z);surfaces[z*16+x]=s;surfaceMs+=std::chrono::duration<double,std::milli>(clock()-began).count();began=clock();auto beforeNoise=noiseSamples;out.push_back(uint8_t(field.elevationVariantsEnabled()&&s.snowCover>.70&&s.biome<29?29+s.biome:s.biome));int top=std::max({s.height+s.drip,s.water,s.lava});auto col=raw.data()+(z*16+x)*Height;std::fill(col,col+4,1);field.materialColumn(s,x0+x,z0+z,top,col);columnNoise+=noiseSamples-beforeNoise;columnMs+=std::chrono::duration<double,std::milli>(clock()-began).count();}
 // Build the glacier landform before plants. Cave decoration still runs at
 // its old post-underground stage, so cave habitats keep their existing order.
 if(glacierEcologyEnabled){bool icy=false;for(auto const&s:surfaces)icy|=s.glacier>.18;
  if(icy){refinedIceSpires(x0,z0,raw,surfaces);smallIceSpikes(x0,z0,raw,surfaces);}}
 auto put=[&](double X,double Y,double Z,int id){int x=int(std::floor(X))-x0,y=int(std::floor(Y)),z=int(std::floor(Z))-z0;if(x>=0&&x<16&&z>=0&&z<16&&y>=-64&&y<=Height-65){auto const&s=surfaces[z*16+x];if(y>s.height&&(y<=s.water||y<=s.lava))return;raw[(z*16+x)*Height+y+64]=uint8_t(id);}};
 auto ss=structures(x0,z0,16);auto ts=smallTrees(x0,z0,16,ss);auto gs=giants(x0,z0,16);
 for(auto const&t:ts){if(t.type!=5&&!forestTreeKept(t.x,t.z))continue;int x=t.x,z=t.z,h=t.h,type=t.type;double g=t.ground,r=t.radius;if(type==5){for(int y=1;y<=h;y++)put(x,g+y,z,18);continue;}constexpr int logs[5]={12,10,14,16,10},leaves[5]={13,11,15,17,11};put(x,g,z,3);double ry=type==0?h*.65:type==3?1.2:r*.8,cy=g+h;for(int dz=-int(std::ceil(r));dz<=int(std::ceil(r));dz++)for(int dx=-int(std::ceil(r));dx<=int(std::ceil(r));dx++)for(int dy=-int(std::ceil(ry));dy<=int(std::ceil(ry));dy++){double cr=type==0?r*(1-(dy+ry)/(2*ry))+.3:r;if((dx*dx+dz*dz)/(cr*cr)+(type==0?0:dy*dy/(ry*ry))<=1)put(x+dx,cy+dy,z+dz,leaves[type]);}for(int y=1;y<=h;y++)put(x,g+y,z,logs[type]);}
 constexpr int boxIds[19]={19,20,6,21,22,20,7,24,19,2,11,25,26,10,13,34,35,12,61};
 auto box=[&](double x,double z,int base,double gy,double sx,double sy,double sz,int color){for(int zz=std::max(z0,int(std::ceil(z-sz/2)));zz<z+sz/2&&zz<z0+16;zz++)for(int xx=std::max(x0,int(std::ceil(x-sx/2)));xx<x+sx/2&&xx<x0+16;xx++)for(int yy=int(std::ceil(base+gy-sy/2));yy<base+gy+sy/2;yy++)put(xx,yy,zz,boxIds[color]);};
 auto house=[&](int x,int z,int w,int d,int h,int c=0,int roof=1){int base=field.base(x,z).height;box(x,z,base,.5,w,1,d,c);box(x-w/2.+.5,z,base,h/2.,1,h,d,c);box(x+w/2.-.5,z,base,h/2.,1,h,d,c);box(x,z+d/2.-.5,base,h/2.,w,h,1,c);box(x-(w+2)/4.,z-d/2.+.5,base,h/2.,(w-2)/2.,h,1,c);box(x+(w+2)/4.,z-d/2.+.5,base,h/2.,(w-2)/2.,h,1,c);box(x,z-d/2.+.5,base,h-1,w,2,1,c);box(x,z,base,h+.5,w+2,1,d+2,roof);};
for(auto const&s:ss){if(field.surface(s.x,s.z,false).glacier>.15)continue;int x=s.x,z=s.z,g=field.base(x,z).height;if(s.type==0){int gx=int(std::floor(x/num(C.at("structure"),"grid"))),gz=int(std::floor(z/num(C.at("structure"),"grid"))),count=5+int(hash01(gx,gz,terrainSeed^131)*4);for(int i=0;i<count;i++){double a=double(i)/count*6.283185307179586;house(x+roundJS(std::cos(a)*24),z+roundJS(std::sin(a)*24),9,11,6);}}else if(s.type==1){house(x,z,23,23,9,2,3);box(x,z,g,11,15,3,15,3);box(x,z,g,14,7,3,7,2);}else if(s.type==2){house(x,z,19,21,8,4,5);box(x,z,g,10,13,3,15,4);}else if(s.type==3)house(x,z,9,9,4,6,6);else if(s.type==4){house(x,z,9,25,5,7,7);box(x,z,g,10,1,16,1,7);}else house(x,z,9,11,5,7,8);}
 for(auto&a:gs)if(forestTreeKept(a->x,a->z))model(*a);
 for(bool leaves:{true,false})for(auto const&a:gs)for(auto const&r:a->runs){if(!forestTreeKept(a->x,a->z))continue;if((r.color==14)!=leaves)continue;int x=a->x+r.x-x0,z=a->z+r.z-z0;if(x<0||x>=16||z<0||z>=16)continue;auto col=raw.data()+(z*16+x)*Height;auto const&s=surfaces[z*16+x];if(s.river==0)std::fill(col+r.lo+64,col+r.lo+r.n+64,uint8_t(boxIds[r.color]));else for(int y=r.lo;y<r.lo+r.n;++y)if(y<=s.height||y>s.water)col[y+64]=uint8_t(boxIds[r.color]);}
 naturalFeatures(x0,z0,put,ss,ts);
 plainsEcology(x0,z0,raw,surfaces,ss);
 {auto t=clock();coniferEcology(x0,z0,raw,surfaces,ss,ts);forestMs[0]=std::chrono::duration<double,std::milli>(clock()-t).count();}
 {auto t=clock();broadleafForest(x0,z0,raw,surfaces);forestMs[1]=std::chrono::duration<double,std::milli>(clock()-t).count();}
 {auto t=clock();birchForest(x0,z0,raw,surfaces);forestMs[2]=std::chrono::duration<double,std::milli>(clock()-t).count();}
 {auto t=clock();jungleEcology(x0,z0,raw,surfaces);forestMs[3]=std::chrono::duration<double,std::milli>(clock()-t).count();}
 wetDryEcology(x0,z0,raw,surfaces);forestMushrooms(x0,z0,raw,surfaces);
 understory(x0,z0,raw,surfaces,ss,gs);
 oceanFeatures(x0,z0,raw,surfaces);
 sealedFluidCaves(x0,z0,raw);
 smallUndergroundPockets(x0,z0,raw,surfaces);
 auto undergroundBegin=clock();undergroundEcology(x0,z0,raw,surfaces);undergroundMs=std::chrono::duration<double,std::milli>(clock()-undergroundBegin).count();
 mountainEcology(x0,z0,raw,surfaces,ss);
 riparianEcology(x0,z0,raw,surfaces,ss);desertEcology(x0,z0,raw,surfaces,ss);
 caveWaterfalls(x0,z0,raw);
 stoneGrotto(x0,z0,raw);
 iceFeatures(x0,z0,raw,surfaces);
 cutClosedRifts(raw);
 snowCover(x0,z0,raw,surfaces);
 landscapePass(x0,z0,raw,surfaces);
 surfaceFlora(x0,z0,raw,surfaces);
 lavaFlows(x0,z0,raw,surfaces);
 variantSnowCover(x0,z0,raw,surfaces);
 if(finalTerrainOres){cleanupSnowSupports(raw,surfaces);if(oreProfilesEnabled&&undergroundEcologyEnabled){if(vanillaOres)vanillaOreProfiles(x0,z0,raw,surfaces);else oreProfiles(x0,z0,raw,surfaces,oreRockField,oreHabitat);}}
 featureMs=std::chrono::duration<double,std::milli>(clock()-chunkBegin).count()-surfaceMs-columnMs;
 for(int col=0;col<256;col++){size_t start=out.size();out.push_back(0);out.push_back(0);int count=0;auto a=raw.data()+col*Height;for(int y=0;y<Height;){int end=y+1;while(end<Height&&a[end]==a[y])end++;int n=end-y;out.push_back(uint8_t(n));out.push_back(uint8_t(n>>8));out.push_back(uint8_t(a[y]));out.push_back(uint8_t(a[y]>>8));y=end;count++;}out[start]=uint8_t(count);out[start+1]=uint8_t(count>>8);}return out;
 }
};
}
