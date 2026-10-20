// Included within jungleTree: shared placement, foliage and branch helpers.
int top=a.ground+a.h-3;double center=(a.width-1)*.5;
double lean=a.style==1?1.4+r(13001)*.7:a.style==2?.65:a.style==4?.45:0.;
int taper2=roundJS(a.h*(.54+.05*r(13002))),taper1=roundJS(a.h*(.80+.03*r(13003)));
OakPoint prev{a.x+int(center),a.ground,a.z+int(center)};
for(int y=a.ground+1;y<=top;y++){
 double f=clamp((double(y-a.ground)/a.h-.28)/.72,0.,1.);f=f*f*(3-2*f);
 int cx=a.x+int(center)+roundJS(std::cos(a.phase)*lean*f),cz=a.z+int(center)+roundJS(std::sin(a.phase)*lean*f);
 int width=a.width==1?1:y-a.ground<taper2?3:y-a.ground<taper1?2:1;
 int minX=cx-(width>1),minZ=cz-(width>1);tree->stem.push_back({minX,y,minZ,width});
 for(int dz=0;dz<width;dz++)for(int dx=0;dx<width;dx++)put(minX+dx,y,minZ+dz,14);
 // A one-block leaning leader stays face-connected at each voxel step.
 if(y>a.ground+1)for(int z=std::min(prev.z,cz);z<=std::max(prev.z,cz);z++)
  for(int x=std::min(prev.x,cx);x<=std::max(prev.x,cx);x++)put(x,y,z,14);
 prev={cx,y,cz};
}
for(int dz=0;dz<a.width;dz++)for(int dx=0;dx<a.width;dx++)
 for(int y=floorAt(a.x+dx,a.z+dz)+1;y<=a.ground;y++)put(a.x+dx,y,a.z+dz,14);
auto axis=[&](int y){auto s=tree->stem[clamp(y-a.ground-1,0,int(tree->stem.size())-1)];return OakPoint{s[0]+s[3]/2,y,s[2]+s[3]/2};};
int count=(a.style==4?3:3+a.layer)+int(r(12201)*3),highestBranch=a.ground;
for(int i=0;i<count;i++){
 double angle=a.phase+i*2.39996323+(r(13010+i)-.5)*1.65;
 double reach=a.radius*(.93+.42*r(12210+i))*(a.style==2?1.14:a.style==1?.84+.28*std::cos(angle-a.phase):1.);
 int start=a.ground+roundJS(a.h*((a.h<15?.43:.48)+.18*r(12220+i)));
 auto from=axis(start);int rise=a.h<15?0:-1+int(r(12230+i)*6);
 int branchTop=std::min(a.ground+int(a.h*.73),from.y+rise);
 bool broken=a.style==3&&i==0;if(broken)reach*=.45;
 OakPoint to{from.x+roundJS(std::cos(angle)*reach),branchTop,from.z+roundJS(std::sin(angle)*reach)};
 auto path=branch(from,to,a.width==3);
 for(int attempt=0;path.empty()&&attempt<2;attempt++){
  double redirected=angle+(attempt?-.6:.6);
  to={from.x+roundJS(std::cos(redirected)*reach*.80),branchTop,from.z+roundJS(std::sin(redirected)*reach*.80)};
  path=branch(from,to,a.width==3);
 }
 if(path.empty())continue;tree->primaryBranches++;to=path.back();
 if(broken){tree->brokenBranches++;continue;}
 highestBranch=std::max(highestBranch,to.y);
 // Foliage follows the outer branch and its forks, with small gaps at crotches.
 auto shoulder=path[path.size()*7/10];
 crown(shoulder.x,shoulder.y+1,shoulder.z,a.radius*.60,a.radius*.42,20+i,angle);
 crown(to.x,to.y+1,to.z,a.radius*(.67+.18*r(12240+i)),a.radius*(.52+.17*r(12260+i)),i+1,angle);
 int forks=a.layer?1+int(r(13040+i)<.55):int(r(13040+i)<.65);
 for(int k=0;k<forks;k++){
  auto joint=path[path.size()*(k?6:8)/10];double forkAngle=angle+(k?-.8:.7)+(r(13060+i*3+k)-.5)*.4;
  double len=1.8+r(13080+i*3+k)*(a.layer?1.7:.8);
  OakPoint tip{joint.x+roundJS(std::cos(forkAngle)*len),std::min(a.ground+int(a.h*.73),joint.y-1+int(r(13100+i*3+k)*4)),joint.z+roundJS(std::sin(forkAngle)*len)};
  auto fork=branch(joint,tip,false);if(fork.empty())continue;tree->secondaryBranches++;tip=fork.back();
  crown(tip.x,tip.y+1,tip.z,a.radius*.50,a.radius*.40,40+i*2+k,forkAngle);
 }
 // Vanilla moss and a fern suggest small epiphytes on supported branch crotches.
 if(a.layer&&r(13130+i)<.38){
  auto joint=path[path.size()/2];auto base=cells.find(key(joint.x,joint.y,joint.z));
  bool free=base!=cells.end()&&(base->second.block==14||base->second.block==JungleWood);
  for(int dy=1;dy<=2;dy++){auto it=cells.find(key(joint.x,joint.y+dy,joint.z));if(it!=cells.end()&&it->second.block!=15)free=false;}
  if(free){put(joint.x,joint.y+1,joint.z,JungleMoss);put(joint.x,joint.y+2,joint.z,37);tree->epiphytes++;}
 }
}
// One connected broad mantle with unequal off-axis wings. Each wing has its own
// shape seed and roof; no duplicate upper/lower plate or bare neck between them.
auto tip=axis(top);double spread=a.style==2?1.16:a.style==1?.94:1.;
double mantle=a.h<15?1.35:a.layer?2.35:1.85;
double depth=a.h<15?2.8:std::max(a.h*.36,double(a.ground+a.h-highestBranch+3));
leafLobe(tip.x,a.ground+a.h-2,tip.z,a.radius*mantle*spread,a.radius*mantle*.84,depth,a.phase,14000);
int wings=a.h<15?3:a.style==2?7:5+int(r(14030)*2);
for(int i=0;i<wings;i++){
 double angle=a.phase+i*6.283185307/wings+(r(14040+i)-.5)*.65;
 double reach=a.radius*(a.h<15?.45:.65+.40*r(14060+i));
 double side=a.style==1?1+.23*std::cos(angle-a.phase):1.;
 double cy=a.ground+a.h-(a.h<15?2:3+int(r(14080+i)*6));
 leafLobe(tip.x+std::cos(angle)*reach*side,cy,tip.z+std::sin(angle)*reach,
  a.radius*(.86+.28*r(14100+i))*spread,a.radius*(.63+.25*r(14120+i)),a.h<15?2.:4.+r(14140+i)*3,angle,14200+i*71);
}
for(int y=top+1;y<=a.ground+a.h;y++)put(tip.x,y,tip.z,15);
// Unequal radial buttresses, each following soil with a descending ridge.
int roots=a.width==3?3+int(r(13200)*4):2+int(r(13200)*2);
for(int k=0;k<roots;k++){
 double angle=a.phase+k*6.283185307/roots+(r(13210+k)-.5)*.8;
 double len=a.width==3?3.4+r(13220+k)*2.8:1.6+r(13220+k)*1.1;
 int lift=a.width==3?6+int(r(13230+k)*4):3;
 std::vector<std::pair<int,int>> path;int px=a.x+int(center),pz=a.z+int(center);
 for(int step=1;step<=int(len*3);step++){
  double d=double(step)/int(len*3)*len;int x=a.x+int(center)+roundJS(std::cos(angle)*d),z=a.z+int(center)+roundJS(std::sin(angle)*d);
  while(px!=x||pz!=z){if(px!=x){px+=px<x?1:-1;path.emplace_back(px,pz);}if(pz!=z){pz+=pz<z?1:-1;path.emplace_back(px,pz);}}
 }
 for(auto [x,z]:path){
  auto s=field.surface(x,z);int ground=s.height;double f=std::hypot(x-a.x-center,z-a.z-center)/len;
  if(!jungleHabitat(s)||s.rock||s.water>=ground||std::abs(ground-a.ground)>3||reserved(x,ground+1,z))break;
  int high=std::max(ground+1,a.ground+roundJS(1+lift*std::pow(std::max(0.,1-f),1.7)));
  for(int y=ground+1;y<=high;y++)put(x,y,z,JungleWood);
  if(a.width==3&&f<.48){int X=x+(std::abs(std::cos(angle))<.7),Z=z+(std::abs(std::cos(angle))>=.7);auto b=field.surface(X,Z);
   if(jungleHabitat(b)&&!b.rock&&b.water<b.height&&std::abs(b.height-a.ground)<=3)
    for(int y=b.height+1;y<=high-1;y++)put(X,y,Z,JungleWood);
  }
 }
}
