#pragma once
#include <algorithm>
#include <cmath>
#include <array>

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
// Smooth a surveyed contour, rather than attaching fine ripples to a plane.
// All control points are immutable patch data; columns perform no surveys.
inline double traceOffset(std::array<double,9> const&points,double w){
 double p=std::clamp(w/48.+4.,0.,8.);int i=std::min(7,int(p));double t=p-i;
 double a=points[std::max(0,i-1)],b=points[i],c=points[i+1],d=points[std::min(8,i+2)];
 double m0=(c-a)*.5,m1=(d-b)*.5;
 return (2*t*t*t-3*t*t+1)*b+(t*t*t-2*t*t+t)*m0+(-2*t*t*t+3*t*t)*c+(t*t*t-t*t)*m1;
}
inline Sample curvedSample(Footprint const&f,double u,double w,double amplitude,double offset){
 auto result=sample(f,u,w,amplitude);
 result.displacement=amplitude*(2*smooth(-.45,.45,u-offset)-1)*result.weight;
 return result;
}
struct Displacement {
 double x=0,z=0,weight=0;
 void add(Sample const&s,double nx,double nz){x+=nx*s.displacement;z+=nz*s.displacement;weight+=s.weight;}
 double scale()const{return 1/std::max(1.,weight);}
};
}
