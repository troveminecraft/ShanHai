// Seed-selected lowland basins. Selection reads the existing base field only;
// it never recursively samples the decorated surface or alters river routing.
struct WetlandPatch {int x=0,z=0,level=0;double radius=0,phase=0;};
Cache<uint64_t,WetlandPatch> wetlandSites{512};
Perlin wetlandNoise{seedHash(terrainSeed,15301)};
// Keep independent wetland lowering away from the finished drainage banks.
// A river column is excluded below, so its neighbouring wetland displacement
// must already have faded to zero rather than stopping at that column.
Cache<uint64_t,double> wetlandWaterNodes{4096};
double wetlandWaterNode(int gx,int gz){return wetlandWaterNodes.get(key2(gx,gz),[&]{
 double x=gx*4.-32768,z=gz*4.-32768,distance=16.;
 auto bin=riverBins.find(key2(int(std::floor(x/64)),int(std::floor(z/64))));
 if(bin!=riverBins.end())for(auto id:bin->second){auto const&r=riverSegments[id];
  double dx=r.bx-r.ax,dz=r.bz-r.az,t=clamp(((x-r.ax)*dx+(z-r.az)*dz)/std::max(1.,dx*dx+dz*dz),0,1);
  // Includes the maximum channel widening and the finite bank transition.
  distance=std::min(distance,std::hypot(x-r.ax-t*dx,z-r.az-t*dz)-std::max(r.ar,r.br)*1.5);
 }
 // Row spans include the dry retaining collar as well as the lake water.
 // Only nearby rows are queried, once per four-block cached node.
 for(int dz=-16;dz<=16;++dz){int row=int(z)+dz-terrain_bounds::MinBlock;
  std::vector<LakeSpan> const* spansPtr=nullptr;
  if(unboundedRegions){auto found=streamingLakeRows.find(int(z)+dz);if(found==streamingLakeRows.end())continue;spansPtr=&found->second;}
  else{if(row<0||row>=int(lakeRows.size()))continue;spansPtr=&lakeRows[row];}
  auto const&spans=*spansPtr;
  auto it=std::lower_bound(spans.begin(),spans.end(),x-16,[](LakeSpan const&s,double v){return s.x1<v;});
  for(;it!=spans.end()&&it->x0<=x+16;++it){
   double dx=std::max({0.,it->x0-x,x-it->x1});distance=std::min(distance,std::hypot(dx,double(dz)));
  }
 }
 return smooth(6.,16.,distance);
});}
double wetlandWaterWeight(int x,int z){
 int gx=int(std::floor(x/4.)),gz=int(std::floor(z/4.));
 double u=smooth(0,1,x/4.-gx),v=smooth(0,1,z/4.-gz);
 return (1-u)*(1-v)*wetlandWaterNode(gx,gz)+u*(1-v)*wetlandWaterNode(gx+1,gz)
  +(1-u)*v*wetlandWaterNode(gx,gz+1)+u*v*wetlandWaterNode(gx+1,gz+1);
}
bool wetlandGround(Surface const&s,int x,int z){
 return landscapeAltitude(s.height)>=34&&landscapeAltitude(s.height)<=82&&s.water<s.height&&!s.lake&&!s.river&&s.stone==0&&s.volcanic==0&&s.barren<.01&&s.coast<.05&&s.cliffLow==-32768&&giantWeight(x,z,s)<.01;
}
WetlandPatch wetlandPatch(int gx,int gz){return wetlandSites.get(key2(gx,gz),[&]{
 WetlandPatch a;if(!C.value("wetlandsEnabled",true))return a;
 auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 int x=gx*512+256+int((r(15303)-.5)*80),z=gz*512+256+int((r(15304)-.5)*80);
 auto center=base(x,z);
 if(!wetlandGround(center,x,z)||center.T<.34||center.T>.76||center.H<.54)return a;
 int lo=center.height,hi=center.height;
 // Stable spacing makes basins disjoint. Their shorelines use a warped
 // footprint and continuous blend instead of a grid or circular cutoff.
 for(int dz:{-144,0,144})for(int dx:{-144,0,144}){
  auto s=base(x+dx,z+dz);if(!wetlandGround(s,x+dx,z+dz))return a;
  lo=std::min(lo,s.height);hi=std::max(hi,s.height);
 }
 if(hi-lo>24)return a;
 a={x,z,lo-1,166+r(15305)*20,r(15306)*6.283185307};return a;
});}
void wetlandRelief(Surface&s,int x,int z){
 // Existing oceans, lakes, channels and special regions keep their beds.
 if(!wetlandGround(s,x,z))return;
 int gx=int(std::floor(x/512.)),gz=int(std::floor(z/512.));
 auto a=wetlandPatch(gx,gz);if(!a.level)return;
 double dx=x-a.x,dz=z-a.z;if(std::abs(dx)>220||std::abs(dz)>220)return;
 double angle=std::atan2(dz,dx),edge=a.radius*(1+.06*std::sin(3*angle+a.phase)+.035*std::cos(5*angle-a.phase));
 double distance=std::hypot(dx,dz)/edge;if(distance>=1)return;
 double weight=(1-smooth(.56,1.,distance))*smooth(a.level+1.,a.level+5.,s.height)*(1-smooth(16.,28.,std::abs(s.height-a.level)));
 // The eligibility cutoff at Y=82 must be reached with zero displacement.
 // New lowland belts expose this boundary inside otherwise valid wetland sites.
 if(regionalEnabled)weight*=smooth(34.,42.,landscapeAltitude(s.height))*(1-smooth(64.,82.,landscapeAltitude(s.height)));
 if(weight<=0)return;
 if(glacierWetlands){
  // Evaluate glacier suitability before wetland excavation: lowering terrain
  // must not manufacture a new cold wetland inside a pre-existing ice region.
  auto climate=s;climate.height=std::min(climate.height,elevatedLandscape?96:72);
  glacierSkin(climate,x,z);
  weight*=1-smooth(.02,.18,climate.glacier);
  if(weight<=0)return;
 }
 if(regionalEnabled){weight*=wetlandWaterWeight(x,z);if(weight<=0)return;}
 // A connected low-water matrix with irregular islands, not isolated
 // hash-selected puddle columns. Banks fade to the untouched dry terrain.
 double n=wetlandNoise.at(x/67.,z/67.)+.22*wetlandNoise.at(x/23.,z/23.);
 double island=smooth(-.02,.30,n);
 double target=a.level-2.2+5.8*island+.5*wetlandNoise.at(x/13.,z/13.);
 int h=int(std::floor(s.height+weight*(target-s.height)));
 // Original ground must be above the water plane. The eligibility weight
 // tends to zero before lower terrain, keeping a dry retaining shore.
 s.wetland=weight;s.wetlandMoisture=1-island;s.wetlandLevel=a.level;
 s.height=std::max(h,a.level-3);s.continuous=(s.height-seaLevelValue)/123.;s.e=s.continuous;
 if(s.height<a.level){s.water=a.level;s.kind=2;}
}
J wetlandInventory(int minimum,int maximum){
 J out=J::array();
 for(int gz=int(std::floor((minimum+32768)/512.));gz<=int(std::floor((maximum+32768)/512.));gz++)
 for(int gx=int(std::floor((minimum+32768)/512.));gx<=int(std::floor((maximum+32768)/512.));gx++){
  auto a=wetlandPatch(gx,gz);if(a.level&&a.x-32768>=minimum&&a.x-32768<=maximum&&a.z-32768>=minimum&&a.z-32768<=maximum)
   out.push_back({{"x",a.x-32768},{"z",a.z-32768},{"level",a.level},{"radius",a.radius}});
 }return out;
}
