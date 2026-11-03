// Large forests use compact descriptors and clip every write to this chunk.
// No full giant-tree voxel model is built or revisited for adjacent chunks.
struct CanopyTree {
 int x=0,z=0,ground=0,height=0,radius=0,biome=0,door=0,holeHeight=0;
 double phase=0;bool hollow=false,lamp=false,heart=false;
 double doorAngle=0;
};
Cache<uint64_t,CanopyTree> canopyTrees{2048};
Cache<uint64_t,int> canopyGroundNodes{4096};
static constexpr int CanopyGrid=32,CanopyReach=29;
double canopyRoof(int x,int z){
 int gx=int(std::floor(x/32.)),gz=int(std::floor(z/32.));
 auto node=[&](int X,int Z){return canopyGroundNodes.get(key2(X,Z),[&]{return field.base(X*32,Z*32).height;});};
 double u=smooth(0,1,x/32.-gx),v=smooth(0,1,z/32.-gz);
 return (node(gx,gz)*(1-u)+node(gx+1,gz)*u)*(1-v)+(node(gx,gz+1)*(1-u)+node(gx+1,gz+1)*u)*v
     +45.+2.*std::sin(x/73.+z/111.);
}
CanopyTree canopyTree(int gx,int gz){return canopyTrees.get(key2(gx,gz),[&]{
 CanopyTree a;if(!field.canopyForestsEnabled())return a;
 auto r=[&](int salt){return hash01(gx,gz,terrainSeed^(89200+salt));};
 a.x=gx*CanopyGrid+10+int(r(1)*12);a.z=gz*CanopyGrid+10+int(r(2)*12);
 if(field.naturalForestsEnabled()){
  auto point=[&](int X,int Z){return std::pair{X*CanopyGrid+int(hash01(X,Z,terrainSeed^89201)*32),
                                           Z*CanopyGrid+int(hash01(X,Z,terrainSeed^89202)*32)};};
  auto [x,z]=point(gx,gz);a.x=x;a.z=z;
  // Full-cell jitter with a deterministic exclusion distance removes rows
  // without overlapping trunks or depending on chunk traversal order.
  for(int Z=gz-1;Z<=gz+1;++Z)for(int X=gx-1;X<=gx+1;++X){
   if(X==gx&&Z==gz)continue;auto [nx,nz]=point(X,Z);
   if((nx-x)*(nx-x)+(nz-z)*(nz-z)<20*20&&
      std::tuple{hash01(X,Z,terrainSeed^89211),X,Z}<std::tuple{r(11),gx,gz})return CanopyTree{};
  }
 }
 auto s=field.surface(a.x,a.z,false);
 if((s.biome!=forest_capabilities::Dark&&s.biome!=forest_capabilities::Pale)||!specialForestHabitat(s)
    ||!treeGroundAllowed(s,a.x,a.z,false)||!glacierPlantKept(s,a.x,a.z)||blocked(a.x,a.z,false)
    ||r(3)>field.ecologyTreeWeight(s))return CanopyTree{};
 a.radius=5+int(r(4)*3);a.ground=s.height;a.biome=s.biome;
 a.height=roundJS(canopyRoof(a.x,a.z))-a.ground;
 if(a.height<40||a.height>50||a.ground+a.height>Height-67)return CanopyTree{};
 int low=s.height,high=s.height;
 for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=field.surface(a.x+dx*(a.radius+2),a.z+dz*(a.radius+2),false);
  if(!specialForestHabitat(b)||b.biome!=a.biome)return CanopyTree{};
  low=std::min(low,b.height);high=std::max(high,b.height);
 }
 if(high-low>9)return CanopyTree{};
 a.phase=r(5)*6.283185307;a.door=int(r(6)*4);a.holeHeight=9+int(r(7)*4);
 a.doorAngle=r(6)*6.283185307;
 a.hollow=r(8)<.35;a.lamp=a.hollow&&r(9)<.66;
 a.heart=a.hollow&&a.biome==forest_capabilities::Pale&&r(10)<.11;
 return a;
});}
std::vector<CanopyTree> localCanopyTrees(int x0,int z0,int size){
 std::vector<CanopyTree> out;if(!field.canopyForestsEnabled())return out;
 for(int z=int(std::floor((z0-CanopyReach)/32.));z<=int(std::floor((z0+size+CanopyReach)/32.));++z)
 for(int x=int(std::floor((x0-CanopyReach)/32.));x<=int(std::floor((x0+size+CanopyReach)/32.));++x){
  auto a=canopyTree(x,z);
  // Structure reservations are a placement fix, including frozen recipes.
  // With no reservation, the original generated tree remains byte-identical.
  bool reserved=nativeReservations.intersects(a.x,a.z,int(std::ceil(a.radius*1.5)));
  if(a.height&&!reserved)out.push_back(a);
 }return out;
}
struct CanopyColumn {double distance,roots,forward,side;};
CanopyColumn naturalCanopyColumn(CanopyTree const&a,int x,int z)const{
 double dx=x-a.x,dz=z-a.z,angle=std::atan2(dz,dx),c=std::cos(a.doorAngle),s=std::sin(a.doorAngle);
 double lobes=4+int(a.phase*10)%3;
 double roots=std::pow(std::max(0.,std::cos(angle*lobes+a.phase+.25*std::sin(angle*3))),3);
 double distortion=1+.07*std::sin(angle*3+a.phase)+.04*std::cos(angle*5-a.phase);
 return {std::hypot(dx,dz)/distortion,roots,dx*c+dz*s,-dx*s+dz*c};
}
bool naturalCanopyHollow(CanopyTree const&a,CanopyColumn const&p,int y)const{
 double dy=y-a.ground;if(!a.hollow||dy>=a.holeHeight)return false;
 double t=std::max(0.,(dy-1)/double(a.holeHeight-1));
 double width=a.radius*(.51+.05*std::sin(a.phase))*std::sqrt(std::max(0.,1-t*t));
 double bend=.55*std::sin(dy*.37+a.phase)+.25*std::sin(p.forward*.65+a.phase);
 double rim=.25*std::sin(dy*1.1+p.forward*.7+a.phase);
 return p.forward>-a.radius*.55+.45*std::sin(dy*.4+a.phase)&&std::abs(p.side-bend)<width+rim;
}
bool naturalCanopyWood(CanopyTree const&a,CanopyColumn const&p,int y)const{
 double dy=y-a.ground;if(dy<=-9||dy>a.height-5)return false;
 double radius=a.radius*(1-.32*smooth(6.,a.height-6.,dy));
 radius+=(1-smooth(0.,8.,dy))*a.radius*.4*p.roots;
 return p.distance<=radius&&!naturalCanopyHollow(a,p,y);
}
bool canopyHollow(CanopyTree const&a,int x,int y,int z)const{
 if(field.naturalForestsEnabled())return naturalCanopyHollow(a,naturalCanopyColumn(a,x,z),y);
 if(!a.hollow||y<=a.ground||y>=a.ground+a.holeHeight)return false;
 constexpr int dx[4]={0,1,0,-1},dz[4]={-1,0,1,0};
 double forward=(x-a.x)*dx[a.door]+(z-a.z)*dz[a.door];
 double side=-(x-a.x)*dz[a.door]+(z-a.z)*dx[a.door],t=(y-a.ground)/double(a.holeHeight);
 double width=a.radius*.54*std::sqrt(std::max(0.,1-t*t));
 return forward>-a.radius*.52&&std::abs(side)<width;
}
bool canopyWood(CanopyTree const&a,int x,int y,int z)const{
 if(field.naturalForestsEnabled())return naturalCanopyWood(a,naturalCanopyColumn(a,x,z),y);
 double dy=y-a.ground;if(dy<=-9||dy>a.height-5)return false;
 double dx=x-a.x,dz=z-a.z,angle=std::atan2(dz,dx);
 double radius=a.radius*(1-.32*smooth(6.,a.height-6.,dy));
 radius+=(1-smooth(0.,7.,dy))*a.radius*.5*std::pow(std::max(0.,std::cos(angle*6+a.phase)),3);
 return dx*dx+dz*dz<=radius*radius&&!canopyHollow(a,x,y,z);
}
bool canopyTrunkNear(int x,int z,double radius){
 for(auto const&a:localCanopyTrees(x,z,0))if(std::hypot(a.x-x,a.z-z)<a.radius*1.5+radius)return true;
 return false;
}
bool canopyReplaceable(int b)const{
 using namespace forest_palette;
 return b==0||leaves(b)||b==146||b==147||b==148||(b>=36&&b<=41)||(b>=62&&b<=64)||b==67||b==68
  ||(b>=71&&b<=74)||b==PaleCarpet||b==Eyeblossom||b==HangingMoss||b==HangingMossTip;
}
void canopyLimb(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,
 double ax,double ay,double az,double bx,double by,double bz,double radius0,double radius1,int block){
 double vx=bx-ax,vy=by-ay,vz=bz-az,length2=vx*vx+vy*vy+vz*vz,radius=std::max(radius0,radius1);
 int xlo=std::max(x0,int(std::floor(std::min(ax,bx)-radius))),xhi=std::min(x0+15,int(std::ceil(std::max(ax,bx)+radius)));
 int zlo=std::max(z0,int(std::floor(std::min(az,bz)-radius))),zhi=std::min(z0+15,int(std::ceil(std::max(az,bz)+radius)));
 for(int z=zlo;z<=zhi;++z)for(int x=xlo;x<=xhi;++x){if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;
  int lo=std::max({surfaces[i].height+1,surfaces[i].water+1,surfaces[i].lava+1,int(std::floor(std::min(ay,by)-radius))});
  int hi=std::min(Height-65,int(std::ceil(std::max(ay,by)+radius)));auto col=raw.data()+i*Height;
  for(int y=lo;y<=hi;++y){double t=clamp(((x-ax)*vx+(y-ay)*vy+(z-az)*vz)/std::max(1.,length2),0.,1.);
   double dx=x-ax-t*vx,dy=y-ay-t*vy,dz=z-az-t*vz,r=radius0+(radius1-radius0)*t;
   if(dx*dx+dy*dy+dz*dz<=r*r&&canopyReplaceable(col[y+64]))col[y+64]=uint16_t(block);
  }
 }
}
void joinedCanopyForest(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!field.canopyForestsEnabled())return;using namespace forest_palette;
 auto trees=localCanopyTrees(x0,z0,16);if(trees.empty())return;
 // One continuous roof height field, shared by every supporting trunk.
 for(int i=0;i<256;++i){int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z))continue;
  CanopyTree const*owner=nullptr;double nearest=INFINITY;
  for(auto const&a:trees){double d=std::hypot(x-a.x,z-a.z);if(d<nearest&&d<CanopyReach){nearest=d;owner=&a;}}
  if(!owner)continue;auto const&s=surfaces[i];int top=roundJS(canopyRoof(x,z));
  if(top-s.height<30||top-s.height>65||top>Height-66)continue;
  double edge=1-smooth(23.,double(CanopyReach),nearest);int thickness=1+int(6*edge);
  int leaf=owner->biome==forest_capabilities::Pale?CanopyPaleLeaves:CanopyDarkLeaves;auto col=raw.data()+i*Height;
  for(int y=std::max({top-thickness,s.height+1,s.water+1,s.lava+1});y<=top;++y)if(canopyReplaceable(col[y+64]))col[y+64]=uint16_t(leaf);
  if(leaf==CanopyPaleLeaves&&edge>.5&&hash01(x,z,terrainSeed^89301)<.08){
   int length=1+int(hash01(x,z,terrainSeed^89303)*4);
   for(int n=1;n<=length;++n){int y=top-thickness-n;if(y<=s.height||col[y+64]!=0)break;col[y+64]=n==length?HangingMossTip:HangingMoss;}
  }
 }
 for(auto const&a:trees){bool pale=a.biome==forest_capabilities::Pale;int log=pale?PaleLog:DarkLog,wood=pale?PaleWood:DarkWood;
  bool natural=field.naturalForestsEnabled();int radius=int(std::ceil(a.radius*(natural?1.6:1.5)));
  for(int z=std::max(z0,a.z-radius);z<=std::min(z0+15,a.z+radius);++z)
  for(int x=std::max(x0,a.x-radius);x<=std::min(x0+15,a.x+radius);++x){if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;
   auto const&s=surfaces[i];auto col=raw.data()+i*Height;
   auto column=natural?naturalCanopyColumn(a,x,z):CanopyColumn{};
   for(int y=std::max({s.height,s.water,s.lava})+1;y<=a.ground+a.height-5;++y)
    if((natural?naturalCanopyWood(a,column,y):canopyWood(a,x,y,z))&&canopyReplaceable(col[y+64]))
     col[y+64]=uint16_t(natural&&y<=a.ground+8?wood:log);
  }
  for(int k=0;k<5;++k){double angle=a.phase+k*1.256637061,reach=15.+(k%2)*3.;
   double x=a.x+std::cos(angle)*reach,z=a.z+std::sin(angle)*reach;
   canopyLimb(x0,z0,raw,surfaces,a.x,a.ground+a.height-17,a.z,x,canopyRoof(roundJS(x),roundJS(z))-4,z,2.4,1.3,wood);
  }
  auto place=[&](int x,int y,int z,int block){if(x<x0||x>=x0+16||z<z0||z>=z0+16||nativeReserved(x,z))return;
   int i=(z-z0)*16+x-x0;if(y<=surfaces[i].height||y<=surfaces[i].water||y>Height-65)return;
   auto&b=raw[i*Height+y+64];if(canopyReplaceable(b)||b==PaleLog||(natural&&b==PaleWood))b=uint16_t(block);
  };
  if(a.lamp){
   int x=a.x,z=a.z,y=a.ground+1;
   if(natural){x+=roundJS(std::cos(a.doorAngle)*a.radius*.48);z+=roundJS(std::sin(a.doorAngle)*a.radius*.48);
    if(x>=x0&&x<x0+16&&z>=z0&&z<z0+16)y=surfaces[(z-z0)*16+x-x0].height+1;}
   place(x,y,z,HollowLampNorth+a.door);
  }
  if(a.heart){int y=a.ground+(natural?3:a.holeHeight-3);
   if(natural){
    // Grounded living wood, with vertical pale logs immediately above and
    // below the heart as required by the native activation rule.
    int floor=a.ground;
    if(a.x>=x0&&a.x<x0+16&&a.z>=z0&&a.z<z0+16)floor=surfaces[(a.z-z0)*16+a.x-x0].height;
    for(int yy=floor+1;yy<y;++yy)place(a.x,yy,a.z,PaleLog);
    for(int yy=y+1;yy<=a.ground+a.holeHeight+1;++yy)place(a.x,yy,a.z,PaleLog);
   }
   for(int up=1;up<=4;++up)place(a.x,y+up,a.z,PaleLog);
   place(a.x,y-1,a.z,PaleLog);place(a.x,y,a.z,Heart);
  }
  // Only supported faces receive lichen. Neighbour support is read from the
  // same immutable tree, including when the supporting log is another chunk.
  constexpr int dx[4]={0,1,0,-1},dz[4]={-1,0,1,0};
  for(int z=std::max(z0,a.z-radius-1);z<=std::min(z0+15,a.z+radius+1);++z)
  for(int x=std::max(x0,a.x-radius-1);x<=std::min(x0+15,a.x+radius+1);++x){if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;
   for(int y=std::max(surfaces[i].height+1,a.ground+1);y<=a.ground+5;++y){auto&b=raw[i*Height+y+64];
    if(b!=0||hash01(x+y*29,z,terrainSeed^89311)>.66)continue;
    for(int d=0;d<4;++d){int X=x+dx[d],Z=z+dz[d];if(nativeReserved(X,Z)||!canopyWood(a,X,y,Z))continue;
     int j=(Z-z0)*16+X-x0;int ground=X>=x0&&X<x0+16&&Z>=z0&&Z<z0+16?surfaces[j].height:field.surface(X,Z,false).height;
     if(y>ground){b=uint16_t(RootLichenNorth+d);break;}
    }
   }
  }
 }
}
#include "BranchingCherry.inc"
void centralCherryTree(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!field.canopyForestsEnabled())return;using namespace forest_palette;
 auto a=field.centralCherrySite(x0+8,z0+8);if(!a.valid||nativeReserved(a.x,a.z))return;
 if(nativeReservations.intersects(a.x,a.z,int(std::ceil(a.trunkRadius*1.8))))return;
 if(C.value("cherryRevision",0)>=2){
  if(x0>a.x+80||x0+15<a.x-80||z0>a.z+80||z0+15<a.z-80)return;
  branchingCherryTree(x0,z0,raw,surfaces,a.x,a.z,a.ground,a.trunkRadius,a.phase);return;
 }
 double crown=50.+a.trunkRadius*1.3;
 if(x0>a.x+crown*1.12||x0+15<a.x-crown*1.12||z0>a.z+crown*1.12||z0+15<a.z-crown*1.12)return;
 // Broad irregular crown, fluted buttress roots and substantial rising limbs.
 for(int i=0;i<256;++i){int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z))continue;
  auto const&s=surfaces[i];auto col=raw.data()+i*Height;double dx=x-a.x,dz=z-a.z,d=std::hypot(dx,dz),angle=std::atan2(dz,dx);
  double r=crown*(1+.055*std::sin(angle*3+a.phase)+.035*std::cos(angle*5-a.phase)),q=d/r;
  if(q<1){double half=12*std::sqrt(1-q*q),mid=a.ground+58+1.5*std::sin(angle*3+a.phase)*q;
   for(int y=std::max({int(std::ceil(mid-half)),s.height+1,s.water+1,s.lava+1});y<=std::min(Height-65,int(std::floor(mid+half)));++y)
    if(canopyReplaceable(col[y+64]))col[y+64]=CanopyCherryLeaves;
  }
  if(d>a.trunkRadius*1.8)continue;
  for(int y=std::max({s.height,s.water,s.lava})+1;y<=std::min(Height-65,a.ground+51);++y){double h=y-a.ground;
   double radius=a.trunkRadius*(1-.48*smooth(8.,52.,h));
   radius+=(1-smooth(0.,14.,h))*a.trunkRadius*.65*std::pow(std::max(0.,std::cos(angle*7+a.phase)),4);
   radius*=1+.055*std::sin(angle*9+a.phase+h*.04);
   if(d<=radius&&canopyReplaceable(col[y+64]))col[y+64]=CherryLog;
  }
 }
 for(int k=0;k<7;++k){double angle=a.phase+k*.897597901,reach=crown*(.60+.07*(k%3));
  canopyLimb(x0,z0,raw,surfaces,a.x,a.ground+31+(k%3)*4,a.z,
   a.x+std::cos(angle)*reach,a.ground+55+(k%2)*3,a.z+std::sin(angle)*reach,4.5,2.,CherryWood);
 }
}

void cliffSideTrees(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 using namespace forest_palette;
 for(auto const&a:field.cliffTreeSites(x0,z0,16)){
  if(nativeReserved(a.x,a.z))continue;
  double ex=a.x-a.nx*a.length,ez=a.z-a.nz*a.length,ey=a.y+3;
  for(int z=std::max(z0,int(std::floor(ez-3)));z<=std::min(z0+15,int(std::ceil(ez+3)));++z)
  for(int x=std::max(x0,int(std::floor(ex-3)));x<=std::min(x0+15,int(std::ceil(ex+3)));++x){
   if(nativeReserved(x,z))continue;int i=(z-z0)*16+x-x0;auto const&s=surfaces[i];
   for(int y=std::max({int(ey)-2,s.height+1,s.water+1,s.lava+1});y<=std::min(int(ey)+2,Height-65);++y){
    double d=(x-ex)*(x-ex)+(z-ez)*(z-ez)+(y-ey)*(y-ey)*1.8;
    auto&b=raw[i*Height+y+64];if(d<=8&&canopyReplaceable(b))b=CanopyDarkLeaves;
   }
  }
  canopyLimb(x0,z0,raw,surfaces,a.x,a.y,a.z,ex,ey,ez,.85,.55,DarkWood);
  canopyLimb(x0,z0,raw,surfaces,ex,ey-1,ez,ex,ey+1,ez,.65,.55,DarkWood);
 }
}
