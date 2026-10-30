#pragma once
#include "Noise.h"
namespace nativegen {
// Immutable, owner-planned landform. The lip is the present headwall after
// retreat; the original ledge lies downstream at `retreat`.
struct WaterfallLandform {
 double x=0,z=0,nx=0,nz=0,upper=0,lower=0,radius=0,retreat=0,runout=0;
};
struct WaterfallSection {int cutLow=-32768,cutHigh=-32768;double poolDepth=0;bool rock=false;};
inline WaterfallSection waterfallSection(WaterfallLandform const&f,double x,double z){
 double dx=x-f.x,dz=z-f.z,u=dx*f.nx+dz*f.nz,w=-dx*f.nz+dz*f.nx;
 double side=1-smooth(.62*f.radius,f.radius,std::abs(w));WaterfallSection out;
 if(side<=0||u<-14||u>f.runout)return out;
 out.rock=true;
 // A recessed lower wall leaves a 7-block resistant lip. This is a finite
 // open alcove under existing rock, never an added roof or a solid water tube.
 double undercut=5.+std::min(7.,f.radius*.5);
 if(u<1.5&&u>-undercut){double open=side*smooth(-undercut,0.,u);
  out.cutLow=int(std::ceil(f.lower+3+(1-open)*12));
  out.cutHigh=int(std::floor(f.upper-7-(1-open)*std::min(24.,(f.upper-f.lower)*.3)));
  if(out.cutHigh<out.cutLow)out.cutLow=out.cutHigh=-32768;
 }
 // The impact hollow fades into the downstream bed; only existing wet
 // columns are deepened by the caller, so no new uncontained water is added.
 double pool=1-smooth(.1*f.radius,1.8*f.radius,std::abs(u-3.));
 out.poolDepth=pool*side*(6.+.035*(f.upper-f.lower));return out;
}
}
