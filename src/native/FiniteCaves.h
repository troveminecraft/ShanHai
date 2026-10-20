#pragma once
#include "Noise.h"
namespace nativegen {
// Finite, independent cave systems. Swept asymmetric rock sections replace
// unions of ellipsoids; global XY wall/floor detail is evaluated once per column.
class FiniteCaves {
 struct Point {double x,y,z;};
 struct Knot {Point p;double left,right,roof,depth;};
 struct Span {Knot a,b;double pad;};
 struct System {Point root;std::vector<Span> spans;bool hall=false;double rx=0,rz=0,tall=0,angle=0;double minX=INFINITY,maxX=-INFINITY,minZ=INFINITY,maxZ=-INFINITY;};
 using Mask=std::array<std::array<uint64_t,6>,256>;
 static constexpr int Pitch=352;
 uint32_t seed;Perlin rock;bool halls=false;
 Cache<uint64_t,std::shared_ptr<System>> systems{64};
 Cache<uint64_t,std::shared_ptr<Mask>> masks{64};
 std::shared_ptr<Mask> active;int activeX=INT32_MIN,activeZ=INT32_MIN;
 double rnd(int x,int z,int salt)const{return hash01(x,z,seedHash(seed,3600+salt));}
 static Knot blend(Knot a,Knot b,double t){return {{mix(t,a.p.x,b.p.x),mix(t,a.p.y,b.p.y),mix(t,a.p.z,b.p.z)},mix(t,a.left,b.left),mix(t,a.right,b.right),mix(t,a.roof,b.roof),mix(t,a.depth,b.depth)};}
 Knot shape(Point p,int gx,int gz,int salt,bool wide=false){
  double type=rnd(gx,gz,salt),r=1.8+1.4*rnd(gx,gz,salt+1);
  double roof=2.5+1.5*rnd(gx,gz,salt+2),depth=1.1+.8*rnd(gx,gz,salt+3);
  if(type<.26){r*=.7;roof+=1.8;} // narrow, taller fissure
  if(type>.78){r*=1.3;roof*=.8;} // broader, lower bedding passage
  if(wide){r=5+2*rnd(gx,gz,salt+4);roof=4+2*rnd(gx,gz,salt+5);}
  return {p,r*(.8+.3*rnd(gx,gz,salt+6)),r*(.8+.3*rnd(gx,gz,salt+7)),roof,depth};
 }
 void sweep(System& s,Knot a,Knot b,double bend){
  double dx=b.p.x-a.p.x,dz=b.p.z-a.p.z,len=std::hypot(dx,dz);if(len<.5)return;
  int n=std::max(2,int(std::ceil(len/2.)));
  Knot prev=a;
  for(int i=1;i<=n;++i){
   double t=double(i)/n;Knot next=blend(a,b,t);
   double w=std::sin(t*3.141592653589793)*bend;
   next.p.x-=dz/len*w;next.p.z+=dx/len*w;
   // Pad includes the maximum permitted roughness multiplier. It is also
   // used by the inventory to verify system separation independently.
   double pad=1.3*std::max({prev.left,prev.right,next.left,next.right})+1;
   s.minX=std::min(s.minX,std::min(prev.p.x,next.p.x)-pad);s.maxX=std::max(s.maxX,std::max(prev.p.x,next.p.x)+pad);
   s.minZ=std::min(s.minZ,std::min(prev.p.z,next.p.z)-pad);s.maxZ=std::max(s.maxZ,std::max(prev.p.z,next.p.z)+pad);
   s.spans.push_back({prev,next,pad});prev=next;
  }
 }
 void arm(System& s,Point origin,double angle,int gx,int gz,int salt,int steps,bool fork){
  std::vector<Knot> knots{shape(origin,gx,gz,salt)};
  for(int j=0;j<steps;++j){
   angle+=(rnd(gx,gz,salt+20+j*13)-.5)*1.35;
   double length=20+17*rnd(gx,gz,salt+21+j*13);
   auto p=knots.back().p;p.x+=std::cos(angle)*length;p.z+=std::sin(angle)*length;
   p.y=clamp(p.y+(rnd(gx,gz,salt+22+j*13)-.5)*7,-50.,170.);
   // Stop and taper before leaving the system. Never carve to a tile wall.
   if(std::hypot(p.x-s.root.x,p.z-s.root.z)>116)break;
   bool wide=j==1&&rnd(gx,gz,salt+23)<.42;
   knots.push_back(shape(p,gx,gz,salt+30+j*13,wide));
  }
  if(knots.size()<2)return;
  // A blind end contracts over its final section instead of a round end-cap.
  auto& end=knots.back();end.left=end.right=.25;end.roof=.45;end.depth=.35;
  for(size_t i=1;i<knots.size();++i)sweep(s,knots[i-1],knots[i],(rnd(gx,gz,salt+90+int(i))-.5)*7);
  if(fork&&knots.size()>2){
   auto p=knots[1].p;
   arm(s,p,angle+(rnd(gx,gz,salt+96)<.5?-1.3:1.3),gx,gz,salt+110,2,false);
  }
 }
 template<class Height> std::shared_ptr<System> system(int gx,int gz,Height& height){
  return systems.get(key2(gx,gz),[&]{
   auto s=std::make_shared<System>();s->spans.reserve(350);
   s->root={(gx+.5)*Pitch+(rnd(gx,gz,1)-.5)*32,-46+9*rnd(gx,gz,2),(gz+.5)*Pitch+(rnd(gx,gz,3)-.5)*32};
   if(rnd(gx,gz,4)<.12)return s;
   double angle=rnd(gx,gz,5)*6.283185307179586;
   int arms=2+(rnd(gx,gz,6)<.65?1:0);
   for(int a=0;a<arms;++a)arm(*s,s->root,angle+a*6.283185307179586/arms,gx,gz,200+a*250,3+int(rnd(gx,gz,7+a)*2),a==0);
   // Some systems have an upper branch, not a second universal world network.
   if(rnd(gx,gz,10)<.62){
    Point up=s->root;up.x+=std::cos(angle+.8)*32;up.z+=std::sin(angle+.8)*32;
    double h=height(int(up.x),int(up.z));up.y=std::max(-18.,mix(.42,-12.,std::min(135.,h-32)));
    auto a=shape(s->root,gx,gz,1000),b=shape(up,gx,gz,1020);
    auto mid=blend(a,b,.5);mid.p.x+=std::cos(angle-1)*26;mid.p.z+=std::sin(angle-1)*26;
    sweep(*s,a,mid,3);sweep(*s,mid,b,-3);
    arm(*s,up,angle+1.8,gx,gz,1100,3,true);
   }
   // Rare broad rooms belong to a finite system, not a second connected grid.
   if(halls&&rnd(gx,gz,1500)<.10&&height(int(s->root.x),int(s->root.z))>55){
    s->hall=true;s->rx=27+17*rnd(gx,gz,1501);s->rz=s->rx*(.78+.18*rnd(gx,gz,1502));
    s->tall=29+15*rnd(gx,gz,1503);s->angle=rnd(gx,gz,1504)*6.283185307179586;
    double reach=s->rx*1.18;
    s->minX=std::min(s->minX,s->root.x-reach);s->maxX=std::max(s->maxX,s->root.x+reach);
    s->minZ=std::min(s->minZ,s->root.z-reach);s->maxZ=std::max(s->maxZ,s->root.z+reach);
   }
   // Construction keeps knots within 116 blocks; bend <=3.5 and the widest
   // section + detail add <12. This catches future parameter changes that
   // could silently join separate systems.
   if(!s->spans.empty()&&(s->minX<s->root.x-132||s->maxX>s->root.x+132||s->minZ<s->root.z-132||s->maxZ>s->root.z+132))
    throw std::runtime_error("Finite cave system exceeded its support bound");
   return s;
  });
 }
 static void setRange(std::array<uint64_t,6>& col,int lo,int hi){
  lo=std::max(-55,lo)+64;hi=std::min(319,hi)+64;if(lo>hi)return;
  for(int w=lo/64;w<=hi/64;++w){int a=w==lo/64?lo%64:0,b=w==hi/64?hi%64:63;col[w]|=(~uint64_t(0)<<a)&(~uint64_t(0)>>(63-b));}
 }
 template<class Height> std::shared_ptr<Mask> raster(int cx,int cz,Height& height){
  auto out=std::make_shared<Mask>();int x0=cx*16,z0=cz*16;
  int gx=int(std::floor(double(x0)/Pitch)),gz=int(std::floor(double(z0)/Pitch));
  // Minimum anchor separation 320, maximum support diameter 264: systems
  // cannot touch. These are geometric bounds, not hard clipping planes.
  auto s=system(gx,gz,height);if(s->spans.empty()||s->maxX<x0||s->minX>x0+15||s->maxZ<z0||s->minZ>z0+15)return out;
  std::array<double,256> wall{},floor{},ceiling{};
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){
   int i=z*16+x;double X=x+x0,Z=z+z0;
   wall[i]=clamp(.20*rock.at(X/13.,Z/13.)+.08*rock.at(X/4.5+17,Z/4.5-9),-.3,.3);
   floor[i]=.65*rock.at(X/11.-31,Z/11.+23);
   ceiling[i]=.85*rock.at(X/7.+11,Z/7.-17);
  }
  for(auto const& a:s->spans){
   double dx=a.b.p.x-a.a.p.x,dz=a.b.p.z-a.a.p.z,len2=dx*dx+dz*dz,len=std::sqrt(len2);
   int l=std::max(x0,int(std::ceil(std::min(a.a.p.x,a.b.p.x)-a.pad))),r=std::min(x0+15,int(std::floor(std::max(a.a.p.x,a.b.p.x)+a.pad)));
   int n=std::max(z0,int(std::ceil(std::min(a.a.p.z,a.b.p.z)-a.pad))),b=std::min(z0+15,int(std::floor(std::max(a.a.p.z,a.b.p.z)+a.pad)));
   for(int z=n;z<=b;++z)for(int x=l;x<=r;++x){
    double t=((x-a.a.p.x)*dx+(z-a.a.p.z)*dz)/len2;
    if(t<-.08||t>1.08)continue;t=clamp(t,0,1);
    int i=(z-z0)*16+x-x0;auto k=blend(a.a,a.b,t);
    double side=((x-k.p.x)*(-dz)+(z-k.p.z)*dx)/len;
    double width=(side<0?k.left:k.right)*(1+wall[i]),q=std::abs(side)/width;
    if(q>=1)continue;
    // Unequal shoulders, a weathered roof and a shallow irregular floor.
    // No ellipsoids, spherical room union, or extra per-voxel 3-D noise.
    double low=k.p.y-k.depth*(1-.45*q*q)+floor[i];
    double high=k.p.y+k.roof*(1-.72*q*q)+ceiling[i]*(1-.3*q);
    setRange((*out)[i],int(std::ceil(low)),int(std::floor(high)));
   }
  }
  if(s->hall){
   double cs=std::cos(s->angle),sn=std::sin(s->angle);
   for(int z=0;z<16;++z)for(int x=0;x<16;++x){
    int i=z*16+x;double X=x0+x,Z=z0+z,dx=X-s->root.x,dz=Z-s->root.z;
    double u=cs*dx+sn*dz,v=-sn*dx+cs*dz;
    double radius=std::hypot(u/s->rx,v/s->rz);
    if(radius>1.17)continue;
    double edge=radius-.10*rock.at(X/19.,Z/19.)-.035*std::sin(std::atan2(v,u)*5+s->angle);
    if(edge>=1)continue;
    double w=1-smooth(.70,1.,edge);
    // Irregular bedding and broad raised benches; a high arch rather than
    // a giant sphere. The root stays open to the existing side passages.
    double bench=7*smooth(.0,.4,rock.at(X/29.+31,Z/29.-17))*smooth(8,20,std::hypot(dx,dz));
    double low=s->root.y-3+bench+floor[i]*2;
    double vault=4*rock.at(X/23.-47,Z/23.+19)+ceiling[i]*2;
    double high=s->root.y-3+s->tall*std::pow(w,.45)+vault*w;
    if(high-low<1)continue;
    int lo=int(std::ceil(low)),hi=int(std::floor(high));
    setRange((*out)[i],lo,hi);
    // Two tapered load-bearing columns. They join floor and roof, never
    // floating boulders, and leave the central junction clear.
    for(int p=0;p<2;++p){
     double px=(p?-.30:.37)*s->rx,pz=(p?.26:-.22)*s->rz;
     double d=std::hypot(u-px,v-pz);
     if(d>5)continue;
     for(int y=std::max(-55,lo);y<=hi;++y){
      double t=(y-low)/std::max(1.,high-low),r=2.3+2.1*std::pow(2*t-1,2)+wall[i];
      if(d<r)(*out)[i][(y+64)/64]&=~(uint64_t(1)<<((y+64)%64));
     }
    }
   }
  }
  return out;
 }
public:
 explicit FiniteCaves(uint32_t s):seed(s),rock(seedHash(s,3501)){}
 void enableHalls(bool value){halls=value;}
 template<class Height> bool contains(int x,int y,int z,Height height){
  if(y<-55||y>319)return false;int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
  if(cx!=activeX||cz!=activeZ){active=masks.get(key2(cx,cz),[&]{return raster(cx,cz,height);});activeX=cx;activeZ=cz;}
  return ((*active)[(z-cz*16)*16+x-cx*16][(y+64)/64]>>((y+64)%64))&1;
 }
 template<class Height> J inventory(int lo,int hi,Height height){
  J rows=J::array();
  for(int z=int(std::floor(double(lo)/Pitch));z<=int(std::floor(double(hi)/Pitch));++z)
   for(int x=int(std::floor(double(lo)/Pitch));x<=int(std::floor(double(hi)/Pitch));++x){
    auto s=system(x,z,height);if(s->spans.empty())continue;
    rows.push_back({{"x",s->root.x},{"y",s->root.y},{"z",s->root.z},{"minX",s->minX},{"maxX",s->maxX},{"minZ",s->minZ},{"maxZ",s->maxZ},{"spans",s->spans.size()}});
    if(halls){rows.back()["hall"]=s->hall;if(s->hall){rows.back()["radiusX"]=s->rx;rows.back()["radiusZ"]=s->rz;rows.back()["height"]=s->tall;}}
   }
  return rows;
 }
};
}
