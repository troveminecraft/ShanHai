// Tiny pools fit wholly inside their owner chunk. Inspect existing geology;
// do not rebuild cave masks or sample neighbouring terrain a second time.
 void smallUndergroundPockets(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256> const&surfaces){
  int gx=int(std::floor(x0/96.)),gz=int(std::floor(z0/96.));auto r=[&](int k){return hash01(gx,gz,terrainSeed^(19800+k));};if(r(0)>.28)return;
  int x=gx*96+int(r(1)*6)*16+5+int(r(2)*6),z=gz*96+int(r(3)*6)*16+5+int(r(4)*6),radius=2+int(r(5)*2);
  if(x<x0||x>=x0+16||z<z0||z>=z0+16)return;
  auto at=[&](int X,int Y,int Z)->uint16_t&{return raw[((Z-z0)*16+X-x0)*Height+Y+64];};
  auto rock=[](int id){return id==2||id==48||id==94||id==95||id==96||id==97||id==98||id==99;};
  for(int y=std::min(80,surfaces[(z-z0)*16+x-x0].height-12);y>=-51;--y){if(at(x,y,z)!=0||!rock(at(x,y-1,z)))continue;bool sealed=true;
   for(int dz=-radius-1;dz<=radius+1;++dz)for(int dx=-radius-1;dx<=radius+1;++dx){if(dx*dx+dz*dz>(radius+1)*(radius+1))continue;if(!rock(at(x+dx,y-1,z+dz))||!rock(at(x+dx,y-2,z+dz)))sealed=false;}
   if(!sealed)continue;int level=y-1;for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx)if(dx*dx+dz*dz<radius*radius&&at(x+dx,y,z+dz)==0)at(x+dx,level,z+dz)=level<=-52?43:8;break;
  }
  if(!field.highMountainUndergroundEnabled())return;
  auto const&s=surfaces[(z-z0)*16+x-x0];
  double mountain=high_mountain_underground::weight(s);
  // Reuse ownership, but independently thin the additional upper pool. The
  // original lower pool and its random stream remain unchanged.
  if(mountain<=0||r(6)>.36*mountain)return;
  int top=high_mountain_underground::scanTop(s,true,Height-65,80,14);
  auto read=[&](int X,int Y,int Z){return at(x0+X,Y,z0+Z);};
  auto surface=[&](int X,int Z)->Surface const&{return surfaces[Z*16+X];};
  int X=x-x0,Z=z-z0;
  for(int y=top;y>=83;--y){
   if(read(X,y,Z)!=0||!rock(read(X,y-1,Z)))continue;
   if(!high_mountain_underground::poolFits(X,y,Z,radius,Height-65,read,rock,surface))continue;
   high_mountain_underground::writePool(X,y,Z,radius,[&](int px,int py,int pz,uint16_t b){at(x0+px,py,z0+pz)=b;});break;
  }
 }
