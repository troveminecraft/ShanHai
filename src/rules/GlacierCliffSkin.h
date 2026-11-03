#pragma once
#include <algorithm>
#include <cstdint>
#include <utility>
namespace nativegen {
template<class Surfaces,class Neighbour>
void coatGlacierCliffs(int height,uint16_t*raw,Surfaces const&surfaces,Neighbour neighbour){
 for(int i=0;i<256;++i){auto const&s=surfaces[i];
  if(s.superGlacier<=.5||s.glacier<=.5||s.volcanic>0||s.kind||s.water>=s.height)continue;
  int low=s.height;
  for(auto [dx,dz]:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}}){
   auto n=neighbour(i%16+dx,i/16+dz);low=std::min(low,std::max(n.height,n.water));
  }
  if(s.height-low<8)continue;
  // One exposed column is the ice shell; no new blocks, moved cliff outline,
  // filled cavities, changed fluids or per-voxel neighbour/noise sampling.
  auto col=raw+i*height;
  for(int y=std::max(-59,low+1);y<=std::min(height-65,s.height);++y){int b=col[y+64];
   if(b==2||(b>=95&&b<=99))col[y+64]=uint16_t((y+64)%24<3?144:143);
  }
 }
}
}
