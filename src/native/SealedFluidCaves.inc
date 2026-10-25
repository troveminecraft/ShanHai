// Fully flooded cavities in intact rock. World-space plans are cached once;
// each output chunk writes only its own columns, with no neighbour chunk loads.
struct SealedFluidColumn {int x,z,lo,hi;};
struct SealedFluidCave {int x=0,z=0,y=0,ground=0,fluid=0,reach=0;std::vector<SealedFluidColumn> columns;};
Cache<uint64_t,std::shared_ptr<SealedFluidCave>> sealedFluidPlans{256};
std::shared_ptr<SealedFluidCave> sealedFluidPlan(int gx,int gz,bool lava){
 return sealedFluidPlans.get(key2(gx*2+int(lava),gz),[&]{
  auto p=std::make_shared<SealedFluidCave>();
  auto r=[&](int salt){return hash01(gx,gz,seedHash(terrainSeed,56100+salt+(lava?100:0)));};
  if(r(0)>(lava?.38:.46))return p;
  int margin=lava?48:32,spread=lava?32:64;
  p->x=gx*128+margin+int(r(1)*spread);p->z=gz*128+margin+int(r(2)*spread);p->fluid=lava?43:8;
  auto s=field.surface(p->x,p->z,false);p->ground=s.height;
  auto unsuitable=[&](Surface const&s){return s.height<34||
   (s.height>180&&(!field.highMountainUndergroundEnabled()||high_mountain_underground::weight(s)<=0))||
   s.volcanic>.01||s.vent||s.stone>.01||s.iceHabitat>.01||s.scarp>.01||s.cliffLow>-32768;};
  if(unsuitable(s)||(!lava&&(s.desert>.15||s.H<.35)))return p;
  if(s.height>180){
   // Deep lava remains in the legacy distribution. Sparse mountain water
   // stays near its local roof, never becoming a regional high aquifer.
   if(lava||r(0)>.14*high_mountain_underground::weight(s))return p;
  }
  double rx=lava?24+10*r(3):13+8*r(3),rz=lava?14+7*r(4):6+4*r(4);
  double ry=lava?3.2:3+2*r(5),angle=r(6)*6.283185307,phase=r(7)*6.283185307;
  p->y=lava?-54:s.height-18-int(r(8)*12);if(!lava&&p->y-ry<0)return p;
  p->reach=int(std::ceil(rx+rz*.24+2));int low=Height-64,high=-64;
  for(int dz=-p->reach;dz<=p->reach;++dz)for(int dx=-p->reach;dx<=p->reach;++dx){
   double along=dx*std::cos(angle)+dz*std::sin(angle),across=-dx*std::sin(angle)+dz*std::cos(angle);
   double u=along/rx;if(std::abs(u)>=1)continue;
   across-=std::sin(u*3.14159265+phase)*rz*.24;
   double width=rz*(.86+.14*std::sin(u*5+phase)),q=u*u+across*across/(width*width);if(q>=1)continue;
   double arch=std::sqrt(1-q);int lo=int(std::ceil(p->y-ry*arch)),hi=int(std::floor(p->y+ry*arch));
   if(lava){lo=std::max(-56,lo);hi=std::min(-52,hi);}
   if(lo>hi)continue;
   p->columns.push_back({p->x+dx,p->z+dz,lo,hi});low=std::min(low,lo);high=std::max(high,hi);
  }
  // Validate the entire enclosing rock box, including a two-block shell.
  // Reject the whole candidate on contact with a natural cave, surface cut,
  // water or lava. A later chunk cannot produce a clipped/open half-cavity.
  if(p->columns.empty())return p;
  int minX=p->x,maxX=p->x,minZ=p->z,maxZ=p->z;
  for(auto const&c:p->columns){minX=std::min(minX,c.x);maxX=std::max(maxX,c.x);minZ=std::min(minZ,c.z);maxZ=std::max(maxZ,c.z);}
  bool valid=high_mountain_underground::sealedRockBoxFits(minX,maxX,minZ,maxZ,low,high,Height-65,lava,
   [&](int x,int z){return field.surface(x,z,false);},[&](Surface const&local){return !unsuitable(local);},
   [&](Surface const&local,int x,int y,int z){return field.material(local,x,y,z);});
  if(!valid)p->columns.clear();return p;
 });
}
void sealedFluidCaves(int x0,int z0,std::vector<uint16_t>&raw){
 if(!sealedFluidsEnabled)return;
 // Margins keep all candidates inside their 128-block ownership cell.
 int gx=int(std::floor(x0/128.)),gz=int(std::floor(z0/128.));
 for(bool lava:{false,true}){
  auto r=[&](int salt){return hash01(gx,gz,seedHash(terrainSeed,56100+salt+(lava?100:0)));};
  if(r(0)>(lava?.38:.46))continue;
  int margin=lava?48:32,spread=lava?32:64,reach=lava?42:26;
  int x=gx*128+margin+int(r(1)*spread),z=gz*128+margin+int(r(2)*spread);
  if(x<x0-reach||x>x0+15+reach||z<z0-reach||z>z0+15+reach)continue;
  auto p=sealedFluidPlan(gx,gz,lava);
  for(auto const&c:p->columns){int X=c.x-x0,Z=c.z-z0;if(X<0||X>=16||Z<0||Z>=16)continue;
   auto col=raw.data()+(Z*16+X)*Height;
   for(int y=c.lo;y<=c.hi;++y)if(col[y+64]==2)col[y+64]=uint16_t(p->fluid);
  }
 }
}
