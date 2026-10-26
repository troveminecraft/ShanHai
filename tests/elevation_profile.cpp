#include "native/ElevationProfile.h"
#include <stdexcept>
#include <iostream>
using namespace nativegen::elevation;
void check(bool b){if(!b)throw std::runtime_error("Elevation profile invariant");}
int main(){
 check(raise(64)==100);check(raise(496)==496);
 double last=raise(-61);
 for(double y=-60.9;y<497;y+=.1){double h=raise(y);check(h>last);last=h;auto b=bands(h);check(b[0]>=0&&b[1]>=0&&b[2]>=0&&std::abs(b[0]+b[1]+b[2]-1)<1e-10);}
 check(band(90)==0&&band(210)==1&&band(400)==2);
 for(double q=0;q<=1.001;q+=.001){auto h=basinFloor(q,75,135);check(h>=75&&h<=135);}
 check(basinFloor(0,75,135)==75&&basinFloor(1,75,135)==135);
 check(riftCut(0,0,300,25,60)==60);check(riftCut(0,300,300,25,60)==0);check(riftCut(50,0,300,25,60)==0);
 check(riftCut(-20,0,300,25,60)>riftCut(20,0,300,25,60));
 std::cout<<"PASS monotone uplift, continuous altitude weights, closed bowl and asymmetric tapered rifts\n";
}
