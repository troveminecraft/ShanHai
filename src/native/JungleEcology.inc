// Three rainforest strata, generated once per world anchor and clipped at output.
static constexpr uint16_t JungleWood=75,JungleVine=76,JungleMoss=80,JungleMelon=81,JungleBamboo=82;
static constexpr int JungleHalo=24,JungleSpan=JungleHalo*2+1,JungleKeyStride=56;
struct JungleSite{int x=0,z=0,ground=0,h=0,width=1,layer=0,style=0;double priority=0,phase=0,radius=0;};
struct JungleCell{int x,y,z;uint16_t block;};
struct JungleTree{JungleSite site;std::vector<JungleCell> cells;std::vector<std::array<int,4>> stem;
 // A 24-block crown reaches at most two chunk columns each way. Index cells
 // once, by destination chunk and placement pass, instead of rescanning the
 // entire tree six times for every neighboring output chunk.
 std::array<uint32_t,151> tileBegin{};std::vector<uint32_t> tileCells;
 int primaryBranches=0,secondaryBranches=0,brokenBranches=0,attachedVines=0,epiphytes=0;};
static int jungleKind(uint16_t b){return b==14?0:b==JungleWood?1:b==15?2:b==JungleMoss?3:b==37?4:5;}
Cache<uint64_t,JungleSite> jungleSites{2048};
Cache<uint64_t,std::shared_ptr<JungleTree>> jungleShapes{192};
bool jungleHabitat(Surface const&s)const{
 return s.biome==13&&s.giant<.01&&s.stone<.01&&s.barren<.01&&!s.kind&&!s.lake&&!s.river&&s.cliffLow==-32768&&s.height>33&&s.height<235;
}
double junglePatch(int x,int z)const{return clamp(.58+.65*forestNoise.at(x/83.,z/83.)+.22*understoryNoise.at(x/29.,z/29.),0.,1.);}
JungleSite jungleSite(int gx,int gz){return jungleSites.get(key2(gx,gz),[&]{
 JungleSite a;auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};
 a.x=gx*7+int(r(12101)*7);a.z=gz*7+int(r(12102)*7);a.priority=r(12103);
 // Biome/height/water rejection needs no slope. Computing rock eligibility
 // first sampled four extra terrain columns for every non-jungle candidate.
 auto s=field.surface(a.x,a.z,false);if(!jungleHabitat(s))return a;
 s=field.surface(a.x,a.z);if(s.rock||blocked(a.x,a.z,false)||r(12104)>.58+.4*junglePatch(a.x,a.z))return a;
 double interior=forestInterior(a.x,a.z,4);
 if(r(12104)>(.58+.4*junglePatch(a.x,a.z))*(.5+.5*interior))return a;
 double pick=r(12105);a.layer=pick<.14*interior*interior?2:pick<.68*interior?1:0;
 // Near an ecotone, smaller trees feather the canopy instead of a tall wall.
 a.width=a.layer?3:1;int lo=s.height,hi=s.height;
 for(int dz=-1;dz<=a.width;dz++)for(int dx=-1;dx<=a.width;dx++){
  auto b=field.surface(a.x+dx,a.z+dz);int mat=field.material(b,a.x+dx,b.height,a.z+dz);
  if(!jungleHabitat(b)||b.rock||(mat!=3&&mat!=4&&mat!=32&&mat!=33)||b.water>=b.height||b.lava>=b.height)return a;
  lo=std::min(lo,b.height);hi=std::max(hi,b.height);
 }
 if(hi-lo>(a.width>=2?3:4))return a;
 a.ground=s.height;for(int dz=0;dz<a.width;dz++)for(int dx=0;dx<a.width;dx++)a.ground=std::max(a.ground,field.surface(a.x+dx,a.z+dz,false).height);
 a.h=a.layer==2?46+int(r(12106)*13):a.layer==1?34+int(r(12106)*12):18+int(r(12106)*10);
 a.radius=a.layer==2?5.8+r(12107)*1.1:a.layer==1?4.5+r(12107)*1.2:2.9+r(12107)*1.0;
 if(!a.layer&&r(12111)<.46){a.h=5+int(r(12112)*8);a.radius=2.1+r(12113)*1.0;}
 a.phase=r(12108)*6.283185307;double form=r(12110);
 a.style=!a.layer?4:form<.42?0:form<.70?1:form<.93?2:3;return a;
});}
bool jungleAccepted(int gx,int gz,JungleSite const&a){
 if(!a.h)return false;
 for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
  if(!dx&&!dz)continue;auto b=jungleSite(gx+dx,gz+dz);if(!b.h)continue;
  if(std::hypot(a.x-b.x+(a.width-b.width)*.5,a.z-b.z+(a.width-b.width)*.5)<(a.width>=2||b.width>=2?7.:4.7)&&
    (b.priority<a.priority||(b.priority==a.priority&&std::pair{b.x,b.z}<std::pair{a.x,a.z})))return false;
 }return true;
}
std::shared_ptr<JungleTree> jungleTree(int gx,int gz){return jungleShapes.get(key2(gx,gz),[&]{
 auto tree=std::make_shared<JungleTree>();auto a=jungleSite(gx,gz);tree->site=a;if(!jungleAccepted(gx,gz,a))return tree;
 auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 std::vector<JungleSite> neighbors;
 for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)if(dx||dz){auto b=jungleSite(gx+dx,gz+dz);if(jungleAccepted(gx+dx,gz+dz,b))neighbors.push_back(b);}
 std::unordered_map<int,JungleCell> cells;std::array<int,JungleSpan*JungleSpan> floors;floors.fill(-32768);
 auto key=[&](int x,int y,int z){return ((y-a.ground+4)*JungleKeyStride+z-a.z+JungleHalo+2)*JungleKeyStride+x-a.x+JungleHalo+2;};
 auto floorAt=[&](int x,int z){auto&i=floors[(z-a.z+JungleHalo)*JungleSpan+x-a.x+JungleHalo];if(i==-32768){auto s=field.surface(x,z,false);i=std::max({s.height,s.water,s.lava});}return i;};
 auto reserved=[&](int x,int y,int z){for(auto const&b:neighbors)if(x>=b.x-1&&x<=b.x+b.width&&z>=b.z-1&&z<=b.z+b.width&&y>b.ground&&y<b.ground+b.h)return true;return false;};
 auto put=[&](int x,int y,int z,uint16_t block){
  if(std::abs(x-a.x)>JungleHalo||std::abs(z-a.z)>JungleHalo||y<a.ground-3||y>a.ground+a.h||y<=floorAt(x,z))return;
  int k=key(x,y,z);if(block==15){if(!cells.contains(k))cells.emplace(k,JungleCell{x,y,z,block});}
  else if(!reserved(x,y,z)){
   auto old=cells.find(k);if(block==JungleWood&&old!=cells.end()&&old->second.block==14)return;
   cells.insert_or_assign(k,JungleCell{x,y,z,block});
  }
 };
 // Faceted leaf sprays thin toward their actual polygon boundary. The upper
 // mantle uses the same tapered volume, not a pair of copied horizontal plates.
 auto leafLobe=[&](double cx,double cy,double cz,double length,double width,double depth,double angle,int salt){
  constexpr int N=9;std::array<std::pair<double,double>,N> rim;double c=std::cos(angle),s=std::sin(angle);
  double minX=1e9,maxX=-1e9,minZ=1e9,maxZ=-1e9;
  for(int k=0;k<N;k++){
   double t=(k+.22*(r(salt+k)-.5))*6.283185307/N,scale=.70+.30*r(salt+10+k);
   double u=std::cos(t)*length*scale,v=std::sin(t)*width*scale;
   double X=cx+u*c-v*s,Z=cz+u*s+v*c;rim[k]={X,Z};
   minX=std::min(minX,X);maxX=std::max(maxX,X);minZ=std::min(minZ,Z);maxZ=std::max(maxZ,Z);
  }
  double tiltU=(r(salt+21)-.5)*1.7,tiltV=(r(salt+22)-.5)*1.4;
  for(int Z=int(std::floor(minZ));Z<=int(std::ceil(maxZ));Z++)for(int X=int(std::floor(minX));X<=int(std::ceil(maxX));X++){
   bool inside=false;double edgeSquared=1e18;
   for(int i=0,j=N-1;i<N;j=i++){
    auto [ax,az]=rim[i];auto [bx,bz]=rim[j];double ex=bx-ax,ez=bz-az;
    if((az>Z)!=(bz>Z)&&X<(bx-ax)*(Z-az)/(bz-az)+ax)inside=!inside;
    double t=clamp(((X-ax)*ex+(Z-az)*ez)/(ex*ex+ez*ez),0.,1.);
    double ox=X-ax-t*ex,oz=Z-az-t*ez;
    edgeSquared=std::min(edgeSquared,ox*ox+oz*oz);
   }
   if(!inside)continue;
   double dx=X-cx,dz=Z-cz,u=dx*c+dz*s,v=-dx*s+dz*c;
   double q=clamp(std::sqrt(edgeSquared)/(std::min(length,width)*.70),0.,1.);
   double wave=.55*std::sin(u*.65+a.phase+salt)+.40*std::sin(v*.73-a.phase);
   int roof=roundJS(cy+tiltU*u/length+tiltV*v/width+wave+q*1.4);
   int thickness=1+int(depth*std::pow(q,.78));
   for(int Y=roof-thickness+1;Y<=roof;Y++)put(X,Y,Z,15);
  }
 };
 auto crown=[&](double x,double y,double z,double rx,double rz,int lobe,double heading){
  int salt=12700+lobe*200;
  leafLobe(x,y,z,rx,rz,3.4,heading,salt);
  int lobes=2+int(r(salt+29)*2);
  for(int k=0;k<lobes;k++){
   double angle=heading+(k-1)*1.65+(r(salt+31+k)-.5)*.9,reach=.48+.22*r(salt+41+k);
   leafLobe(x+std::cos(angle)*rx*reach,y-1-int(r(salt+51+k)*2),z+std::sin(angle)*rz*reach,
    rx*(.45+.17*r(salt+61+k)),rz*(.40+.15*r(salt+71+k)),2.3,angle,salt+40+k*35);
  }
 };
 auto branch=[&](OakPoint from,OakPoint to,bool thick){
  std::vector<OakPoint> path{from};int steps=std::max({std::abs(to.x-from.x),std::abs(to.y-from.y),std::abs(to.z-from.z)})*2+1;
  auto prev=from;double mx=(from.x+to.x)*.5+(to.z-from.z)*.12,mz=(from.z+to.z)*.5-(to.x-from.x)*.12,my=from.y+(to.y-from.y)*.25;
  for(int i=1;i<=steps;i++){double t=double(i)/steps,u=1-t;OakPoint q{roundJS(u*u*from.x+2*u*t*mx+t*t*to.x),roundJS(u*u*from.y+2*u*t*my+t*t*to.y),roundJS(u*u*from.z+2*u*t*mz+t*t*to.z)};
   while(prev.x!=q.x||prev.y!=q.y||prev.z!=q.z){
    if(prev.x!=q.x){prev.x+=prev.x<q.x?1:-1;path.push_back(prev);}
    if(prev.z!=q.z){prev.z+=prev.z<q.z?1:-1;path.push_back(prev);}
    if(prev.y!=q.y){prev.y+=prev.y<q.y?1:-1;path.push_back(prev);}
   }
  }
  int tx=std::abs(to.x-from.x)>=std::abs(to.z-from.z)?0:1,tz=tx?0:1;
  std::vector<OakPoint> brush;size_t clear=0;
  for(size_t i=0;i<path.size();i++){
   auto p=path[i];std::vector<OakPoint> section{p};
   if(thick&&i<path.size()*55/100){section.push_back({p.x,p.y-1,p.z});section.push_back({p.x+tx,p.y,p.z+tz});section.push_back({p.x+tx,p.y-1,p.z+tz});}
   if(std::any_of(section.begin(),section.end(),[&](auto q){return reserved(q.x,q.y,q.z);}))break;
   brush.insert(brush.end(),section.begin(),section.end());clear=i+1;
  }
  if(clear<2||std::hypot(path[clear-1].x-from.x,path[clear-1].z-from.z)<1.4)return std::vector<OakPoint>{};
  path.resize(clear);
  for(auto p:brush)put(p.x,p.y,p.z,JungleWood);return path;
 };
#include "JungleStructure.inc"
 // Wood must connect through wood, never through a leaf bridge over a slope.
 int woodRoot=key(a.x,a.ground+1,a.z);std::unordered_set<int> woodSeen;std::vector<JungleCell> woodQueue;
 auto woody=[](uint16_t b){return b==14||b==JungleWood;};
 if(cells.contains(woodRoot)){woodSeen.insert(woodRoot);woodQueue.push_back(cells.at(woodRoot));}
 for(size_t i=0;i<woodQueue.size();i++){auto p=woodQueue[i];for(auto d:{std::array<int,3>{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}){int k=key(p.x+d[0],p.y+d[1],p.z+d[2]);auto it=cells.find(k);if(it!=cells.end()&&woody(it->second.block)&&woodSeen.insert(k).second)woodQueue.push_back(it->second);}}
 for(auto it=cells.begin();it!=cells.end();)if(woody(it->second.block)&&!woodSeen.contains(it->first))it=cells.erase(it);else ++it;
 for(int block:{int(JungleMoss),37})for(auto it=cells.begin();it!=cells.end();){auto p=it->second;auto below=cells.find(key(p.x,p.y-1,p.z));
  if(p.block==block&&(below==cells.end()||(block==37?below->second.block!=JungleMoss:!woody(below->second.block))))it=cells.erase(it);else ++it;
 }
 // Prune isolated voxels before adding hanging foliage. Geometry uses local
 // immutable coordinates, so neither order nor chunk borders split a tree.
 int root=key(a.x,a.ground+1,a.z);std::unordered_set<int> seen;std::vector<JungleCell> queue;
 if(cells.contains(root)){seen.insert(root);queue.push_back(cells.at(root));}
 for(size_t i=0;i<queue.size();i++){auto p=queue[i];for(auto d:{std::array<int,3>{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}){int k=key(p.x+d[0],p.y+d[1],p.z+d[2]);auto it=cells.find(k);if(it!=cells.end()&&seen.insert(k).second)queue.push_back(it->second);}}
 for(auto it=cells.begin();it!=cells.end();)if(!seen.contains(it->first))it=cells.erase(it);else ++it;
 // Sparse, bounded strands: actual side attachment at the top and clearance
 // over the forest floor. Bits are south/west/north/east support directions.
 constexpr std::array<std::array<int,3>,4> sides{{{0,-1,0},{1,0,1},{0,1,2},{-1,0,3}}};
 int strands=0;double vinePatch=junglePatch(a.x+149,a.z-87);
 int strandLimit=a.h<15?1:2+a.layer*2+int(vinePatch*6);
 // Ranking by a stable spatial hash samples middle and upper foliage fairly;
 // sorting by height previously spent the entire budget at the crown's roof.
 auto vineRank=[&](auto p){return hash01(p.x,p.z,terrainSeed^(14501+p.y*31));};
 std::sort(queue.begin(),queue.end(),[&](auto const&u,auto const&v){double U=vineRank(u),V=vineRank(v);return U==V?std::tie(u.x,u.y,u.z)<std::tie(v.x,v.y,v.z):U<V;});
 std::vector<std::pair<int,int>> hung;
 for(auto p:queue){
  if(p.block!=15||p.y<a.ground+a.h*.36||strands>=strandLimit||vineRank(p)>.05)continue;
  for(auto d:sides){int X=p.x+d[0],Z=p.z+d[1];if(std::abs(X-a.x)>JungleHalo||std::abs(Z-a.z)>JungleHalo||cells.contains(key(X,p.y,Z)))continue;
   if(std::any_of(hung.begin(),hung.end(),[&](auto q){return std::abs(q.first-X)+std::abs(q.second-Z)<3;}))continue;
   double lengthPick=hash01(X,Z,terrainSeed^(12403+p.y));
   int n=lengthPick<.22?9+int(lengthPick/.22*8):3+int(lengthPick*6),clear=0;
   for(int Y=p.y;Y>p.y-n&&Y>floorAt(X,Z)+3;Y--){if(cells.contains(key(X,Y,Z))||reserved(X,Y,Z))break;clear++;}
   if(clear<(n>=9?5:3))continue;
   for(int i=0;i<clear;i++)cells.emplace(key(X,p.y-i,Z),JungleCell{X,p.y-i,Z,uint16_t(JungleVine+d[2])});
   hung.emplace_back(X,Z);strands++;break;
  }
 }
 // Broken patches climb actual trunk faces, including the tapered/leaning part.
 int faces=1+int(r(13300)*3);
 for(int k=0;k<faces;k++){
  auto d=sides[(int(r(13301)*4)+k)%4];
  for(int patch=0;patch<3;patch++){
   int high=a.ground+roundJS(a.h*(.23+patch*.19+r(13310+k*3+patch)*.07));
   int length=4+int(r(13330+k*3+patch)*7);
   for(int y=high;y>high-length&&y>a.ground+3;y--){
    if(hash01(gx,y/3,terrainSeed^(13350+k*13+gz))<.16)continue;
    auto s=tree->stem[clamp(y-a.ground-1,0,int(tree->stem.size())-1)];
    int X=s[0]+s[3]/2,Z=s[2]+s[3]/2;
    if(d[0])X=s[0]+(d[0]>0?s[3]-1:0);if(d[1])Z=s[2]+(d[1]>0?s[3]-1:0);
    auto support=cells.find(key(X,y,Z));if(support==cells.end()||support->second.block!=14)continue;
    X+=d[0];Z+=d[1];int K=key(X,y,Z);
    if(cells.contains(K)||reserved(X,y,Z)||y<=floorAt(X,Z)+3)continue;
    cells.emplace(K,JungleCell{X,y,Z,uint16_t(JungleVine+d[2])});tree->attachedVines++;
   }
  }
 }
 // A few low cocoa pods on real jungle log faces. IDs encode support direction
 // (+Z,-X,-Z,+X), as verified against vanilla neighbor updates in BDS.
 if(a.h>=18&&r(14801)<.22)for(int i=0;i<1+int(r(14802)*2);i++){
  int y=a.ground+2+int(r(14803+i)*3),direction=int(r(14810+i)*4);
  auto s=tree->stem[clamp(y-a.ground-1,0,int(tree->stem.size())-1)];
  int X=s[0]+s[3]/2,Z=s[2]+s[3]/2;
  if(direction==0)Z=s[2];if(direction==1)X=s[0]+s[3]-1;
  if(direction==2)Z=s[2]+s[3]-1;if(direction==3)X=s[0];
  auto support=cells.find(key(X,y,Z));if(support==cells.end()||support->second.block!=14)continue;
  if(direction==0)--Z;if(direction==1)++X;if(direction==2)++Z;if(direction==3)--X;
  if(!cells.contains(key(X,y,Z))&&!reserved(X,y,Z)&&y>floorAt(X,Z)+1)cells.emplace(key(X,y,Z),JungleCell{X,y,Z,uint16_t(90+direction)});
 }
 std::vector<int> keys;for(auto const&[k,c]:cells)keys.push_back(k);std::sort(keys.begin(),keys.end());for(int k:keys)tree->cells.push_back(cells.at(k));
 auto bucket=[&](JungleCell const&p){int X=p.x/16-a.x/16+2,Z=p.z/16-a.z/16+2;return (Z*5+X)*6+jungleKind(p.block);};
 for(auto const&p:tree->cells)++tree->tileBegin[bucket(p)+1];
 for(size_t i=1;i<tree->tileBegin.size();i++)tree->tileBegin[i]+=tree->tileBegin[i-1];
 auto cursor=tree->tileBegin;tree->tileCells.resize(tree->cells.size());
 for(uint32_t i=0;i<tree->cells.size();i++)tree->tileCells[cursor[bucket(tree->cells[i])]++]=i;
 return tree;
});}
std::vector<std::shared_ptr<JungleTree>> jungles(int x0,int z0,int size){
 std::vector<std::shared_ptr<JungleTree>> out;
 for(int gz=int(std::floor((z0-JungleHalo-1)/7.));gz<=int(std::floor((z0+size+JungleHalo+1)/7.));gz++)for(int gx=int(std::floor((x0-JungleHalo-1)/7.));gx<=int(std::floor((x0+size+JungleHalo+1)/7.));gx++){
  // Empty/rejected sites belong in the cheap site cache. They used to allocate
  // an empty tree and evict real shapes even in biomes with no jungle trees.
  auto a=jungleSite(gx,gz);if(!jungleAccepted(gx,gz,a))continue;
  auto t=jungleTree(gx,gz);if(!t->cells.empty())out.push_back(t);
 }return out;
}
void jungleEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 auto trees=jungles(x0,z0,16);
 auto put=[&](int x,int y,int z,uint16_t id,bool wood=false){int X=x-x0,Z=z-z0;if(X<0||X>=16||Z<0||Z>=16||y<-60||y>319)return;auto const&s=surfaces[Z*16+X];if(y<=s.height||y<=s.water||y<=s.lava)return;auto offset=(Z*16+X)*Height+y+64;auto&b=raw[offset];
  if(id==JungleMoss||id==37){auto below=raw[offset-1];if(id==37?below!=JungleMoss:(below!=14&&below!=JungleWood))return;if(b==0||b==15)b=id;return;}
  if(b==0||(wood&&(b==11||b==13||b==15||b==17||b==66)))b=id;};
 // All woody skeletons precede foliage across the entire overlapping stand.
 for(int pass=0;pass<6;pass++)for(auto const&t:trees){
  if(nativeReserved(t->site.x,t->site.z)||!forestTreeKept(t->site.x,t->site.z))continue;
  int X=x0/16-t->site.x/16+2,Z=z0/16-t->site.z/16+2;
  if(X<0||X>=5||Z<0||Z>=5)continue;int bucket=(Z*5+X)*6+pass;
  for(uint32_t i=t->tileBegin[bucket];i<t->tileBegin[bucket+1];i++){
   auto p=t->cells[t->tileCells[i]];put(p.x,p.y,p.z,p.block,pass<2);
  }
 }
 // Crown spill across biome borders was already applied above. Ground details
 // cannot place anything when this chunk has no eligible jungle surface.
 if(std::none_of(surfaces.begin(),surfaces.end(),[&](auto const&s){return jungleHabitat(s);}))return;
 auto soil=[](int b){return b==3||b==4||b==32||b==33||b==JungleMoss;};
 auto nearStem=[&](int x,int z){for(auto const&t:trees){auto const&a=t->site;if(x>=a.x-2&&x<=a.x+a.width+1&&z>=a.z-2&&z<=a.z+a.width+1)return true;}return false;};
 // Rare grounded deadwood, selected per world anchor. Validate its whole path
 // independently of the requested chunk so seams cannot split a fallen log.
 std::vector<std::pair<int,int>> debris;
 for(int gz=int(std::floor((z0-9)/24.));gz<=int(std::floor((z0+24)/24.));gz++)for(int gx=int(std::floor((x0-9)/24.));gx<=int(std::floor((x0+24)/24.));gx++){
  auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};int x=gx*24+int(r(14601)*24),z=gz*24+int(r(14602)*24);
  if(r(14603)>.22+.18*junglePatch(x,z))continue;
  auto s=field.surface(x,z);if(!jungleHabitat(s)||s.rock||blocked(x,z))continue;
  bool stump=r(14604)<.28;double angle=r(14605)*6.283185307;int len=stump?0:3+int(r(14606)*5);
  std::vector<std::pair<int,int>> path{{x,z}};int px=x,pz=z;
  for(int i=1;i<=len;i++){int X=x+roundJS(std::cos(angle)*i),Z=z+roundJS(std::sin(angle)*i);
   while(px!=X||pz!=Z){if(px!=X){px+=px<X?1:-1;path.emplace_back(px,pz);}if(pz!=Z){pz+=pz<Z?1:-1;path.emplace_back(px,pz);}}
  }
  bool valid=true;std::vector<int> heights;
  for(auto [X,Z]:path){auto b=field.surface(X,Z);int mat=field.material(b,X,b.height,Z);heights.push_back(b.height);
   if(!jungleHabitat(b)||b.rock||b.water>=b.height||!soil(mat)||std::abs(b.height-s.height)>1){valid=false;break;}
   // Query a bounded local stand, not a chunk-relative tree list.
   int siteX=int(std::floor(X/7.)),siteZ=int(std::floor(Z/7.));
   for(int dz=-2;dz<=2&&valid;dz++)for(int dx=-2;dx<=2&&valid;dx++){
    auto a=jungleSite(siteX+dx,siteZ+dz);
    if(a.h&&std::hypot(X-a.x,Z-a.z)<a.width+3&&jungleAccepted(siteX+dx,siteZ+dz,a))valid=false;
   }
   if(!valid)break;
  }
  if(!valid)continue;
  for(size_t i=0;i<path.size();i++){auto [X,Z]=path[i];int h=heights[i];
   int high=stump?h+2:h+1;
   // Adjacent columns share a face on one-block changes in ground level.
   if(!stump){if(i)high=std::max(high,heights[i-1]+1);if(i+1<heights.size())high=std::max(high,heights[i+1]+1);}
   for(int y=h+1;y<=high;y++)put(X,y,Z,JungleWood);
   debris.emplace_back(X,Z);
  }
 }
 // Low shrubs are clustered, with wide gaps between patches. The ground layer
 // is never a uniform carpet of collision blocks under the dense upper canopy.
 for(int gz=int(std::floor((z0-3)/9.));gz<=int(std::floor((z0+18)/9.));gz++)for(int gx=int(std::floor((x0-3)/9.));gx<=int(std::floor((x0+18)/9.));gx++){
  auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};int x=gx*9+int(r(12501)*9),z=gz*9+int(r(12502)*9);auto s=field.surface(x,z);
  if(!jungleHabitat(s)||s.rock||blocked(x,z)||nearStem(x,z)||r(12503)>.12+.52*junglePatch(x,z))continue;
  double radius=1.3+r(12504),phase=r(12505)*6.283185307;
  for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
   double rim=radius*(1+.13*std::sin(3*std::atan2(double(dz),double(dx))+phase)),q=(dx*dx+dz*dz)/(rim*rim);if(q>1||nearStem(x+dx,z+dz))continue;
   int X=x+dx-x0,Z=z+dz-z0;if(X<0||X>=16||Z<0||Z>=16)continue;auto const&local=surfaces[Z*16+X];int h=local.height;
   if(!jungleHabitat(local)||local.rock||std::abs(h-s.height)>1)continue;auto col=raw.data()+(Z*16+X)*Height;int n=q<.25?2+(r(12506)<.25):1;
   if(!soil(col[h+64]))continue;bool free=true;for(int dy=1;dy<=n;dy++)free&=col[h+64+dy]==0;if(!free)continue;
   for(int dy=1;dy<=n;dy++)col[h+64+dy]=(dx==0&&dz==0&&dy==1)?JungleWood:15;
  }
 }
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(!jungleHabitat(s)||s.rock||blocked(x0+x,z0+z))continue;int h=s.height,X=x0+x,Z=z0+z;auto col=raw.data()+(z*16+x)*Height;
  if(!soil(col[h+64])||col[h+65])continue;bool shade=false;
  for(int y=h+4;y<=std::min(319,h+60);y++)if(col[y+64]==15){shade=true;break;}
  double patch=clamp(.5+.8*understoryNoise.at(X/19.,Z/19.),0.,1.);
  if(shade&&patch>.58)col[h+64]=patch>.73?JungleMoss:33;
  if(nearStem(X,Z))continue;
  double pick=hash01(X,Z,terrainSeed^12601);
  bool nearDebris=std::any_of(debris.begin(),debris.end(),[&](auto p){return std::abs(p.first-X)+std::abs(p.second-Z)<=3;});
  double litter=nearDebris?.32:shade?.08+.13*patch:0.;
  if(pick<litter){col[h+65]=71+int(hash01(X,Z,terrainSeed^12602)*4);continue;}
  if(pick>.12+.40*patch)continue;
  if(patch>.5&&pick<.18&&!col[h+66]){col[h+65]=40;col[h+66]=41;}else col[h+65]=shade?37:36;
 }
 // Sparse food/bamboo patches: world anchors, no extra terrain/noise samples.
 // Include the four-block spill halo; reject off-chunk candidates before all
 // surface work. Tree geometry and the existing ground layer stay untouched.
 auto softPlant=[](int b){return b==0||b==36||b==37||b==40||b==41||(b>=71&&b<=74);};
 for(int gz=int(std::floor((z0-4)/32.));gz<=int(std::floor((z0+19)/32.));gz++)for(int gx=int(std::floor((x0-4)/32.));gx<=int(std::floor((x0+19)/32.));gx++){
  auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};double kind=r(14701);if(kind>=.50)continue;
  bool bamboo=kind>=.24;int radius=bamboo?4:2,count=bamboo?3+int(r(14702)*4):1+int(r(14702)*3);
  int ax=gx*32+int(r(14703)*32),az=gz*32+int(r(14704)*32);
  for(int i=0;i<count;i++){
   int X=ax+int(r(14710+i*3)*(2*radius+1))-radius,Z=az+int(r(14711+i*3)*(2*radius+1))-radius;
   int x=X-x0,z=Z-z0;if(x<0||x>=16||z<0||z>=16)continue;
   auto const&s=surfaces[z*16+x];int h=s.height;
   if(!jungleHabitat(s)||s.rock||s.water>=h||s.lava>=h||nearStem(X,Z)||blocked(X,Z))continue;
   auto col=raw.data()+(z*16+x)*Height;int n=bamboo?4+int(r(14712+i*3)*6):1;
   if(h+n+1>319||!soil(col[h+64]))continue;
   bool free=true;for(int dy=1;dy<=std::max(2,n+1);dy++)if(!softPlant(col[h+64+dy])){free=false;break;}
   if(!free)continue;
   // Replacing a fern base must also remove its upper half.
   if(col[h+65]==40&&col[h+66]==41)col[h+66]=0;
   if(!bamboo){col[h+65]=JungleMelon;continue;}
   for(int dy=1;dy<=n;dy++)col[h+64+dy]=JungleBamboo+(dy>=n-1?2:dy==n-2?1:0);
  }
 }
}
