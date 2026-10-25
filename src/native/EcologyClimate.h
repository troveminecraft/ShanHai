#pragma once
#include <algorithm>

namespace nativegen {
// Height is the pre-uplift climate altitude, not the voxel placement height.
// Field owns the recipe/terrain gate so lowlands and old recipes return 1.
inline double mountainEcologyTreeWeight(double equivalentHeight,double temperature){
 auto smooth=[](double lo,double hi,double value){
  double t=std::clamp((value-lo)/(hi-lo),0.,1.);
  return t*t*(3.-2.*t);
 };
 return (1.-smooth(190.,230.,equivalentHeight))*smooth(.18,.26,temperature);
}
}
