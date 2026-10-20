// Vanilla voxel birches: stable stand anchors, crown-led litter, no BP/RP.
struct BirchCell{int x,y,z;uint16_t block;};
struct BirchTree{int x=0,z=0,ground=0,height=0;std::vector<BirchCell> cells;std::array<float,289> litter{};};
Cache<uint64_t,std::shared_ptr<BirchTree>> birchCache{1024};
struct BirchStandSite{int x=0,z=0,ground=0,height=0;bool oak=false;double priority=0;};
Cache<uint64_t,BirchStandSite> birchStandCache{2048};
static std::array<int,3> birchRotate(int x,int y,int z,int yaw){for(int i=0;i<yaw;i++){int v=x;x=z;z=-v;}return {x,y,z};}
bool birchHabitat(Surface const&s)const{return s.biome==23&&!s.kind&&!s.lake&&!s.river&&s.giant<.01&&s.stone<.01&&s.barren<.01&&s.height<260&&s.cliffLow==-32768;}
BirchStandSite birchStandSite(int gx,int gz){return birchStandCache.get(key2(gx,gz),[&]{
 BirchStandSite a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 a.x=gx*5+int(r(5101)*5);a.z=gz*5+int(r(5102)*5);a.priority=r(5104);
 auto s=field.surface(a.x,a.z,false);if(!birchHabitat(s))return a;
 s=field.surface(a.x,a.z);if(s.rock||blocked(a.x,a.z,false))return a;
 if(!treeGroundAllowed(s,a.x,a.z,false))return a;
 double interior=forestInterior(a.x,a.z,3);
 if(r(5103)>clamp(.95+.15*forestNoise.at(a.x/73.,a.z/73.),.82,1.)*(.5+.5*interior))return a;
 for(auto [dx,dz]:{std::pair{4,0},{-4,0},{0,4},{0,-4}})if(std::abs(field.base(a.x+dx,a.z+dz).height-s.height)>3)return a;
 a.oak=r(5106)<.085*interior*interior;a.ground=s.height;a.height=a.oak?28+int(r(5105)*9):15+int(r(5105)*11*(.5+.5*interior));return a;
});}
bool birchStandAccepted(int gx,int gz,BirchStandSite const&a){
 if(!a.height)return false;
 for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
  if(!dx&&!dz)continue;auto b=birchStandSite(gx+dx,gz+dz);if(!b.height)continue;
  if(std::hypot(a.x-b.x,a.z-b.z)<(a.oak||b.oak?5.5:4.)&&
     (b.priority<a.priority||(b.priority==a.priority&&std::pair{b.x,b.z}<std::pair{a.x,a.z})))return false;
 }return true;
}
std::vector<BirchStandSite> birchStandNeighbors(int x,int z){
 std::vector<BirchStandSite> out;int gx=int(std::floor(x/5.)),gz=int(std::floor(z/5.));
 for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){auto b=birchStandSite(gx+dx,gz+dz);if((b.x!=x||b.z!=z)&&birchStandAccepted(gx+dx,gz+dz,b))out.push_back(b);}return out;
}

std::shared_ptr<BirchTree> birch(int gx,int gz){return birchCache.get(key2(gx,gz),[&]{
 auto tree=std::make_shared<BirchTree>();auto site=birchStandSite(gx,gz);
 if(site.oak||!birchStandAccepted(gx,gz,site))return tree;
 int x=site.x,z=site.z,h=site.height,g=site.ground;
 tree->x=x;tree->z=z;tree->ground=g;tree->height=h;
 auto neighbors=birchStandNeighbors(x,z);auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 std::unordered_map<int,BirchCell> cells;std::array<int,121> floors;floors.fill(-32768);
 auto key=[&](int X,int Y,int Z){return ((Y-g)*16+Z-z+7)*16+X-x+7;};
 auto put=[&](int X,int Y,int Z,uint16_t block){
  int dx=X-x,dz=Z-z,dy=Y-g;if(std::abs(dx)>5||std::abs(dz)>5||dy<1||dy>h)return;
  auto&floor=floors[(dz+5)*11+dx+5];if(floor==-32768){auto s=field.surface(X,Z,false);floor=std::max({s.height,s.water,s.lava});}
  if(Y<=floor)return;
  if(block!=66)for(auto const&n:neighbors)if(std::hypot(X-n.x,Z-n.z)<1.5&&Y>n.ground&&Y<n.ground+n.height)return;
  int k=key(X,Y,Z);if(block!=66||!cells.contains(k))cells.insert_or_assign(k,BirchCell{X,Y,Z,block});
 };
 double phase=r(5110)*6.283185307;
 auto crown=[&](double X,double Y,double Z,double rx,double ry,double rz,int salt){
  for(int dz=-int(std::ceil(rz*1.15));dz<=int(std::ceil(rz*1.15));dz++)
  for(int dx=-int(std::ceil(rx*1.15));dx<=int(std::ceil(rx*1.15));dx++)
  for(int dy=-int(std::ceil(ry));dy<=int(std::ceil(ry));dy++){
   double angle=std::atan2(double(dz),double(dx));
   double edge=1+.09*std::sin(3*angle+phase+salt)+.04*std::cos(5*angle-phase);
   double d=(dx*dx/(rx*rx)+dz*dz/(rz*rz))/(edge*edge)+dy*dy/(ry*ry);
   if(d<=1)put(roundJS(X)+dx,roundJS(Y)+dy,roundJS(Z)+dz,66);
  }
 };
 // A connected, narrow upper crown with uneven side lobes and hanging lower
 // tips. Leaves cap the trunk; no exposed pole or single flat crown underside.
 crown(x,g+h-4,z,2.1+r(5111)*.5,4.,2.0+r(5112)*.6,0);
 int count=6+int(r(5113)*4),first=std::max(6,roundJS(h*.43));
 for(int i=0;i<count;i++){
  double t=double(i)/std::max(1,count-1),angle=phase+i*2.39996323;
  int base=first+roundJS(t*(h-first-5)),rise=1+int(r(5120+i)*2);
  double reach=(2.8-t*1.2)+r(5140+i)*.7;
  OakPoint a{x,g+base,z},b{x+roundJS(std::cos(angle)*reach),g+base+rise,z+roundJS(std::sin(angle)*reach)};
  std::vector<OakPoint> branch{a};bool clear=true;
  while(a.x!=b.x||a.y!=b.y||a.z!=b.z){
   if(a.x!=b.x){a.x+=a.x<b.x?1:-1;branch.push_back(a);}
   if(a.z!=b.z){a.z+=a.z<b.z?1:-1;branch.push_back(a);}
   if(a.y!=b.y){a.y+=a.y<b.y?1:-1;branch.push_back(a);}
  }
  for(auto q:branch)for(auto const&n:neighbors)if(std::hypot(q.x-n.x,q.z-n.z)<1.5&&q.y>n.ground&&q.y<n.ground+n.height)clear=false;
  if(!clear)continue;
  crown(b.x,b.y,b.z,1.7+r(5160+i)*.5,1.6+r(5180+i)*.9,1.7+r(5200+i)*.5,i+1);
  for(auto q:branch)put(q.x,q.y,q.z,70);
 }
 for(int y=1;y<=h-2;y++)put(x,g+y,z,65);
 put(x,g+h-1,z,66);put(x,g+h,z,66);
 // Keep only the component attached to the root, including on hillside edges.
 std::unordered_set<int> connected;std::vector<BirchCell> queue;
 int root=key(x,g+1,z);if(!cells.contains(root)){tree->height=0;return tree;}
 connected.insert(root);queue.push_back(cells.at(root));
 for(size_t i=0;i<queue.size();i++){
  auto p=queue[i];for(auto d:{std::array<int,3>{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}){
   int k=key(p.x+d[0],p.y+d[1],p.z+d[2]);auto it=cells.find(k);
   if(it!=cells.end()&&connected.insert(k).second)queue.push_back(it->second);
  }
 }
 std::vector<int> ordered(connected.begin(),connected.end());std::sort(ordered.begin(),ordered.end());
 for(int k:ordered){auto p=cells.at(k);tree->cells.push_back(p);
  if(p.block==66)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
   int X=p.x-x+dx,Z=p.z-z+dz;double w=std::max(0.,1-(dx*dx+dz*dz)/7.5625);
   tree->litter[(Z+8)*17+X+8]+=float(.055*w*w);
  }
 }
 return tree;
});}
std::vector<std::shared_ptr<BirchTree>> birches(int x0,int z0,int size){
 std::vector<std::shared_ptr<BirchTree>> out;for(int gz=int(std::floor((z0-9)/5.));gz<=int(std::floor((z0+size+9)/5.));gz++)for(int gx=int(std::floor((x0-9)/5.));gx<=int(std::floor((x0+size+9)/5.));gx++){auto a=birchStandSite(gx,gz);if(a.oak||!birchStandAccepted(gx,gz,a))continue;auto t=birch(gx,gz);if(!t->cells.empty())out.push_back(t);}return out;
}
static bool birchLitterGround(uint16_t b){return b==2||b==3||b==4||b==20||b==22||b==27||b==32||b==33||b==42;}
void birchForest(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 auto trees=birches(x0,z0,16);
 // Crown overlap in denser stands must never replace another tree's trunk.
 for(int pass=0;pass<3;pass++)for(auto const&t:trees)for(auto const&p:t->cells){
  if(nativeReserved(t->x,t->z)||!forestTreeKept(t->x,t->z))continue;
  bool solid=p.block!=66;
  int priority=!solid?2:(p.x==t->x&&p.z==t->z?0:1);if(priority!=pass)continue;
  int x=p.x-x0,z=p.z-z0;if(x<0||x>=16||z<0||z>=16||p.y>319)continue;
  auto&b=raw[(z*16+x)*Height+p.y+64];
  bool leaves=b==11||b==13||b==15||b==17||b==66;
  if(b==0||(solid&&leaves))b=p.block;
 }
 std::array<float,256> oakLitter{};birchCompanions(x0,z0,raw,surfaces,trees,oakLitter);
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){auto const&s=surfaces[z*16+x];if(s.height>=319||s.water>=s.height||s.lava>=s.height)continue;auto col=raw.data()+(z*16+x)*Height;
  if(col[s.height+65]||!birchLitterGround(col[s.height+64]))continue;
  double canopy=oakLitter[z*16+x];
  for(auto const&t:trees)if(forestTreeKept(t->x,t->z)){int dx=x0+x-t->x,dz=z0+z-t->z;if(std::abs(dx)<=8&&std::abs(dz)<=8)canopy+=t->litter[(dz+8)*17+dx+8];}
  if(canopy<=0)continue;
  double patches=clamp(.9+.18*understoryNoise.at((x0+x)/11.,(z0+z)/11.),.65,1.);
  if(hash01(x0+x,z0+z,terrainSeed^11001)<(1-std::exp(-canopy))*patches){
   int yaw=int(hash01(x0+x,z0+z,terrainSeed^11003)*4);
   col[s.height+65]=uint16_t(71+yaw); // Vanilla leaf litter, including its original color and silhouette.
  }
 }
}
