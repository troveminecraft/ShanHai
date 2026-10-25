#pragma once
#include <algorithm>
namespace nativegen {
inline double plateauSmooth(double lo,double hi,double value){
 double t=std::clamp((value-lo)/(hi-lo),0.,1.);return t*t*(3.-2.*t);
}
// Regional transition belts can contain broad uplands as well as foothills.
// Mountain cores (lowland near0) and established plains (near1) stay intact.
inline double regionalPlateauWeight(double lowland,double selector){
 return plateauSmooth(.05,.30,lowland)*(1.-plateauSmooth(.65,.95,lowland))*selector;
}
// Operates before the shared mountain uplift. The upper branch retains a
// positive relief slope: no clipped horizontal cap or repeated stair levels.
inline double plateauRelief(double height,double undulation){
 if(height<=70.)return height;
 double result;
 if(height>=130.)result=148.+.10*(height-130.);
 else{
  double t=(height-70.)/60.,t2=t*t,t3=t2*t;
  result=(2*t3-3*t2+1)*70.+(t3-2*t2+t)*60.+(-2*t3+3*t2)*148.+(t3-t2)*6.;
 }
 return result+.65*undulation*plateauSmooth(70.,130.,height);
}
inline double plateauHeight(double broadGround,double broadTarget,double legacy,double lowland,double selector,double protect){
 double weight=regionalPlateauWeight(lowland,selector)*(1.-protect)*plateauSmooth(70.,110.,broadGround);
 return legacy+(broadTarget-legacy)*weight;
}
}
