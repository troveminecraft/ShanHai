// Sparse small swept cones, attached to the same continuous glacier body.
// Descriptors are seed-derived and cached; no voxel noise or neighbour loads.
struct SmallIceDescriptor {
 bool valid=false;int base=0,height=0;double ax=0,az=0;
 std::array<double,5> nx{},nz{},distance{};
};
Cache<uint64_t,SmallIceDescriptor> smallIceDescriptors{512};
SmallIceDescriptor smallIceDescriptor(int gx,int gz,int X,int Z){return smallIceDescriptors.get(key2(gx,gz),[&]{
 SmallIceDescriptor d;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 auto s=field.surface(X+32768,Z+32768,false);
 auto eligible=[](Surface const&t){return t.superGlacier>.70&&t.glacier>.65&&!t.kind&&!t.river&&!t.lake&&!t.fluvialType&&t.wetland<=.01&&t.water<t.height&&t.scarp<=.01&&t.cliffLow==-32768;};
 if(!eligible(s))return d;
 double n=fractureNoise.at(X/110.,Z/110.),m=fractureNoise.at(X/110.+37.,Z/110.-61.);
 double wx=X+9*n,wz=Z+9*m,first=INFINITY,second=INFINITY;
 int px=int(std::floor(wx/64)),pz=int(std::floor(wz/64));
 if(!snowRifts)for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto p=icePlatePoint(px+dx,pz+dz);double v=(wx-p.x)*(wx-p.x)+(wz-p.z)*(wz-p.z);if(v<first){second=first;first=v;}else second=std::min(second,v);}
 double edge=snowRifts?100.:.5*(std::sqrt(second)-std::sqrt(first)),width=1.3+.5*(n+1);
 double lift=11+9*n+6*m+7*std::abs(n-m)-(12+4*m)*(1-smooth(width,width+5.,edge));
 if(snowRifts){auto rift=closedIceRifts?riftSample(X,Z,riftNeighborhood(X,Z),false):riftAt(X,Z);if(rift.cut>.6)return d;lift=snowBodyLift(n,m,rift);}
 d.base=s.height+int(std::floor((continuousGlacierBodies?s.superGlacier:smooth(.35,.75,s.superGlacier))*(glacierEcologyEnabled&&s.glacier<.65?std::max(0.,(.65/.47)*(s.glacier-.18)/std::max(.001,s.glacier)):1.)*std::max(3.,lift)))-1;
 d.height=8+int(15*r(51511));double angle=6.283185307179586*r(51513),lean=d.height*(.22+.30*r(51515));
 d.ax=lean*std::cos(angle);d.az=lean*std::sin(angle);
 auto tip=field.surface(X+int(std::round(d.ax))+32768,Z+int(std::round(d.az))+32768,false);
 if(!eligible(tip)||std::abs(tip.height-s.height)>8)return d;
 double rx=2.3+1.8*r(51517),rz=rx*(.55+.35*r(51519));std::array<double,5> vx{},vz{};
 for(int k=0;k<5;++k){double a=angle+6.283185307179586*k/5,j=.88+.12*r(51521+k);vx[k]=rx*std::cos(a)*j;vz[k]=rz*std::sin(a)*j;}
 for(int k=0;k<5;++k){int next=(k+1)%5;double dx=vx[next]-vx[k],dz=vz[next]-vz[k],len=std::hypot(dx,dz);d.nx[k]=dz/len;d.nz[k]=-dx/len;d.distance[k]=d.nx[k]*vx[k]+d.nz[k]*vz[k];}
 d.valid=true;return d;
});}
KARMA_NOINLINE void smallIceSpikes(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 bool any=false;for(auto const&s:surfaces)any|=s.superGlacier>.65;if(!any)return;
 constexpr int grid=24,halo=18;
 for(int gz=int(std::floor((z0-32768-halo)/double(grid)));gz<=int(std::floor((z0-32768+15+halo)/double(grid)));++gz)
 for(int gx=int(std::floor((x0-32768-halo)/double(grid)));gx<=int(std::floor((x0-32768+15+halo)/double(grid)));++gx){
  auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
  // Coarse patches leave irregular gaps instead of a uniform field of needles.
  double chance=.35+.35*hash01(int(std::floor(gx/3.)),int(std::floor(gz/3.)),terrainSeed^51500);
  if(r(51505)>chance)continue;
  int X=int(std::floor((gx+.12+.76*r(51501))*grid)),Z=int(std::floor((gz+.12+.76*r(51503))*grid));
  if(X+halo<x0-32768||X-halo>x0+15-32768||Z+halo<z0-32768||Z-halo>z0+15-32768)continue;
  double h=8+int(15*r(51511)),angle=6.283185307179586*r(51513),lean=h*(.22+.30*r(51515));
  double ax=lean*std::cos(angle),az=lean*std::sin(angle);
  // Reject non-intersecting swept bounds before any terrain sampling.
  if(X+std::max(5.5,ax+1.5)<x0-32768||X+std::min(-5.5,ax-1.5)>x0+15-32768||Z+std::max(5.5,az+1.5)<z0-32768||Z+std::min(-5.5,az-1.5)>z0+15-32768)continue;
  auto d=smallIceDescriptor(gx,gz,X,Z);if(!d.valid)continue;
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;auto const&s=surfaces[i];
   if(s.superGlacier<.65||s.kind||s.river||s.lake||s.water>=s.height)continue;
   double dx=x0+x-32768-X,dz=z0+z-32768-Z;
   if(dx<std::min(-5.5,d.ax-1.5)||dx>std::max(5.5,d.ax+1.5)||dz<std::min(-5.5,d.az-1.5)||dz>std::max(5.5,d.az+1.5))continue;
   auto interval=[&](double inset){double lo=0,hi=1;
    for(int k=0;k<5;++k){double a=d.nx[k]*dx+d.nz[k]*dz+inset-d.distance[k],b=d.distance[k]-d.nx[k]*d.ax-d.nz[k]*d.az;
     if(std::abs(b)<1e-12){if(a>0)hi=-1;}else if(b>0)hi=std::min(hi,-a/b);else lo=std::max(lo,-a/b);
    }
    if(hi<lo||hi<0||lo>1)return std::pair{320,-64};
    return std::pair{d.base+int(std::ceil(d.height*clamp(lo,0.,1.))),d.base+int(std::floor(d.height*clamp(hi,0.,1.)))};
   };
   auto outer=interval(-.8);if(outer.first>outer.second)continue;
   auto mantle=interval(.6),core=interval(1.6);auto col=raw.data()+i*Height;
   for(int y=std::max(s.height+1,outer.first);y<=std::min(319,outer.second);++y){
    if(col[y+64])continue; // Preserve the accepted large cones and all solids.
    col[y+64]=uint16_t(y<mantle.first||y>mantle.second?9:y>=core.first&&y<=core.second&&y<d.base+d.height*.65?144:143);
   }
  }
 }
}
