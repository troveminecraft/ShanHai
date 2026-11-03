// One coherent grove owns its central tree. Candidate footprints stay inside
// their 1,024-block owner, avoiding neighbour-region planning dependencies.
struct CherryGrove {bool valid=false;int x=0,z=0,ground=0,trunkRadius=0;double rx=0,rz=0,phase=0;};
Cache<uint64_t,CherryGrove> cherryGroves{256};
bool cherryGround(Surface const&s)const{
 double t=ecologyTemperature(s);
 return s.height>=seaLevelValue+12&&s.height<=330&&s.water<s.height&&s.lava<s.height
  &&!s.closedBasin&&!s.lake&&!s.river&&s.stone<.01&&s.volcanic<.01&&s.barren<.01
  &&!s.badlands&&!s.badlandsDesert&&!s.aridType&&s.desert<.05&&s.coast<.35
  &&s.giant<.01&&s.riftWeight<.05&&s.glacier<.05&&s.iceHabitat<.08&&t>=.36&&t<=.72&&s.H>=.38&&s.H<=.82;
}
CherryGrove cherryGroveSite(int gx,int gz){return cherryGroves.get(key2(gx,gz),[&]{
 CherryGrove a;if(!canopyForests||hash01(gx,gz,terrainSeed^88901)>(branchingCherries?.90:.64))return a;
 auto random=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 a.x=roundJS((gx+.38+.24*random(88903))*1024);a.z=roundJS((gz+.38+.24*random(88905))*1024);
 a.rx=88+132*random(88907);a.rz=a.rx*(.82+.16*random(88909));a.phase=random(88911)*6.283185307;
 a.trunkRadius=10+int(random(88913)*11);
 auto sample=[&](int x,int z){return streamingRegions?naturalSamplingField().compute(x,z,false):compute(x,z,false);};
 auto center=sample(a.x,a.z);if(!cherryGround(center)||center.height<seaLevelValue+35||center.height>310)return CherryGrove{};
 if(branchingCherries){
  bool upland=center.height>=200&&center.height<=300;
  if(!upland&&random(88901)>.24)return CherryGrove{};
  a.rx=upland?180+60*random(88907):88+48*random(88907);a.rz=a.rx*(.90+.10*random(88909));
 }
 int low=center.height,high=center.height;
 for(int z=-32;z<=32;z+=16)for(int x=-32;x<=32;x+=16){auto s=sample(a.x+x,a.z+z);
  if(!cherryGround(s))return CherryGrove{};low=std::min(low,s.height);high=std::max(high,s.height);
 }
 if(high-low>(branchingCherries?24:16))return CherryGrove{};
 // Qualify the complete stand, so a climate threshold cannot leave a tiny
 // isolated cherry patch. Final rivers may pass around the reserved centre.
 for(int z=-int(a.rz*1.08);z<=a.rz*1.08;z+=32)for(int x=-int(a.rx*1.08);x<=a.rx*1.08;x+=32){
  if((x*x)/(a.rx*a.rx)+(z*z)/(a.rz*a.rz)>1.18)continue;
  if(!cherryGround(sample(a.x+x,a.z+z)))return CherryGrove{};
 }
 a.ground=center.height;a.valid=true;return a;
});}
double cherryGroveRadius(CherryGrove const&a,int x,int z)const{
 double dx=(x-a.x)/a.rx,dz=(z-a.z)/a.rz,angle=std::atan2(dz,dx);
 return std::hypot(dx,dz)/(1+.045*std::sin(angle*3+a.phase)+.025*std::cos(angle*5-a.phase));
}
CherryGrove cherryGroveAt(int x,int z){
 if(!canopyForests)return {};
 auto a=cherryGroveSite(int(std::floor(x/1024.)),int(std::floor(z/1024.)));
 return a.valid&&cherryGroveRadius(a,x,z)<=1?a:CherryGrove{};
}
bool cherryCoreReserved(int x,int z){
 if(!canopyForests)return false;auto a=cherryGroveAt(x,z);
 return a.valid&&std::hypot(x-a.x,z-a.z)<a.trunkRadius+60;
}
