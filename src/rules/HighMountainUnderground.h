#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Only the added mountain volume gets these rules. Original lowland geology,
// deep ore heights and lava levels keep their existing world-space anchors.
namespace high_mountain_underground {
inline double ramp(double low,double high,double value){
 double t=std::clamp((value-low)/(high-low),0.,1.);return t*t*(3-2*t);
}
template<class Surface> double weight(Surface const&s){
 // Older generic structure adapters have no mountain fields. They continue
 // using their original scan range until they provide the complete habitat.
 if constexpr(requires{s.mount;s.glacier;s.superGlacier;s.iceHabitat;s.stone;s.volcanic;s.vent;s.scarp;s.cliffLow;}){
  if(s.height<=180||s.mount<.35||s.glacier>.01||s.superGlacier>.01||s.iceHabitat>.01||
     s.stone>.01||s.volcanic>.01||s.vent||s.scarp>.01||s.cliffLow>-32768)return 0;
  return ramp(180,260,s.height)*ramp(.35,.55,s.mount);
 }else return 0;
}
template<class Surface> int scanTop(Surface const&s,bool enabled,int maxY,int legacyTop,int cover){
 int oldTop=std::min(legacyTop,s.height-cover);
 double amount=enabled&&maxY==511?weight(s):0;
 if(amount<=0)return oldTop;
 int high=std::min(s.height-cover,maxY-2);
 return oldTop+int(std::floor(amount*std::max(0,high-oldTop)));
}
inline uint16_t upperRock(int y,double broad,double fold,double habitat,double grain,double amount){
 if(y<=120||amount<=0)return 2;
 // Sparse folded veins repeat with altitude without copying the deep-slate
 // transition or replacing whole mountain interiors with decorative stone.
 double phase=std::remainder(y+18*broad+8*fold,72.);
 double seam=(1-ramp(3,9,std::abs(phase)))*ramp(120,152,y)*amount;
 if(grain>=seam*.72)return 2;
 return uint16_t(broad>.16?97:broad<-.16?98:habitat<-.08?99:2);
}
// Water occupies one former floor layer. Require two solid layers below it,
// an unbroken sidewall at water level, and a genuine rock ceiling over every
// wet column. The complete footprint must belong to this chunk before reads.
template<class Read,class Rock,class SurfaceAt>
bool poolFits(int x,int y,int z,int radius,int maxY,Read read,Rock rock,SurfaceAt surface){
 if(radius<2||radius>3||x-radius-1<0||x+radius+1>=16||z-radius-1<0||z+radius+1>=16||
    y<83||y+3>maxY)return false;
 int outer=(radius+1)*(radius+1);
 for(int dz=-radius-1;dz<=radius+1;++dz)for(int dx=-radius-1;dx<=radius+1;++dx){
  if(dx*dx+dz*dz>outer)continue;
  int X=x+dx,Z=z+dz;
  if(!rock(read(X,y-1,Z))||!rock(read(X,y-2,Z))||!rock(read(X,y-3,Z)))return false;
  if(dx*dx+dz*dz>=radius*radius)continue;
  if(read(X,y,Z)!=0||read(X,y+1,Z)!=0)return false;
  auto const&s=surface(X,Z);int roofTop=std::min({y+32,s.height-8,maxY-2});
  bool roof=false;
  for(int Y=y+2;Y<=roofTop;++Y){int b=read(X,Y,Z);
   if(b==0)continue;
   roof=rock(b)&&rock(read(X,Y+1,Z));break;
  }
  if(!roof)return false;
 }
 return true;
}
template<class Write>
int writePool(int x,int y,int z,int radius,Write write){
 int count=0;
 for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx)
  if(dx*dx+dz*dz<radius*radius){write(x+dx,y-1,z+dz,uint16_t(8));++count;}
 return count;
}
template<class Sample,class Suitable,class Material>
bool sealedRockBoxFits(int minX,int maxX,int minZ,int maxZ,int low,int high,int maxY,bool lava,
                      Sample sample,Suitable suitable,Material material){
 if(low-2< -64||high+2>maxY||low>high)return false;
 for(int z=minZ-2;z<=maxZ+2;++z)for(int x=minX-2;x<=maxX+2;++x){
  auto local=sample(x,z);
  if(!suitable(local)||high>local.height-8||(!lava&&local.height-low>48))return false;
  for(int y=low-2;y<=high+2;++y)if(material(local,x,y,z)!=2)return false;
 }
 return true;
}
}
