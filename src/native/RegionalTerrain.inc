// Large continuous lowland/hill belts, independent of ecological biome labels.
// Only 2-D coarse nodes are cached; no extra per-voxel density layers.
struct RegionalNode {double lowland,undulation;};
bool regionalEnabled=false;
bool monotoneRegional=false;
double regionalPitch=64,regionalWavelength=1800,regionalLow=-.28,regionalHigh=.22;
double regionalRetention=.22,regionalStrength=1.;
Perlin regionalNoise{seedHash(terrainSeed,1801)};
Cache<uint64_t,RegionalNode> regionalNodes{8192};
struct ProtectedTerrain {double x,z,radius;};
std::array<ProtectedTerrain,2> regionalProtected{};
void loadRegionalTerrain(){
 if(!C.contains("regionalTerrain")||!C.at("regionalTerrain").value("enabled",false))return;
 auto const&q=C.at("regionalTerrain");regionalEnabled=true;
 monotoneRegional=q.value("monotoneProfile",0)>=1;
 regionalWavelength=num(q,"wavelength");regionalLow=num(q,"lowlandThreshold");regionalHigh=num(q,"mountainThreshold");
 regionalRetention=num(q,"lowlandReliefRetention");regionalStrength=num(q,"strength");
 if(regionalWavelength<1024||regionalWavelength>4096||regionalHigh<=regionalLow||regionalRetention<.1||regionalRetention>.5||regionalStrength<0||regionalStrength>1)
  throw std::runtime_error("Invalid regional terrain distribution");
 int i=0;for(auto name:{"stoneForest","giantForest"}){
  auto const&s=C.at(name);regionalProtected[i++]={num(s,"centerX"),num(s,"centerZ"),(std::string_view(name)=="stoneForest"&&!largeStoneForestEnabled())?-1.:num(s,"outerRadius")+(s.contains("edgeWarp")?num(s,"edgeWarp"):0.)};
 }
}
RegionalNode regionalNode(int gx,int gz){return regionalNodes.get(key2(gx,gz),[&]{
 double x=gx*regionalPitch,z=gz*regionalPitch;
 double dx=180*regionalNoise.at(x/760.+31,z/760.-17),dz=180*regionalNoise.at(x/760.-43,z/760.+29);
 double region=regionalNoise.at((x+dx)/regionalWavelength+.37,(z+dz)/regionalWavelength-.19);
 return RegionalNode{1-smooth(regionalLow,regionalHigh,region),4*regionalNoise.at(x/320.+53,z/320.-73)};
});}
double regionalHeight(double wx,double wz,double height){
 if(!regionalEnabled||height<=46)return height;
 double x=wx-32768,z=wz-32768;
 // Keep the accepted special forests and a broad continuous outer transition.
 double protect=0;
 for(auto const&r:regionalProtected){
  if(r.radius<0)continue;
  double dx=x-r.x,dz=z-r.z;
  if(std::abs(dx)>r.radius+320||std::abs(dz)>r.radius+320)continue;
  protect=std::max(protect,1-smooth(r.radius+64,r.radius+320,std::hypot(dx,dz)));
 }
 if(protect>=1)return height;
 int gx=int(std::floor(x/regionalPitch)),gz=int(std::floor(z/regionalPitch));
 double u=smooth(0,1,x/regionalPitch-gx),v=smooth(0,1,z/regionalPitch-gz);
 auto a=regionalNode(gx,gz),b=regionalNode(gx+1,gz),c=regionalNode(gx,gz+1),d=regionalNode(gx+1,gz+1);
 auto blend=[&](double A,double B,double C,double D){return(1-u)*(1-v)*A+u*(1-v)*B+(1-u)*v*C+u*v*D;};
 double lowland=blend(a.lowland,b.lowland,c.lowland,d.lowland);
 double undulation=blend(a.undulation,b.undulation,c.undulation,d.undulation);
 double target=46+(height-46)*regionalRetention+undulation;
 if(monotoneRegional){
  // Monotone rational quadratic between the unchanged lower terrain and
  // the established compressed upper terrain. Positive endpoint slopes
  // keep higher input ground higher, even at full lowland strength.
  // Multiplying the old cut by smoothstep(height) could reverse that order.
  constexpr double span=38.;
  double rise=std::max(1.,span*regionalRetention+undulation);
  if(height<84.){
   double t=(height-46.)/span,d=rise/span,cross=t*(1-t);
   target=46.+rise*(d*t*t+cross)/(d+(1.+regionalRetention-2*d)*cross);
  }else target=46.+rise+(height-84.)*regionalRetention;
  return mix(regionalStrength*lowland*(1-protect),height,std::min(height,target));
 }
 double weight=regionalStrength*lowland*(1-protect)*smooth(46,84,height);
 // No height cap: intact mountain cores retain their original peaks; converted
 // belts keep some original relief, with continuous slopes between both.
 return mix(weight,height,std::min(height,target));
}
void finalTerrainClimate(Surface&s,double temperatureNoise,double erosionValue){
 if(!regionalEnabled)return;
 s.continuous=s.e=(s.height-31)/123.;
 auto const&q=C.at("transition");auto const&r=C.at("regionalTerrain");
 double high=smooth(num(r,"mountainElevation")-num(q,"domHeightWidth"),num(r,"mountainElevation")+num(q,"domHeightWidth"),s.height);
 s.plateau=high*smooth(.65-num(q,"domErosionWidth"),.65+num(q,"domErosionWidth"),erosionValue);s.mount=high-s.plateau;
 s.basin=1-smooth(26,46,s.height);
 s.T=(1-s.stone)*clamp(.62+temperatureNoise*.45-std::max(0.,s.height-48.)*.004,0,1)+s.stone*.67;
}
