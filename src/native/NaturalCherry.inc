Cache<uint64_t,worldgen_rules::BranchingCherry> naturalCherryModels{16};
void naturalCherryTree(int x0,int z0,std::vector<uint16_t>& raw,std::array<Surface,256> const& surfaces,
                       int ax,int az,int ground,int trunk,double phase){
 using namespace forest_palette;
 bool fuller=C.value("cherryRevision",0)>=4;
 auto const& model=naturalCherryModels.get(key2(ax,az),[&]{
  return worldgen_rules::natural_cherry::build(trunk,phase,[&](int salt){return hash01(ax,az,terrainSeed^(89701+salt));},fuller);
 });
 for(auto const& t:model.tufts){
  double X=ax+t.x,Z=az+t.z,Y=ground+t.y,reach=std::max(t.rx,t.rz)*1.12;
  if(x0>X+reach||x0+15<X-reach||z0>Z+reach||z0+15<Z-reach)continue;
  double ca=std::cos(t.phase),sa=std::sin(t.phase);
  for(int z=std::max(z0,int(std::floor(Z-reach)));z<=std::min(z0+15,int(std::ceil(Z+reach)));++z)
  for(int x=std::max(x0,int(std::floor(X-reach)));x<=std::min(x0+15,int(std::ceil(X+reach)));++x){
   if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;auto const& s=surfaces[i];
   double dx=x-X,dz=z-Z,u=(dx*ca+dz*sa)/t.rx,v=(-dx*sa+dz*ca)/t.rz;
   double q=u*u+v*v;if(q>1.18)continue;
   for(int y=std::max({int(std::floor(Y-t.ry*1.1)),s.height+1,s.water+1,s.lava+1});
       y<=std::min(Height-65,int(std::ceil(Y+t.ry*1.1)));++y){
    double dy=(y-Y)/t.ry,d=q+dy*dy;
    if(d>1.18||d>.78&&d>1+.18*(2*hash01(x+y*31,z-y*17,terrainSeed^89791)-1))continue;
    auto& b=raw[i*Height+y+64];if(canopyReplaceable(b))b=CanopyCherryLeaves;
   }
  }
 }
 // Short fluted root flare joins the curved leader. Above it, the complete
 // wood skeleton (including the trunk) is curved and tapers through forks.
 double rootRadius=trunk*(fuller?.66:1.);
 for(int i=0;i<256;++i){int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z))continue;
  auto const& s=surfaces[i];double dx=x-ax,dz=z-az,angle=std::atan2(dz,dx);
  if(std::hypot(dx,dz)>rootRadius*(fuller?1.55:1.8))continue;
  for(int y=std::max({s.height,s.water,s.lava})+1;y<=std::min(Height-65,ground+17);++y){
   double h=y-ground,r=rootRadius*(1-.60*smooth(0.,21.,h));
   r+=(1-smooth(0.,13.,h))*rootRadius*(fuller?.42:.65)*std::pow(std::max(0.,std::cos(angle*7+phase)),4);
   r*=1+.06*std::sin(angle*5+phase+h*.10);double bend=2.5*smooth(4.,18.,h);
   auto& b=raw[i*Height+y+64];
   if(std::hypot(dx-bend*std::cos(phase-1.1),dz-bend*std::sin(phase-1.1))<=r&&canopyReplaceable(b))b=CherryWood;
  }
 }
 for(auto const& b:model.branches)canopyLimb(x0,z0,raw,surfaces,ax+b.x,ground+b.y,az+b.z,
   ax+b.X,ground+b.Y,az+b.Z,b.r,b.R,CherryWood);
}
