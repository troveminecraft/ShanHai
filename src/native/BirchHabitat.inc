// Region-scale suitability, shared by biome selection and trees. Never calls
// surface(): that would recurse into biome selection. Cached base samples keep
// the shoreline/slope buffer independent of chunk order and inexpensive.
Cache<uint64_t,double> birchHabitatNodes{4096};
Cache<uint64_t,bool> birchSeaNodes{8192};
double birchHabitatNode(int gx,int gz){return birchHabitatNodes.get(key2(gx,gz),[&]{
 int x=gx*32,z=gz*32;auto center=base(x,z);
 if(center.water>=center.height||center.coast>.1||center.cliffLow!=-32768)return 0.;
 double slope=0;
 for(auto [dx,dz]:{std::pair{16,0},{-16,0},{0,16},{0,-16},{12,12},{12,-12},{-12,12},{-12,-12}})
  slope=std::max(slope,std::abs(base(x+dx,z+dz).height-center.height)/std::hypot(dx,dz));
 double weight=1-smooth(.30,.65,slope);if(weight<=0)return 0.;
 // A conservative coarse sea buffer; the extra reach absorbs the 32-block
 // interpolation cell and coastline sampling error. Lakes remain distinct.
 for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){
  if(dx*dx+dz*dz>10)continue;
  int X=gx+dx,Z=gz+dz;
  if(birchSeaNodes.get(key2(X,Z),[&]{auto s=base(X*32,Z*32);return s.water==seaLevelValue&&s.height<seaLevelValue&&!s.lake;}))return 0.;
 }
 // Use the actual drainage graph, so even narrow channels between sea-grid
 // samples retain a bank buffer. Wider downstream rivers get a larger buffer.
 double wx=x-32768,wz=z-32768;
 for(int bz=int(std::floor((wz-112)/64));bz<=int(std::floor((wz+112)/64));bz++)
 for(int bx=int(std::floor((wx-112)/64));bx<=int(std::floor((wx+112)/64));bx++){
  auto it=riverBins.find(key2(bx,bz));if(it==riverBins.end())continue;
  for(auto id:it->second){auto const&r=riverSegments[id];double dx=r.bx-r.ax,dz=r.bz-r.az;
   double t=clamp(((wx-r.ax)*dx+(wz-r.az)*dz)/std::max(1.,dx*dx+dz*dz),0,1);
   double radius=mix(t,r.ar,r.br),edge=std::hypot(wx-r.ax-t*dx,wz-r.az-t*dz)-radius*1.5;
   weight=std::min(weight,smooth(radius>=3.5?72.:48.,radius>=3.5?112.:88.,edge));
  }
 }
 return weight;
});}
double birchSuitability(int x,int z){
 int gx=int(std::floor(x/32.)),gz=int(std::floor(z/32.));
 double u=smooth(0,1,x/32.-gx),v=smooth(0,1,z/32.-gz);
 return (1-u)*(1-v)*birchHabitatNode(gx,gz)+u*(1-v)*birchHabitatNode(gx+1,gz)
       +(1-u)*v*birchHabitatNode(gx,gz+1)+u*v*birchHabitatNode(gx+1,gz+1);
}
