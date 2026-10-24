#pragma once
#include <cmath>
namespace nativegen {
// Leave foothills intact and raise existing ridges continuously. The upper
// tail keeps a positive slope instead of clipping every tall summit flat.
inline double raisedMountainHeight(double height,int peakY){
 if(!peakY||height<=96.)return height;
 if(height>240.)return peakY+8.*std::tanh((height-240.)/8.);
 double t=(height-96.)/144.;
 return height+(peakY-240.)*t*t*(3.-2.*t);
}
// Invert the uplift for surface climate only. Drainage and the terrain profile
// keep their original climate inputs; lowland ice remains at its old altitude.
inline double mountainClimateHeight(double height,int peakY){
 if(peakY<=240||height<=96.)return height;
 if(height>=peakY)return 240.+height-peakY;
 double lo=96.,hi=240.;
 for(int i=0;i<24;++i){double mid=(lo+hi)*.5;if(raisedMountainHeight(mid,peakY)<height)lo=mid;else hi=mid;}
 return (lo+hi)*.5;
}
}
