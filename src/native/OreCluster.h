#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace nativegen {
// Reconstruct a finite union of overlapping spheres along a random segment.
// The BDS SDK exposes OreFeature::SphereData / mAllSpheres; its private RNG
// and exact sphere construction are not available here. This is an explicit
// geometry adapter, not a claim of bit-for-bit Bedrock feature equivalence.
struct OreSphere {double x,y,z,r;};
struct OreCluster {
 std::array<OreSphere,20> spheres{};int size=0;
 int xlo=0,xhi=-1,ylo=0,yhi=-1,zlo=0,zhi=-1;
 template<class Random> OreCluster(int x,int y,int z,int count,Random&&r){
  size=std::clamp(count,1,20);double angle=r(100)*3.141592653589793,span=count/8.;
  double ax=x+std::sin(angle)*span,bx=x-std::sin(angle)*span;
  double az=z+std::cos(angle)*span,bz=z-std::cos(angle)*span;
  double ay=y+int(r(101)*3)-2,by=y+int(r(102)*3)-2;
  double lx=1e9,ly=1e9,lz=1e9,hx=-1e9,hy=-1e9,hz=-1e9;
  for(int i=0;i<size;++i){double t=double(i)/size,rad=((std::sin(t*3.141592653589793)+1)*r(103+i)*count/16.+1)*.5;
   auto&s=spheres[i];s={ax+(bx-ax)*t,ay+(by-ay)*t,az+(bz-az)*t,rad};
   lx=std::min(lx,s.x-rad);hx=std::max(hx,s.x+rad);ly=std::min(ly,s.y-rad);hy=std::max(hy,s.y+rad);lz=std::min(lz,s.z-rad);hz=std::max(hz,s.z+rad);
  }
  // Reject voxels outside the complete union AABB before any surface queries.
  xlo=int(std::floor(lx));xhi=int(std::floor(hx));ylo=int(std::floor(ly));yhi=int(std::floor(hy));zlo=int(std::floor(lz));zhi=int(std::floor(hz));
  // Nested spheres contribute no extra cells. Remove them before voxel tests.
  for(int i=0;i<size;++i)for(int j=i+1;j<size;++j){auto&a=spheres[i];auto&b=spheres[j];if(a.r<=0||b.r<=0)continue;
   double dr=a.r-b.r,dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
   if(dr*dr>dx*dx+dy*dy+dz*dz)(dr>0?b:a).r=0;
  }
  int active=0;for(int i=0;i<size;++i)if(spheres[i].r>0)spheres[active++]=spheres[i];size=active;
 }
 bool contains(int x,int y,int z)const{
  for(int i=0;i<size;++i){auto const&s=spheres[i];if(s.r<=0)continue;double dx=x+.5-s.x,dy=y+.5-s.y,dz=z+.5-s.z;
   if(dx*dx+dy*dy+dz*dz<s.r*s.r)return true;
  }return false;
 }
};
}
