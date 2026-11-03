#pragma once
#include <array>
#include <vector>
#include <cmath>

namespace worldgen_rules {
struct CherryBranch {double x,y,z,X,Y,Z,r,R;};
struct CherryTuft {double x,y,z,rx,ry,rz,phase;};
struct BranchingCherry {std::vector<CherryBranch> branches;std::vector<CherryTuft> tufts;};
// A small immutable skeleton, not a cached full-volume tree. Forks end inside
// their own leaf clusters; empty intervals between tiers remain open sky.
template<class Random> BranchingCherry branchingCherry(double trunk,double phase,Random random){
 BranchingCherry out;
 for(int k=0;k<9;++k){
  double angle=phase+k*2.399963229728653+(.5-random(k*13))*.46;
  double start=17+3.1*k,reach=28+23*random(k*13+1),top=37+2.7*k+4*random(k*13+2);
  double ex=std::cos(angle)*reach,ez=std::sin(angle)*reach;
  double mx=ex*.43+std::sin(angle)*4,mz=ez*.43-std::cos(angle)*4;
  double root=trunk*(.30+.12*random(k*13+3));
  // Lower forks rise before spreading; the upper ones make the apex ragged.
  out.branches.push_back({0,start,0,mx,start+(top-start)*.62,mz,root,root*.67});
  out.branches.push_back({mx,start+(top-start)*.62,mz,ex,top,ez,root*.67,1.5});
  for(int j=0;j<2;++j){
   double a=angle+(j?1:-1)*(.48+.32*random(k*13+4+j));
   double length=7+6*random(k*13+6+j),X=ex+std::cos(a)*length,Z=ez+std::sin(a)*length;
   double Y=top+2+3*random(k*13+8+j);
   // Fork on the actual parent segment; a scaled endpoint can sit outside
   // the rising, curved parent and leave an isolated twig.
   double t=.76;
   out.branches.push_back({mx+(ex-mx)*t,start+(top-start)*(.62+.38*t),mz+(ez-mz)*t,X,Y,Z,2.1,1.05});
   out.tufts.push_back({X,Y+1.5,Z,7+3*random(k*13+10+j),4.5+2*random(k*13+11+j),6+3*random(k*13+12+j),a});
  }
 }
 out.branches.push_back({0,30,0,3,59,-2,trunk*.45,2.});
 out.branches.push_back({3,59,-2,-3,65,1,2.,1.05});
 out.tufts.push_back({-3,66,1,8,6,7,phase});
 return out;
}
}
