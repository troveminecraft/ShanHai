struct IceSpireDescriptor{bool valid=false;int base=0,height=0;double ax=0,az=0;std::array<double,7> nx{},nz{},distance{};};
Cache<uint64_t,IceSpireDescriptor> iceSpireDescriptors{256};
IceSpireDescriptor iceSpireDescriptor(int gx,int gz,int X,int Z){return iceSpireDescriptors.get(key2(gx,gz),[&]{
 IceSpireDescriptor out;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 if(slantedGlaciers&&r(51305)>.70)return out;
  auto s=field.surface(X+32768,Z+32768,false);bool large=s.superGlacier>.65;
  if(s.glacier<.65||s.river||s.lake||s.water>=s.height||s.scarp>.01||s.cliffLow!=-32768||r(51305)>(large?.70:.35))return out;
  if(snowRifts&&(closedIceRifts?riftSample(X,Z,riftNeighborhood(X,Z),false):riftAt(X,Z)).cut>.12)return out;
  bool grounded=field.elevationVariantsEnabled();
  double rx=large?(grounded?24+14*r(51307):11+9*r(51307)):3+3*r(51307),rz=rx*(.42+.35*r(51309));
  bool tall=C.value("tallIceSpires",false);
  int height=std::min(large?(grounded&&!tall?48+int(40*r(51311)):88+int(72*r(51311))):16+int(27*r(51311)),Height-74-s.height);
  if(height<=0)return out;
  double phase=r(51313)*6.283185307179586;
  std::array<double,7> vx{},vz{},nx{},nz{},distance{};
  for(int k=0;k<7;++k){double a=phase+6.283185307179586*k/7,j=.84+.16*r(51320+k);vx[k]=rx*std::cos(a)*j;vz[k]=rz*std::sin(a)*j;}
  // Legacy apex stays inside the base; slanted profiles move it with height.
  double lean=grounded?std::min(rx,rz)*(.12+.12*r(51341)):height*(.40+.30*r(51341));
  double ax=slantedGlaciers?lean*std::cos(phase+.7):.48*rx*std::cos(phase+.7),az=slantedGlaciers?lean*std::sin(phase+.7):.48*rz*std::sin(phase+.7);
  for(int k=0;k<7;++k){int next=(k+1)%7;double dx=vx[next]-vx[k],dz=vz[next]-vz[k],len=std::hypot(dx,dz);nx[k]=dz/len;nz[k]=-dx/len;distance[k]=nx[k]*vx[k]+nz[k]*vz[k];}
  bool valid=true;
  for(int k=0;k<7;++k){auto t=field.surface(X+int(std::round(vx[k]))+32768,Z+int(std::round(vz[k]))+32768,false);if(t.glacier<.45||t.river||t.lake||t.water>=t.height||t.volcanic>.01||std::abs(t.height-s.height)>12)valid=false;}
  if(slantedGlaciers)for(double u:{.25,.5,.75,1.}){auto t=field.surface(X+int(std::round(ax*u))+32768,Z+int(std::round(az*u))+32768,false);if(t.glacier<.45||t.river||t.lake||t.water>=t.height||t.volcanic>.01)valid=false;}
  if(!valid)return out;
  out.valid=true;out.base=s.height;out.height=height;out.ax=ax;out.az=az;out.nx=nx;out.nz=nz;out.distance=distance;return out;
 });}
// Irregular polygon sections: slanted profiles clip both vertical ends,
// while the legacy profile uses nested sections down to its ground.
// Geometry is clipped once per column/section, never sampled with 3-D noise.
KARMA_NOINLINE void refinedIceSpires(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 glacierBody(x0,z0,raw,surfaces);
 constexpr int grid=96,N=7;int halo=slantedGlaciers?116:28;
 for(int gz=int(std::floor((z0-32768-halo)/double(grid)));gz<=int(std::floor((z0-32768+15+halo)/double(grid)));++gz)
 for(int gx=int(std::floor((x0-32768-halo)/double(grid)));gx<=int(std::floor((x0-32768+15+halo)/double(grid)));++gx){
  auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
  int X=int(std::floor((gx+.18+.64*r(51301))*grid)),Z=int(std::floor((gz+.18+.64*r(51303))*grid));
  if(slantedGlaciers&&!field.elevationVariantsEnabled()){double h=88+int(72*r(51311)),lean=h*(.40+.30*r(51341)),phase=r(51313)*6.283185307179586+.7;
   double ax=lean*std::cos(phase),az=lean*std::sin(phase);
   if(X+std::max(22.,ax+2)<x0-32768||X+std::min(-22.,ax-2)>x0+15-32768||Z+std::max(22.,az+2)<z0-32768||Z+std::min(-22.,az-2)>z0+15-32768)continue;
  }else {int reach=field.elevationVariantsEnabled()?40:27;if(X+reach<x0-32768||X-reach>x0+15-32768||Z+reach<z0-32768||Z-reach>z0+15-32768)continue;}
  auto const d=iceSpireDescriptor(gx,gz,X,Z);if(!d.valid)continue;
  auto const&nx=d.nx;auto const&nz=d.nz;auto const&distance=d.distance;
  int height=d.height,base=d.base;double ax=d.ax,az=d.az;
  std::array<double,N> q{};for(int k=0;k<N;++k)q[k]=nx[k]*ax+nz[k]*az;
  constexpr std::array<double,5> levels{0,.32,.62,.87,1},scales{1,.68,.38,.14,0};
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;auto const&t=surfaces[i];
   if(t.glacier<.35||t.river||t.lake||t.water>=t.height)continue;
   double dx=x0+x-32768-X,dz=z0+z-32768-Z;
   std::array<double,N> p{};bool inside=true,possible=true;
   for(int k=0;k<N;++k){p[k]=nx[k]*dx+nz[k]*dz;if(p[k]>distance[k])inside=false;if(p[k]>std::max(distance[k],q[k])+1.2)possible=false;}
   auto col=raw.data()+i*Height;
   if(slantedGlaciers&&!field.elevationVariantsEnabled()){
    if(!possible)continue;
    // Sweep the whole cross-section sideways with height. Clip BOTH ends
    // of each column interval: filling down to the ground would straighten it.
    for(int seg=0;seg<4;++seg){
     auto interval=[&](double inset){double lo=0,hi=1;
      for(int k=0;k<N;++k){double a=p[k]+inset-q[k]*levels[seg]-distance[k]*scales[seg];double b=-q[k]*(levels[seg+1]-levels[seg])-distance[k]*(scales[seg+1]-scales[seg]);
       if(std::abs(b)<1e-12){if(a>0)hi=-1;}else if(b>0)hi=std::min(hi,-a/b);else lo=std::max(lo,-a/b);
      }
      if(hi<lo||hi<0||lo>1)return std::pair{Height-64,-64};
      return std::pair{base+int(std::ceil(height*(levels[seg]+(levels[seg+1]-levels[seg])*clamp(lo,0.,1.)))),base+int(std::floor(height*(levels[seg]+(levels[seg+1]-levels[seg])*clamp(hi,0.,1.))))};
     };
     // A sub-block conservative rim keeps a tilted voxel tip face-connected.
     auto outer=interval(-.9);if(outer.first>outer.second)continue;
     auto mantle=interval(.45),core=interval(2.5);core.second=std::min(core.second,base+int(std::ceil(height*.76))-1);
     int offset=int(r(51331)*19);
     for(int y=std::max(t.height+1,outer.first);y<=std::min(Height-65,outer.second);++y){uint16_t b=y<mantle.first||y>mantle.second?9:y>=core.first&&y<=core.second&&(y+offset)%23<15?144:143;
      if(col[y+64]!=144&&(b!=9||col[y+64]!=143))col[y+64]=b;
     }
    }
    continue;
   }
   if(!inside)continue;
   // Clip each inset shell once. The sections are nested, so material
   // transitions are column intervals; no seven-plane test per ice voxel.
   auto layerTop=[&](double inset){
    for(int k=0;k<N;++k)if(p[k]+inset>distance[k])return -32768;
    double topU=0;
    for(int seg=0;seg<4;++seg){double lo=0,hi=1;
     for(int k=0;k<N;++k){double a=p[k]+inset-q[k]*(1-scales[seg])-distance[k]*scales[seg],b=(q[k]-distance[k])*(scales[seg+1]-scales[seg]);
      if(std::abs(b)<1e-12){if(a>0)hi=-1;}else if(b>0)hi=std::min(hi,-a/b);else lo=std::max(lo,-a/b);
     }
     if(hi>=lo&&hi>=0&&lo<=1)topU=levels[seg]+(levels[seg+1]-levels[seg])*clamp(hi,0.,1.);
    }
    return base+int(std::floor(height*topU));
   };
   int top=layerTop(0),mantleTop=layerTop(1.35),coreTop=std::min(layerTop(3.4),base+int(std::ceil(height*.76))-1),bandOffset=int(r(51331)*19);
   for(int y=t.height+1;y<=std::min(top,Height-65);++y)col[y+64]=uint16_t(y>mantleTop||y==top?9:y<=coreTop&&((y+bandOffset)%23<15)?144:143);
  }
 }
}
