// Seed-driven, non-overlapping volcanic regions. No configured center coordinates.
struct VolcanoCone {double x,z,radius,crater,height,depth,vent;};
struct VolcanoRegion {bool valid=false,mountain=false;VolcanoCone cone{};double inner=0,outer=0,base=0;std::array<float,16> terrain{};};
bool revisedVolcano=C.value("volcanoRevision",0)>=1;
bool integratedVolcano=terrainRevision(data)>=182;
bool volcanoEnabled=false;double volcanoGrid=4096,volcanoChance=.7,volcanoMin=425,volcanoMax=505;
uint32_t volcanicSeed=terrainSeed;
Perlin volcanicNoise{seedHash(terrainSeed,1201)};
Cache<uint64_t,VolcanoRegion> volcanoRegions{128};
void loadVolcano(){
 if(!C.contains("volcano")||!C.at("volcano").value("enabled",false))return;
 auto const&q=C.at("volcano");volcanoEnabled=true;volcanicSeed=C.at("seed").get<uint32_t>();
 volcanicNoise=Perlin(seedHash(volcanicSeed,1201));
 volcanoGrid=num(q,"regionSize");volcanoChance=num(q,"chance");volcanoMin=num(q,"outerRadiusMin");volcanoMax=num(q,"outerRadiusMax");
 if(volcanoGrid<3200||volcanoGrid>8192||volcanoMin<360||volcanoMax<volcanoMin||volcanoMax+258>=volcanoGrid*.25||volcanoChance<0||volcanoChance>1)
  throw std::runtime_error("Invalid volcanic region distribution");
}
VolcanoRegion volcanoRegion(int gx,int gz){return volcanoRegions.get(key2(gx,gz),[&]{
 VolcanoRegion v;auto rnd=[&](int salt){return hash01(gx,gz,volcanicSeed^salt);};
 if(rnd(1203)>=volcanoChance)return v;
 double x=std::floor((gx+.25+.5*rnd(1211))*volcanoGrid),z=std::floor((gz+.25+.5*rnd(1213))*volcanoGrid);
 v.outer=volcanoMin+(volcanoMax-volcanoMin)*rnd(1217);v.inner=v.outer-100;
 // Protect the existing stone forest and giant forest, including their borders.
 auto const&sf=C.at("stoneForest");auto const&gf=C.at("giantForest");
 if(largeStoneForestEnabled()&&std::hypot(x-num(sf,"centerX"),z-num(sf,"centerZ"))<num(sf,"outerRadius")+num(sf,"edgeWarp")+v.outer+258)return v;
 if(nearSmallStoneForest(x,z,v.outer+120))return v;
 if(std::hypot(x-num(gf,"centerX"),z-num(gf,"centerZ"))<num(gf,"outerRadius")+v.outer+258)return v;
 v.base=clamp(shaped(x+32768,z+32768,vcont(x+32768,z+32768),false),55,85);
 v.cone={x,z,185+25*rnd(1219),58+16*rnd(1223),160+30*rnd(1229),48+8*rnd(1231),10+3*rnd(1237)};
 if(integratedVolcano){
  // A cached regional foundation, evaluated before volcanic shaping. Never
  // call base()/surface() here: they would recurse into this region.
  double sum=0,peak=-64;
  for(int i=0;i<16;++i){double a=i*6.283185307179586/16;
   double px=x+std::cos(a)*v.outer*.62+32768,pz=z+std::sin(a)*v.outer*.62+32768;
   double h=shaped(px,pz,vcont(px,pz));if(elevatedLandscape)h+=raisedLandOffset(h);v.terrain[i]=float(h);sum+=h;peak=std::max(peak,h);
  }
  double center=shaped(x+32768,z+32768,vcont(x+32768,z+32768));
  if(elevatedLandscape)center+=raisedLandOffset(center);
  v.base=clamp(std::max(.4*center+.6*sum/16,peak-100),revisedVolcano?double(seaLevelValue+8):35.,(revisedVolcano?410.:292.)-v.cone.height);
 }
 v.mountain=rnd(1249)<.50;v.valid=true;return v;
});}
double volcanicDistance(VolcanoRegion const&v,double x,double z){
 double r=std::hypot(x-v.cone.x,z-v.cone.z);
 return r+smooth(145.,260.,r)*(56*volcanicNoise.at(x/150.,z/150.)+22*volcanicNoise.at(x/57.+19,z/57.-31));
}
double craterRadius(VolcanoCone const&a,double angle){
 double u=std::cos(angle),v=std::sin(angle),phase=a.x/177.+a.z/311.;
 return a.crater*(1+.10*std::cos(2*angle+phase)
  +.22*volcanicNoise.at(u*2.1+phase,v*2.1-9)
  +.09*volcanicNoise.at(u*5.3-41,v*5.3+phase));
}
J volcanoInventory(int min,int max){
 J rows=J::array();if(!volcanoEnabled)return rows;
 for(int z=int(std::floor(min/volcanoGrid));z<=int(std::floor(max/volcanoGrid));z++)for(int x=int(std::floor(min/volcanoGrid));x<=int(std::floor(max/volcanoGrid));x++){
  auto v=volcanoRegion(x,z);if(!v.valid)continue;auto a=v.cone;
  rows.push_back({{"x",a.x},{"z",a.z},{"outer",v.outer},{"base",v.base},{"height",a.height},{"crater",a.crater},{"depth",a.depth},{"vent",a.vent},{"rockType",v.mountain?"mountain":"basalt"}});
 }return rows;
}
void volcanicRelief(Surface&s,double&height,int wx,int wz){
 if(!volcanoEnabled)return;
 double x=wx-32768,z=wz-32768;
 auto region=volcanoRegion(int(std::floor(x/volcanoGrid)),int(std::floor(z/volcanoGrid)));
 if(!region.valid)return;
 double volcanoOuter=region.outer,volcanoInner=region.inner,volcanoBase=region.base;
 if(std::hypot(x-region.cone.x,z-region.cone.z)>=volcanoOuter+258)return;
 double d=volcanicDistance(region,x,z);
 s.volcanicMountainRock=region.mountain;
 s.barren=revisedVolcano?0:1-smooth(volcanoOuter+48,volcanoOuter+180,d);
 if(d>=volcanoOuter)return;
 s.volcanic=1-smooth(volcanoInner,volcanoOuter,d);
 double rough=6*volcanicNoise.at(x/57,z/57)+2*volcanicNoise.at(x/17,z/17);
 double target=height;int lava=-32768;bool vent=false;
 {auto const&a=region.cone;
  double rx=x-a.x,rz=z-a.z,r=std::hypot(rx,rz);
  double angle=std::atan2(rz,rx),ux=std::cos(angle),uz=std::sin(angle);
  double crater=craterRadius(a,angle);
  if(revisedVolcano){
   s.volcanoDeep=float(-4+4*volcanicNoise.at(x/150.,z/150.));s.volcanoPatch=float(clamp(.5+1.2*volcanicNoise.at(x/39.,z/39.),.01,.99));s.volcanoRock=float(volcanicNoise.at(x/61.,z/61.));
   s.volcanoDx=float(rx);s.volcanoDz=float(rz);s.volcanoVent=float(a.vent);s.volcanoCrater=float(crater);s.volcanoRadius=float(volcanoOuter);
   s.volcanoHeat=float(volcanicDiamondBonus(r,volcanoOuter));s.volcanoLavaTop=int(std::floor(volcanoBase+a.height-a.depth+6));
   // Sparse dry thermal chambers, outside the sealed central magma reservoir.
   int gx=int(std::floor(x/96.)),gz=int(std::floor(z/96.));
   auto random=[&](int k){return hash01(gx,gz,volcanicSeed^uint32_t(12501+k));};
   double X=gx*96.+24+48*random(1),Z=gz*96.+24+48*random(2);
   double rxC=18+12*random(3),rzC=15+10*random(4),q=std::pow((x-X)/rxC,2)+std::pow((z-Z)/rzC,2);
   if(random(0)<.36&&q<1&&r>crater+48&&d<volcanoInner){
    double Y=-12+random(5)*std::max(12.,std::min(172.,volcanoBase-30.)),span=(7+6*random(6))*std::sqrt(1-q);
    int lo=int(std::ceil(Y-span)),hi=int(std::floor(Y+span));
    if(lo>-54&&hi<height-16&&hi-lo>=3){s.thermalCaveLow=lo;s.thermalCaveHigh=hi;}
   }
  }
  s.volcanicHot=1-smooth(crater+6,crater+35,r);
  double direction=volcanicNoise.at(ux*2.7+a.x/137,uz*2.7+a.z/137);
  double radius=a.radius*(1+.085*std::sin(angle*3+a.x*.01)+.07*direction);
  double rim=std::max(28-a.depth,7*std::sin(angle*3+a.z*.013)+5*direction);
  double h;
  if(r<crater){
   h=a.height-a.depth+a.depth*smooth(crater*.45,crater,r);
   h+=rim*smooth(crater*.72,crater,r);
  }else if(integratedVolcano){
   // Directional runout responds to the actual surrounding ridges. Low
   // valleys retain shorter aprons; high shoulders carry longer spurs.
   double az=(angle+3.141592653589793)*16/6.283185307179586;
   int index=int(std::floor(az));double f=az-index;
   double regional=mix(smooth(0,1,f),region.terrain[(index+8)&15],region.terrain[(index+9)&15]);
   double outer=volcanoOuter*(.88+.10*std::tanh((regional-volcanoBase)/45.));
   double t=clamp((d-crater)/(outer-crater),0,1),fall=1-t;
   double profile=fall*fall*fall;
   double inheritance=smooth(crater+6,crater+(outer-crater)*.48,r);
   double channel=std::pow(.5+.5*std::sin(angle*9+2.8*direction),7);
   double incision=16*channel*smooth(crater,crater+24,r)*profile;
   // Background height participates throughout the flank, rather than only
   // at the edge of a fixed cone. Its own valleys and ridges survive.
   double crest=volcanoBase+(a.height+rim)*profile
       +(height-volcanoBase)*inheritance
       +(rough*.55-incision)*smooth(crater,crater+18,r)*fall*fall;
   double join=smooth(crater+12,crater+55,r);
   double excess=crest-height;
   // Positive smooth union: do not dig a circular moat into existing hills.
   double raised=excess<=0?0:excess<8?excess*excess*(16-excess)/64:excess;
   h=mix(join,crest,height+raised)-volcanoBase;
  }else{
   // The same warped boundary drives the bowl, rim and outer slope.
   double t=(r-crater)/(radius-crater);
   double profile=std::pow(std::max(0.,1-t),1.18);
   h=(a.height+rim)*profile;
   // Radial channels persist downhill; warped angles prevent evenly spaced flutes.
   double channel=std::pow(.5+.5*std::sin(angle*9+2.8*direction),7);
   double erosion=smooth(crater+3,crater+22,r)*(1-smooth(.58,.96,t));
   h-=24*channel*erosion;
   // Match the surrounding base roughness at the apron edge without a height step.
   h+=rough*smooth(crater,crater+18,r);
   double join=crater+.30*(radius-crater);
   if(r>join){
    // The apron reaches the region edge, where both its contribution and
    // derivative vanish. Background relief remains visible toward the edge.
    double u=std::clamp((d-join)/(volcanoOuter-join),0.,1.);
    double anchor=volcanoBase+(a.height+rim)*std::pow(.70,1.18)+rough-24*channel;
    double delta=anchor-height,length=volcanoOuter-join;
    double slope=-1.18*(a.height+rim)*std::pow(.70,.18)/(radius-crater);
    slope=delta>0?std::max(slope,-3*delta/length):0.;
    double u2=u*u,u3=u2*u;
    h=height+delta*(2*u3-3*u2+1)+slope*length*(u3-2*u2+u)-volcanoBase;
   }
  }
  h+=volcanoBase;
  if(integratedVolcano)s.volcanicDeposit=float(std::max(1-smooth(crater+8,crater+60,r),smooth(.5,24.,h-height)));
  target=h;lava=r<crater?int(std::floor(volcanoBase+a.height-a.depth+6)):-32768;vent=r<a.vent;
 }
 if(revisedVolcano){
  // Blend mineral cover on the actual apron; remove the separate barren halo.
  s.volcanic*=s.volcanicDeposit;
  s.barren=s.volcanic*.85;
 }
 height=target;
 if(s.volcanic>.99){s.lava=lava;s.vent=vent;s.cliffLow=s.cliffHigh=-32768;}
}
// Called after bedrock and before generic caves, water or vegetation material.
int volcanicMaterial(Surface const&s,int x,int y,int z){
 if(s.volcanic<=0)return -1;
 if(revisedVolcano){
  if(y>s.height){if(s.lava>=y)return 43;return -1;}
  double radius=volcanicPlumbingRadius(y,s.volcanoLavaTop,s.volcanoVent,s.volcanoCrater);
  double bend=6*std::sin(y/61.)*smooth(-54.,-10.,double(y));
  if(radius>0&&std::hypot(s.volcanoDx-bend,s.volcanoDz-bend*.55)<radius)return 43;
  if(y>=s.thermalCaveLow&&y<=s.thermalCaveHigh)return 0;
  if(y<s.height-5){
   if(y<s.volcanoDeep)return 95;
   return 2;
  }
  double cover=s.volcanic;
  double patch=s.volcanoPatch;
  if(cover<patch)return -1;
  double broad=s.volcanoRock;
  return s.volcanoHeat>.8?(broad>.1?46:45):(broad>.18?98:broad<-.18?45:2);
 }
 if(s.lava>-32768&&y<=s.lava&&(s.vent||y>s.height))return 43;
 if(y>s.height)return -1;
 double pick=hash01(x,z,volcanicSeed^1207^((y+64)*7919));
 if(y<-30&&s.volcanic>.65){
  // Solid deep rocks also isolate lava shafts from the generic cave aquifer.
  return pick<.55?44:pick<.78?45:pick<.96?46:47;
 }
 if(integratedVolcano&&y>=s.height-4){
  double cover=s.volcanic*s.volcanicDeposit;
  double patch=clamp(.5+1.4*volcanicNoise.at((x-32768)/39.,(z-32768)/39.),.01,.99);
  if(cover<patch)return s.volcanicMountainRock?2:(pick<.80?2:42);
 }
 if(s.volcanic>.65){
  if(y<s.height-4)return 2; // Closed solid cones; no accidental holes or water contacts.
  if(s.volcanicMountainRock){
   double hot=s.volcanicHot;
   if(pick<hot*.65)return pick<hot*.40?45:46;
   return pick<.82?2:pick<.94?20:42;
  }
  return pick<.52?45:pick<.86?46:42;
 }
 if(y>=s.height-3&&pick<s.volcanic)return s.volcanicMountainRock?(pick<s.volcanic*.85?2:42):(pick<s.volcanic*.6?45:46);
 return -1;
}
