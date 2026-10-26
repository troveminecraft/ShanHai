#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace nativegen::elevation {
inline double smooth(double a,double b,double x){double t=std::clamp((x-a)/(b-a),0.,1.);return t*t*(3.-2*t);}
// Lift coasts by the sea-level delta, add broad inland relief, and retain room
// above the existing peaks. This function is monotone, including both blends.
inline double raise(double h){return h+36*(1-smooth(230,496,h))+14*smooth(64,120,h)*(1-smooth(180,360,h));}
inline std::array<double,3> bands(double height){double mid=smooth(135,175,height),high=smooth(255,305,height);return {1-mid,mid-high,high};}
inline int band(double height){auto b=bands(height);return int(std::max_element(b.begin(),b.end())-b.begin());}
inline double basinFloor(double radius,double floor,double rim){return floor+(rim-floor)*smooth(.28,1.,radius);}
// Smooth asymmetric fault escarpments and a sediment-filled valley floor.
inline double riftCut(double across,double along,double halfLength,double halfWidth,double depth){
 double tip=1-smooth(.65,1.,std::abs(along)/halfLength);
 double width=halfWidth*(.20+.80*std::sqrt(std::max(0.,tip)));
 double side=across<0?1.20:.80;
 return depth*tip*(1-smooth(.22,1.5,std::abs(across)/(width*side)));
}
}
