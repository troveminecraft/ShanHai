#pragma once
#include "PlateauProfile.h"
#include "MountainHeight.h"
namespace nativegen {
inline double mountainTierPeak(double regionalNoise){
 return 260.+100.*plateauSmooth(-.25,.05,regionalNoise)+120.*plateauSmooth(.12,.36,regionalNoise);
}
inline double raisedLandOffset(double height){return 24.*(1.-plateauSmooth(320.,480.,height));}
inline double raisedClimateAltitude(double height){
 return height-24.-40.*plateauSmooth(140.,300.,height);
}
inline double softenedMountain(double local,double broadGround,double broadTop,double peak){
 double mountain=raisedMountainHeight(local,int(peak));
 double support=plateauSmooth(76.,118.,broadGround);
 // Broad shoulders carry the ascent. Fine relief returns towards real peaks;
 // no discrete height bands or flat summit clamp are introduced.
 double retained=.22+.78*plateauSmooth(190.,240.,local);
 return mountain+(broadTop-mountain)*support*(1.-retained);
}
}
