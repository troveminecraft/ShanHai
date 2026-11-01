#pragma once
#include <algorithm>
#include <cmath>

namespace nativegen::cliff_transition {
inline double smooth(double lo,double hi,double value){
 double t=std::clamp((value-lo)/(hi-lo),0.,1.);return t*t*(3-2*t);
}
struct Footprint {double core,end,front;};
inline Footprint footprint(double halfLength,double front,double drop){
 // Keep the full-height central face; give each end enough horizontal room
 // to reconnect a 100--300 block scarp instead of folding it into 14 blocks.
 double core=std::max(16.,halfLength-14.);
 return {core,core+std::clamp(drop*.5,72.,136.),front};
}
struct Sample {double displacement=0,weight=0;};
inline Sample sample(Footprint const&f,double u,double w,double amplitude){
 if(std::abs(u)>=f.front||std::abs(w)>=f.end)return {};
 double weight=(1-smooth(8.,f.front,std::abs(u)))*(1-smooth(f.core,f.end,std::abs(w)));
 return {amplitude*(2*smooth(-.45,.45,u)-1)*weight,weight};
}
struct Displacement {
 double x=0,z=0,weight=0;
 void add(Sample const&s,double nx,double nz){x+=nx*s.displacement;z+=nz*s.displacement;weight+=s.weight;}
 double scale()const{return 1/std::max(1.,weight);}
};
}
