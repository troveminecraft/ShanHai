// Bounded anchor and shape caches. Every shape is computed once per candidate,
// in world coordinates, so neighbouring chunks consume the same tree model.
struct ForestCell {int x,y,z;uint16_t block;uint8_t phase;};
struct ForestTree {
 int x=0,z=0,ground=0,height=0,biome=0,width=1;
 std::array<int,4> feet{};
 double angle=0,crown=0;
 bool heart=false,exceptional=false;
};
struct ForestModel {ForestTree tree;std::vector<ForestCell> cells;};
Cache<uint64_t,ForestTree> specialTreeCache{2048};
Cache<uint64_t,std::shared_ptr<ForestModel>> specialShapeCache{192};
static constexpr int ForestGrid=12,ForestHalo=18;
bool specialForestHabitat(Surface const&s)const{
 return forest_capabilities::biome(s.biome)&&!s.kind&&!s.lake&&!s.river&&s.water<s.height
     &&s.lava<s.height&&s.stone<.01&&s.barren<.01&&s.cliffLow==-32768;
}
ForestTree specialTreeCandidate(int gx,int gz){return specialTreeCache.get(key2(gx,gz),[&]{
 ForestTree a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^(88100+salt));};
 a.x=gx*ForestGrid+3+int(r(1)*6);a.z=gz*ForestGrid+3+int(r(2)*6);
 // This independent rejection precedes any neighbour terrain request.
 if(r(3)>.78)return a;
 auto s=field.surface(a.x,a.z,false);
 if(!specialForestHabitat(s)||!glacierPlantKept(s,a.x,a.z)||!treeGroundAllowed(s,a.x,a.z,false)
    ||r(3)>.78*field.ecologyTreeWeight(s)||blocked(a.x,a.z,false))return a;
 a.biome=s.biome;a.width=2;a.ground=s.height;
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
 a.height=12+int(r(5)*10)+(a.exceptional?8:0);
 a.height=std::min(a.height,Height-68-a.ground);
 if(a.height<10)return ForestTree{};
 a.crown=3.5+r(6)*1.7+(a.exceptional?1.2:0);a.angle=r(7)*6.283185307;
 a.heart=a.biome==forest_capabilities::Pale&&r(8)<.10;
 return a;
});}
std::shared_ptr<ForestModel> specialTreeModel(int gx,int gz){return specialShapeCache.get(key2(gx,gz),[&]{
 auto a=specialTreeCandidate(gx,gz);auto out=std::make_shared<ForestModel>();out->tree=a;
 if(!a.height)return out;
 using namespace forest_palette;
 bool pale=a.biome==forest_capabilities::Pale;
 int log=pale?PaleLog:DarkLog,wood=pale?PaleWood:DarkWood,leaf=pale?PaleLeaves:DarkLeaves;
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
 int top=a.ground+a.height-3;
 crown(a.x+.5,top+1,a.z+.5,a.crown,2.5);
 for(int k=0;k<4;++k){
  double angle=a.angle+k*1.570796327;
  double reach=3.+hash01(a.x+k,a.z,terrainSeed^88251)*3.;
  OakPoint start{a.x,top-3-int(k%2),a.z};
  OakPoint end{a.x+roundJS(std::cos(angle)*reach),top-1+int(k%2),a.z+roundJS(std::sin(angle)*reach)};
  crown(end.x,end.y+1,end.z,a.crown*.78,2.1);limb(start,end);
 }
 for(int z=0;z<a.width;++z)for(int x=0;x<a.width;++x)
  for(int y=a.feet[z*2+x]+1;y<=top;++y)add(a.x+x,y,a.z+z,log,1);
 // Natural hearts are enclosed on all six faces by matching-axis pale logs.
 // The adapter creates their native block actors after applying the volume.
 if(a.heart){int y=top-5;
  for(auto p:{OakPoint{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})
   add(a.x+p.x,y+p.y,a.z+p.z,PaleLog,1);
  add(a.x,y,a.z,Heart,1);
 }
 if(pale){
  // Each hanging chain belongs to one leaf column and ends in a native tip.
  std::map<std::pair<int,int>,int> bottoms;
  for(auto const&c:out->cells)if(c.block==leaf){auto key=std::pair{c.x,c.z};auto it=bottoms.find(key);if(it==bottoms.end()||c.y<it->second)bottoms[key]=c.y;}
  for(auto const&[p,y]:bottoms)if(hash01(p.first,p.second,terrainSeed^88301)<.085){
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
  auto const&s=surfaces[z*16+x];if(c.y<=std::max({s.height,s.water,s.lava}))continue;
  auto col=raw.data()+(z*16+x)*Height;auto&v=col[c.y+64];
  if(phase==2){int above=col[c.y+65];if(v==0&&(above==PaleLeaves||above==HangingMoss))v=c.block;}
  else if(replaceable(v)||(phase==1&&(leaves(v)||v==PaleLog||v==DarkLog||v==PaleWood||v==DarkWood)))v=c.block;
 }
 for(int i=0;i<256;++i){auto const&s=surfaces[i];if(s.biome!=forest_capabilities::Pale||s.snowCover>.7||s.height>=Height-66||s.water>=s.height)continue;
  int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z)||!glacierPlantKept(s,x,z))continue;
  auto col=raw.data()+i*Height;int y=s.height+1;
  if(col[y+64]!=0||!(col[y+63]==4||col[y+63]==3||col[y+63]==PaleMoss))continue;
  double q=hash01(x,z,terrainSeed^88341);
  if(q<.012)col[y+64]=Eyeblossom;
  else if(q<.24&&understoryNoise.at(x/23.,z/23.)>-.1)col[y+64]=PaleCarpet;
 }
}
