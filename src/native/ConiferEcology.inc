// Ordinary conifer ecology. World anchored descriptors; no chunk-local RNG.
struct Conifer {int x=0,z=0,h=0,ground=0,shape=0;double radius=0,phase=0,priority=0;};
Cache<uint64_t,Conifer> coniferCache{2048};
bool coniferHabitat(Surface const&s)const{
 double temperature=field.ecologyTemperature(s),height=field.ecologyHeight(s);
 bool climate=s.biome==6||((s.biome==9||s.biome==14)&&temperature>=.22&&temperature<.42&&s.H>.35);
 return climate&&s.giant<.02&&s.stone<.01&&s.barren<.01&&!s.kind&&!s.lake&&s.cliffLow==-32768&&s.height>38&&height<240&&field.ecologyTreeWeight(s)>0;
}
double coniferPatch(int x,int z)const{return clamp(.5+.8*forestNoise.at(x/91.,z/91.)+.25*understoryNoise.at(x/29.,z/29.),0,1);}
Conifer coniferCandidate(int gx,int gz){return coniferCache.get(key2(gx,gz),[&]{
 Conifer a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 a.x=gx*8+1+int(r(1901)*6);a.z=gz*8+1+int(r(1902)*6);a.priority=r(1903);
 auto s=field.surface(a.x,a.z,false);
 if(!coniferHabitat(s)||r(1904)>.10+.85*coniferPatch(a.x,a.z)||blocked(a.x,a.z,false))return a;
 double interior=forestInterior(a.x,a.z,1);
 if(r(1904)>(.10+.85*coniferPatch(a.x,a.z))*(.5+.5*interior)*iceVegetation(s)*field.ecologyTreeWeight(s))return a;
 if(!treeGroundAllowed(s,a.x,a.z,false))return a;
 int lo=s.height,hi=s.height;
 for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
  auto b=field.surface(a.x+dx,a.z+dz,false);
  if(b.water>b.height||b.lava>b.height||b.cliffLow!=-32768||b.stone>.01||b.barren>.01||!treeGroundAllowed(b,a.x+dx,a.z+dz,false))return a;
  lo=std::min(lo,b.height);hi=std::max(hi,b.height);
 }
 for(auto const&g:giants(a.x,a.z,1))if(std::hypot(a.x-g->x,a.z-g->z)<g->radius+6)return a;
 if(hi-lo>5)return a;
 a.ground=s.height;a.h=11+int(r(1905)*12*(.55+.45*interior));a.radius=2.4+r(1906)*1.8;a.phase=r(1907)*6.283185307179586;a.shape=int(r(1908)*3);return a;
});}
std::vector<Conifer> conifers(int x0,int z0,int size){
 std::vector<Conifer> out;
 for(int gz=int(std::floor((z0-12)/8.));gz<=int(std::floor((z0+size+12)/8.));gz++)for(int gx=int(std::floor((x0-12)/8.));gx<=int(std::floor((x0+size+12)/8.));gx++){
  auto a=coniferCandidate(gx,gz);if(!a.h)continue;bool reject=false;
  for(int dz=-1;dz<=1&&!reject;dz++)for(int dx=-1;dx<=1;dx++){
   if(!dx&&!dz)continue;auto b=coniferCandidate(gx+dx,gz+dz);
   if(b.h&&std::hypot(a.x-b.x,a.z-b.z)<5.5&&(b.priority<a.priority||(b.priority==a.priority&&std::pair{b.x,b.z}<std::pair{a.x,a.z}))){reject=true;break;}
  }
  if(!reject)out.push_back(a);
 }
 return out;
}
void coniferEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,std::vector<Structure>const&ss,std::vector<SmallTree>const&other){
 bool any=false;for(auto const&s:surfaces)any|=coniferHabitat(s);
 // A tree just outside the biome/chunk may still have a crown reaching in.
 auto trees=conifers(x0,z0,16);if(!any&&trees.empty())return;
 auto soil=[](int id){return id==3||id==4||id==32||id==33;};
 auto put=[&](int x,int y,int z,int id,bool wood=false){
  int X=x-x0,Z=z-z0;if(X<0||X>=16||Z<0||Z>=16||y<-60||y>Height-65)return;
  auto const&s=surfaces[Z*16+X];if(y<=s.height||s.water>=y||s.lava>=y)return;
  auto&b=raw[(Z*16+X)*Height+y+64];
  if(b==0||(wood&&b==13))b=uint8_t(id);
 };
 auto nearTrunk=[&](int x,int z,double extra){
  for(auto const&a:trees)if(std::hypot(x-a.x,z-a.z)<2+extra)return true;
  for(auto const&a:other)if(std::hypot(x-a.x,z-a.z)<2+extra)return true;
  for(auto const&a:ss)if(std::abs(x-a.x)<48+extra&&std::abs(z-a.z)<48+extra)return true;
  return false;
 };
 for(auto const&a:trees){
  if(nativeReserved(a.x,a.z)||!forestTreeKept(a.x,a.z))continue;
  bool collision=false;for(auto const&b:other)if(std::hypot(a.x-b.x,a.z-b.z)<5){collision=true;break;}if(collision)continue;
  // One continuous crown envelope. Shallow branch whorls never disconnect
  // the upper foliage from the lower crown; the leader ends inside the leaves.
  int bottom=int(std::floor(a.h*(a.shape==0?.30:a.shape==1?.39:.34)));
  int top=a.h+1,woodTop=a.h-3;
  for(int y=std::max(2,bottom-3);y<=top;y++){
   double t=clamp(double(y-bottom)/(top-bottom),0,1);
   double taper=std::pow(1-t,a.shape==0?.88:a.shape==1?1.08:.96);
   double whorl=.91+.09*std::cos(std::max(0,y-bottom)*2.05+a.phase);
   double radius=(a.radius+.55)*taper*whorl;
   // The narrow leader is foliage only. Keep the final wooden levels covered.
   if(y<=woodTop)radius=std::max(radius,1.25);
   double drift=a.shape==2?.35*std::sin(t*3+a.phase)*(1-t):0;
   int n=int(std::ceil(radius*1.18));
   // Crown decisions are column-local. Clip before trigonometry, retaining the
   // complete anchor/footprint checks above for chunk-border determinism.
   for(int dz=std::max(-n,z0-a.z);dz<=std::min(n,z0+15-a.z);dz++)
   for(int dx=std::max(-n,x0-a.x);dx<=std::min(n,x0+15-a.x);dx++){
    double angle=std::atan2(double(dz),double(dx));
    double edge=radius*(1+.09*std::sin(3*angle+a.phase)+.05*std::sin(5*angle-a.phase+y*.3));
    // Branch-shaped underside: inner foliage starts higher; outward
    // tufts droop by different amounts around each tree, not a flat disk.
    double radial=clamp(std::hypot(double(dx),double(dz))/(a.radius+.55),0,1);
    double lobe=.5+.5*std::sin(3*angle+a.phase);
    int underside=std::max(2,int(std::floor(bottom+1-3.2*radial*(.25+.75*lobe)+.65*std::sin(5*angle-a.phase))));
    if(y>=underside&&(dx-drift)*(dx-drift)+dz*dz<=edge*edge+.12)put(a.x+dx,a.ground+y,a.z+dz,13);
   }
   if(y>=bottom)put(a.x,a.ground+y,a.z,13);
  }
  for(int y=1;y<=woodTop;y++)put(a.x,a.ground+y,a.z,y<=2?61:12,true);
  for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto s=field.surface(a.x+dx,a.z+dz,false);if(s.height<=a.ground&&a.ground-s.height<=2)put(a.x+dx,s.height+1,a.z+dz,61,true);}
 }
 // Small features claim a separate jittered cell. Complete footprints are
 // validated before clipping, so adjacent chunks make identical decisions.
 for(int gz=int(std::floor((z0-10)/16.));gz<=int(std::floor((z0+25)/16.));gz++)for(int gx=int(std::floor((x0-10)/16.));gx<=int(std::floor((x0+25)/16.));gx++){
  auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};int x=gx*16+2+int(r(1921)*12),z=gz*16+2+int(r(1922)*12);
  auto s=field.surface(x,z,false);if(!glacierPlantKept(s,x,z)||!coniferHabitat(s)||r(1923)>(.18+.5*coniferPatch(x,z))*iceVegetation(s))continue;
  // Blockers are queried around the feature anchor, never around the output
  // chunk. Both halves of a log crossing a seam see the same neighbours.
  auto blockers=conifers(x-2,z-2,12);auto structuresHere=structures(x-2,z-2,12);auto others=smallTrees(x-2,z-2,12,structuresHere);
  auto featureBlocked=[&](int X,int Z){
   if(nativeReserved(X,Z))return true;
   for(auto const&a:blockers)if(std::hypot(X-a.x,Z-a.z)<3)return true;
   for(auto const&a:others)if(std::hypot(X-a.x,Z-a.z)<3)return true;
   for(auto const&a:structuresHere)if(std::abs(X-a.x)<49&&std::abs(Z-a.z)<49)return true;
   return false;
  };
  int kind=int(r(1924)*3);bool ok=true;
  if(kind==0){
   int axis=r(1925)<.5?0:1,len=4+int(r(1926)*5),lo=1000,hi=-1000;
   for(int i=0;i<len;i++){int X=x+(axis==0?i:0),Z=z+(axis==1?i:0);auto b=field.surface(X,Z,false);
    if(!coniferHabitat(b)||featureBlocked(X,Z)||!soil(field.material(b,X,b.height,Z))){ok=false;break;}lo=std::min(lo,b.height);hi=std::max(hi,b.height);
   }
   // A fallen trunk is one log thick. Filling a lower slope with more wood
   // made two stacked trunks; reject unsupported sites instead.
   if(!ok||hi!=lo)continue;
   for(int i=0;i<len;i++)put(x+(axis==0?i:0),hi+1,z+(axis==1?i:0),61,true);
  }else{
   int n=kind==1?2:1;std::vector<std::array<int,3>> footprint;int lo=1000,hi=-1000;
   for(int dz=-n;dz<=n;dz++)for(int dx=-n;dx<=n;dx++)if(dx*dx+dz*dz<=n*n){int X=x+dx,Z=z+dz;auto b=field.surface(X,Z,false);
    if(!coniferHabitat(b)||featureBlocked(X,Z)||!soil(field.material(b,X,b.height,Z))){ok=false;break;}lo=std::min(lo,b.height);hi=std::max(hi,b.height);footprint.push_back({X,b.height,Z});
   }
   if(!ok||hi-lo>2)continue;
   for(auto a:footprint){put(a[0],a[1]+1,a[2],kind==1?13:20);if(kind==1&&(a[0]-x)*(a[0]-x)+(a[2]-z)*(a[2]-z)<=2)put(a[0],a[1]+2,a[2],13);}
  }
 }
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(!glacierPlantKept(s,x0+x,z0+z))continue;if(!coniferHabitat(s)||s.rock||nativeReserved(x0+x,z0+z))continue;
  auto col=raw.data()+(z*16+x)*Height;int h=s.height;
  // Continuous litter patches, leaving grass in the open parts of the stand.
  double litter=coniferPatch(x0+x,z0+z);
  if(soil(col[h+64])&&litter>.47&&hash01(x0+x,z0+z,terrainSeed^1934)<iceVegetation(s))col[h+64]=litter>.56?32:33;
  if(nearTrunk(x0+x,z0+z,0)||!soil(col[h+64])||col[h+65])continue;
  double patch=coniferPatch(x0+x,z0+z),pick=hash01(x0+x,z0+z,terrainSeed^1931);if(pick>(.08+.36*patch)*iceVegetation(s))continue;
  if(hash01(x0+x,z0+z,terrainSeed^1932)<.22&&col[h+66]==0){col[h+65]=40;col[h+66]=41;}
  else col[h+65]=hash01(x0+x,z0+z,terrainSeed^1933)<.75?37:36;
 }
}
