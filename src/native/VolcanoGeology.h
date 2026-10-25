#pragma once
#include "PlateauProfile.h"
#include <cmath>
namespace nativegen {
inline double volcanicPlumbingRadius(double y,double lavaLevel,double vent,double crater){
 if(y<-54||y>lavaLevel)return 0;
 double neck=vent*(.82+.15*std::sin(y/27.));
 neck*=plateauSmooth(-54.,-25.,y);
 double chamber=std::max(0.,1.-std::pow((y+25.)/24.,2));
 double reservoir=(crater*.72+8.)*std::sqrt(chamber);
 double mouth=crater*.50*plateauSmooth(lavaLevel-42.,lavaLevel-5.,y);
 return std::max({neck,reservoir,mouth});
}
inline double volcanicDiamondBonus(double distance,double radius){
 return 1.-plateauSmooth(radius*.12,radius*.88,distance);
}
}
