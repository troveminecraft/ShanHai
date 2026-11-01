// Bounded anchor and shape caches. Every shape is computed once per candidate,
// in world coordinates, so neighbouring chunks consume the same tree model.
struct ForestCell {int x,y,z;uint16_t block;uint8_t phase;};
struct ForestTree {
 int x=0,z=0,ground=0,height=0,biome=0,width=1,stemBottom=0,leafColor=0,fallenLength=0,fallenAxis=0;
 std::array<int,4> feet{};
 std::array<std::array<int,3>,4> rootFeet{};
 double angle=0,crown=0;
 bool heart=false,exceptional=false;
};
struct ForestModel {ForestTree tree;std::vector<ForestCell> cells;};
Cache<uint64_t,ForestTree> specialTreeCache{2048};
Cache<uint64_t,std::shared_ptr<ForestModel>> specialShapeCache{192};
Cache<uint64_t,ForestTree> mangroveTreeCache{2048};
Cache<uint64_t,std::shared_ptr<ForestModel>> mangroveShapeCache{192};
static constexpr int ForestGrid=12,ForestHalo=18;
static constexpr int MangroveGrid=8;
bool specialForestHabitat(Surface const&s)const{
 if(s.biome==forest_capabilities::Mangrove&&field.basinMangrovesEnabled())
  return s.closedBasin&&s.basinWeight>.75&&s.water>s.height&&s.water-s.height<=6
      &&s.basinWater==s.water&&s.river==0&&s.mainRiver<.20&&s.lava<s.height&&s.stone<.01&&s.barren<.01;
 if(s.biome==forest_capabilities::Mangrove)return !s.lake&&s.river==0&&s.mainRiver<.20&&s.water<=s.seaLevel
     &&s.height>=s.seaLevel-4&&s.height<=s.seaLevel+6&&s.lava<s.height&&s.stone<.01&&s.barren<.01;
 return forest_capabilities::biome(s.biome)&&!s.kind&&!s.lake&&!s.river&&s.water<s.height
     &&s.lava<s.height&&s.stone<.01&&s.barren<.01&&s.cliffLow==-32768;
}
ForestTree specialTreeCandidate(int gx,int gz,bool denseMangrove=false){return (denseMangrove?mangroveTreeCache:specialTreeCache).get(key2(gx,gz),[&]{
 ForestTree a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^(88100+salt));};
 int grid=denseMangrove?MangroveGrid:ForestGrid;
 a.x=gx*grid+(denseMangrove?2:3)+int(r(1)*(denseMangrove?4:6));a.z=gz*grid+(denseMangrove?2:3)+int(r(2)*(denseMangrove?4:6));
 // This independent rejection precedes any neighbour terrain request.
 if(r(3)>(denseMangrove?.90:.78))return a;
 if(denseMangrove&&!field.mangroveBasinAt(a.x,a.z))return a;
 auto s=field.surface(a.x,a.z,false);
 if(denseMangrove&&s.biome!=forest_capabilities::Mangrove)return a;
 if(s.biome==forest_capabilities::Mangrove){
  if(field.basinMangrovesEnabled()!=denseMangrove)return a;
  if(!specialForestHabitat(s)||!glacierPlantKept(s,a.x,a.z)||r(3)>(denseMangrove?.90:.60)*field.ecologyTreeWeight(s)||blocked(a.x,a.z,false))return a;
  a.biome=s.biome;a.width=1;a.ground=s.height;a.feet.fill(s.height);a.angle=r(7)*6.283185307;
  int low=s.height,high=s.height;
  for(int k=0;k<4;++k){double angle=a.angle+k*1.570796327;
   int x=a.x+roundJS(std::cos(angle)*4),z=a.z+roundJS(std::sin(angle)*4);auto b=field.surface(x,z);
   if(denseMangrove){if(b.biome!=forest_capabilities::Mangrove||!specialForestHabitat(b)||b.water!=s.water||b.rock)return ForestTree{};}
   else if(b.height<s.seaLevel-4||b.height>s.seaLevel+6||b.water>s.seaLevel||b.river>0||b.mainRiver>.20
      ||b.desert>.05||b.badlands||b.aridType||b.closedBasin||b.lake||b.stone>.01||b.rock||b.lava>=b.height||b.barren>.01)return ForestTree{};
   int soil=field.material(b,x,b.height,z);if(soil!=85&&soil!=3&&soil!=4&&soil!=5&&soil!=20&&soil!=42)return ForestTree{};
   a.rootFeet[k]={x,b.height,z};low=std::min(low,b.height);high=std::max(high,b.height);
  }
  if(high-low>4)return ForestTree{};
  a.stemBottom=std::max((denseMangrove?s.water+2:s.seaLevel+3),high+3);a.height=a.stemBottom-a.ground+9+int(r(5)*8);
  a.crown=(denseMangrove?4.5:3.5)+r(6)*1.5;return a;
 }
 if(!specialForestHabitat(s)||!glacierPlantKept(s,a.x,a.z)||!treeGroundAllowed(s,a.x,a.z,false)
    ||r(3)>.78*field.ecologyTreeWeight(s)||blocked(a.x,a.z,false))return a;
 a.biome=s.biome;a.width=(a.biome==forest_capabilities::Cherry||a.biome==forest_capabilities::Dappled)?1:2;a.ground=s.height;
 int lo=s.height,hi=s.height;
 for(int z=0;z<2;++z)for(int x=0;x<2;++x){
  auto b=field.surface(a.x+x,a.z+z,false);
  if(b.biome!=a.biome||!treeGroundAllowed(b,a.x+x,a.z+z,false))return ForestTree{};
  a.feet[z*2+x]=b.height;lo=std::min(lo,b.height);hi=std::max(hi,b.height);
 }
 for(auto [dx,dz]:{std::pair{-3,0},{4,0},{0,-3},{0,4}}){
  auto b=field.surface(a.x+dx,a.z+dz,false);
  if(!specialForestHabitat(b)||!treeGroundAllowed(b,a.x+dx,a.z+dz,false))return ForestTree{};
  lo=std::min(lo,b.height);hi=std::max(hi,b.height);
 }
 if(hi-lo>5)return ForestTree{};
 a.ground=*std::max_element(a.feet.begin(),a.feet.end());
 a.exceptional=r(4)<.045&&hi-lo<=2&&s.height<270&&s.H>.62;
 a.height=(a.biome==forest_capabilities::Cherry?8:12)+int(r(5)*(a.biome==forest_capabilities::Cherry?6:10))+(a.exceptional?8:0);
 a.height=std::min(a.height,Height-68-a.ground);
 if(a.height<(a.biome==forest_capabilities::Cherry?8:10))return ForestTree{};
 a.crown=3.5+r(6)*1.7+(a.exceptional?1.2:0);a.angle=r(7)*6.283185307;
 a.heart=a.biome==forest_capabilities::Pale&&r(8)<.10;
 if(a.biome==forest_capabilities::Dappled){
  if(!field.dappledForestEnabled())return ForestTree{};
  // Colour belongs to a coherent stand, rather than an independent tree roll.
  double stand=forestNoise.at(a.x/185.+37,a.z/185.-53);
  a.leafColor=stand<-.10?0:stand>.12?2:1;
  a.height=15+int(r(5)*11)+(a.exceptional?7:0);a.crown=2.1+r(6)*1.1;
  if(r(9)<.08){
   int length=6+int(r(10)*4),axis=r(11)<.5?0:1;bool supported=true;
   for(int n=0;n<length;++n){int x=a.x+(axis==0?n:0),z=a.z+(axis==1?n:0);auto b=field.surface(x,z,false);
    if(b.biome!=a.biome||b.height!=a.feet[0]||!treeGroundAllowed(b,x,z,false)){supported=false;break;}}
   if(supported){a.fallenLength=length;a.fallenAxis=axis;a.ground=a.feet[0];a.height=2;}
  }
 }
 return a;
});}
std::shared_ptr<ForestModel> specialTreeModel(int gx,int gz,bool denseMangrove=false){return (denseMangrove?mangroveShapeCache:specialShapeCache).get(key2(gx,gz),[&]{
 auto a=specialTreeCandidate(gx,gz,denseMangrove);auto out=std::make_shared<ForestModel>();out->tree=a;
 if(!a.height||(a.biome==forest_capabilities::Dappled&&!field.dappledForestEnabled()))return out;
 using namespace forest_palette;
 bool pale=a.biome==forest_capabilities::Pale,cherry=a.biome==forest_capabilities::Cherry,mangrove=a.biome==forest_capabilities::Mangrove,dappled=a.biome==forest_capabilities::Dappled;
 int log=pale?PaleLog:cherry?CherryLog:mangrove?MangroveLog:dappled?PoplarLog:DarkLog,wood=pale?PaleWood:cherry?CherryWood:mangrove?MangroveWood:dappled?PoplarWood:DarkWood,leaf=pale?PaleLeaves:cherry?CherryLeaves:mangrove?MangroveLeaves:dappled?RedPoplarLeaves+a.leafColor:DarkLeaves;
 auto add=[&](int x,int y,int z,int b,int phase){
  if(y>=-59&&y<=Height-65)out->cells.push_back({x,y,z,uint16_t(b),uint8_t(phase)});
 };
 auto crown=[&](double cx,double cy,double cz,double radius,double ry){
  for(int z=int(std::floor(cz-radius*1.13));z<=int(std::ceil(cz+radius*1.13));++z)
  for(int x=int(std::floor(cx-radius*1.13));x<=int(std::ceil(cx+radius*1.13));++x){
   double dx=x-cx,dz=z-cz,angle=std::atan2(dz,dx);
   double r=radius*(1+.09*std::sin(angle*3+a.angle)+.035*std::cos(angle*5-a.angle));
   double radial=(dx*dx+dz*dz)/(r*r);
   for(int y=int(std::floor(cy-ry));y<=int(std::ceil(cy+ry));++y)
    if(radial+(y-cy)*(y-cy)/(ry*ry)<=1)add(x,y,z,leaf,0);
  }
 };
 auto limb=[&](OakPoint from,OakPoint to){
  add(from.x,from.y,from.z,wood,1);
  while(from.x!=to.x||from.y!=to.y||from.z!=to.z){
   if(from.x!=to.x){from.x+=from.x<to.x?1:-1;add(from.x,from.y,from.z,wood,1);}
   if(from.z!=to.z){from.z+=from.z<to.z?1:-1;add(from.x,from.y,from.z,wood,1);}
   if(from.y!=to.y){from.y+=from.y<to.y?1:-1;add(from.x,from.y,from.z,wood,1);}
  }
 };
 auto shelf=[&](int x,int y,int z,int direction,int size){
  constexpr int dx[4]={0,1,0,-1},dz[4]={-1,0,1,0};
  add(x+dx[direction],y,z+dz[direction],Shelf0North+size*4+direction,2);
 };
 if(dappled){
  if(a.fallenLength){
   for(int n=0;n<a.fallenLength;++n){int x=a.x+(a.fallenAxis==0?n:0),z=a.z+(a.fallenAxis==1?n:0);
    add(x,a.ground+1,z,a.fallenAxis==0?PoplarLogX:PoplarLogZ,1);
    if(n%3==1)shelf(x,a.ground+1,z,a.fallenAxis==0?0:1,n%2);
   }
   return out;
  }
  int top=a.ground+a.height-2;
  crown(a.x,a.ground+a.height*.64,a.z,a.crown,a.height*.29);
  crown(a.x,a.ground+a.height*.86,a.z,a.crown*.70,a.height*.15);
  for(int k=0;k<3;++k){double angle=a.angle+k*2.094395102;int y=a.ground+int(a.height*(.52+k*.09));
   OakPoint end{a.x+roundJS(std::cos(angle)*2),y+2,a.z+roundJS(std::sin(angle)*2)};
   crown(end.x,end.y+1,end.z,a.crown*.72,2.2);limb({a.x,y,a.z},end);
  }
  for(int y=a.feet[0]+1;y<=top;++y)add(a.x,y,a.z,PoplarLog,1);
  if(hash01(a.x,a.z,terrainSeed^88501)<.28){
   int direction=int(hash01(a.x,a.z,terrainSeed^88503)*4);
   shelf(a.x,a.ground+2,a.z,direction,0);
   if(hash01(a.x,a.z,terrainSeed^88505)<.45)shelf(a.x,a.ground+4,a.z,(direction+1)%4,1);
  }
  return out;
 }
 int top=a.ground+a.height-3;
 crown(a.x+.5,top+1,a.z+.5,a.crown,cherry?1.8:2.5);
 for(int k=0;k<(cherry?3:4);++k){
  double angle=a.angle+k*(cherry?2.094395102:1.570796327);
  double reach=3.+hash01(a.x+k,a.z,terrainSeed^88251)*3.;
  OakPoint start{a.x,top-3-int(k%2),a.z};
  OakPoint end{a.x+roundJS(std::cos(angle)*reach),top-1+int(k%2),a.z+roundJS(std::sin(angle)*reach)};
  crown(end.x,end.y+1,end.z,a.crown*(cherry?.95:.78),cherry?1.65:2.1);limb(start,end);
 }
 for(int z=0;z<a.width;++z)for(int x=0;x<a.width;++x)
  for(int y=mangrove?a.stemBottom:a.feet[z*2+x]+1;y<=top;++y)add(a.x+x,y,a.z+z,log,1);
 if(mangrove){
  for(auto end:a.rootFeet){
   OakPoint previous{a.x,a.stemBottom,a.z};
   // Bowed stilt roots remain face connected while curving into shallow mud.
   for(int step=1;step<=12;++step){double t=step/12.;
    OakPoint next{roundJS(a.x+(end[0]-a.x)*std::sin(t*1.570796327)),roundJS(a.stemBottom+(end[1]-a.stemBottom)*t*t),roundJS(a.z+(end[2]-a.z)*std::sin(t*1.570796327))};
    while(previous.x!=next.x||previous.y!=next.y||previous.z!=next.z){
     if(previous.x!=next.x){previous.x+=previous.x<next.x?1:-1;add(previous.x,previous.y,previous.z,Roots,1);}
     if(previous.z!=next.z){previous.z+=previous.z<next.z?1:-1;add(previous.x,previous.y,previous.z,Roots,1);}
     if(previous.y!=next.y){previous.y+=previous.y<next.y?1:-1;add(previous.x,previous.y,previous.z,Roots,1);}
    }
   }
  }
 }
 // Natural hearts are enclosed on all six faces by matching-axis pale logs.
 // The adapter creates their native block actors after applying the volume.
 if(a.heart){int y=top-5;
  for(auto p:{OakPoint{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})
   add(a.x+p.x,y+p.y,a.z+p.z,PaleLog,1);
  add(a.x,y,a.z,Heart,1);
 }
 if(pale||mangrove){
  // Each hanging chain belongs to one leaf column and ends in a native tip.
  std::map<std::pair<int,int>,int> bottoms;
  for(auto const&c:out->cells)if(c.block==leaf){auto key=std::pair{c.x,c.z};auto it=bottoms.find(key);if(it==bottoms.end()||c.y<it->second)bottoms[key]=c.y;}
  for(auto const&[p,y]:bottoms)if(hash01(p.first,p.second,terrainSeed^88301)<.085){
   if(mangrove){add(p.first,y-1,p.second,Propagule0+int(hash01(p.first,p.second,terrainSeed^88304)*5),2);continue;}
   int length=1+int(hash01(p.first,p.second,terrainSeed^88302)*4);
   if(y-length<=a.ground+2)continue;
   for(int n=1;n<=length;++n)add(p.first,y-n,p.second,n==length?HangingMossTip:HangingMoss,2);
  }
 }
 return out;
});}
std::vector<std::shared_ptr<ForestModel>> specialForestModels(int x0,int z0,int size){
 std::vector<std::shared_ptr<ForestModel>> models;
 if(!field.forestEcologyEnabled())return models;
 for(int gz=int(std::floor((z0-ForestHalo)/double(ForestGrid)));gz<=int(std::floor((z0+size+ForestHalo)/double(ForestGrid)));++gz)
 for(int gx=int(std::floor((x0-ForestHalo)/double(ForestGrid)));gx<=int(std::floor((x0+size+ForestHalo)/double(ForestGrid)));++gx){
  auto a=specialTreeCandidate(gx,gz);
  if(a.height&&!nativeReserved(a.x,a.z))models.push_back(specialTreeModel(gx,gz));
 }
 if(field.mangroveBasinArea(x0-ForestHalo,z0-ForestHalo,size+2*ForestHalo+MangroveGrid))
 for(int gz=int(std::floor((z0-ForestHalo)/double(MangroveGrid)));gz<=int(std::floor((z0+size+ForestHalo)/double(MangroveGrid)));++gz)
 for(int gx=int(std::floor((x0-ForestHalo)/double(MangroveGrid)));gx<=int(std::floor((x0+size+ForestHalo)/double(MangroveGrid)));++gx){
  auto a=specialTreeCandidate(gx,gz,true);
  if(a.height&&!nativeReserved(a.x,a.z))models.push_back(specialTreeModel(gx,gz,true));
 }
 return models;
}
void specialForests(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!field.forestEcologyEnabled())return;
 using namespace forest_palette;
 auto models=specialForestModels(x0,z0,16);
 auto replaceable=[](int b){return b==0||b==146||b==147||b==148||(b>=36&&b<=41)||(b>=62&&b<=64)||b==67||b==68||(b>=71&&b<=74)||b==PaleCarpet||b==Eyeblossom;};
 for(int phase=0;phase<3;++phase)for(auto const&m:models)for(auto const&c:m->cells){
  if(c.phase!=phase)continue;int x=c.x-x0,z=c.z-z0;
  if(x<0||x>=16||z<0||z>=16||nativeReserved(c.x,c.z))continue;
  auto const&s=surfaces[z*16+x];
  if(c.block==Roots){
   if(c.y<s.height||c.y<=s.lava||c.y>Height-65)continue;
   auto&v=raw[(z*16+x)*Height+c.y+64];
   if(c.y==s.height){if(v==85||v==3||v==4||v==5||v==20||v==42)v=MuddyRoots;}
   else if(v==8)v=WetRoots;else if(replaceable(v)||leaves(v))v=Roots;
   continue;
  }
  if(c.y<=std::max({s.height,s.water,s.lava}))continue;
  auto col=raw.data()+(z*16+x)*Height;auto&v=col[c.y+64];
  if(phase==2&&c.y<Height-65){int above=col[c.y+65];bool propagule=c.block>=Propagule0&&c.block<=Propagule4;
   if(c.block>=Shelf0North&&c.block<=Shelf1West){
    constexpr int dx[4]={0,1,0,-1},dz[4]={-1,0,1,0};int d=(c.block-Shelf0North)%4;
    int sx=c.x-dx[d],sz=c.z-dz[d],lx=sx-x0,lz=sz-z0;bool supported=false;
    if(nativeReserved(sx,sz))continue;
    if(lx>=0&&lx<16&&lz>=0&&lz<16){int b=raw[(lz*16+lx)*Height+c.y+64];supported=b>=PoplarLog&&b<=PoplarWood;}
    else for(auto const&p:m->cells)if(p.x==sx&&p.y==c.y&&p.z==sz&&p.block>=PoplarLog&&p.block<=PoplarWood){supported=true;break;}
    if(v==0&&supported)v=c.block;
   }else if(v==0&&(propagule?above==MangroveLeaves:above==PaleLeaves||above==HangingMoss))v=c.block;
  }else if(replaceable(v)||(phase==1&&(leaves(v)||v==PaleLog||v==DarkLog||v==PaleWood||v==DarkWood||v==CherryWood||v==CherryLog||v==MangroveLog||v==MangroveWood||(v>=PoplarLog&&v<=PoplarWood))))v=c.block;
 }
 for(int i=0;i<256;++i){auto const&s=surfaces[i];if(s.biome!=forest_capabilities::Pale||s.snowCover>.7||s.height>=Height-66||s.water>=s.height)continue;
  int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z)||!glacierPlantKept(s,x,z))continue;
  auto col=raw.data()+i*Height;int y=s.height+1;
  if(col[y+64]!=0||!(col[y+63]==4||col[y+63]==3||col[y+63]==PaleMoss))continue;
  double q=hash01(x,z,terrainSeed^88341);
  if(q<.012)col[y+64]=Eyeblossom;
  else if(q<.24&&understoryNoise.at(x/23.,z/23.)>-.1)col[y+64]=PaleCarpet;
 }
 for(int i=0;i<256;++i){auto const&s=surfaces[i];if(s.biome!=forest_capabilities::Cherry||s.snowCover>.65||s.height>=Height-66||s.water>=s.height)continue;
  int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z)||!glacierPlantKept(s,x,z))continue;
  auto col=raw.data()+i*Height;int y=s.height+1;
  if(col[y+64]!=0||(col[y+63]!=4&&col[y+63]!=3&&col[y+63]!=80))continue;
  double patch=understoryNoise.at(x/21.+41,z/21.-17),q=hash01(x,z,terrainSeed^88413);
  if(q<.12+.40*smooth(-.2,.35,patch)){
   int facing=int(hash01(x,z,terrainSeed^88415)*4);
   col[y+64]=(q<.11?Petal3North:Petal0North)+facing;
  }
 }
 if(field.dappledForestEnabled())for(int i=0;i<256;++i){auto const&s=surfaces[i];if(s.biome!=forest_capabilities::Dappled||s.snowCover>.65||s.height>=Height-66||s.water>=s.height)continue;
  int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z)||!glacierPlantKept(s,x,z))continue;
  auto col=raw.data()+i*Height;int y=s.height+1;
  if(col[y+64]!=0||(col[y+63]!=4&&col[y+63]!=3&&col[y+63]!=80))continue;
  double patch=understoryNoise.at(x/29.-71,z/29.+91),q=hash01(x,z,terrainSeed^88551);
  if(q<.025&&patch>.04)col[y+64]=RedShrub;
  else if(q<.22+.22*smooth(-.2,.3,patch))col[y+64]=71+int(hash01(x,z,terrainSeed^88553)*4);
 }
}
