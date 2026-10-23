// Regional selection, never a per-column lottery. Only small pillars are used;
// an accepted patch has a continuous border and no legacy west/east split.
bool rareStoneForests=C.at("stoneForest").value("distribution",std::string{})=="rare-small-regions-v1";
bool largeStoneForestEnabled()const{return C.at("stoneForest").value("largeEnabled",true)&&!rareStoneForests;}
struct SmallStoneRegion {bool valid=false;double x=0,z=0,floor=0;int gx=0,gz=0;};
Cache<uint64_t,SmallStoneRegion> smallStoneRegions{256};
SmallStoneRegion smallStoneRegion(int gx,int gz){return smallStoneRegions.get(key2(gx,gz),[&]{
 SmallStoneRegion r;r.gx=gx;r.gz=gz;if(!rareStoneForests)return r;
 auto const&q=C.at("stoneForest");double grid=num(q,"smallRegionGrid");
 if(hash01(gx,gz,terrainSeed^83101)>=num(q,"smallRegionChance"))return r;
 r.x=std::floor((gx+.30+.40*hash01(gx,gz,terrainSeed^83103))*grid);
 r.z=std::floor((gz+.30+.40*hash01(gx,gz,terrainSeed^83107))*grid);
 // Immutable dry upland only. No querying generated chunks or voxel noise.
 r.floor=uncliff(r.x+32768,r.z+32768);
 r.valid=r.floor>42&&r.floor<140;return r;
});}
bool nearSmallStoneForest(double x,double z,double extra){
 if(!rareStoneForests)return false;auto const&q=C.at("stoneForest");double grid=num(q,"smallRegionGrid");
 int gx=int(std::floor(x/grid)),gz=int(std::floor(z/grid));int reach=int(std::ceil((num(q,"smallRegionOuter")+extra)/grid))+1;
 for(int dz=-reach;dz<=reach;++dz)for(int dx=-reach;dx<=reach;++dx){auto r=smallStoneRegion(gx+dx,gz+dz);if(r.valid&&std::hypot(x-r.x,z-r.z)<num(q,"smallRegionOuter")+extra)return true;}
 return false;
}
void smallStoneRelief(Surface&s,double&height,int wx,int wz){
 if(!rareStoneForests)return;auto const&q=C.at("stoneForest");double grid=num(q,"smallRegionGrid"),X=wx-32768,Z=wz-32768;
 int gx=int(std::floor(X/grid)),gz=int(std::floor(Z/grid));
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto region=smallStoneRegion(gx+dx,gz+dz);if(!region.valid)continue;
  double x=X-region.x,z=Z-region.z,outer=num(q,"smallRegionOuter"),inner=num(q,"smallRegionInner");
  if(std::abs(x)>outer+12||std::abs(z)>outer+12)continue;
  double distance=std::hypot(x,z)+num(q,"smallRegionEdgeWarp")*stoneNoise.at(x/90.,z/90.);
  double weight=1-smooth(inner,outer,distance);if(weight<=0)continue;
  double pitch=q.value("smallPillarPitch",56.);int px=int(std::floor(x/pitch)),pz=int(std::floor(z/pitch));double best=-INFINITY,ph=0;
  for(int iz=pz-1;iz<=pz+1;++iz)for(int ix=px-1;ix<=px+1;++ix){
   uint32_t seed=terrainSeed^seedHash(uint32_t(region.gx),uint32_t(region.gz));
   double cx=(ix+.28+.44*hash01(ix,iz,seed^83111))*pitch,cz=(iz+.28+.44*hash01(ix,iz,seed^83113))*pitch;
   double radius=num(q,"smallRadiusMin")+(num(q,"smallRadiusMax")-num(q,"smallRadiusMin"))*hash01(ix,iz,seed^83117);
   if(std::hypot(cx,cz)+radius*1.165>inner)continue;
   double phase=hash01(ix,iz,seed^83119)*6.283185307179586,angle=std::atan2(z-cz,x-cx);
   double R=radius*(1+.07*std::cos(2*angle+phase)+.06*std::sin(3*angle+phase*1.7)+.035*std::cos(5*angle-phase));
   double gap=R-std::hypot(x-cx,z-cz);if(gap<=best)continue;best=gap;
   ph=num(q,"smallHeightMin")+(num(q,"smallHeightMax")-num(q,"smallHeightMin"))*hash01(ix,iz,seed^83123);
  }
  double mask=best>0?smooth(0,num(q,"wallBevel"),best):0;
  double ground=region.floor+2.5*stoneNoise.at(x/40.,z/40.);
  double top=std::min(310.,ground+mask*(ph+num(q,"crownDetail")*stoneNoise.at(x/9.,z/9.)));
  height=mix(weight,height,top);s.stone=weight;s.pillar=mask;
  if(weight>=.85&&mask<=0&&hash01(wx,wz,terrainSeed^917)<=num(q,"dripstoneChance"))s.drip=2+int(hash01(wx,wz,terrainSeed^919)*5);
 }
}
