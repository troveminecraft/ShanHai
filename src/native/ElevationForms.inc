// New 0.2.0 landforms are cached 2-D regional descriptors. They never query
// generated chunks, and their surveys exclude this pass to avoid recursion.
struct BasinSite {bool valid=false,mangrove=false;double x=0,z=0,rx=0,rz=0,floor=0,rim=0;};
Cache<uint64_t,BasinSite> basinSites{256};
Cache<uint64_t,bool> mangroveBasinHabitats{256};
 int basinGrid()const{return broadBasins?2048:3072;}
 double basinRadius(BasinSite const&r,double x,double z)const{
  double dx=(x-r.x)/r.rx,dz=(z-r.z)/r.rz,q=std::hypot(dx,dz);if(!landformRefresh)return q;
  double a=std::atan2(dz,dx),phase=r.x/193.+r.z/271.;
  return q/(1+.13*std::sin(3*a+phase)+.08*std::sin(5*a-phase)+.06*regionalNoise.at(x/140.,z/140.));
 }
BasinSite buildBasinSite(int gx,int gz){
 BasinSite b;auto rnd=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 if(!elevationVariants||rnd(62001)>(broadBasins?.98:.65))return b;
 b.rx=broadBasins?600+200*rnd(62007):landformRefresh?320+180*rnd(62007):220+160*rnd(62007);b.rz=b.rx*(broadBasins?.85+.15*rnd(62009):.72+.36*rnd(62009));
 b.floor=64+20*rnd(62011);b.rim=seaLevelValue+24+12*rnd(62013);
 // Broad owners cover more neighbouring terrain; cap placement retries so
 // cold glacier/water surveys cannot multiply the old per-owner work.
 for(int attempt=0;attempt<(broadBasins?4:landformRefresh?12:4);++attempt){
 double inset=broadBasins?.24:.30;
 b.x=(gx+inset+(1-2*inset)*rnd(62003+attempt*20))*basinGrid();b.z=(gz+inset+(1-2*inset)*rnd(62005+attempt*20))*basinGrid();
 bool valid=true;
 for(auto const&p:regionalProtected)if(p.radius>=0&&std::hypot(b.x-p.x,b.z-p.z)<p.radius+b.rx*(landformRefresh?1.8:1.)+160)valid=false;
 if(!valid||nearSmallStoneForest(b.x,b.z,b.rx+100))continue;
 // Entire footprint must be inland. A second surrounding ring establishes a
 // drainage divide; no basin floor is used to decide its own eligibility.
 for(int z=-4;z<=4&&valid;++z)for(int x=-4;x<=4&&valid;++x){
  if(x*x+z*z>20)continue;
  double X=b.x+x*b.rx*.35+32768,Z=b.z+z*b.rz*.35+32768;
  double h=shaped(X,Z,vcont(X,Z));if(h<seaLevelValue+(broadBasins?4:12)||(!broadBasins&&h>350))valid=false;
 }
 for(int i=0;i<32&&valid;++i){double a=i*6.283185307179586/32;
  double X=b.x+std::cos(a)*b.rx*(landformRefresh?1.85:1.55)+32768,Z=b.z+std::sin(a)*b.rz*(landformRefresh?1.85:1.55)+32768;
  if(shaped(X,Z,vcont(X,Z))<seaLevelValue+(broadBasins?2:12))valid=false;
 }
 if(!valid)continue;
 int vx=int(std::floor(b.x/volcanoGrid)),vz=int(std::floor(b.z/volcanoGrid));
 for(int z=vz-1;z<=vz+1;++z)for(int x=vx-1;x<=vx+1;++x){auto v=volcanoRegion(x,z);if(v.valid&&std::hypot(b.x-v.cone.x,b.z-v.cone.z)<v.outer+b.rx*(landformRefresh?1.8:1.)+160)valid=false;}
 if(valid){
  b.valid=true;
  // One climate decision for the whole water-filled bowl. Its flat core has
  // a diameter >= 200 blocks even with the strongest irregular boundary warp.
  // Sample raw climate functions, never surface()/base(): those call basinSite.
  if(basinMangrovesEnabled()&&landformRefresh&&std::min(b.rx,b.rz)>=230){
   double scale=num(C.at("epic"),"horizontalScale");b.mangrove=true;
   for(int i=0;i<9&&b.mangrove;++i){double a=i*6.283185307179586/8;
    double X=(b.x+(i?std::cos(a)*b.rx*.35:0))*scale;
    double Z=(b.z+(i?std::sin(a)*b.rz*.35:0))*scale;
    // The wet bowl's centre sets its climate; the ring excludes cold/arid
    // intrusions without requiring every naturally drier bank to be tropical.
    b.mangrove=.62+.45*temp(X,0,Z)>=(i?.55:.62)&&.52+.45*humidity(X,0,Z)>(i?.52:.60);
   }
  }
  break;
 }
 }return b;
}
BasinSite basinSite(int gx,int gz){return basinSites.get(key2(gx,gz),[&]{
 if(!broadBasins||streamingIdentity.empty())return buildBasinSite(gx,gz);
 // Survey the pre-desert field: streamed desert placement itself consults
 // basins. Sharing a descriptor built from the streamed field would create
 // a basin -> desert -> basin dependency cycle across worker caches.
 auto saved=regional::get(streamingIdentity,"basin-v3",gx,gz,[&]{
  auto b=streamingRegions?naturalSamplingField().buildBasinSite(gx,gz):buildBasinSite(gx,gz);
  return J::array({b.valid,b.mangrove,b.x,b.z,b.rx,b.rz,b.floor,b.rim});});
 auto const& b=*saved;return BasinSite{b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]};
});}
struct LandRift {bool valid=false;int size=0;double x=0,z=0,dx=0,dz=0,length=0,width=0,depth=0,phase=0;};
Cache<uint64_t,LandRift> landRifts{256};
LandRift landRift(int gx,int gz){return landRifts.get(key2(gx,gz),[&]{
 LandRift r;auto rnd=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 if(!elevationVariants||rnd(62101)>.62)return r;
 r.size=rnd(62107)<.45?1:rnd(62107)<.82?2:3;
 r.length=(r.size==1?110:r.size==2?270:570)*(0.8+.4*rnd(62109));
 r.width=landformRefresh?(r.size==1?3.5:r.size==2?5.5:9.):(r.size==1?10:r.size==2?26:52);
 r.depth=landformRefresh?(r.size==1?38:r.size==2?66:104):(r.size==1?26:r.size==2?58:110);
 r.phase=rnd(62111)*6.283185307179586;r.dx=std::cos(r.phase);r.dz=std::sin(r.phase);
 for(int attempt=0;attempt<4;++attempt){
  r.x=(gx+.3+.4*rnd(62103+attempt*20))*2304;r.z=(gz+.3+.4*rnd(62105+attempt*20))*2304;
  bool valid=true;
  for(auto const&p:regionalProtected)if(p.radius>=0&&std::hypot(r.x-p.x,r.z-p.z)<p.radius+r.length+100)valid=false;
  for(int i=-4;i<=4&&valid;++i){double X=r.x+r.dx*i*r.length/4+32768,Z=r.z+r.dz*i*r.length/4+32768;
   if(shaped(X,Z,vcont(X,Z))<seaLevelValue+(i==0?28:12))valid=false;
  }
  if(valid){r.valid=true;break;}
 }return r;
});}
void elevationRelief(Surface&s,double&height,int wx,int wz){
 if(!elevationVariants||s.stone>0||s.volcanic>0)return;
 double x=wx-32768,z=wz-32768;int grid=basinGrid(),gx=int(std::floor(x/grid)),gz=int(std::floor(z/grid));
 for(int b=gz-1;b<=gz+1;++b)for(int a=gx-1;a<=gx+1;++a){
  // Broad depressions can join across owner cells; all affecting neighbours
  // are evaluated in stable order, so no cell-edge truncation is introduced.
  if(!broadBasins&&(x<a*grid+(landformRefresh?0:400)||x>(a+1)*grid-(landformRefresh?0:400)||z<b*grid+(landformRefresh?0:400)||z>(b+1)*grid-(landformRefresh?0:400)))continue;
  if(broadBasins){double dx=std::max(0.,std::abs(x-(a+.5)*grid)-.26*grid),dz=std::max(0.,std::abs(z-(b+.5)*grid)-.26*grid);
   if(dx*dx+dz*dz>1380.*1380.)continue;}
  auto r=basinSite(a,b);if(!r.valid)continue;
  double q=basinRadius(r,x,z);
  if(q>=1.35)continue;
  double floor=r.floor+2*regionalNoise.at(x/160.,z/160.);
  double rim=std::max(r.rim,std::min(height,200.));
  double bowl=landformRefresh?floor+(rim-floor)*smooth(.48,1.,q):elevation::basinFloor(q,floor,rim);
  if(broadBasins){
   // A regional depression retains subordinate ridges and drainage valleys.
   // Only a warm, wet basin's central wetland is level enough for mangroves.
   double hills=std::max(0.,height-seaLevelValue-12)*.24;
   double rolling=18*(1+regionalNoise.at(x/310.,z/310.));
   double valley=1-smooth(.03,.24,std::abs(regionalNoise.at(x/480.+37,z/480.-71)));
   double relief=std::max(0.,hills+rolling-28*valley);
   if(r.mangrove)relief*=smooth(.18,.42,q);
   bowl=floor+relief*(1-smooth(.65,1.20,q))+(rim-floor)*smooth(.55,1.20,q);
  }
  double blend=1-smooth(1.,1.35,q);height=mix(blend,height,bowl);
  if(broadBasins){s.basinWeight=std::max(s.basinWeight,1-smooth(.65,1.35,q));s.closedBasin|=q<1.05;}
  else{s.basinWeight=1-smooth(.65,1.35,q);s.closedBasin=q<1.05;}
  if(s.closedBasin){double scale=num(C.at("epic"),"horizontalScale");double H=.52+.45*humidity(r.x*scale,0,r.z*scale);
   if(H>.58&&height<r.floor+3){s.basinWater=int(std::floor(r.floor+3));}
  }
 }
 if(s.basinWeight>0)return;
 gx=int(std::floor(x/2304));gz=int(std::floor(z/2304));
 for(int b=gz-1;b<=gz+1;++b)for(int a=gx-1;a<=gx+1;++a){
  if(x<a*2304-100||x>(a+1)*2304+100||z<b*2304-100||z>(b+1)*2304+100)continue;
  auto r=landRift(a,b);if(!r.valid)continue;
  double dx=x-r.x,dz=z-r.z,u=dx*r.dx+dz*r.dz;
  if(std::abs(u)>=r.length)continue;
  double curve=(landformRefresh?12+10*r.size:r.width)*(.65*std::sin(u/r.length*3+r.phase)+.25*std::sin(u/r.length*7-r.phase));
  double v=-dx*r.dz+dz*r.dx-curve;
  double cut=elevation::riftCut(v,u,r.length,r.width,r.depth);
  if(landformRefresh){
   double taper=1-smooth(.70,1.,std::abs(u)/r.length),width=r.width*(.65+.35*std::sin(u/38.+r.phase))*(.18+.82*std::sqrt(taper));
   double edge=std::abs(v)/(width*(v<0?1.12:.88));
   cut=r.depth*taper*(1-smooth(.45,1.20,edge))*(.88+.12*std::sin(u/27.+r.phase));
  }else cut*=smooth(seaLevelValue+12.,seaLevelValue+40.,height);
  if(cut<=0)continue;
  height=std::max(landformRefresh?-48.:double(seaLevelValue+8),height-cut);
  if(landformRefresh)s.closedBasin=true; // surveyed inland cut is not an ocean sink
  s.riftSize=r.size;s.riftWeight=std::max(s.riftWeight,cut/r.depth);
 }
}
J elevationInventory(int lo,int hi){
 if(hi<lo||int64_t(hi)-lo>100000)throw std::runtime_error("Elevation inventory range too wide");
 J out={{"basins",J::array()},{"rifts",J::array()}};if(!elevationVariants)return out;
 for(int z=int(std::floor(double(lo)/basinGrid()));z<=int(std::floor(double(hi)/basinGrid()));++z)for(int x=int(std::floor(double(lo)/basinGrid()));x<=int(std::floor(double(hi)/basinGrid()));++x){auto b=basinSite(x,z);if(b.valid)out["basins"].push_back({{"x",b.x},{"z",b.z},{"radiusX",b.rx},{"radiusZ",b.rz},{"floor",b.floor},{"rim",b.rim},{"mangrove",b.mangrove}});}
 for(int z=int(std::floor(lo/2304.));z<=int(std::floor(hi/2304.));++z)for(int x=int(std::floor(lo/2304.));x<=int(std::floor(hi/2304.));++x){auto r=landRift(x,z);if(r.valid)out["rifts"].push_back({{"x",r.x},{"z",r.z},{"size",r.size},{"halfLength",r.length},{"halfWidth",r.width},{"depth",r.depth},{"dx",r.dx},{"dz",r.dz}});}
 return out;
}
