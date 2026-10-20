// Rare, seed-baked dry mountain scarps. Indexed columns, no 3-D noise field.
struct OverhangScarp {double x,z,nx,nz,toe,crest,tangent,halfLength,front,inset,phase,back=64;};
std::vector<OverhangScarp> overhangScarps;
std::unordered_map<uint64_t,std::vector<size_t>> scarpBins;
bool loftedScarps=false,noiseWarpedScarps=false;
#include "NoiseScarps.inc"
void loadScarps(){
 if(!data.contains("overhangCliffs"))return;
 auto const&q=data.at("overhangCliffs");
 noiseWarpedScarps=q.at("algorithm")=="noise-warped-scarps-v3";
 if(noiseWarpedScarps){loadNoiseScarps();return;}
 loftedScarps=q.at("algorithm")=="lofted-dry-scarps-v2";
 if(q.at("seed").get<uint32_t>()!=terrainSeed||(!loftedScarps&&q.at("algorithm")!="rare-dry-scarps-v1"))throw std::runtime_error("Scarp seed mismatch");
 for(auto const&r:q.at("sites")){
  OverhangScarp a{r.at("x"),r.at("z"),r.at("nx"),r.at("nz"),r.at("toe"),r.at("crest"),r.at("tangent"),r.at("halfLength"),r.at("front"),r.at("inset"),r.at("phase")};
  if(loftedScarps)a.back=r.at("back");
  if(!std::isfinite(a.x+a.z+a.nx+a.nz+a.toe+a.crest+a.tangent+a.halfLength+a.front+a.inset+a.phase)
      ||std::abs(a.x)>3950||std::abs(a.z)>3950||std::abs(std::hypot(a.nx,a.nz)-1)>.001
      ||a.toe<40||a.toe>250||a.crest-a.toe<20||a.crest>290||std::abs(a.tangent)>.5||a.halfLength<32||a.halfLength>80
      ||a.front<32||a.front>64||a.inset<4||a.inset>16||!std::isfinite(a.back)||a.back<48||a.back>96)throw std::runtime_error("Invalid overhang scarp");
  size_t id=overhangScarps.size();overhangScarps.push_back(a);
  double halo=std::hypot(a.halfLength,loftedScarps?std::max(a.front,a.back):a.front)+5;
  for(int z=int(std::floor((a.z-halo)/64));z<=int(std::floor((a.z+halo)/64));z++)
   for(int x=int(std::floor((a.x-halo)/64));x<=int(std::floor((a.x+halo)/64));x++)scarpBins[key2(x,z)].push_back(id);
 }
 if(overhangScarps.size()>64)throw std::runtime_error("Scarp site budget exceeded");
}
void legacyScarpRelief(Surface&s,int wx,int wz){
 if(overhangScarps.empty()||s.water>=s.height||s.lake||s.mainRiver>0||s.stone>0||s.volcanic>0||s.coast>.01||s.wetland>0||giantWeight(wx,wz,s)>0)return;
 double x=wx-32768,z=wz-32768;
 auto bin=scarpBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));if(bin==scarpBins.end())return;
 for(auto id:bin->second){auto const&a=overhangScarps[id];
  double dx=x-a.x,dz=z-a.z,w=-dx*a.nz+dz*a.nx;
  if(std::abs(w)>=a.halfLength)continue;
  double u=dx*a.nx+dz*a.nz-1.8*std::sin(w/19+a.phase)-.6*std::sin(w/7-a.phase);
  if(u<=-a.front||u>=a.inset)continue;
  double side=1-smooth(a.halfLength*.55,a.halfLength,std::abs(w));
  double original=s.height,target=a.toe+.18*u+w*a.tangent
      +1.1*std::sin(u/17+w/23+a.phase)+.65*std::sin(w/8-a.phase);
  double floor=original-side*clamp(original-target,0.,64.);
  if(original-floor<4)continue;
  if(u<0){
   double fade=smooth(-a.front,-a.front*.25,u);
   s.height=int(std::floor(mix(fade,original,floor)));
   if(s.height<original){s.scarp=side*fade;s.cliffLow=s.cliffHigh=-32768;}
  }else{
   // A tall open face, not a small ellipsoidal hole in a hillside. The
   // low exterior on u<0 opens the entire curved interval to the valley.
   double depth=a.inset*side,roof=5.+1.5*(1+std::sin(w/23+a.phase));
   if(u>=depth||original-floor<16)continue;
   // Anchor the underside to the crest rather than lifting it farther into
   // the uphill slope. Otherwise the rim becomes a downward hanging tooth.
   double ceiling=std::min(original-4.,a.crest+w*a.tangent-roof);
   double half=(ceiling-floor)*.5,mid=floor+half;
   if(half<3)continue;
   double span=half*std::sqrt(std::max(0.,1-u*u/(depth*depth)));
   int lo=int(std::ceil(mid-span)),hi=int(std::floor(mid+span));
   if(hi-lo<3)continue;
   s.cliffLow=lo;s.cliffHigh=std::min(hi,s.height-4);s.scarp=side;
  }
  s.continuous=(s.height-31)/123.;s.e=s.continuous;
 }
}
// Original analytic loft, informed by volume/density terrain representations
// (see CLIFF-PROFILES-0.8.6.md). There is one solid mountain behind the face,
// including an uplifted shoulder; this is not a cavity subtracted from a slope.
// Solve the two monotone profile branches once per XZ column. The existing
// material loop then consumes a top height and at most one air interval.
void scarpRelief(Surface&s,int wx,int wz){
 if(noiseWarpedScarps){noiseScarpRelief(s,wx,wz);return;}
 if(!loftedScarps){legacyScarpRelief(s,wx,wz);return;}
 if(overhangScarps.empty()||s.water>=s.height||s.lake||s.mainRiver>0||s.stone>0||s.volcanic>0||s.coast>.01||s.wetland>0||giantWeight(wx,wz,s)>0)return;
 double x=wx-32768,z=wz-32768;
 auto bin=scarpBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));if(bin==scarpBins.end())return;
 for(auto id:bin->second){auto const&a=overhangScarps[id];
  double dx=x-a.x,dz=z-a.z,w=-dx*a.nz+dz*a.nx;
  if(std::abs(w)>=a.halfLength)continue;
  double u=dx*a.nx+dz*a.nz-1.8*std::sin(w/19+a.phase)-.6*std::sin(w/7-a.phase);
  if(u<=-a.front||u>=a.back)continue;
  double side=1-smooth(a.halfLength*.36,a.halfLength,std::abs(w));
  double original=s.height,toe=a.toe+w*a.tangent;
  double crest=a.crest+w*a.tangent+.8*std::sin(w/21+a.phase);
  double waist=a.inset*.6,lip=-a.inset*.65,relief=crest-toe;
  double bend=.34+.035*std::sin(w/27+a.phase),waistY=toe+relief*bend;
  // Broad talus/bedrock foot, becoming vertical at the waist.
  double f=1-std::sqrt(clamp((waist-u)/(waist+a.front),0.,1.));
  double foot=toe+(waistY-toe)*f*f;
  double frontFade=smooth(-a.front,-a.front*.60,u);
  foot=mix(frontFade,original,foot);
  double shoulder=crest+.06*(u-lip);
  shoulder=mix(smooth(waist+8,a.back,u),shoulder,original);
  double top=u<lip?foot:shoulder;
  int lo=-32768,hi=-32768;
  if(u>=lip&&u<waist){
   // The upper face retreats progressively into the mountain; its tip joins
   // the high shoulder with a horizontal tangent, not a flat cut-off ceiling.
   double q=clamp((waist-u)/(waist-lip),0.,1.);
   double upper=waistY+(crest-waistY)*std::sqrt(q*(2-q));
   lo=int(std::floor(mix(side,original,foot)))+1;
   hi=int(std::ceil(mix(side,original,upper)))-1;
   // A continuous tip can turn into detached single blocks after rounding,
   // especially where the lateral taper climbs several blocks per column.
   // End the tip before it becomes thinner than its voxel support budget.
   int support=2+int(std::ceil(4*(1-side)));
   if(int(std::floor(mix(side,original,shoulder)))-hi<support){top=foot;lo=hi=-32768;}
  }
  s.height=int(std::floor(mix(side,original,top)));
  s.cliffLow=lo;s.cliffHigh=hi;
  if(hi-lo<2||side<.18)s.cliffLow=s.cliffHigh=-32768;
  // Exposed face uses rock; the inland shoulder returns to ordinary soil.
  s.scarp=side*frontFade*(1-smooth(waist+4,a.back*.75,u));
  // Keep a rock shell below the foot and throughout the upper lip. The cave
  // depth must not use the raised rim for a column also open below that rim.
  if(s.scarp>0)s.scarpCaveCeiling=int(std::floor(mix(side,original,foot)-caveRoof));
  s.continuous=(s.height-31)/123.;s.e=s.continuous;
 }
}
// Used only by the offline seed builder / diagnostic CLI. No world scan in BDS.
J scarpCandidates(){
 // Keep candidate density stable as the playable square grows. A fixed 256
 // block grid made the ±10000 build test 6x more expensive for no visual
 // benefit; the accepted deformation still uses its full local noise sample.
 J out=J::array();constexpr int grid=512;
 for(int gz=terrain_bounds::MinBlock/grid+1;gz<terrain_bounds::MaxBlock/grid;gz++)for(int gx=terrain_bounds::MinBlock/grid+1;gx<terrain_bounds::MaxBlock/grid;gx++){
  if(hash01(gx,gz,terrainSeed^19201)>=.16)continue;
  int cx=gx*grid+128,cz=gz*grid+128;double best=0;J chosen;
  for(int dz:{-64,-32,0,32,64})for(int dx:{-64,-32,0,32,64}){
   int x=cx+dx,z=cz+dz;if(std::max(std::abs(x),std::abs(z))>terrain_bounds::MaxBlock-265)continue;
   auto s=base(x+32768,z+32768);
   if(s.height<110||s.height>255||s.water>=s.height||s.lake||s.mainRiver>0||s.stone>0||s.volcanic>0||s.coast>.01||giantWeight(x+32768,z+32768,s)>0)continue;
   auto h=[&](double X,double Z){return base(roundJS(X)+32768,roundJS(Z)+32768).height;};
   double ex=(h(x+12,z)-h(x-12,z))/24.,ez=(h(x,z+12)-h(x,z-12))/24.,slope=std::hypot(ex,ez);
   if(slope<.65||slope>3.2||slope<=best)continue;
   double nx=ex/slope,nz=ez/slope,toe=h(x-nx*42,z-nz*42);
   if(s.height-toe<26||s.height-toe>64||toe<45)continue;
   double tangent=clamp((h(x-nz*24,z+nx*24)-h(x+nz*24,z-nx*24))/48.,-.4,.4);
   best=slope;
   chosen=J{{"x",x},{"z",z},{"nx",nx},{"nz",nz},{"toe",toe},{"crest",s.height},{"tangent",tangent},
     {"halfLength",44+24*hash01(gx,gz,terrainSeed^19203)},{"front",56.},
     {"inset",9+5*hash01(gx,gz,terrainSeed^19207)},{"phase",6.283185307*hash01(gx,gz,terrainSeed^19211)}};
  }
  if(!chosen.is_null())out.push_back(chosen);
 }
 return out;
}
