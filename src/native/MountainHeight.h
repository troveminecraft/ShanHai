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
}
