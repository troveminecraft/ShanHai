#pragma once
#include "StructurePlan.h"
#include "../native/Noise.h"
namespace worldgen_rules {
using uint=uint32_t;
inline double cityNoise(int x,int z,uint seed,double scale){
 double xx=x/scale,zz=z/scale;int gx=int(std::floor(xx)),gz=int(std::floor(zz));double u=xx-gx,v=zz-gz;u=u*u*(3-2*u);v=v*v*(3-2*v);
 return std::lerp(std::lerp(nativegen::hash01(gx,gz,seed),nativegen::hash01(gx+1,gz,seed),u),std::lerp(nativegen::hash01(gx,gz+1,seed),nativegen::hash01(gx+1,gz+1,seed),u),v);
}
struct CityLink {StructurePoint a,b;};
template<class Sample,class Material> std::vector<CityLink> cityConnections(StructureBox box,ChunkPosition anchor,uint seed,std::vector<StructureBox>const&neighbours,Sample sample,Material materialAt){
 std::vector<CityLink>links;int cx=(box.min.x+box.max.x)/2,cz=(box.min.z+box.max.z)/2;
 std::map<std::pair<int,int>,decltype(sample(0,0))>probeSurfaces;
 for(int side=0;side<4;++side){int dx=side==0?1:side==1?-1:0,dz=side==2?1:side==3?-1:0;bool found=false;
  for(int distance=48;distance<=96&&!found;distance+=8)for(int lateral:{0,-24,24,-48,48,-72,72})for(int dy:{8,16,24,4,32,40,48,56,64,72}){int x=dx?(dx>0?box.max.x+distance:box.min.x-distance):cx+lateral,z=dz?(dz>0?box.max.z+distance:box.min.z-distance):cz+lateral,y=std::max(-49,box.min.y+dy);if(found||std::abs(x-anchor.x*16)>240||std::abs(z-anchor.z*16)>240)continue;auto key=std::pair{x,z};auto it=probeSurfaces.find(key);if(it==probeSurfaces.end())it=probeSurfaces.emplace(key,sample(x,z)).first;auto const&surface=it->second;
   auto material=[&](int yy){return materialAt(surface,x,yy,z);};
   if(material(y)!=0||material(y+2)!=0)continue;
   // Connect to a reachable dry cave floor, not a point floating in a room.
   int foot=y;while(foot>std::max(-55,y-12)&&material(foot-1)==0)--foot;
   int support=material(foot-1);if(support==0||support==8||support==43||material(foot+2)!=0)continue;y=foot;
   StructurePoint begin{cx,std::max(-49,box.min.y+5),cz};double length=std::hypot(x-cx,z-cz),phase=nativegen::hash01(x,z,seed^57139)*6.28318530718;
   int steps=std::max(2,int(std::ceil(length/3.)));std::vector<StructurePoint> route{begin};bool blocked=false;
   for(int i=1;i<=steps;++i){double t=double(i)/steps,envelope=std::sin(t*3.14159265359),bend=envelope*(6*std::sin(t*6.28318530718+phase)+3*std::sin(t*3.14159265359));
    StructurePoint next{int(std::round(std::lerp(double(cx),double(x),t)+dz*bend)),int(std::round(std::lerp(double(begin.y),double(y),t)+3*envelope*std::sin(t*6.28318530718+phase))),int(std::round(std::lerp(double(cz),double(z),t)-dx*bend))};
    // Stay below upper buildings, then rise gradually after passing them.
    // Reject only impossible low detours, rather than losing every city exit.
    for(auto const&b:neighbours)if(next.x>=b.min.x-12&&next.x<=b.max.x+12&&next.z>=b.min.z-12&&next.z<=b.max.z+12&&next.y-3<=b.max.y+8)next.y=std::min(next.y,b.min.y-10);
    if(next.y<-52){blocked=true;break;}route.push_back(next);
   }
   if(blocked)continue;
   // Bounded forward/backward slope relaxation: no vertical step at the edge
   // of an exclusion. Endpoints must still reach the surveyed cave floor.
   for(size_t i=1;i<route.size();++i)route[i].y=std::min(route[i].y,route[i-1].y+2);
   for(size_t i=route.size()-1;i>0;--i)route[i-1].y=std::min(route[i-1].y,route[i].y+2);
   if(route.back().y!=y||route.front().y<-49)continue;
   for(size_t i=1;i<route.size();++i)links.push_back({route[i-1],route[i]});found=true;break;}
 }
 return links;
}
// Empty columns must stay above either world range and leave arithmetic headroom.
struct CityColumn {int floor=INT_MAX/2,ceiling=-64,requiredCeiling=-64;};
struct CityShape {StructureBox box;double cx,cz,rx,rz;};
inline std::vector<CityShape> cityShapes(std::span<StructureBlueprintPiece const>pieces,StructureBox const&clip){
 std::vector<CityShape> shapes;
 for(auto const&p:pieces){if(p.kind==StructurePieceKind::Feature)continue;auto b=p.box;if(!b.valid())continue;
  if(b.max.x+40<clip.min.x||b.min.x-40>clip.max.x||b.max.z+40<clip.min.z||b.min.z-40>clip.max.z)continue;
  double hx=(b.max.x-b.min.x+1)*.5,hz=(b.max.z-b.min.z+1)*.5;
  shapes.push_back({b,(b.min.x+b.max.x)*.5,(b.min.z+b.max.z)*.5,std::min(hx*1.42+16,hx+32),std::min(hz*1.42+16,hz+32)});
 }return shapes;
}
inline CityColumn cityColumn(std::vector<CityShape>const&shapes,std::vector<CityLink>const&links,std::vector<StructureBox>const&neighbours,int x,int z,uint seed){
 CityColumn out;int protectedFloor=-64,requiredCeiling=-64;
 double broad=cityNoise(x,z,seed^146731145,37),detail=cityNoise(x,z,seed^146731149,13);
 double warpX=(cityNoise(x,z,seed^146731153,29)-.5)*10,warpZ=(cityNoise(x,z,seed^146731157,31)-.5)*10;
 for(auto const&s:shapes){auto const&b=s.box;double dx=(x-s.cx+warpX)/s.rx,dz=(z-s.cz+warpZ)/s.rz,q=(dx*dx+dz*dz)/( .88+.12*broad );
  bool inside=x>=b.min.x&&x<=b.max.x&&z>=b.min.z&&z<=b.max.z;
  // Room headroom fades outside the room; a hard inside-box exemption
  // would leave rectangular rock corners where another roof constraint overlaps.
  double edge=std::hypot(std::max({b.min.x-x,0,x-b.max.x}),std::max({b.min.z-z,0,z-b.max.z}));
  if(edge<24)out.requiredCeiling=std::max(out.requiredCeiling,int(std::floor(b.max.y+5-.10*edge*edge)));
  if(q>=1&&!inside)continue;
  int base=std::max(-57,b.min.y-1);double dome=std::sqrt(std::max(0.,1-q));
  int floor=std::max(-57,base+int(std::round((1-dome)*(5+5*broad)+2*(detail-.5))));
  int ceiling=base+int((b.max.y-base+18+18*broad+5*(detail-.5))*dome);
  if(inside){protectedFloor=std::max(protectedFloor,base);requiredCeiling=std::max(requiredCeiling,b.max.y+5);floor=base;ceiling=std::max(ceiling,b.max.y+5);}
  if(ceiling<=floor)continue;out.floor=std::min(out.floor,floor);out.ceiling=std::max(out.ceiling,ceiling);
 }
 for(auto const&link:links){double dx=link.b.x-link.a.x,dz=link.b.z-link.a.z,len2=dx*dx+dz*dz;if(len2<1)continue;double t=std::clamp(((x-link.a.x)*dx+(z-link.a.z)*dz)/len2,0.,1.);double xx=x-std::lerp(double(link.a.x),double(link.b.x),t),zz=z-std::lerp(double(link.a.z),double(link.b.z),t),distance2=xx*xx+zz*zz;if(distance2>=12.25)continue;double cy=std::lerp(double(link.a.y),double(link.b.y),t);out.floor=std::min(out.floor,std::max(-55,int(cy-2)));out.ceiling=std::max(out.ceiling,int(cy+4*std::sqrt(1-distance2/12.25)));}
 // Do not undercut a higher native foundation because a lower adjacent room
 // or connection also influences this column.
 if(protectedFloor>-64){out.floor=std::max(out.floor,protectedFloor);out.ceiling=std::max(out.ceiling,requiredCeiling);out.requiredCeiling=std::max(out.requiredCeiling,requiredCeiling);}
 // Supporting rock under another building is a broad rounded hanging mass,
 // not the building's rectangular bounding box extruded down into the city.
 for(auto const&b:neighbours){
  double hx=(b.max.x-b.min.x+1)*.5,hz=(b.max.z-b.min.z+1)*.5;
  double dx=(x-(b.min.x+b.max.x)*.5+warpX*.5)/(hx*1.5+18),dz=(z-(b.min.z+b.max.z)*.5+warpZ*.5)/(hz*1.5+18),q=std::sqrt(dx*dx+dz*dz);
  if(q>=1.25)continue;double w=std::clamp((1.25-q)/.35,0.,1.);w=w*w*(3-2*w);
  double underside=b.min.y-5-(4+6*broad)*std::sqrt(std::max(0.,1-q*q));
  int cap=int(std::floor(std::lerp(double(out.ceiling),underside,w)));
  out.ceiling=std::max(out.requiredCeiling,std::min(out.ceiling,std::max(out.floor+4,cap)));
 }
 // Narrow passages have their own vertical interval. Preserve that clearance
 // even when a low room and a higher connection influence the same column.
 for(auto const&l:links){double dx=l.b.x-l.a.x,dz=l.b.z-l.a.z,len=dx*dx+dz*dz;if(len<1)continue;double t=std::clamp(((x-l.a.x)*dx+(z-l.a.z)*dz)/len,0.,1.);double xx=x-std::lerp(double(l.a.x),double(l.b.x),t),zz=z-std::lerp(double(l.a.z),double(l.b.z),t);if(xx*xx+zz*zz<9)out.ceiling=std::max(out.ceiling,int(std::lerp(double(l.a.y),double(l.b.y),t)+3));}
 return out;
}
inline bool cityNatural(std::string_view n){return n=="minecraft:stone"||n=="minecraft:deepslate"||n=="minecraft:tuff"||n=="minecraft:granite"||n=="minecraft:diorite"||n=="minecraft:andesite"||n=="minecraft:calcite"||n=="minecraft:gravel"||n=="minecraft:sand"||n=="minecraft:dirt"||n=="minecraft:clay"||n.ends_with("_ore")||n=="minecraft:water"||n=="minecraft:flowing_water"||n=="minecraft:lava"||n=="minecraft:flowing_lava"||n=="minecraft:moss_block"||n=="minecraft:moss_carpet"||n=="minecraft:rooted_dirt"||n=="minecraft:hanging_roots"||n=="minecraft:dripstone_block"||n=="minecraft:pointed_dripstone"||n=="minecraft:red_mushroom"||n=="minecraft:brown_mushroom"||n=="minecraft:azalea"||n=="minecraft:flowering_azalea"||n=="minecraft:spore_blossom"||n.starts_with("minecraft:cave_vines")||n.starts_with("minecraft:big_dripleaf")||n.starts_with("minecraft:small_dripleaf")||n=="minecraft:glow_lichen"||n=="minecraft:short_grass"||n=="minecraft:tall_grass"||n=="minecraft:fern"||n=="minecraft:large_fern";}
inline bool cityRock(std::string_view n){return n=="minecraft:deepslate"||n=="minecraft:stone"||n=="minecraft:tuff"||n=="minecraft:andesite"||n=="minecraft:granite"||n=="minecraft:diorite"||n=="minecraft:moss_block";}
}
