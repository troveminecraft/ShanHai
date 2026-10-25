#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace nativegen::magma {
inline constexpr int Bottom = -56; // Four bedrock layers plus a solid rock seal.
inline double widening(double t) {
    // Continuous throat-to-root profile, with most lateral growth at depth.
    double t2=t*t;
    return .12*t2+.88*t2*t2*t;
}
inline float columnTop(double distance,double mouth,double root,double lavaTop) {
    if(distance>=root)return -32768;
    if(distance<=mouth)return float(lavaTop);
    double target=(distance-mouth)/(root-mouth),lo=0,hi=1;
    // Solve once per column. Material generation then uses only a Y comparison.
    for(int i=0;i<20;++i){double mid=(lo+hi)*.5;if(widening(mid)<target)lo=mid;else hi=mid;}
    return float(lavaTop-(lavaTop-Bottom)*(lo+hi)*.5);
}
// Revision 4 caches a signed boundary at depth knots. Interior shape is free
// to bend and form rock shelves; voxel queries need no noise or trigonometry.
inline constexpr int Knots=17;
using Boundary=std::array<float,Knots>;
struct Layer {double x,z,growth,amplitude,sin5,cos5,sin7,cos7;};
using Layers=std::array<Layer,Knots>;
inline Layers layers(double phase){
    Layers result{};
    for(int i=0;i<Knots;++i){double t=double(i)/(Knots-1),bend=t*(1-.35*t);
        result[i]={20*bend*std::sin(phase+3.1*t),17*bend*std::cos(phase*.8-4.3*t),
            .22*t+.78*t*t*t,std::sin(3.141592653589793*t)*(6+15*t),
            std::sin(5*t),std::cos(5*t),std::sin(7*t),std::cos(7*t)};
    }return result;
}
inline bool contains(Boundary const& margins,int y,float lavaTop,float ceiling) {
    if(y<Bottom||y>lavaTop||y>ceiling)return false;
    double t=std::clamp((double(lavaTop)-y)/(lavaTop-Bottom),0.,1.)*(Knots-1);
    int i=std::min(int(t),Knots-2);double f=t-i;
    return margins[i]+(margins[i+1]-margins[i])*f>0;
}
}
