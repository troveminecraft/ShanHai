// Native deciduous forest: seed anchored trees with connected branch crowns.
struct Broadleaf{int x=0,z=0,h=0,ground=0,species=0;double radius=0,phase=0,priority=0;};
Cache<uint64_t,Broadleaf> broadleafCache{2048};
bool broadleafHabitat(Surface const&s)const{return s.biome==9&&!coniferHabitat(s)&&s.giant<.01&&s.stone<.01&&s.barren<.01&&!s.kind&&!s.lake&&!s.river&&s.cliffLow==-32768&&s.height<235;}
Broadleaf broadleafCandidate(int gx,int gz){return broadleafCache.get(key2(gx,gz),[&]{
 Broadleaf a;auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};a.x=gx*7+1+int(r(2401)*5);a.z=gz*7+1+int(r(2402)*5);a.priority=r(2403);
 auto s=field.surface(a.x,a.z,false);if(!broadleafHabitat(s))return a;
 double density=clamp(.82+.55*forestNoise.at(a.x/87.,a.z/87.),.50,.98);
 if(r(2404)>density||blocked(a.x,a.z,false))return a;
 double interior=forestInterior(a.x,a.z,2);
 if(r(2404)>density*(.45+.55*interior))return a;
 if(!treeGroundAllowed(s,a.x,a.z,false))return a;
 int lo=s.height,hi=s.height;for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
  auto b=field.surface(a.x+dx,a.z+dz,false);if(b.water>b.height||b.lava>b.height||b.cliffLow!=-32768||b.stone>.01||b.barren>.01||!treeGroundAllowed(b,a.x+dx,a.z+dz,false))return a;
  lo=std::min(lo,b.height);hi=std::max(hi,b.height);
 }
 if(hi-lo>3)return a;
 auto nearbyStructures=structures(a.x,a.z,0);for(auto const&t:smallTrees(a.x,a.z,0,nearbyStructures))if(std::hypot(a.x-t.x,a.z-t.z)<4)return a;
 for(auto const&c:conifers(a.x-5,a.z-5,10))if(std::hypot(a.x-c.x,a.z-c.z)<7)return a;
 for(auto const&g:giants(a.x,a.z,1))if(std::hypot(a.x-g->x,a.z-g->z)<g->radius+8)return a;
 a.species=0;a.h=12+int(std::pow(r(2406),1.55)*29*(.45+.55*interior));a.ground=s.height;a.radius=3.+r(2407)*1.7+std::max(0,a.h-20)*.075;a.phase=r(2408)*6.283185307;return a;
});}
std::vector<Broadleaf> broadleaves(int x0,int z0,int size){
 std::vector<Broadleaf> out;
 for(int gz=int(std::floor((z0-24)/7.));gz<=int(std::floor((z0+size+24)/7.));gz++)for(int gx=int(std::floor((x0-24)/7.));gx<=int(std::floor((x0+size+24)/7.));gx++){
  auto a=broadleafCandidate(gx,gz);if(!a.h)continue;bool reject=false;
  for(int dz=-1;dz<=1&&!reject;dz++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dz)continue;auto b=broadleafCandidate(gx+dx,gz+dz);if(b.h&&std::hypot(a.x-b.x,a.z-b.z)<5&&(b.priority<a.priority||(b.priority==a.priority&&std::pair{b.x,b.z}<std::pair{a.x,a.z}))){reject=true;break;}}
  if(!reject)out.push_back(a);
 }return out;
}
// A curved, face-connected woody skeleton shared by generation and inspection.
struct OakPoint{int x,y,z;};
std::vector<OakPoint> oakSpine(Broadleaf const&a){
 std::vector<OakPoint> points;
 int top=a.h-std::max(3,a.h/8);double bend=2.+hash01(a.x,a.z,terrainSeed^2471)*3.;
 int style=int(hash01(a.x,a.z,terrainSeed^2472)*4);
 for(int y=1;y<=top;y++){
  double t=double(y-1)/std::max(1,top-1),forward=bend*t,side=0;
  if(style==0)forward=bend*std::sin(t*3.141592653589793*.8);
  if(style==1){forward=bend*t;side=bend*.65*std::sin(t*6.283185307)*t;}
  if(style==2){forward=bend*t*t;side=bend*.5*std::sin(t*3.141592653589793);}
  if(style==3){forward=bend*std::sin(t*3.141592653589793);side=bend*.6*t;}
  points.push_back({a.x+roundJS(std::cos(a.phase)*forward-std::sin(a.phase)*side),a.ground+y,a.z+roundJS(std::sin(a.phase)*forward+std::cos(a.phase)*side)});
 }
 return points;
}
void broadleafForest(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 auto trees=broadleaves(x0,z0,16);
 auto put=[&](int x,int y,int z,int id,bool wood=false){int X=x-x0,Z=z-z0;if(X<0||X>=16||Z<0||Z>=16||y<-60||y>319)return;auto const&s=surfaces[Z*16+X];if(y<=s.height||y<=s.water||y<=s.lava)return;auto&v=raw[(Z*16+X)*Height+y+64];if(v==0||(wood&&(v==11||v==13||v==15||v==17||v==66)))v=uint8_t(id);};
 for(auto const&a:trees){if(nativeReserved(a.x,a.z)||!forestTreeKept(a.x,a.z))continue;
  auto spine=oakSpine(a);
  auto crown=[&](double cx,double cy,double cz,double r,double ry){
   int xa=std::max(x0,int(std::floor(cx-r*1.2))),xb=std::min(x0+15,int(std::ceil(cx+r*1.2)));
   int za=std::max(z0,int(std::floor(cz-r*1.2))),zb=std::min(z0+15,int(std::ceil(cz+r*1.2)));
   for(int z=za;z<=zb;z++)for(int x=xa;x<=xb;x++)for(int y=int(std::floor(cy-ry));y<=std::min(a.ground+a.h,int(std::ceil(cy+ry)));y++){
    double dx=x-cx,dz=z-cz,dy=(y-cy)/ry,angle=std::atan2(dz,dx),R=r*(1+.13*std::sin(3*angle+a.phase)+.06*std::cos(5*angle-a.phase));
    if((dx*dx+dz*dz)/(R*R)+dy*dy<=1)put(x,y,z,11);
   }
  };
  auto wood=[&](OakPoint p,double thick){
   put(p.x,p.y,p.z,69,true);
   if(thick>0)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
    double angle=std::atan2(double(dz),double(dx));
    double radius=thick+.16*std::sin(angle*2+a.phase)+.10*std::cos(angle*3-a.phase);
    if(dx*dx+dz*dz<=radius*radius)put(p.x+dx,p.y,p.z+dz,69,true);
   }
  };
  auto connect=[&](OakPoint from,OakPoint to,double thick){
   wood(from,thick);
   while(from.x!=to.x||from.y!=to.y||from.z!=to.z){
    if(from.x!=to.x){from.x+=from.x<to.x?1:-1;wood(from,thick);}
    if(from.z!=to.z){from.z+=from.z<to.z?1:-1;wood(from,thick);}
    if(from.y!=to.y){from.y+=from.y<to.y?1:-1;wood(from,thick);}
   }
  };
  auto top=spine.back();crown(top.x,top.y,top.z,a.radius*.8,std::max(3,a.h/8));
  int branches=3+(a.h>=26?2:0)+int(hash01(a.x,a.z,terrainSeed^2473)*4);
  std::vector<std::pair<OakPoint,OakPoint>> limbs;
  for(int k=0;k<branches;k++){
   double pick=hash01(a.x,a.z,terrainSeed^(2480+k)),angle=a.phase+k*2.39996323+pick*.6;
   auto from=spine[std::min(spine.size()-1,size_t(spine.size()*(.38+.40*pick)))];
   double reach=a.radius*(.65+.6*hash01(a.x,a.z,terrainSeed^(2490+k)));
   OakPoint end{top.x+roundJS(std::cos(angle)*reach),a.ground+a.h-std::max(3,a.h/8)-int(pick*std::min(12,a.h/3)),top.z+roundJS(std::sin(angle)*reach)};
   crown(end.x,end.y+1,end.z,a.radius*(.56+.33*hash01(a.x,a.z,terrainSeed^(2500+k))),1.5+pick*1.5+std::max(0,a.h-20)*.06);
   limbs.emplace_back(from,end);
  }
  for(auto [from,end]:limbs){
   // Bow each limb with a sampled curve, then voxelize each short section.
   // This avoids long straight staircases and a hard elbow halfway out.
   double mx=(from.x+end.x)*.5+(end.z-from.z)*.16;
   double mz=(from.z+end.z)*.5-(end.x-from.x)*.16;
   double my=from.y+(end.y-from.y)*.25;
   int steps=std::max({std::abs(end.x-from.x),std::abs(end.z-from.z),std::abs(end.y-from.y)})*3+1;
   OakPoint previous=from;
   for(int k=1;k<=steps;k++){
    double t=double(k)/steps,u=1-t;
    OakPoint q{roundJS(u*u*from.x+2*u*t*mx+t*t*end.x),roundJS(u*u*from.y+2*u*t*my+t*t*end.y),roundJS(u*u*from.z+2*u*t*mz+t*t*end.z)};
    connect(previous,q,a.h>=24?1.15-.65*t:0.);previous=q;
    // A small off-centre leafy twig joins outer boughs to their main crown.
    if(k==steps*3/4){
     double angle=a.phase+end.x*.7+end.z*.3;
     OakPoint twig{q.x+roundJS(std::cos(angle)*1.8),q.y+1,q.z+roundJS(std::sin(angle)*1.8)};
     crown(twig.x,twig.y+1,twig.z,a.radius*.43,1.4);
     connect(q,twig,0);
    }
   }
  }
  // Continuous asymmetric taper: side ribs end at different heights rather
  // than all four sides ending on the same horizontal plane.
  for(size_t i=0;i<spine.size();i++){
   double t=double(i)/std::max(size_t(1),spine.size()-1);
   connect(i?spine[i-1]:spine[i],spine[i],a.h>=14?1.45-.90*t:.50);
  }

  if(forestDesign&&hash01(a.x,a.z,terrainSeed^52601)<.16){
   int side=int(hash01(a.x,a.z,terrainSeed^52602)*4);auto d=std::array<std::array<int,2>,4>{{{0,-1},{1,0},{0,1},{-1,0}}}[side];
   int height=std::min(8,int(spine.size())-1);for(int j=2;j<=height;++j){auto p=spine[j];auto next=spine[j-1];
    // Only a straight stem section has continuous side support.
    if(p.x!=next.x||p.z!=next.z)continue;
    double t=double(j)/std::max(size_t(1),spine.size()-1),thick=a.h>=14?1.45-.90*t:.50;
    double angle=std::atan2(double(d[1]),double(d[0])),radius=thick+.16*std::sin(angle*2+a.phase)+.10*std::cos(angle*3-a.phase);
    int outside=radius>=1?2:1;put(p.x+d[0]*outside,p.y,p.z+d[1]*outside,76+side);
   }
  }

 }

 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(!broadleafHabitat(s)||s.rock)continue;int h=s.height,X=x0+x,Z=z0+z;auto col=raw.data()+(z*16+x)*Height;
  if(col[h+64]!=4||col[h+65])continue;
  double patch=clamp(.5+understoryNoise.at(X/27.,Z/27.),0,1),pick=hash01(X,Z,terrainSeed^2451);bool shade=false;
  for(int y=h+3;y<=std::min(h+18,319);y++)if(col[y+64]==11||col[y+64]==66){shade=true;break;}
  if(shade&&patch>.58)col[h+64]=33;
  int gx=int(std::floor(X/16.)),gz=int(std::floor(Z/16.));
  int ax=gx*16+2+int(hash01(gx,gz,terrainSeed^2461)*12),az=gz*16+2+int(hash01(gx,gz,terrainSeed^2462)*12);
  bool nearTree=false;for(auto const&a:trees)if(std::hypot(X-a.x,Z-a.z)<2.5){nearTree=true;break;}
  if(!nearTree&&patch>.5&&std::abs(X-ax)<=1&&std::abs(Z-az)<=1){col[h+65]=11;if(X==ax&&Z==az&&!col[h+66])col[h+66]=11;continue;}
  if(shade&&pick<.035){col[h+65]=hash01(X,Z,terrainSeed^2452)<.75?67:68;continue;}
  if(pick>.12+.24*patch)continue;
  if(patch>.65&&pick<.09&&!col[h+66]){col[h+65]=40;col[h+66]=41;}else col[h+65]=shade?37:36;
 }
}
