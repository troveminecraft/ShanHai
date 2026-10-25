#pragma once
#include <algorithm>
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
}
