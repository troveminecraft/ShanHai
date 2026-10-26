// Resolve all owners touching a column before caching it; exploration cannot
// add a new influence to previously evaluated terrain.
bool streamingRegions=data.contains("regionalGeneration");
bool unboundedRegions=streamingRegions&&data.at("regionalGeneration").value("boundsMode",std::string{})=="infinite";
std::string streamingIdentity;
std::unique_ptr<J> samplingData;
std::unique_ptr<Field> samplingField;
std::set<uint64_t> preparedRegions,preparedIce;
std::unordered_map<int,std::vector<LakeSpan>> streamingLakeRows;
Field& naturalSamplingField(){
 if(!samplingField){samplingData=std::make_unique<J>(data);samplingData->erase("regionalGeneration");samplingField=std::make_unique<Field>(*samplingData);}
 return *samplingField;
}
Surface regionalClimateSample(int x,int z){
 auto s=naturalSamplingField().base(x,z);s.height=int(std::floor(desertHeight(x,z,s.height)));
 if(!s.closedBasin)s.water=s.height<seaLevelValue?seaLevelValue:-32768;desertClimate(s,x,z);return s;
}
struct DesertRegionPlan{bool valid;double x,z,rx,rz;};
Cache<uint64_t,DesertRegionPlan> desertPlans{64};
double streamingDesertWeight(double wx,double wz){
 if(!streamingRegions||!C.contains("deserts")||!C.at("deserts").value("enabled",false))return 0;
 double X=wx-32768,Z=wz-32768,weight=0;constexpr double grid=6144.;
 int gx=int(std::floor(X/grid)),gz=int(std::floor(Z/grid));
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){int a=gx+dx,b=gz+dz;
  auto plan=desertPlans.get(key2(a,b),[&]{auto saved=regional::get(streamingIdentity,"desert",a,b,[&]{
   double cx=(a+.40+.20*hash01(a,b,terrainSeed^63201))*grid,cz=(b+.40+.20*hash01(a,b,terrainSeed^63203))*grid;
   bool valid=hash01(a,b,terrainSeed^63205)<.45;auto center=naturalSamplingField().base(roundJS(cx)+32768,roundJS(cz)+32768);
   valid&=(elevationVariants?naturalSamplingField().surfaceTemperature(center,roundJS(cx)+32768,roundJS(cz)+32768):center.T)>.48&&center.H<.66&&center.height>=(elevationVariants?seaLevelValue+8:44)&&center.height<=(elevationVariants?400:140);
   // Keep an inland margin around a >100-chunk core. An invalid site does
   // not generate a small substitute desert.
   for(int zz=-3;zz<=3&&valid;++zz)for(int xx=-3;xx<=3&&valid;++xx){auto s=naturalSamplingField().base(roundJS(cx+xx*880)+32768,roundJS(cz+zz*880)+32768);valid&=s.height>(elevationVariants?seaLevelValue+4:36)&&s.stone<=.001&&s.volcanic<=0;}
   return J{{"valid",valid},{"x",cx},{"z",cz},{"rx",1900.},{"rz",1850.}};
  });return DesertRegionPlan{saved->at("valid"),saved->at("x"),saved->at("z"),saved->at("rx"),saved->at("rz")};});
  if(!plan.valid)continue;
  double px=(X-plan.x)/plan.rx,pz=(Z-plan.z)/plan.rz;
  double distance=std::hypot(px,pz)+.055*desertNoise.at(X/450.,Z/450.);weight=std::max(weight,1-smooth(.72,1.25,distance));
 }
 for(auto const&r:regionalProtected)if(r.radius>=0)weight*=smooth(r.radius+100,r.radius+480,std::hypot(X-r.x,Z-r.z));
 return weight;
}
std::shared_ptr<J const> regionalWaterPlan(int a,int b){
 auto algorithm=data.at("regionalGeneration").value("algorithm",std::string{});
 bool curved=algorithm!="regional-drainage-v1";
 return regional::get(streamingIdentity,"water",a,b,[&]{return regional::drainage(a,b,terrainSeed,[&](int x,int z){
  auto s=regionalClimateSample(x+32768,z+32768);return regional::Ground{s.height,s.H,s.T,s.stone,s.volcanic,s.closedBasin,s.water};
 },curved,seaLevelValue,generatedMaxYExclusive-1,algorithm=="regional-drainage-v3",C.value("riverBankSurvey",0)>=1);});
}
void ensureStreamingRegions(int wx,int wz){
 if(!streamingRegions)return;
 int gx=int(std::floor((wx-32768)/double(regional::Core))),gz=int(std::floor((wz-32768)/double(regional::Core)));bool changed=false;
 if(unboundedRegions&&preparedRegions.size()>64&&!preparedRegions.contains(key2(gx,gz))){
  preparedRegions.clear();streamingLakeRows.clear();riverSegments.clear();riverBins.clear();
 }
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){int a=gx+dx,b=gz+dz;auto owner=key2(a,b);if(preparedRegions.contains(owner))continue;
  if(localPlanningCulling){
   // Drainage endpoints stay in Core +/- Halo. Generated widths are <=8;
   // their indexed bank reach is <54, plus at most 64 for bin rounding.
   // Keep a conservative extra 128; omitted owners cannot affect this column.
   constexpr int reach=regional::Halo+128;
   int64_t x=int64_t(wx)-32768,z=int64_t(wz)-32768;
   if(x<int64_t(a)*regional::Core-reach||x>int64_t(a+1)*regional::Core+reach||
      z<int64_t(b)*regional::Core-reach||z>int64_t(b+1)*regional::Core+reach)continue;
  }
  auto plan=regionalWaterPlan(a,b);
  for(auto const&lake:plan->at("lakes"))for(auto const&r:lake.at("rows")){
   int z=r.at(0),x0=r.at(1),x1=r.at(2);
   if(!unboundedRegions){if(z<terrain_bounds::MinBlock||z>terrain_bounds::MaxBlock)continue;x0=std::max(x0,terrain_bounds::MinBlock);x1=std::min(x1,terrain_bounds::MaxBlock);}
   if(x0>x1)continue;auto&row=unboundedRegions?streamingLakeRows[z]:lakeRows[z-terrain_bounds::MinBlock];
   row.push_back({x0,x1,{lake.at("level"),true,lake.at("frozen"),true}});
  }
  for(auto const&r:plan->at("segments")){
   RiverSegment s{r.at(0),r.at(1),r.at(2),r.at(3),r.at(4),r.at(5),r.at(6),r.at(7)};
   s.runA=r.at(8);s.runB=r.at(9);s.main=r.at(10);size_t id=riverSegments.size();riverSegments.push_back(s);double halo=std::max(s.ar,s.br)*1.55+17+s.main*24;
   for(int z=int(std::floor((std::min(s.az,s.bz)-halo)/64));z<=int(std::floor((std::max(s.az,s.bz)+halo)/64));++z)
    for(int x=int(std::floor((std::min(s.ax,s.bx)-halo)/64));x<=int(std::floor((std::max(s.ax,s.bx)+halo)/64));++x)riverBins[key2(x,z)].push_back(id);
  }
  preparedRegions.insert(owner);changed=true;
 }
 if(changed){
  for(auto&row:lakeRows)if(row.size()>1)std::sort(row.begin(),row.end(),[](auto const&a,auto const&b){return a.x0<b.x0;});
  for(auto&[z,row]:streamingLakeRows)if(row.size()>1)std::sort(row.begin(),row.end(),[](auto const&a,auto const&b){return a.x0<b.x0;});
  for(auto&[key,ids]:riverBins)std::sort(ids.begin(),ids.end(),[&](size_t a,size_t b){auto const&A=riverSegments[a];auto const&B=riverSegments[b];return std::tie(A.ax,A.az,A.bx,A.bz,A.aw,A.bw,A.ar,A.br,A.runA,A.runB,A.main)<std::tie(B.ax,B.az,B.bx,B.bz,B.aw,B.bw,B.ar,B.br,B.runA,B.runB,B.main);});
 }
}
void ensureStreamingIce(int wx,int wz){
 if(!streamingRegions||!snowUpdateEnabled)return;
 int gx=int(std::floor((wx-32768)/4608.)),gz=int(std::floor((wz-32768)/4608.));
 if(unboundedRegions&&preparedIce.size()>64&&!preparedIce.contains(key2(gx,gz))){preparedIce.clear();iceAtlas.clear();}
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){int a=gx+dx,b=gz+dz;auto key=key2(a,b);if(preparedIce.contains(key))continue;
  auto saved=regional::get(streamingIdentity,"ice",a,b,[&]{auto r=iceRegion(a,b);return J{{"valid",r.valid},{"x",r.x},{"z",r.z},{"rx",r.rx},{"rz",r.rz}};});
  if(saved->at("valid").get<bool>()){IceRegion r;r.valid=true;r.gx=a;r.gz=b;r.x=saved->at("x");r.z=saved->at("z");r.rx=saved->at("rx");r.rz=saved->at("rz");iceAtlas.push_back(r);}
  preparedIce.insert(key);
 }
 std::sort(iceAtlas.begin(),iceAtlas.end(),[](auto const&a,auto const&b){return std::tie(a.gx,a.gz)<std::tie(b.gx,b.gz);});iceAtlasReady=true;
}
