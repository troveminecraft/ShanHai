// Seed-driven, non-overlapping volcanic regions. No configured center coordinates.
struct VolcanoCone {double x,z,radius,crater,height,depth,vent;};
struct VolcanoRegion {bool valid=false,mountain=false;VolcanoCone cone{};double inner=0,outer=0,base=0,lavaTop=-32768;int sizeClass=0;std::array<float,16> terrain{};magma::Layers layers{};};
bool irregularMagma=C.value("volcanoRevision",0)==4;
bool continuousMagma=C.value("volcanoRevision",0)==3;
bool volcanicGeology=irregularMagma||continuousMagma||C.value("volcanoRevision",0)==2;
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
 if(continuousMagma)v.cone.crater*=1.35;
 if(irregularMagma){
  double size=rnd(1259),shape=rnd(1223);
  v.sizeClass=size<.30?0:size<.95?1:2;
  v.cone.crater=v.sizeClass==0?18+10*shape:v.sizeClass==1?28+14*shape:45+13*shape;
 }
 if(integratedVolcano){
  // A cached regional foundation, evaluated before volcanic shaping. Never
  // call base()/surface() here: they would recurse into this region.
  double sum=0,peak=-64;
  for(int i=0;i<16;++i){double a=i*6.283185307179586/16;
   double px=x+std::cos(a)*v.outer*.62+32768,pz=z+std::sin(a)*v.outer*.62+32768;
   double h=shaped(px,pz,vcont(px,pz));v.terrain[i]=float(h);sum+=h;peak=std::max(peak,h);
  }
  double center=shaped(x+32768,z+32768,vcont(x+32768,z+32768));
  v.base=clamp(std::max(.4*center+.6*sum/16,peak-100),volcanicGeology?seaLevelValue+16.:35.,(volcanicGeology?470.:292.)-v.cone.height);
 }
 if(irregularMagma){
  double lowest=100;
  for(int i=0;i<128;++i)lowest=std::min(lowest,irregularRim(v.cone,i*6.283185307179586/128));
  // A single horizontal lava level follows the lowest lip, not an unrelated
  // deep bowl parameter. Extra clearance covers integer sampling at the rim.
  v.lavaTop=std::floor(v.base+v.cone.height+lowest-7-3*rnd(1277));
  v.layers=magma::layers(v.cone.x/177.+v.cone.z/311.);
 }
 v.mountain=rnd(1249)<.50;v.valid=true;return v;
});}
double irregularRim(VolcanoCone const&a,double angle){
 double phase=a.x/177.+a.z/311.;
 return 4*std::sin(3*angle+phase)+2.5*std::sin(5*angle-phase*.7)
  +2*volcanicNoise.at(std::cos(angle)*2.7+a.x/137,std::sin(angle)*2.7+a.z/137);
}
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
  if(irregularMagma){rows.back()["sizeClass"]=v.sizeClass;rows.back()["lavaTop"]=v.lavaTop;}
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
 s.barren=volcanicGeology?0:1-smooth(volcanoOuter+48,volcanoOuter+180,d);
 if(d>=volcanoOuter)return;
 s.volcanic=1-smooth(volcanoInner,volcanoOuter,d);
 double rough=6*volcanicNoise.at(x/57,z/57)+2*volcanicNoise.at(x/17,z/17);
 double target=height;int lava=-32768;bool vent=false;
 {auto const&a=region.cone;
  double rx=x-a.x,rz=z-a.z,r=std::hypot(rx,rz);
  double angle=std::atan2(rz,rx),ux=std::cos(angle),uz=std::sin(angle);
  double crater=craterRadius(a,angle);
  s.volcanicHot=1-smooth(crater+6,crater+35,r);
  double direction=volcanicNoise.at(ux*2.7+a.x/137,uz*2.7+a.z/137);
  double radius=a.radius*(1+.085*std::sin(angle*3+a.x*.01)+.07*direction);
  double rim=std::max(28-a.depth,7*std::sin(angle*3+a.z*.013)+5*direction);
  if(irregularMagma)rim=irregularRim(a,angle);
  double h;
  if(r<crater){
   if(irregularMagma){
    double floor=region.lavaTop-volcanoBase-1;
    double lipStart=.72+.055*std::sin(angle*4+a.x*.017)+.035*direction;
    h=floor+(a.height+rim-floor)*smooth(crater*lipStart,crater,r);
   }else if(continuousMagma){
    // Broad lava mouth with a short steep inner rim, connected to the root below.
    double floor=a.height-a.depth+5;
    h=floor+(a.height+rim-floor)*smooth(crater*.86,crater,r);
   }else{
    h=a.height-a.depth+a.depth*smooth(crater*.45,crater,r);
    h+=rim*smooth(crater*.72,crater,r);
   }
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
  if(volcanicGeology){
   s.volcanoDx=float(rx);s.volcanoDz=float(rz);s.volcanoBase=float(volcanoBase);
   s.volcanoLava=float(std::floor(volcanoBase+a.height-a.depth+6));s.volcanoVent=float(a.vent);
   s.volcanoPhase=float(a.x/177.+a.z/311.);s.volcanoGrain=float(volcanicNoise.at(x/61.,z/61.));
   s.volcanoHeat=float(1-smooth(20.,volcanoOuter*.70,r));
   if(irregularMagma){
    s.volcanoLava=float(region.lavaTop);
    s.volcanoMagmaTop=r>=crater?float(std::min(region.lavaTop,h-8)):float(region.lavaTop);
    // The center wanders with depth, and independently phased lobes expand
    // into the surrounding rock. All levels retain an overlapping hot core.
    double phase=s.volcanoPhase;
    double mouth=crater*.80;
    double root=volcanoOuter*.57*(1+.10*std::sin(2*angle+phase)+.07*std::cos(3*angle-phase*.7));
    double sin2=std::sin(2*angle+phase),cos2=std::cos(2*angle+phase);
    double sin5=std::sin(5*angle-phase*.6),cos5=std::cos(5*angle-phase*.6);
    for(int i=0;i<magma::Knots;++i){
     double t=double(i)/(magma::Knots-1);auto const&layer=region.layers[i];
     double px=rx-layer.x,pz=rz-layer.z,dist=std::hypot(px,pz);
     double radius=mouth+(root-mouth)*layer.growth;
     double lobes=sin2*layer.cos5+cos2*layer.sin5+.55*(cos5*layer.cos7+sin5*layer.sin7);
     radius+=layer.amplitude*lobes;
     radius+=9*t*volcanicNoise.at(px/37.+phase+t*3,pz/37.-t*5);
     s.volcanoMargins[i]=float(std::max(12.,radius)-dist);
    }
   }
   if(continuousMagma){
    double root=volcanoOuter*.72*(.96+.04*std::cos(3*angle+s.volcanoPhase));
    s.volcanoMagmaTop=magma::columnTop(r,crater*.86,root,s.volcanoLava);
    // The expanding root stays enclosed even beneath a low valley or coastline.
    if(r>=crater)s.volcanoMagmaTop=std::min(s.volcanoMagmaTop,float(h-8));
   }
   s.barren=s.volcanic*s.volcanicDeposit;
  }
  target=h;lava=r<crater?int(irregularMagma?region.lavaTop:std::floor(volcanoBase+a.height-a.depth+6)):-32768;vent=r<a.vent;
 }
 height=target;
 if(s.volcanic>.99){s.lava=lava;s.vent=vent;s.cliffLow=s.cliffHigh=-32768;}
}
// Called after bedrock and before generic caves, water or vegetation material.
int volcanicMaterial(Surface const&s,int x,int y,int z){
 if(s.volcanic<=0)return -1;
 if(elevationVariants&&s.lavaSeaContact&&s.volcanoHeat<.25&&y<=s.height&&y>=s.height-2){
  double contact=.5+.65*volcanicNoise.at((x-32768)/19.,(z-32768)/19.);
  if(contact>.50&&(!s.closedBasin)&&!s.lake)return 178;
 }
 if(volcanicGeology){
  // Sealed asymmetric magma chamber and a flaring, bent conduit. Both have
  // finite lower bounds; the old constant-radius shaft no longer reaches bedrock.
  double dx=s.volcanoDx,dz=s.volcanoDz,grain=s.volcanoGrain;
  if(irregularMagma){
   if(magma::contains(s.volcanoMargins,y,s.volcanoLava,s.volcanoMagmaTop))return 43;
  }else if(continuousMagma){
   if(y>=magma::Bottom&&y<=s.volcanoMagmaTop)return 43;
  }else if(y<=s.height&&y>-57){
   double u=clamp((s.volcanoLava-y)/(s.volcanoLava+12.),0.,1.);
   double bend=4*u*(1-u),cx=18*bend*std::sin(s.volcanoPhase),cz=14*bend*std::cos(s.volcanoPhase);
   double radius=s.volcanoVent+(28-s.volcanoVent)*smooth(.25,1.,u);
   if(y>=-22&&y<=s.volcanoLava&&(dx-cx)*(dx-cx)+(dz-cz)*(dz-cz)<radius*radius)return 43;
   double q=(dx-9)*(dx-9)/(68*68)+(dz+7)*(dz+7)/(55*55)+(y+12.)*(y+12.)/(40*40);
   if(y<=s.height-6&&q<1.+.12*grain)return y<-29?43:0;
   // A smaller lateral hot chamber adds traversable rock shelves under the cone.
   double sideY=std::max(22.,double(s.volcanoBase)-50.);
   double side=(dx-72)*(dx-72)/(39*39)+(dz+34)*(dz+34)/(32*32)+(y-sideY)*(y-sideY)/(22*22);
   if(y<=s.height-6&&side<1.+.10*grain)return y<sideY-16?43:0;
  }
  if(s.lava>-32768&&y<=s.lava&&y>s.height)return 43;
  if(y>s.height)return -1;
  double pick=hash01(x,z,volcanicSeed^1207^((y+64)*7919));
  if(y<s.height-4){
   if(y<-4+5*grain)return 95;
   if(std::abs(y-(s.volcanoBase-24+6*grain))<4)return 96;
   return 2;
  }
  double cover=s.volcanic*s.volcanicDeposit;
  double patch=clamp(.5+1.2*grain,.01,.99);
  if(cover<patch)return -1; // Retain the actual surrounding soil and vegetation.
  if(s.volcanicMountainRock)return pick<.74?2:pick<.89?45:42;
  return pick<.52?45:pick<.86?46:42;
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
