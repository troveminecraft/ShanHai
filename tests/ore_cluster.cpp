#include "native/OreCluster.h"
#include "native/Noise.h"
#include <iostream>
#include <set>
#include <tuple>
#include <stdexcept>
int main(){
 auto check=[](bool ok){if(!ok)throw std::runtime_error("ore cluster bound/translation regression");};
 int boundary=0,nonempty=0;long long voxels=0;
 for(int n:{3,4,7,8,9,10,12,17,20})for(int seed=0;seed<1000;++seed){
  auto r=[&](int k){return nativegen::hash01(seed,k,99371);};
  nativegen::OreCluster a(15,-20,0,n,r),b(-17,12,48,n,r);
  check(a.xlo>=10&&a.xhi<=20&&a.zlo>=-5&&a.zhi<=5&&a.ylo>=-27&&a.yhi<=-15);
  std::set<std::tuple<int,int,int>> cells;
  for(int z=a.zlo;z<=a.zhi;++z)for(int x=a.xlo;x<=a.xhi;++x)for(int y=a.ylo;y<=a.yhi;++y){
   check(a.contains(x,y,z)==b.contains(x-32,y+32,z+48));
   if(a.contains(x,y,z)){cells.insert({x,y,z});++voxels;if(x>=16)++boundary;}
  }
  nonempty+=!cells.empty();
  check(!a.contains(a.xlo-1,-20,0)&&!a.contains(a.xhi+1,-20,0));
 }
 check(nonempty>8000&&boundary>0);
 std::cout<<"{\"clusters\":9000,\"nonempty\":"<<nonempty<<",\"voxels\":"<<voxels<<",\"boundaryVoxels\":"<<boundary<<"}\n";
}
