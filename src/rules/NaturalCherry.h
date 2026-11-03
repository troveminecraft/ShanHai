#pragma once
#include "BranchingCherry.h"
#include <algorithm>

namespace worldgen_rules::natural_cherry {
struct Point {
 double x,y,z;
 Point operator+(Point b)const{return {x+b.x,y+b.y,z+b.z};}
 Point operator-(Point b)const{return {x-b.x,y-b.y,z-b.z};}
 Point operator*(double t)const{return {x*t,y*t,z*t};}
};
struct Curve {
 Point a,b,c,d;
 Point at(double t)const{double u=1-t;return a*(u*u*u)+b*(3*u*u*t)+c*(3*u*t*t)+d*(t*t*t);}
};
inline double length(Point p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
inline Point horizontal(double angle,double distance){return {std::cos(angle)*distance,0,std::sin(angle)*distance};}
inline void limb(BranchingCherry& model,Curve const& c,double r,double R){
 // Sub-voxel diagonal twigs can leave separated wood at voxel corners.
 // One-block terminal radius keeps the rasterized fork face-connected.
 r=std::max(1.,r);R=std::max(1.,R);
 int count=std::clamp(int(std::ceil((length(c.b-c.a)+length(c.c-c.b)+length(c.d-c.c))/3)),3,24);
 auto last=c.a;double previous=r;
 for(int i=1;i<=count;++i){double t=double(i)/count;auto p=c.at(t);double radius=R+(r-R)*std::pow(1-t,1.15);
  model.branches.push_back({last.x,last.y,last.z,p.x,p.y,p.z,previous,radius});last=p;previous=radius;
 }
}
// All forks are anchored on their parent's curve. A compact seed-derived
// skeleton is shared by clipped chunk writers; no full-tree voxel cache.
template<class Random> BranchingCherry build(double trunk,double phase,Random random,bool fuller=false){
 if(fuller)trunk*=.66;
 BranchingCherry out;int salt=0;auto next=[&]{return random(salt++);};
 auto direction=[&](double a,double d,double y){auto p=horizontal(a,d);p.y=y;return p;};
 Curve spine{{0,7,0},direction(phase-1.1,7+4*next(),24),direction(phase+.7,10+4*next(),42),direction(phase+.1,7+3*next(),63)};
 limb(out,spine,trunk*(fuller?.63:.73),.95);
 auto blossom=[&](Curve const& twig,double angle,double size){
  // Small overlapping lobes make an elongated spray, rather than a ball at
  // the end of every spoke. The stem stays inside the complete flower spray.
  for(int j=0;j<4;++j){double t=.34+.22*j;auto p=twig.at(t);
   p.y+=.8;double side=(j%2?1.:-1.)*(.5+next());p=p+horizontal(angle+1.570796327,side);
   double r=size*(.74+.44*next());
   out.tufts.push_back({p.x,p.y,p.z,r,1.6+next()*1.6,r*(.65+.35*next()),angle+.6*(next()-.5)});
  }
 };
 for(int k=0;k<8;++k){
  double attach=.12+k*.089,angle=phase+k*2.39996323+(next()-.5)*.8;
  auto start=spine.at(attach);double spread=30+13*next()-1.6*k;
  double turn=(next()-.5)*1.35,rise=8+14*next();
  auto end=start+direction(angle+turn,spread,rise);end.y=std::min(61.,end.y);rise=end.y-start.y;
  Curve branch{start,start+direction(angle-.32,spread*.25,rise*.9),
    start+direction(angle+turn*.25,spread*.68,rise*.1),end};
  double root=trunk*(.23+.07*next())*(1-.045*k);
  limb(out,branch,root,1.15);
  for(int j=0;j<3;++j){
   double t=.32+j*.28;auto p=branch.at(t);
   double a=angle+turn+((j+k)%2?1:-1)*(.5+.45*next());
   double reach=11+9*next(),up=5+6*next();auto tip=p+direction(a,reach,up);
   tip.y=std::min(66.,tip.y);
   Curve secondary{p,p+direction(a-.3,reach*.25,up*.8),
     tip-direction(a+.35,reach*.30,up*.7),tip};
   double r=std::max(1.3,root*(1-.5*t)*.68);limb(out,secondary,r,.72);
   for(int m=0;m<3;++m){
    double u=.38+.27*m;auto q=secondary.at(u);
    double az=a+((m+j)%2?1:-1)*(.45+.65*next());double n=4.5+3.5*next();
    auto e=q+direction(az,n,4+5*next());e.y=std::min(70.,e.y);
    Curve twig{q,q+direction(az-.3,n*.2,3),e-direction(az+.2,n*.22,1),e};
    limb(out,twig,1.05,.62);blossom(twig,az,3.+1.1*next());
   }
   // Distal flowers also follow the secondary branch, leaving the inner
   // trunk and larger forks exposed instead of filling one continuous dome.
   Curve spray{secondary.at(.66),secondary.at(.78),secondary.at(.90),secondary.d};
   blossom(spray,a,3.3+next());
  }
  Curve spray{branch.at(.8),branch.at(.87),branch.at(.94),branch.d};
  blossom(spray,angle+turn,3.+next());
 }
 // The upper crown is a forked, asymmetric apex, never a single round cap.
 for(int k=0;k<4;++k){double a=phase+k*2.39996323;auto p=spine.at(.72+k*.065);
  auto tip=p+direction(a,7+5*next(),5+5*next());tip.y=std::min(69.,tip.y);
  Curve twig{p,p+direction(a-.4,3,4),tip-direction(a+.3,3,2),tip};
  limb(out,twig,1.3,.65);blossom(twig,a,2.6+next());
 }
 if(fuller){
  // Fill some inner, upper crown gaps with supported shoots. Keep the
  // existing outer forks and their open intervals instead of adding a roof.
  for(int k=0;k<8;++k){double a=phase+.8+k*2.39996323;auto p=spine.at(.40+k*.047);
   double reach=12+7*next();auto tip=p+direction(a,reach,4+5*next());
   Curve shoot{p,p+direction(a-.4,reach*.25,5),tip-direction(a+.2,reach*.25,2),tip};
   limb(out,shoot,1.5,1.);blossom(shoot,a,3.1+next());
  }
  // Upper shoots return over the leader to fill the central hollow in the
  // silhouette. Flowers still follow individual forks, not a solid cap.
  for(int k=0;k<6;++k){double a=phase+.35+k*2.39996323;auto p=spine.at(.76+k*.032);
   auto tip=direction(a,5+7*next(),64+3*next());
   Curve shoot{p,p+direction(a-.35,3,6),tip-direction(a+.2,3,2),tip};
   limb(out,shoot,1.5,1.);blossom(shoot,a,3.2+next());
  }
  auto smooth=[](double a,double b,double h){double t=std::clamp((h-a)/(b-a),0.,1.);return t*t*(3-2*t);};
  // Raise the centre and gently lower the outer upper forks: side views
  // should arch over the trunk, not form an equally high rim around a bowl.
  // Moving wood and foliage together keeps the flowers on their branches.
  auto arch=[&](double x,double h,double z){double r=std::hypot(x,z);
   return h+smooth(20,45,h)*(5*(1-smooth(8,28,r))-10*smooth(12,58,r));};
  for(auto& b:out.branches){b.y=arch(b.x,b.y,b.z);b.Y=arch(b.X,b.Y,b.Z);}
  for(auto& t:out.tufts){t.y=arch(t.x,t.y,t.z);t.rx*=1.12;t.rz*=1.12;t.ry*=1.10;}
 }
 return out;
}
}
