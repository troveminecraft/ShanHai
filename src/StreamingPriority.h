#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace terrain {
struct StreamFocus {double x,z,aheadX,aheadZ,speed;};
inline double chunkDistance(int x,int z,double px,double pz){return std::max(std::abs(x-px),std::abs(z-pz));}
inline int streamingScore(int x,int z,std::vector<StreamFocus> const& f){
 double best=20000;
 for(auto const& a:f){
  auto d=chunkDistance(x,z,a.x,a.z),ahead=chunkDistance(x,z,a.aheadX,a.aheadZ);
  // The first rings include dependencies needed to finish the nearby chunks.
  auto score=d<=2?d*16:std::min(64+(d-2)*16,96+ahead*16);
  best=std::min(best,score);
 }
 return static_cast<int>(std::clamp(best,0.0,20000.0));
}
}
