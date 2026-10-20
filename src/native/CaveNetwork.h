#pragma once
#include "Noise.h"

namespace nativegen {
// Seed-owned curved passages, branches and compound chambers. This is an
// independent implementation of the general worm-cave idea, not copied game
// source. Geometry is bounded; a chunk never waits for its neighbours.
class CaveNetwork {
 struct Point { double x,y,z; };
 struct Blob { Point p; double rx,ry,rz; };
 using Blobs=std::vector<Blob>;
 using Mask=std::array<std::array<uint64_t,6>,256>;
 struct Nodes { std::array<Point,2> p; };
 uint32_t seed;
 static constexpr int Pitch=112;
 Cache<uint64_t,Nodes> nodes{256};
 Cache<uint64_t,std::shared_ptr<Blobs>> regions{64};
 Cache<uint64_t,std::shared_ptr<Mask>> chunks{64};
 std::shared_ptr<Mask> active;
 int activeX=INT32_MIN,activeZ=INT32_MIN;
 double rnd(int gx,int gz,int salt)const {return hash01(gx,gz,seedHash(seed,2400+salt));}
 static Point lerp(Point a,Point b,double t){return {mix(t,a.x,b.x),mix(t,a.y,b.y),mix(t,a.z,b.z)};}

 template<class Height> Nodes anchors(int gx,int gz,Height& height){
  return nodes.get(key2(gx,gz),[&]{
   Nodes a;
   double x=(gx+.5)*Pitch+(rnd(gx,gz,1)-.5)*66;
   double z=(gz+.5)*Pitch+(rnd(gx,gz,2)-.5)*66;
   double surface=height(int(x),int(z));
   // Deep passages reach the aquifer, but their lowest rounded floor stays
   // above the rock blanket. The upper system follows available rock depth.
   a.p[0]={x,-45+16*rnd(gx,gz,3),z};
   a.p[1]={x+(rnd(gx,gz,4)-.5)*28,
           std::max(-12.,mix(.40+.27*rnd(gx,gz,5),-12.,std::min(180.,surface-32))),
           z+(rnd(gx,gz,6)-.5)*28};
   return a;
  });
 }
 void chamber(Blobs& out,Point p,int gx,int gz,int salt){
  double rx=13+13*rnd(gx,gz,salt),rz=11+12*rnd(gx,gz,salt+1),ry=7+7*rnd(gx,gz,salt+2);
  // Several overlapping asymmetric lobes avoid a single spherical room.
  for(int i=0;i<4;++i){
   double angle=rnd(gx,gz,salt+3+i)*6.283185307179586;
   Point q={p.x+std::cos(angle)*rx*.38,p.y+(rnd(gx,gz,salt+8+i)-.5)*3,p.z+std::sin(angle)*rz*.38};
   double scale=.72+.22*rnd(gx,gz,salt+13+i);
   double h=ry*scale;q.y=std::max(-55.+h,q.y);
   out.push_back({q,rx*scale,h,rz*scale});
  }
 }
 void passage(Blobs& out,Point a,Point b,int gx,int gz,int salt,double size=1){
  double dx=b.x-a.x,dz=b.z-a.z,length=std::hypot(dx,dz);
  double nx=length>1?-dz/length:1,nz=length>1?dx/length:0;
  double bend=(rnd(gx,gz,salt)-.5)*46,phase=rnd(gx,gz,salt+1)*6.283185307179586;
  double radius=(4.3+2.6*rnd(gx,gz,salt+2))*size;
  int count=std::max(16,int(std::ceil((length+std::abs(b.y-a.y)+80)/1.7)));
  for(int i=0;i<=count;++i){
   double t=double(i)/count,envelope=std::sin(t*3.141592653589793);
   double bendAt=envelope*(bend+7*std::sin(t*6.283185307179586+phase));
   Point p=lerp(a,b,t);p.x+=nx*bendAt;p.z+=nz*bendAt;
   p.y+=envelope*(2.8*std::sin(t*6.283185307179586+phase));
   double r=radius*(1+.18*std::sin(t*12.566370614359172+phase));
   double ry=std::max(2.2,r*(.66+.12*std::sin(t*9.42477796076938+phase)));
   p.y=std::max(-55.+ry,p.y);
   out.push_back({p,r,ry,r*(.89+.13*std::sin(t*6.283185307179586-phase))});
  }
 }
 template<class Height> std::shared_ptr<Blobs> region(int gx,int gz,Height& height){
  return regions.get(key2(gx,gz),[&]{
   auto out=std::make_shared<Blobs>();out->reserve(1200);
   auto a=anchors(gx,gz,height),east=anchors(gx+1,gz,height),south=anchors(gx,gz+1,height);
   for(int layer=0;layer<2;++layer){
    int salt=100+layer*100;
    // Every node has an outgoing passage, and some have a second route.
    // The edge belongs only to its west/north owner, independent of load order.
    bool e=rnd(gx,gz,salt)<.5;
    passage(*out,a.p[layer],e?east.p[layer]:south.p[layer],gx,gz,salt+1);
    if(rnd(gx,gz,salt+5)<.46)
     passage(*out,a.p[layer],e?south.p[layer]:east.p[layer],gx,gz,salt+6);
    if(rnd(gx,gz,salt+10)<.70)chamber(*out,a.p[layer],gx,gz,salt+11);
    // Short blind branches grow out of the actual junction, not isolated voids.
    double angle=rnd(gx,gz,salt+35)*6.283185307179586,len=20+29*rnd(gx,gz,salt+36);
    Point end={a.p[layer].x+std::cos(angle)*len,a.p[layer].y+(rnd(gx,gz,salt+37)-.5)*15,a.p[layer].z+std::sin(angle)*len};
    passage(*out,a.p[layer],end,gx,gz,salt+38,.73);
   }
   // Gradual rising connectors join deep and upper networks; a lateral bend
   // provides horizontal run even when their anchors almost coincide.
   if(rnd(gx,gz,320)<.64){
    auto mid=lerp(a.p[0],a.p[1],.5);double angle=rnd(gx,gz,321)*6.283185307179586;
    mid.x+=std::cos(angle)*34;mid.z+=std::sin(angle)*34;
    passage(*out,a.p[0],mid,gx,gz,322,.86);
    passage(*out,mid,a.p[1],gx,gz,327,.86);
   }
   return out;
  });
 }
 static void setRange(std::array<uint64_t,6>& col,int lo,int hi){
  lo=std::max(-55,lo)+64;hi=std::min(319,hi)+64;if(lo>hi)return;
  int first=lo/64,last=hi/64;
  for(int w=first;w<=last;++w){
   int a=w==first?lo%64:0,b=w==last?hi%64:63;
   col[w]|=(~uint64_t(0)<<a)&(~uint64_t(0)>>(63-b));
  }
 }
 template<class Height> std::shared_ptr<Mask> raster(int cx,int cz,Height& height){
  auto out=std::make_shared<Mask>();
  int x0=cx*16,z0=cz*16,gx=int(std::floor(double(x0)/Pitch)),gz=int(std::floor(double(z0)/Pitch));
  // Anchors +/-33, branches <=49, bend <=30, chamber reach <35: all
  // primitives lie inside the owner's tile plus one adjacent tile. Two cells
  // of search cover edges crossing either side, including negative positions.
  for(int iz=gz-2;iz<=gz+1;++iz)for(int ix=gx-2;ix<=gx+1;++ix){
   auto blobs=region(ix,iz,height);
   for(auto const& b:*blobs){
    if(b.p.x+b.rx<x0||b.p.x-b.rx>x0+15||b.p.z+b.rz<z0||b.p.z-b.rz>z0+15)continue;
    int l=std::max(x0,int(std::ceil(b.p.x-b.rx))),r=std::min(x0+15,int(std::floor(b.p.x+b.rx)));
    int n=std::max(z0,int(std::ceil(b.p.z-b.rz))),s=std::min(z0+15,int(std::floor(b.p.z+b.rz)));
    for(int z=n;z<=s;++z)for(int x=l;x<=r;++x){
     double dx=(x-b.p.x)/b.rx,dz=(z-b.p.z)/b.rz,q=1-dx*dx-dz*dz;
     if(q<=0)continue;double span=b.ry*std::sqrt(q);
     setRange((*out)[(z-z0)*16+x-x0],int(std::ceil(b.p.y-span)),int(std::floor(b.p.y+span)));
    }
   }
  }
  return out;
 }
public:
 explicit CaveNetwork(uint32_t s):seed(s){}
 template<class Height> bool contains(int x,int y,int z,Height height){
  if(y<-55||y>319)return false;
  int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
  if(cx!=activeX||cz!=activeZ){
   active=chunks.get(key2(cx,cz),[&]{return raster(cx,cz,height);});activeX=cx;activeZ=cz;
  }
  return ((*active)[(z-cz*16)*16+x-cx*16][(y+64)/64]>>((y+64)%64))&1;
 }
};
}
