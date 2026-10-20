// Low-frequency inland aridity is frozen at seed build time. Per-worker
// four-node interpolation and cached 2-D dune samples only; no voxel noise.
std::vector<uint8_t> desertWeights;
int desertSide=0,desertOrigin=0,desertPitch=64;
double desertWindCos=1,desertWindSin=0;
bool largeDesertOnly=false;
bool foothillSandCover=false;
Perlin desertNoise{seedHash(terrainSeed,19001)};
struct DesertNode {double sand,dune,crest,patch;};
Cache<uint64_t,DesertNode> desertNodes{8192};
void loadDeserts(){
 largeDesertOnly=C.contains("deserts")&&C.at("deserts").value("largeSandOnly",false);
 foothillSandCover=C.contains("deserts")&&C.at("deserts").value("foothillSandCover",0)==1;
 if(!C.contains("deserts")||!C.at("deserts").value("enabled",false)||!data.contains("deserts"))return;
 auto const&d=data.at("deserts");
 if((d.at("algorithm")!="inland-arid-mask-v1"&&d.at("algorithm")!="large-inland-sand-v2")||d.at("seed")!=terrainSeed)throw std::runtime_error("Invalid desert seed mask");
 desertSide=d.at("side");desertOrigin=d.at("origin");desertPitch=d.at("pitch");
 if((desertSide<192||desertSide>1024)||desertOrigin!=-desertSide*32||desertPitch!=64||d.at("weights").size()!=size_t(desertSide*desertSide))throw std::runtime_error("Invalid desert grid");
 for(auto const&v:d.at("weights")){int b=v;if(b<0||b>255)throw std::runtime_error("Invalid desert weight");desertWeights.push_back(uint8_t(b));}
 double angle=d.at("wind");desertWindCos=std::cos(angle);desertWindSin=std::sin(angle);
}
double desertWeight(double wx,double wz){
 if(streamingRegions)return streamingDesertWeight(wx,wz);
 if(desertWeights.empty())return 0;
 double x=(wx-32768-desertOrigin)/desertPitch,z=(wz-32768-desertOrigin)/desertPitch;
 int ix=int(std::floor(x)),iz=int(std::floor(z));if(ix<0||iz<0||ix+1>=desertSide||iz+1>=desertSide)return 0;
 double u=smooth(0,1,x-ix),v=smooth(0,1,z-iz);int i=iz*desertSide+ix;
 return ((desertWeights[i]*(1-u)+desertWeights[i+1]*u)*(1-v)+(desertWeights[i+desertSide]*(1-u)+desertWeights[i+desertSide+1]*u)*v)/255.;
}
DesertNode desertNode(int gx,int gz){return desertNodes.get(key2(gx,gz),[&]{
 double x=gx*8.,z=gz*8.;
 double patch=desertNoise.at(x/460.+19,z/460.-23);
 double sand=smooth(-.19,.12,patch);
 double across=x*desertWindCos+z*desertWindSin;
 double along=-x*desertWindSin+z*desertWindCos;
 double warp=42*desertNoise.at(x/210.+7,z/210.-13)+16*desertNoise.at(x/85.,z/85.);
 double phase=(across+warp)/118.+.15*std::sin(along/190.);
 double t=phase-std::floor(phase);
 // Long windward rise, short lee fall; smooth tips avoid sharp grid seams.
 double ridge=t<.77?smooth(0,.77,t):1-smooth(.77,1.,t);
 double amplitude=8+10*clamp(.5+desertNoise.at(x/320.-9,z/320.+17),0,1);
 return DesertNode{largeDesertOnly?1.:sand,amplitude*ridge*(largeDesertOnly?.65+.35*sand:1.),ridge,patch};
});}
DesertNode desertDetail(double wx,double wz){
 double x=(wx-32768)/8.,z=(wz-32768)/8.;int ix=int(std::floor(x)),iz=int(std::floor(z));
 double u=x-ix,v=z-iz;auto a=desertNode(ix,iz),b=desertNode(ix+1,iz),c=desertNode(ix,iz+1),d=desertNode(ix+1,iz+1);
 auto bl=[&](double A,double B,double C,double D){return(A*(1-u)+B*u)*(1-v)+(C*(1-u)+D*u)*v;};
 return {bl(a.sand,b.sand,c.sand,d.sand),bl(a.dune,b.dune,c.dune,d.dune),bl(a.crest,b.crest,c.crest,d.crest),bl(a.patch,b.patch,c.patch,d.patch)};
}
double desertHeight(double wx,double wz,double h){
 double w=desertWeight(wx,wz);if(w<=0||h<=36)return h;
 auto d=desertDetail(wx,wz);
 // Monotone compression with no elevation cap. Retain mountain rims and
 // their original drainage gradients; the broad transition shares this field.
 double rise=std::max(0.,h-42.);double target=42.+rise/(1.+rise/45.);
 // Retain high mountain rims primarily in the transition. Leaving every
 // original mountain untouched inside the core makes an arid mountain range,
 // rather than the requested predominantly low, broad desert basin.
 double foothill=1-smooth(135,220,h)*(1-w);
 double weight=w*foothill*smooth(36,48,h);
 return mix(weight,h,target+d.sand*d.dune);
}
void desertClimate(Surface&s,int x,int z){
 s.desert=desertWeight(x,z)*(1-smooth(150,225,s.height));
 if(s.desert<=0||s.stone>.01||s.volcanic>0||s.height<34){s.desert=0;return;}
 auto d=desertDetail(x,z);s.desertSand=d.sand;s.desertCrest=d.crest;s.desertPatch=d.patch;
 s.T=mix(s.desert,s.T,.77);s.H=mix(s.desert,s.H,.13);
}
void desertSkin(Surface&s,int x,int z){
 if(s.desert<=.02||s.water>s.height||s.lake||s.river||s.riverDeposit||s.giant>.01||s.stone>.01||s.barren>0||s.scarp>.01)return;
 // Rivers expose their real nearest wet-bank distance and water datum.
 // A high terrace does not become green merely because a river is below it.
 if(s.riverBankWater>31&&std::isfinite(s.riverBankEdge)&&s.height>=s.riverBankWater){
  double width=10+14*clamp(s.mainRiver,0,1);
  s.oasis=(1-smooth(2,width,s.riverBankEdge))*(1-smooth(2,7,s.height-s.riverBankWater))*s.desert;
 }
 // Lake rims use actual cached base columns, only for arid dry land and
 // coarse candidates. No search or random water is added to the world.
 if(s.oasis<.1&&s.desert>.18){
  for(auto [dx,dz]:{std::pair{8,0},{-8,0},{0,8},{0,-8}}){auto b=lakeAt(x+dx,z+dz);if(b&&b->flooded&&s.height>=b->level&&s.height-b->level<=3)s.oasis=std::max(s.oasis,s.desert*(1-smooth(1,4,s.height-b->level)));}
 }
 double dry=smooth(.18,.70,s.desert)*(1-s.oasis);
 if(largeDesertOnly)dry=smooth(.04,.65,s.desert)*(1-s.oasis);
 if(foothillSandCover){
  // Climate is not sediment supply. Keep the large lowland desert but let
  // thin sand sheets yield to the original foothill soil. Existing cached
  // dune detail varies the broad transition without extra terrain samples.
  // No height clamp, mountain excavation, or change to dunes/water routing.
  s.sandCover=1-smooth(94+6*s.desertPatch,132+10*s.desertPatch,s.preRiverHeight);
  dry*=s.sandCover;
 }
 s.treeD*=1-dry;s.soilSand=mix(dry,s.soilSand,largeDesertOnly?1.:.72+.28*s.desertSand);s.surf=7;
 s.rockCap*=1-smooth(.25,.6,s.desert)*s.desertSand*s.sandCover;
 if(s.desert>.4&&(!foothillSandCover||dry>.5)){s.biome=11;s.treeK=5;s.treeD=0;}
 if(s.oasis>.20){s.soilSand*=1-smooth(.2,.6,s.oasis);s.surf=7;s.biome=12;s.treeK=3;s.treeD=.003*s.oasis;}
}
int desertMaterial(Surface const&s,int x,int y,int z)const{
 if(s.desert<=.4||s.oasis>.20||s.surf==5||s.surf==6||s.surf==8||s.lake||s.river||s.riverDeposit||s.fluvialType||s.barren>0||s.stone>.01||s.scarp>.01)return -1;
 if(largeDesertOnly){
  // One sandy interior. No patch threshold, rock/gravel bands or hashed
  // column replacement; the broad outer soil transition remains in surf=7.
  if(s.soilSand<.96)return -1;
  return s.height-y<3&&s.cliffLow==-32768&&s.riverCutLow==-32768?5:6;
 }
 // Sparse stable gravel/rock domains, not scattered rocks laid on the sand.
 // Only sand caps fall; three supported layers terminate on sandstone.
 double blend=smooth(.4,.72,s.desert);
 if(hash01(x,z,terrainSeed^19039)>blend)return -1;
 int depth=s.height-y;
 if(s.desertSand>.50)return depth<3&&s.cliffLow==-32768&&s.riverCutLow==-32768?5:6;
 if(s.desertPatch<-.30)return depth==0?2:6;
 return depth==0?(s.desertPatch<-.12?42:33):6;
}
