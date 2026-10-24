#undef NDEBUG
#include <cassert>
#include <cmath>
#include <initializer_list>
#include "native/MountainHeight.h"
int main(){
 using nativegen::raisedMountainHeight;
 double previous=-64.;
 for(int i=-6400;i<=40000;++i){
  double y=i/100.;
  assert(raisedMountainHeight(y,0)==y);
  double h=raisedMountainHeight(y,480);
  assert(std::isfinite(h)&&h>=previous&&h<488.000001);
  if(y<=96.)assert(h==y);
  previous=h;
 }
 assert(raisedMountainHeight(240.,480)==480.);
 for(int h=-64;h<=240;++h){
  double actual=raisedMountainHeight(h,480);
  assert(std::abs(nativegen::mountainClimateHeight(actual,480)-h)<.00001);
  assert(nativegen::mountainClimateHeight(h,0)==h);
 }
 // Both joins preserve slope as well as height, without a ridge seam.
 for(double join:{96.,240.}){
  constexpr double e=.0001;
  double l=(raisedMountainHeight(join,480)-raisedMountainHeight(join-e,480))/e;
  double r=(raisedMountainHeight(join+e,480)-raisedMountainHeight(join,480))/e;
  assert(std::abs(l-r)<.0001&&std::abs(l-1.)<.0001);
 }
}
