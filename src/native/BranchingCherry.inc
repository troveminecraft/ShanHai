Cache<uint64_t,worldgen_rules::BranchingCherry> branchingCherryModels{128};
void branchingCherryTree(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,
                        int ax,int az,int ground,int trunk,double phase){
 using namespace forest_palette;
 auto const& model=branchingCherryModels.get(key2(ax,az),[&]{
  return worldgen_rules::branchingCherry(trunk,phase,[&](int salt){return hash01(ax,az,terrainSeed^(89601+salt));});
 });
 for(auto const&t:model.tufts){
  double X=ax+t.x,Z=az+t.z,Y=ground+t.y;
  if(x0>X+t.rx*1.2||x0+15<X-t.rx*1.2||z0>Z+t.rz*1.2||z0+15<Z-t.rz*1.2)continue;
  for(int z=std::max(z0,int(std::floor(Z-t.rz*1.2)));z<=std::min(z0+15,int(std::ceil(Z+t.rz*1.2)));++z)
  for(int x=std::max(x0,int(std::floor(X-t.rx*1.2)));x<=std::min(x0+15,int(std::ceil(X+t.rx*1.2)));++x){
   if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;auto const&s=surfaces[i];
   double dx=(x-X)/t.rx,dz=(z-Z)/t.rz,angle=std::atan2(dz,dx);
   double edge=1+.12*std::sin(angle*3+t.phase)+.06*std::sin(angle*7-t.phase);
   double q=(dx*dx+dz*dz)/(edge*edge);if(q>=1)continue;
   double half=t.ry*std::sqrt(1-q),mid=Y+1.2*std::sin(angle*3+t.phase)*q;
   for(int y=std::max({int(std::ceil(mid-half)),s.height+1,s.water+1,s.lava+1});y<=std::min(Height-65,int(std::floor(mid+half)));++y){
    auto& b=raw[i*Height+y+64];if(canopyReplaceable(b))b=CanopyCherryLeaves;
   }
  }
 }
 for(int i=0;i<256;++i){int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z))continue;
  auto const&s=surfaces[i];double dx=x-ax,dz=z-az,angle=std::atan2(dz,dx),d=std::hypot(dx,dz);
  if(d>trunk*1.8)continue;
  for(int y=std::max({s.height,s.water,s.lava})+1;y<=std::min(Height-65,ground+43);++y){
   double h=y-ground,r=trunk*(1-.86*smooth(4.,45.,h));
   r+=(1-smooth(0.,15.,h))*trunk*.65*std::pow(std::max(0.,std::cos(angle*7+phase)),4);
   r*=1+.08*std::sin(angle*5+phase+h*.06);
   double bend=2*smooth(12.,43.,h);
   auto&b=raw[i*Height+y+64];if(std::hypot(dx-bend*std::cos(phase),dz-bend*std::sin(phase))<=r&&canopyReplaceable(b))b=CherryWood;
  }
 }
 for(auto const&b:model.branches)canopyLimb(x0,z0,raw,surfaces,ax+b.x,ground+b.y,az+b.z,
   ax+b.X,ground+b.Y,az+b.Z,b.r,b.R,CherryWood);
}
