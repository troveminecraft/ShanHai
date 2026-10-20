bool oceanEnabled=false;
bool coastalBeds=false;
bool erodedSeaShores=terrainRevision(data)>=181;
Cache<uint64_t,double> seaShoreNodes{4096};
KARMA_NOINLINE double seaShoreNode(int gx,int gz){return seaShoreNodes.get(key2(gx,gz),[&]{
 int nearestSquared=25;
 // Reuse the existing coastal height lattice. Exact drainage is inspected
 // only for possible wet samples; neither operation calls surface().
 for(int ring=0;ring<=3;++ring)for(int dz=-ring;dz<=ring;++dz)for(int dx=-ring;dx<=ring;++dx){
  if(std::max(std::abs(dx),std::abs(dz))!=ring)continue;
  int distanceSquared=dx*dx+dz*dz;if(distanceSquared>=nearestSquared)continue;
  int X=gx+dx,Z=gz+dz;
  double h=coastalHeights.get(key2(X,Z),[&]{return uncliff(X*16.,Z*16.);});
  if(h>=31||h<15)continue;
  auto a=base(X*16,Z*16);
  if(a.water==31&&a.height<31&&a.height>=15&&!a.lake&&!a.river&&a.riverBankWater==-32768&&a.stone==0&&a.volcanic==0){nearestSquared=distanceSquared;if(!nearestSquared)return 1.;}
 }
 return 1-smooth(8.,64.,16.*std::sqrt(double(nearestSquared)));
});}

KARMA_NOINLINE void shallowSeaShore(Surface&s,int x,int z){
 if(!erodedSeaShores||!oceanEnabled||s.height<32||s.height>96||s.water>=s.height
    ||s.lake||s.river||s.riverBankWater>-32768||s.riverDeposit||s.fluvialType
    ||s.stone>.01||s.volcanic>0||s.barren>0||s.glacier>0||s.superGlacier>0
    ||s.iceGround>=0||s.wetland>0||s.scarp>0||s.cliffLow>-32768||s.aridType)return;
 // High inland hills cannot become new coast merely from a height threshold.
 if(s.height>52&&s.coast<=0)return;
 int gx=int(std::floor(x/16.)),gz=int(std::floor(z/16.));
 double u=smooth(0,1,x/16.-gx),v=smooth(0,1,z/16.-gz);
 double a=seaShoreNode(gx,gz),b=u>0?seaShoreNode(gx+1,gz):a;
 double c=v>0?seaShoreNode(gx,gz+1):a,d=v>0?(u>0?seaShoreNode(gx+1,gz+1):c):b;
 double shore=mix(v,mix(u,a,b),mix(u,c,d));if(shore<=.01)return;
 shore=smooth(.02,.55,shore);
 double patch=smooth(-.35,.35,oceanNoise.at((x-32768)/49.+71,(z-32768)/49.-19));
 int before=s.height;
 // Retreat the existing low bank, leaving its native sediment/soil. The
 // continuous fade inland and alongshore avoids a constant parallel ditch.
 int cut=int(std::floor(shore*(1.2+2.6*patch)*smooth(31.,33.,before)*(1-smooth(36.,52.,before))+.5));
 cut=std::min(cut,before-31);s.height-=cut;s.seaBank=shore;
 if(cut){s.treeD=0;s.rockCap*=1-.85*shore;
  // Newly cut soil banks expose earth rather than carrying an intact turf.
  // Sand and explicit rocky coasts keep their own parent material.
  if(s.soilSand<.24&&s.surf!=2&&patch>.35)s.surf=4;
 }
 // Only a stable, already steep bank directly beside open shallow seawater
 // may have a shallow recess. No sand/gravel roof or detached inland cavity.
 if(before<36||s.height<36||s.soilSand>.35||s.surf==1||s.surf==2||s.surf==6||s.T<.28||patch<.62)return;
 bool open=false;
 for(auto [dx,dz]:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}}){
  auto q=base(x+dx,z+dz);
  if(q.water==31&&q.height<=30&&q.height>=15&&!q.lake&&!q.river&&q.stone==0&&q.volcanic==0){open=true;break;}
 }
 if(open){s.seaCutLow=30;s.seaCutHigh=32;s.treeD=0;}
}
bool gradedSeaBeds=terrainRevision(data)>=180;
struct SeaSediment {int material;};
uint64_t seaSedimentKey=0;
bool seaSedimentValid=false;
SeaSediment lastSeaSediment{5};

// One world-anchored sample per column, reused by every sediment layer.
// Province interiors stay continuous; only their margins form mixed patches.
KARMA_NOINLINE SeaSediment seaSediment(int x,int z){
 uint64_t key=key2(x,z);
 if(seaSedimentValid&&seaSedimentKey==key)return lastSeaSediment;
 double px=x-32768,pz=z-32768;
 double dx=45*oceanNoise.at(px/310.+19,pz/310.-7);
 double dz=45*oceanNoise.at(px/310.-31,pz/310.+13);
 double deposit=oceanNoise.at((px+dx)/720.+.37,(pz+dz)/720.-.19);
 double gravel=smooth(.22,.26,deposit),rock=1-smooth(-.40,-.36,deposit);
 int material=5;
 if(gravel>=1)material=42;
 else if(rock>=1)material=2;
 else if(gravel>0||rock>0){
  double patch=clamp(.5+1.7*oceanNoise.at((px+dx*.15)/14.+31,(pz+dz*.15)/14.-11)
      +.13*(hash01(x,z,terrainSeed^1423)-.5),.001,.999);
  if(gravel>patch)material=42;else if(rock>patch)material=2;
 }
 seaSedimentKey=key;seaSedimentValid=true;lastSeaSediment={material};
 return lastSeaSediment;
}

KARMA_NOINLINE void shallowSeaBed(Surface&s,int x,int z){
 // Apply after lake/river/ice classification. Never lower a dry shoreline,
 // frozen basin, river junction, rock forest or a volcanic/ice foundation.
 if(!gradedSeaBeds||!oceanEnabled||s.surf!=6||s.water!=31||s.height>=30||s.height<=15
    ||s.lake||s.river>0||s.riverBankWater>-32768||s.fluvialType||s.stone>.01
    ||s.volcanic>0||s.barren>0||s.glacier>0||s.superGlacier>0||s.iceGround>=0)return;
 double depth=31-s.height;
 double shelf=smooth(.5,3.,depth)*(1-smooth(9.,16.,depth));
 double exposure=.65+.35*smooth(0.,.3,s.ocean);
 // Static elongated scour hollows; a single 2D sample, no simulation.
 double px=x-32768,pz=z-32768;
 double hollow=smooth(-.32,.35,oceanNoise.at((.8*px+.6*pz)/58.+9,(-.6*px+.8*pz)/23.-23));
 int cut=int(std::floor(2.4*shelf*exposure*hollow+.5));
 s.height-=cut;
}
double oceanBasin=18,oceanDeep=26,oceanTrench=12;
Perlin oceanNoise{seedHash(terrainSeed,1401)};
struct OceanNode{double broad,open,mouth;};
Cache<uint64_t,OceanNode> oceanNodes{4096};
void loadOcean(){
 if(!C.contains("ocean"))return;
 auto const&q=C.at("ocean");oceanEnabled=q.value("enabled",false);
 coastalBeds=q.value("coastalBeds",0)>=1;
 oceanBasin=num(q,"basinDepth");oceanDeep=num(q,"deepAmplitude");oceanTrench=num(q,"trenchDepth");
 if(oceanBasin<0||oceanBasin>20||oceanDeep<0||oceanDeep>28||oceanTrench<0||oceanTrench>14)
  throw std::runtime_error("Invalid ocean depth budget");
}
OceanNode oceanNode(int gx,int gz){return oceanNodes.get(key2(gx,gz),[&]{
 std::array<double,8> wet{};int i=0;
 for(auto [dx,dz]:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1},std::pair{-1,-1},std::pair{1,-1},std::pair{-1,1},std::pair{1,1}}){
  double offshore=1-smooth(25.,38.,vcont(gx*128.+dx*192,gz*128.+dz*192));
  wet[i++]=offshore;
 }
 double sum=0;for(double w:wet)sum+=w;
 double broad=smooth(.35,.85,sum/8);
 // Opposing dry banks identify narrow channels; a wet sea-facing fan identifies mouths.
 double bank=std::max((1-wet[0])*(1-wet[1]),(1-wet[2])*(1-wet[3]));
 double fan=std::max({wet[0]*wet[4]*wet[6],wet[1]*wet[5]*wet[7],wet[2]*wet[4]*wet[5],wet[3]*wet[6]*wet[7]});
 return OceanNode{broad,smooth(.32,.76,sum/8)*(1-.8*bank),fan*(1-smooth(.55,.85,sum/8))};
});}
void oceanRelief(Surface&s,double&h,int x,int z){
 if(!oceanEnabled||h>=31||s.stone>.01)return;
 int gx=int(std::floor(x/128.)),gz=int(std::floor(z/128.));
 double u=smooth(0,1,x/128.-gx),v=smooth(0,1,z/128.-gz);
 auto A=oceanNode(gx,gz),B=oceanNode(gx+1,gz),D=oceanNode(gx,gz+1),E=oceanNode(gx+1,gz+1);
 auto blend=[&](double a,double b,double d,double e){return(1-u)*(1-v)*a+u*(1-v)*b+(1-u)*v*d+u*v*e;};
 double broad=blend(A.broad,B.broad,D.broad,E.broad),open=blend(A.open,B.open,D.open,E.open);
 s.estuary=blend(A.mouth,B.mouth,D.mouth,E.mouth);
 double depth=31-h,px=x-32768,pz=z-32768;
 s.ocean=broad*smooth(0.,6.,depth);
 double w=broad*open*smooth(3.,21.,depth)*(1-.65*s.estuary);
 // Submerged sediment fan gently rises inside the river mouth; shore and land stay fixed.
 h+=s.estuary*2.5*smooth(1.,5.,depth)*(1-smooth(16.,24.,depth));
 if(w<=0)return;
 double regional=oceanNoise.at(px/530.,pz/530.);
 double basin=smooth(-.30,.30,regional);
 double ridge=5*oceanNoise.at(px/95.+7,pz/95.-17)+2*oceanNoise.at(px/31.,pz/31.);
 double trench=(1-smooth(.025,.10,std::abs(oceanNoise.at(px/180.+41,pz/180.-3))))*smooth(.16,.35,regional);
 h-=w*(oceanBasin+oceanDeep*basin+oceanTrench*trench-ridge);
 // Smooth bedrock clearance, without a flat clamp across abyssal basins.
 if(h<-48)h=-48-8*(1-std::exp((h+48)/8));
}
int oceanMaterial(Surface const&s,int x,int y,int z){
 if(y<s.height-3)return 2;
 if(gradedSeaBeds)return seaSediment(x,z).material;
 // Broad sediment provinces: hundreds of blocks, with gently warped edges.
 double px=x-32768,pz=z-32768;
 double dx=45*oceanNoise.at(px/310.+19,pz/310.-7);
 double dz=45*oceanNoise.at(px/310.-31,pz/310.+13);
 double deposit=oceanNoise.at((px+dx)/720.+.37,(pz+dz)/720.-.19);
 // Rocky shelves use the same regional field, not per-column rock speckle.
 if(deposit>.24)return 42;
 if(deposit<-.38)return 2;
 return 5;
}

int oceanBiome(Surface const&s,int x,int z){
 double frozen=1-smooth(.18,.26,s.T),deep=smooth(32.,52.,31-s.height);
 double warm=smooth(.58,.70,s.T),cold=1-smooth(.36,.48,s.T);
 double q=clamp(.5+.9*oceanNoise.at((x-32768)/48.+13,(z-32768)/48.-7),0,.999999);
 double a=frozen;if(q<a)return 2;
 a+=(1-frozen)*deep;if(q<a)return 0;
 a+=(1-frozen)*(1-deep)*warm;if(q<a)return 21;
 a+=(1-frozen)*(1-deep)*(1-warm)*cold;if(q<a)return 22;
 return 1;
}
