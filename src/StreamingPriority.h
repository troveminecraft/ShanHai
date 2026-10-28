#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace terrain {
struct StreamFocus {double x,z,aheadX,aheadZ,speed;bool repair=false;double repairX=0,repairZ=0;bool repair2=false;double repair2X=0,repair2Z=0;int radius=64;};
inline double chunkDistance(int x,int z,double px,double pz){return std::max(std::abs(x-px),std::abs(z-pz));}
inline int streamingScore(int x,int z,std::vector<StreamFocus> const& f){
 double best=20000;
 for(auto const& a:f){
  auto d=chunkDistance(x,z,a.x,a.z),ahead=chunkDistance(x,z,a.aheadX,a.aheadZ);
  // The first rings include dependencies needed to finish the nearby chunks.
  auto score=d<=2?d*16:std::min(64+(d-2)*16,96+ahead*16);
  // A distant visible hole also needs decoration/light dependencies. Promote
  // its whole bounded halo, while keeping the player's immediate rings first.
  auto promote=[&](bool enabled,double X,double Z){if(enabled){auto rd=chunkDistance(x,z,X,Z);if(rd<=10)score=std::min(score,160.+rd*8.);}};
  promote(a.repair,a.repairX,a.repairZ);promote(a.repair2,a.repair2X,a.repair2Z);
  best=std::min(best,score);
 }
 return static_cast<int>(std::clamp(best,0.0,20000.0));
}
}
