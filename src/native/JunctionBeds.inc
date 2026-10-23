// A shared channel floor crosses the old river/receiver boundary. This is
// computed from seed drainage geometry, never from user-reported coordinates.
struct BedReach{double ax,az,bx,bz,ah,bh,width;bool marineFan=false;};
std::vector<BedReach> bedReaches;
std::unordered_map<uint64_t,std::vector<size_t>> bedBins;
J junctionReport=J::array();
void addBedReach(BedReach const&a){
 size_t id=bedReaches.size();bedReaches.push_back(a);double halo=a.width+3;
 for(int z=int(std::floor((std::min(a.az,a.bz)-halo)/64));z<=int(std::floor((std::max(a.az,a.bz)+halo)/64));z++)for(int x=int(std::floor((std::min(a.ax,a.bx)-halo)/64));x<=int(std::floor((std::max(a.ax,a.bx)+halo)/64));x++)bedBins[key2(x,z)].push_back(id);
}
void buildJunctionBeds(){
 // During seed-package construction, lakes/forest sampling precedes routing.
 // A missing graph is valid at that stage; a deployed package is checked by
 // prepare-native/new_world before it can reach the server.
 if(!data.contains("rivers"))return;
 struct Port{double x,z,dx,dz,level,radius;bool lake,outlet;};std::vector<Port> ports;
 for(auto const&r:data.at("rivers").at("routes"))if(r.at("receiver")=="ocean"||r.at("receiver")=="lake"){
  double x=r.at("join").at(0),z=r.at("join").at(1);
  for(auto const&a:riverSegments)if(a.bx==x&&a.bz==z){ports.push_back({x,z,r.at("receiver")=="lake"?double(data.at("naturalLakes").at(int(r.at("sink"))-1).at("center").at(0))-x:a.bx-a.ax,r.at("receiver")=="lake"?double(data.at("naturalLakes").at(int(r.at("sink"))-1).at("center").at(1))-z:a.bz-a.az,a.bw,a.br,r.at("receiver")=="lake",false});break;}
 }
 if(data.at("rivers").contains("outlets"))for(auto const&o:data.at("rivers").at("outlets")){
  auto a=o.at("segments").at(0);ports.push_back({a[0],a[1],double(o.at("lakeCenter").at(0))-double(a[0]),double(o.at("lakeCenter").at(1))-double(a[1]),a[4],a[6],true,true});
 }
 for(auto const&p:ports){
  double depth=clamp(2.+p.radius*.32,3.,4.5);
  if(p.lake){auto center=compute(roundJS(p.x+p.dx)+32768,roundJS(p.z+p.dz)+32768,false);depth=std::min(depth,std::max(2.,p.level-center.height));}
  if(!p.lake)depth-=1.;
  double bed=p.level-depth;
  // Include the approach, gradually matching the new floor before the seam.
  for(auto const&a:riverSegments){
   double da=std::hypot(a.ax-p.x,a.az-p.z),db=std::hypot(a.bx-p.x,a.bz-p.z);
   if(std::min(da,db)>64||std::min(std::abs(a.aw-p.level),std::abs(a.bw-p.level))>8)continue;
   double old=std::max(2.,mix(smooth(1.,4.,std::max(a.ar,a.br)),1.,2.5));
   double oldA=old-(p.lake?0.:1-smooth(seaLevelValue,seaLevelValue+5,a.aw)),oldB=old-(p.lake?0.:1-smooth(seaLevelValue,seaLevelValue+5,a.bw));
   auto lake=lakeAt(roundJS(p.x)+32768,roundJS(p.z)+32768);
   bool naturalBasin=p.lake&&lake&&lake->sealAlcoves;
   if(naturalBasin){
    // Follow the finished water profile on both sides of the real shore.
    // The old graph heights could leave a one-block submerged bank crest.
    int count=std::max(1,int(std::ceil(std::hypot(a.bx-a.ax,a.bz-a.az)/2.)));
    double lastX=a.ax,lastZ=a.az,lastBed=0;
    for(int k=0;k<=count;++k){double t=double(k)/count,x=mix(t,a.ax,a.bx),z=mix(t,a.az,a.bz);
     auto surface=compute(roundJS(x)+32768,roundJS(z)+32768,false);
     riverRelief(surface,roundJS(x)+32768,roundJS(z)+32768,false);
     double water=surface.water>surface.height?surface.water:mix(t,a.aw,a.bw);
     double distance=std::hypot(x-p.x,z-p.z),floor=water-mix(1-smooth(8.,64.,distance),old,depth);
     if(k)addBedReach({lastX,lastZ,x,z,lastBed,floor,std::max(a.ar,a.br)*1.1});
     lastX=x;lastZ=z;lastBed=floor;
    }
   }else addBedReach({a.ax,a.az,a.bx,a.bz,a.aw-mix(1-smooth(8.,64.,da),oldA,depth),a.bw-mix(1-smooth(8.,64.,db),oldB,depth),std::max(a.ar,a.br)*1.1});
  }
  double x=p.x,z=p.z,angle=std::atan2(p.dz,p.dx);int deep=0;bool reached=false;J line=J::array();line.push_back({x,z,bed});
  if(coastalBeds&&!p.lake){
   // A sea-level receiver may be a shallow bay, not a deep ocean basin.
   // Do not chase deep water with a flat, greedy trench across that bay.
   // Feather the existing mouth floor into the natural bed over a short fan.
   // Dry land is never excavated and the fan closes before another shoreline.
   double nx=std::cos(angle),nz=std::sin(angle),length=0;
   Surface end;
   for(int step=1;step<=9;++step){
    auto s=compute(roundJS(x+nx*step*4)+32768,roundJS(z+nz*step*4)+32768,false);
    if(s.stone>.01||s.volcanic>0||s.lake||s.height>=p.level||s.water!=seaLevelValue)break;
    length=step*4;end=s;
   }
   if(length>0){
    addBedReach({x,z,x+nx*length,z+nz*length,bed,bed,p.radius*1.7,true});
    line.push_back({x+nx*length,z+nz*length,end.height});
   }
   junctionReport.push_back({{"type","ocean"},{"level",p.level},{"mode","bounded-natural-bed-fan"},
                            {"reachedDeepBed",length>0&&end.height<=std::floor(bed)},{"line",line}});
   continue;
  }
  if(p.lake&&compute(roundJS(x)+32768,roundJS(z)+32768,false).height<=std::floor(bed))reached=true;
  for(int step=0;step<64&&!reached;step++){
   double best=INFINITY,nx=x,nz=z,na=angle;Surface chosen;
   for(double turn:{0.,-.35,.35,-.7,.7,-1.05,1.05}){
    double a=angle+turn,X=std::round(x+4*std::cos(a)),Z=std::round(z+4*std::sin(a));
    if(std::abs(X)>4094||std::abs(Z)>4094)continue;
    auto s=compute(int(X)+32768,int(Z)+32768,false);
    if(s.stone>.01||s.volcanic>0||s.height>=p.level||(p.lake?s.lake!=1:s.water!=seaLevelValue))continue;
    double score=s.height+1.5*std::abs(turn);
    if(score<best){best=score;nx=X;nz=Z;na=a;chosen=s;}
   }
   if(!std::isfinite(best))break;
   addBedReach({x,z,nx,nz,bed,bed,p.radius*(1.1+.008*step)});
   x=nx;z=nz;angle=na;line.push_back({x,z,bed});
   deep=chosen.height<=std::floor(bed)?deep+1:0;
   if((p.lake&&deep>=1)||(step>=2&&deep>=2)){reached=true;break;}
  }
  junctionReport.push_back({{"type",p.outlet?"outlet":p.lake?"lake":"ocean"},{"level",p.level},{"reachedDeepBed",reached},{"line",line}});
 }
}
void junctionBed(Surface&s,int wx,int wz){
 if(s.stone>.01||s.volcanic>0)return;
 double x=wx-32768,z=wz-32768;auto it=bedBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));if(it==bedBins.end())return;
 double target=s.height;
 for(auto id:it->second){auto const&a=bedReaches[id];double dx=a.bx-a.ax,dz=a.bz-a.az;
  if(a.marineFan&&(s.lake||s.water!=seaLevelValue))continue;
  double t=clamp(((x-a.ax)*dx+(z-a.az)*dz)/std::max(1.,dx*dx+dz*dz),0,1),dist=std::hypot(x-a.ax-t*dx,z-a.az-t*dz);
  if(dist>=a.width+3)continue;
  // Only carve submerged, connected water. Never flood dry ground here.
  if(s.water<=s.height||(!s.river&&!s.lake&&s.water!=seaLevelValue))continue;
  double width=a.marineFan?a.width*(.65+.35*t):a.width;
  if(matchedRiverBanks&&a.marineFan)width*=1+.16*matNoise.at(x/13.+47,z/13.-31);
  double floor=mix(t,a.ah,a.bh),weight=1-smooth(width*.55,width+3,dist);
  if(a.marineFan)weight*=1-smooth(0.,1.,t);
  double cut;
  if(gradedJunctionBeds&&!a.marineFan){
   // Preserve a connected thalweg, not a full-width flat trench. Reuse the
   // actual river footprint (including width noise and overlapping bends).
   // Within a lake, taper the channel extension to its own centreline.
   double section=s.river>0?s.riverSection:1-smooth(0.,width,dist);
   double depth=std::max(0.,double(s.water-1)-floor);
   double shaped=s.water-1-depth*section;
   // Rounding avoids a fractional cut becoming a one-block flat shelf.
   // Never raise the pre-existing bed or close a wet column.
   cut=std::min(double(s.height),double(roundJS(shaped)));
  }else cut=mix(weight,double(s.height),std::min(double(s.height),floor));
  // Floor-rounding an arbitrarily small feather still removes one full
  // block, leaving a flat, sharply bounded fan. Quantize the marine feather
  // to its actual depth; submerged approach channels keep their wet floors.
  if(matchedRiverBanks&&a.marineFan)cut=roundJS(cut);
  target=std::min(target,cut);
 }
 if(target<s.height){s.height=int(std::floor(target+1e-6));s.cliffLow=s.cliffHigh=-32768;s.drip=0;}
}
