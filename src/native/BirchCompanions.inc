// Sparse high oaks and low vanilla-leaf shrubs belong to the birch stand.
// They use the same seed/spacing reservations, with world-anchored shapes.
struct BirchOakCell{int x,y,z;uint16_t block;};
struct BirchOak{BirchStandSite site;std::vector<BirchOakCell> cells;std::array<float,529> litter{};};
Cache<uint64_t,std::shared_ptr<BirchOak>> birchOakCache{256};
std::shared_ptr<BirchOak> birchOak(int gx,int gz){return birchOakCache.get(key2(gx,gz),[&]{
 auto tree=std::make_shared<BirchOak>();auto a=birchStandSite(gx,gz);tree->site=a;
 if(!a.oak||!birchStandAccepted(gx,gz,a))return tree;
 auto neighbors=birchStandNeighbors(a.x,a.z);auto r=[&](int k){return hash01(a.x,a.z,terrainSeed^k);};
 std::unordered_map<int,BirchOakCell> cells;
 std::array<int,361> floors;floors.fill(-32768);
 auto put=[&](int x,int y,int z,int block){
  int dx=x-a.x,dz=z-a.z,dy=y-a.ground;
  if(std::abs(dx)>9||std::abs(dz)>9||dy<1||dy>a.height)return;
  if(block!=11)for(auto const&n:neighbors)if(std::hypot(x-n.x,z-n.z)<1.5&&y>n.ground&&y<n.ground+n.height)return;
  auto&floor=floors[(dz+9)*19+dx+9];if(floor==-32768){auto s=field.surface(x,z,false);floor=std::max({s.height,s.water,s.lava});}if(y<=floor)return;
  int key=(dy*32+dz+16)*32+dx+16;
  auto it=cells.find(key);if(it==cells.end()||block!=11)cells.insert_or_assign(key,BirchOakCell{x,y,z,uint16_t(block)});
 };
 double phase=r(11301)*6.283185307;int top=a.ground+a.height-4;
 auto crown=[&](double cx,double cy,double cz,double rx,double ry,double rz){
  for(int z=int(std::floor(cz-rz*1.14));z<=int(std::ceil(cz+rz*1.14));z++)
  for(int x=int(std::floor(cx-rx*1.14));x<=int(std::ceil(cx+rx*1.14));x++)
  for(int y=int(std::floor(cy-ry));y<=int(std::ceil(cy+ry));y++){
   double dx=x-cx,dz=z-cz,dy=(y-cy)/ry,angle=std::atan2(dz,dx);
   double edge=1+.09*std::sin(angle*3+phase)+.04*std::cos(angle*5-phase);
   double d=(dx*dx/(rx*rx)+dz*dz/(rz*rz))/(edge*edge)+dy*dy;
   if(d<=1&&(d<.84||hash01(x,z,terrainSeed^(11303+y*37))>.18))put(x,y,z,11);
  }
 };
 auto line=[&](OakPoint from,OakPoint to){
  put(from.x,from.y,from.z,69);
  while(from.x!=to.x||from.y!=to.y||from.z!=to.z){
   if(from.x!=to.x){from.x+=from.x<to.x?1:-1;put(from.x,from.y,from.z,69);}
   if(from.z!=to.z){from.z+=from.z<to.z?1:-1;put(from.x,from.y,from.z,69);}
   if(from.y!=to.y){from.y+=from.y<to.y?1:-1;put(from.x,from.y,from.z,69);}
  }
 };
 auto clearLine=[&](OakPoint from,OakPoint to){
  auto clear=[&](OakPoint q){for(auto const&n:neighbors)
   if(std::hypot(q.x-n.x,q.z-n.z)<1.5&&q.y>n.ground&&q.y<n.ground+n.height)return false;return true;};
  if(!clear(from))return false;
  while(from.x!=to.x||from.y!=to.y||from.z!=to.z){
   if(from.x!=to.x){from.x+=from.x<to.x?1:-1;if(!clear(from))return false;}
   if(from.z!=to.z){from.z+=from.z<to.z?1:-1;if(!clear(from))return false;}
   if(from.y!=to.y){from.y+=from.y<to.y?1:-1;if(!clear(from))return false;}
  }return true;
 };
 crown(a.x,top,a.z,2.6+r(11304),4.,2.8+r(11305));
 put(a.x,a.ground+a.height,a.z,11); // Keep the declared 28-36 block crown tip.
 int branches=4+int(r(11306)*3);
 for(int k=0;k<branches;k++){
  double angle=phase+k*2.39996323,reach=3.2+r(11310+k)*2.1;
  int start=a.ground+int(a.height*(.64+.12*r(11320+k)));
  OakPoint from{a.x,start,a.z},end{a.x+roundJS(std::cos(angle)*reach),top-1-int(r(11330+k)*4),a.z+roundJS(std::sin(angle)*reach)};
  OakPoint mid{roundJS((from.x+end.x)*.5),start+1,roundJS((from.z+end.z)*.5)};
  if(!clearLine(from,mid)||!clearLine(mid,end))continue;
  crown(end.x,end.y+1,end.z,2.3+r(11340+k)*1.2,2.1+r(11350+k)*1.1,2.2+r(11360+k)*1.2);
  line(from,mid);line(mid,end);
 }
 for(int y=a.ground+1;y<=top;y++){
  put(a.x,y,a.z,10);
  // Small asymmetric root flare; high clear bole instead of a low spreading oak.
  int rise=y-a.ground;
  for(int k=0;k<4;k++)if(rise<=2+int(r(11370+k)*4)){
   auto v=birchRotate(1,0,0,k);put(a.x+v[0],y,a.z+v[2],69);
  }
 }
 std::vector<int> keys;for(auto const&[key,c]:cells)keys.push_back(key);std::sort(keys.begin(),keys.end());
 for(auto key:keys){auto c=cells.at(key);tree->cells.push_back(c);
  if(c.block==11)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
   double w=std::max(0.,1-(dx*dx+dz*dz)/7.5625);
   tree->litter[(c.z-a.z+dz+11)*23+c.x-a.x+dx+11]+=float(.055*w*w);
  }
 }return tree;
});}
std::vector<std::shared_ptr<BirchOak>> birchOaks(int x0,int z0,int size){
 std::vector<std::shared_ptr<BirchOak>> out;
 for(int gz=int(std::floor((z0-11)/5.));gz<=int(std::floor((z0+size+11)/5.));gz++)
 for(int gx=int(std::floor((x0-11)/5.));gx<=int(std::floor((x0+size+11)/5.));gx++){
  auto site=birchStandSite(gx,gz);if(!site.oak||!birchStandAccepted(gx,gz,site))continue;
  auto a=birchOak(gx,gz);if(!a->cells.empty())out.push_back(a);
 }return out;
}
void birchCompanions(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,
                     std::vector<std::shared_ptr<BirchTree>>const&birchTrees,std::array<float,256>&litter){
 auto oaks=birchOaks(x0,z0,16);
 for(auto const&t:oaks)if(forestTreeKept(t->site.x,t->site.z))for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  int dx=x0+x-t->site.x,dz=z0+z-t->site.z;
  if(std::abs(dx)<=11&&std::abs(dz)<=11)litter[z*16+x]+=t->litter[(dz+11)*23+dx+11];
 }
 for(bool wood:{true,false})for(auto const&t:oaks)for(auto const&p:t->cells){
  if(nativeReserved(t->site.x,t->site.z)||!forestTreeKept(t->site.x,t->site.z))continue;
  if((p.block!=11)!=wood)continue;int x=p.x-x0,z=p.z-z0;if(x<0||x>=16||z<0||z>=16)continue;
  auto&v=raw[(z*16+x)*Height+p.y+64];
  bool leaves=v==11||v==66;
  if(v==0||(wood&&leaves))v=p.block;
 }
 if(forestDesign)for(auto const&t:oaks){auto const&a=t->site;if(!forestTreeKept(a.x,a.z)||nativeReserved(a.x,a.z)||hash01(a.x,a.z,terrainSeed^52607)>.06)continue;
  int side=int(hash01(a.x,a.z,terrainSeed^52608)*4);auto d=std::array<std::array<int,2>,4>{{{0,-1},{1,0},{0,1},{-1,0}}}[side];
  for(auto p:t->cells)if(p.block==69&&p.y>a.ground+2&&p.y<a.ground+9){int X=p.x+d[0],Z=p.z+d[1];bool occupied=false;for(auto q:t->cells)if(q.x==X&&q.y==p.y&&q.z==Z){occupied=true;break;}if(occupied)continue;int x=X-x0,z=Z-z0;if(x>=0&&x<16&&z>=0&&z<16){auto&b=raw[(z*16+x)*Height+p.y+64];if(b==0)b=76+side;}}
 }
 auto soil=[](int b){return b==3||b==4||b==32||b==33;};
 // Patchy bushes leave open lanes. Every occupied shrub column touches its
 // local soil, including slopes; no hovering model or artificial soil pillar.
 for(int gz=int(std::floor((z0-3)/6.));gz<=int(std::floor((z0+18)/6.));gz++)
 for(int gx=int(std::floor((x0-3)/6.));gx<=int(std::floor((x0+18)/6.));gx++){
  auto r=[&](int k){return hash01(gx,gz,terrainSeed^k);};
  int x=gx*6+int(r(11401)*6),z=gz*6+int(r(11402)*6);auto s=field.surface(x,z,false);
  if(!birchHabitat(s))continue;s=field.surface(x,z);if(s.rock||blocked(x,z))continue;
  double patch=clamp(.52+.7*understoryNoise.at(x/25.,z/25.),.1,.95);
  if(r(11403)>.12+.42*patch)continue;
  bool nearStem=false;for(auto const&t:birchTrees)if(std::hypot(x-t->x,z-t->z)<2.3)nearStem=true;
  for(auto const&t:oaks)if(std::hypot(x-t->site.x,z-t->site.z)<2.7)nearStem=true;
  if(nearStem)continue;
  int lo=s.height,hi=s.height;for(auto [dx,dz]:{std::pair{-2,0},{2,0},{0,-2},{0,2}}){int y=field.base(x+dx,z+dz).height;lo=std::min(lo,y);hi=std::max(hi,y);}if(hi-lo>2)continue;
  double radius=1.25+r(11404)*.8,phase=r(11405)*6.283185307;int tall=2+(r(11406)<.22);
  for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
   double edge=radius*(1+.14*std::sin(3*std::atan2(double(dz),double(dx))+phase));
   double d=(dx*dx+dz*dz)/(edge*edge);if(d>1)continue;
   int X=x+dx-x0,Z=z+dz-z0;if(X<0||X>=16||Z<0||Z>=16)continue;
   auto const&local=surfaces[Z*16+X];if(!birchHabitat(local)||local.height>315)continue;
   auto col=raw.data()+(Z*16+X)*Height;int h=local.height;
   if(!soil(col[h+64]))continue;int n=d>.57?1:d>.17?2:tall;bool free=true;
   for(int dy=1;dy<=n;dy++)free&=col[h+64+dy]==0;if(!free)continue;
   // Original leaf blocks and biome colors; no custom foliage dependency.
   for(int dy=1;dy<=n;dy++)col[h+64+dy]=(dx==0&&dz==0&&dy==1)?65:11;
  }
 }
}
