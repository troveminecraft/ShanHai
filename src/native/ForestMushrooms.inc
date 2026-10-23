struct ForestMushroom{int x,z,g,h,r;bool red;};
Cache<uint64_t,ForestMushroom> mushroomCache{1024};
bool mushroomHabitat(Surface const&s)const{return (s.mushroom||(s.biome==9||s.biome==10||s.biome==23))&&!coniferHabitat(s)&&s.H>=.45&&!s.kind&&!s.lake&&!s.river&&s.stone<.01&&s.barren<.01&&s.iceHabitat<.01;}
ForestMushroom mushroomSite(int gx,int gz){return mushroomCache.get(key2(gx,gz),[&]{
 auto r=[&](int k){return hash01(gx,gz,terrainSeed^(52100+k));};ForestMushroom a{gx*24+5+int(r(1)*14),gz*24+5+int(r(2)*14),0,0,0,r(3)<.5};
 auto s=field.surface(a.x,a.z,false);
 if(!mushroomHabitat(s)||r(4)>(s.mushroom?.85:s.biome==10?.20:.11)||(!s.mushroom&&!treeGroundAllowed(s,a.x,a.z,false))||blocked(a.x,a.z,false))return a;
 a.r=a.red?2:3;int lo=s.height,hi=lo;
 for(int dz=-a.r;dz<=a.r;++dz)for(int dx=-a.r;dx<=a.r;++dx){auto b=field.surface(a.x+dx,a.z+dz,false);if(!mushroomHabitat(b)||(!b.mushroom&&!treeGroundAllowed(b,a.x+dx,a.z+dz,false))||nativeReserved(a.x+dx,a.z+dz))return ForestMushroom{};lo=std::min(lo,b.height);hi=std::max(hi,b.height);}
 if(hi-lo>1)return ForestMushroom{};a.g=s.height;a.h=(a.red?7:4)+int(r(5)*4);return a;
});}
void forestMushrooms(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!forestDesign)return;
 // Small mushrooms use the same island soil and reservations as the caps.
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){auto const&s=surfaces[z*16+x];if(!s.mushroom||nativeReserved(x0+x,z0+z))continue;
  int y=s.height+1;if(y>Height-65)continue;double q=hash01(x0+x,z0+z,terrainSeed^72311);
  auto&b=raw[(z*16+x)*Height+y+64];if(b==0&&q<.025)b=q<.0125?67:68;
 }
 for(int gz=int(std::floor((z0-5)/24.));gz<=int(std::floor((z0+20)/24.));++gz)for(int gx=int(std::floor((x0-5)/24.));gx<=int(std::floor((x0+20)/24.));++gx){auto a=mushroomSite(gx,gz);if(!a.h||nativeReserved(a.x,a.z))continue;
  // Reject complete tree overlap from analytic descriptors, independent of
  // output-chunk order. Never replace wood/leaves with a mushroom fragment.
  bool collision=false;for(auto const&t:broadleaves(a.x,a.z,0))if(forestTreeKept(t.x,t.z)){
   double distance=std::hypot(t.x-a.x,t.z-a.z);
   if(distance>a.r+5+2.4*t.radius)continue;
   auto spine=oakSpine(t);auto top=spine.back();
   for(auto p:spine)if(p.y<=a.g+a.h&&p.y>=a.g&&std::abs(p.x-a.x)<=a.r+1&&std::abs(p.z-a.z)<=a.r+1){collision=true;break;}
   auto crownHits=[&](double x,double y,double z,double radius,double ry){
    double dy=std::max({0.,y-(a.g+a.h),a.g+1-y});if(dy>ry)return false;
    double extent=radius*1.2*std::sqrt(std::max(0.,1-dy*dy/(ry*ry)));
    double dx=std::max(0.,std::abs(x-a.x)-a.r),dz=std::max(0.,std::abs(z-a.z)-a.r);
    return dx*dx+dz*dz<extent*extent;
   };
   if(crownHits(top.x,top.y,top.z,t.radius*.8,std::max(3,t.h/8)))collision=true;
   int branches=3+(t.h>=26?2:0)+int(hash01(t.x,t.z,terrainSeed^2473)*4);
   for(int k=0;k<branches;++k){double pick=hash01(t.x,t.z,terrainSeed^(2480+k)),angle=t.phase+k*2.39996323+pick*.6;
    double reach=t.radius*(.65+.6*hash01(t.x,t.z,terrainSeed^(2490+k)));
    int y=t.ground+t.h-std::max(3,t.h/8)-int(pick*std::min(12,t.h/3));
    if(crownHits(top.x+roundJS(std::cos(angle)*reach),y+1,top.z+roundJS(std::sin(angle)*reach),t.radius*(.56+.33*hash01(t.x,t.z,terrainSeed^(2500+k))),1.5+pick*1.5+std::max(0,t.h-20)*.06))collision=true;
   }
  }
  for(auto const&t:conifers(a.x,a.z,0))if(forestTreeKept(t.x,t.z)&&std::hypot(t.x-a.x,t.z-a.z)<a.r+2)collision=true;
  auto habitat=field.surface(a.x,a.z,false);
  if(habitat.biome==10)for(int gz=(a.z-12)/16;gz<=(a.z+12)/16;++gz)for(int gx=(a.x-12)/16;gx<=(a.x+12)/16;++gx){auto t=wetDrySite(gx,gz);if(wetDryAccepted(gx,gz,t)&&forestTreeKept(t.x,t.z)&&std::hypot(t.x-a.x,t.z-a.z)<a.r+8)collision=true;}
  if(habitat.biome==23){
   // Test the cached full crown, not only the trunk distance. Expand the
   // descriptor query by the mushroom footprint so adjacent chunks agree.
   auto hits=[&](auto const&cells){for(auto const&p:cells){
    if(p.y<a.g+1||p.y>a.g+a.h)continue;
    if(p.x==a.x&&p.z==a.z)return true;
    int capLow=a.g+(a.red?a.h-3:a.h);
    if(p.y>=capLow&&std::abs(p.x-a.x)<=a.r&&std::abs(p.z-a.z)<=a.r)return true;
   }return false;};
   for(auto const&t:birches(a.x-a.r,a.z-a.r,a.r*2))if(forestTreeKept(t->x,t->z)&&hits(t->cells)){collision=true;break;}
   if(!collision)for(auto const&t:birchOaks(a.x-a.r,a.z-a.r,a.r*2))if(forestTreeKept(t->site.x,t->site.z)&&hits(t->cells)){collision=true;break;}
  }
  if(habitat.biome==13)for(auto const&t:jungles(a.x,a.z,0))if(forestTreeKept(t->site.x,t->site.z)&&std::hypot(t->site.x-a.x,t->site.z-a.z)<a.r+t->site.width+2)collision=true;
  if(habitat.giant>.1)for(auto const&t:giants(a.x,a.z,1))if(forestTreeKept(t->x,t->z)&&std::hypot(t->x-a.x,t->z-a.z)<a.r+t->radius+2)collision=true;
  if(collision)continue;
  auto put=[&](int X,int y,int Z,int id){int x=X-x0,z=Z-z0;if(x<0||x>=16||z<0||z>=16||y>Height-65)return;auto&s=surfaces[z*16+x];if(y<=std::max({s.height,s.water,s.lava}))return;auto&b=raw[(z*16+x)*Height+y+64];if(b==0||(b>=36&&b<=41)||(b>=62&&b<=64)||b==67||b==68||(b>=71&&b<=74))b=id;};
  for(int y=1;y<a.h;++y)put(a.x,a.g+y,a.z,167);
  int low=a.red?a.h-3:a.h;
  for(int y=low;y<=a.h;++y){int radius=a.red?(y==a.h?1:2):3;
   for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx){if((!a.red||y<a.h)&&std::abs(dx)==radius&&std::abs(dz)==radius)continue;if(a.red&&y<a.h&&std::abs(dx)<radius&&std::abs(dz)<radius)continue;
    // Chamfering a corner exposes TWO outward faces on its neighbours.
    // Test the cap silhouette, not just equality with the bounding square.
    auto inside=[&](int x,int z){return std::abs(x)<=radius&&std::abs(z)<=radius&&!((!a.red||y<a.h)&&std::abs(x)==radius&&std::abs(z)==radius);};
    int sideX=dx<0&&!inside(dx-1,dz)?-1:dx>0&&!inside(dx+1,dz)?1:0;
    int sideZ=dz<0&&!inside(dx,dz-1)?-1:dz>0&&!inside(dx,dz+1)?1:0;
    int state=5+sideX+sideZ*3;put(a.x+dx,a.g+y,a.z+dz,(a.red?158:149)+state-1);
   }
  }
 }
}
