// Sparse, seeded landform detail. Only eligible chunks visit this pass.
bool landscapeDetails=false;
Perlin landscapeNoise{seedHash(terrainSeed,52101)};
struct DetailSite {int x=0,z=0,y=0,type=0;double radius=0,angle=0;};
Cache<uint64_t,DetailSite> detailSites{512};
std::vector<std::array<double,4>> detailVolcanoes;
static bool detailIce(int b){return b==9||b==143||b==144;}
DetailSite detailSite(int gx,int gz){return detailSites.get(key2(gx,gz),[&]{
 DetailSite a;auto rnd=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 a.x=int((gx+.2+.6*rnd(52103))*128);a.z=int((gz+.2+.6*rnd(52105))*128);
 if(!field.unbounded()&&(a.x<terrain_bounds::MinBlock||a.x>terrain_bounds::MaxBlock||a.z<terrain_bounds::MinBlock||a.z>terrain_bounds::MaxBlock))return a;
 if(rnd(52111)>=.24&&rnd(52113)>=.10)return a;
 auto s=field.surface(a.x+32768,a.z+32768,false);
 if((s.kind&&s.kind!=4)||s.river||s.lake||s.water>=s.height||s.wetland>.01)return a;
 a.radius=9+8*rnd(52107);a.angle=rnd(52109)*6.28318530718;a.y=s.height;
 if(s.glacier>.65&&rnd(52111)<.24&&riftAt(a.x,a.z).cut<.01){
  int lo=s.height,hi=s.height,lx=0,lz=0;
  for(auto [dx,dz]:{std::pair{-20,0},std::pair{20,0},std::pair{0,-20},std::pair{0,20}}){
   auto t=field.surface(a.x+dx+32768,a.z+dz+32768,false);
   if(t.glacier<.60||t.kind||t.river||t.lake||t.water>=t.height)return a;
   if(t.height<lo){lo=t.height;lx=dx;lz=dz;}hi=std::max(hi,t.height);
  }
  if(hi-lo<=2&&s.superGlacier<.45){a.type=1;a.y=lo-1;}
  else if(hi-lo>=4&&lo>15){
   a.type=3;a.y=lo-12;a.radius=24;a.angle=std::atan2(double(lz),double(lx));
   for(int u=0;u<=24;u+=4){int X=a.x+int(std::cos(a.angle)*u),Z=a.z+int(std::sin(a.angle)*u);auto t=field.surface(X+32768,Z+32768,false);
    for(int y=a.y+int(u*.9)-3;y<=a.y+int(u*.9)+3;++y){int b=field.material(t,X+32768,y,Z+32768);if(b==8||b==43||b==126||b==145)a.type=0;}
   }
  }
 }
 // Rare eroded side vents: shallow open bowls, no extra lava source/cone.
 if(s.volcanic>.8&&!s.vent&&s.lava<s.height&&s.height>70&&s.height<210&&rnd(52113)<.10)a.type=2;
 return a;
});}
KARMA_NOINLINE void landscapePass(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!landscapeDetails)return;
 bool any=false;for(auto const&s:surfaces)any|=s.glacier>.65||s.volcanic>.2;if(!any)return;
 int X0=x0-32768,Z0=z0-32768;
 std::vector<DetailSite> sites;
 for(int gz=int(std::floor((Z0-24)/128.));gz<=int(std::floor((Z0+39)/128.));++gz)
 for(int gx=int(std::floor((X0-24)/128.));gx<=int(std::floor((X0+39)/128.));++gx){
  auto a=detailSite(gx,gz);if(a.type&&a.x+a.radius>=X0&&a.x-a.radius<X0+16&&a.z+a.radius>=Z0&&a.z-a.radius<Z0+16)sites.push_back(a);
 }
 for(int i=0;i<256;++i){auto const&s=surfaces[i];int X=X0+i%16,Z=Z0+i/16;auto col=raw.data()+i*Height;
  if((s.kind&&s.kind!=4)||s.river||s.lake||s.water>=s.height||s.wetland>.01||s.vent||s.lava>=s.height)continue;
  if(s.glacier<=.65&&s.volcanic<=.2)continue;
  double broad=landscapeNoise.at(X/61.,Z/61.),fine=landscapeNoise.at(X/13.+17,Z/13.-11);
  auto const&rift=closedRiftColumns[i];
  if(s.glacier>.65){
   for(auto const&a:sites)if(a.type==3){double dx=X-a.x,dz=Z-a.z,u=dx*std::cos(a.angle)+dz*std::sin(a.angle),v=-dx*std::sin(a.angle)+dz*std::cos(a.angle)-1.2*std::sin(u/7.);
    if(u<0||u>24||std::abs(v)>2.8)continue;
    int span=int(std::floor(2.8*std::sqrt(std::max(0.,1-v*v/7.84))));
    for(int y=std::max(-64,a.y+int(u*.9)-span);y<=std::min(Height-65,a.y+int(u*.9)+span);++y)if(detailIce(col[y+64])||undergroundRock(col[y+64])||(col[y+64]>=146&&col[y+64]<=148))col[y+64]=0;
    for(int y:{a.y+int(u*.9)-span-1,a.y+int(u*.9)+span+1})if(y>=-64&&y<=Height-65&&undergroundRock(col[y+64]))col[y+64]=143;
   }
   // Layer colour on existing walls; never fill crevasses or replace fluids.
   if(s.superGlacier>.5&&rift.valid){
    for(int y=std::max(-55,rift.floor+2);y<=rift.top;++y)if(detailIce(col[y+64])){
     double band=std::sin((y+10*broad)/9.);col[y+64]=uint16_t(band>.6?144:band<-.5?9:143);
    }
    // Small lateral ice alcoves open into the existing deep rift. Their
    // extent stays within solid ice and leaves an intact roof and bedrock.
    if(broad>.20&&rift.distance>rift.opening&&rift.distance<rift.opening+rift.undercut+1.7){
     int mid=rift.top-12-int(4*fine),span=2+int(2*smooth(.20,.55,broad));
     for(int y=std::max(rift.floor+4,mid-span);y<=std::min(rift.top-5,mid+span);++y)
      if(detailIce(col[y+64]))col[y+64]=0;
    }
   }
   if(rift.valid)continue;
   int top=std::min(Height-67,s.height+10);while(top>s.height-4&&!detailIce(col[top+64]))--top;
   if(top<=s.height-4||!detailIce(col[top+64]))continue;
   // Skip tall spires and retain their original lean and ice shell.
   if(col[top+65]!=0&&!(col[top+65]>=146&&col[top+65]<=148))continue;
   double wind=std::sin((X*.83+Z*.55)/17.+2*broad);
   if(wind>.25&&col[top+65]>=146&&col[top+65]<=148)col[top+65]=0;
   if(s.superGlacier>.5&&fine>.28&&broad<.1&&detailIce(col[top+63])&&detailIce(col[top+62])){
    if(col[top+65]>=146&&col[top+65]<=148)col[top+65]=0;
    col[top+64]=0;--top;col[top+64]=9;
   }
   if(s.superGlacier<.3&&wind<-.80&&broad>.05){
    // Reserve the snow cap inside this column; never spill into the next column.
    int rise=std::min(1+int(wind<-.97&&fine>.05),Height-66-top);
    if(col[top+65]>=146&&col[top+65]<=148)col[top+65]=0;
    for(int y=1;y<=rise;++y)col[top+y+64]=143;
    col[top+rise+65]=146;
    if(finalTerrainOres)top+=rise;
   }
   // Frozen shallow pools are cut into a low, screened patch, not perched.
   for(auto const&a:sites)if(a.type==1){
    double dx=X-a.x,dz=Z-a.z,q=std::hypot(dx,dz)/(a.radius*(1+.12*fine));
    if(q>=.88||std::abs(top-a.y)>6)continue;
    bool sealed=true;for(int y=a.y-3;y<=top;++y)sealed&=detailIce(col[y+64]);if(!sealed)continue;
    for(int y=a.y+1;y<=std::min(Height-65,top+2);++y)if(detailIce(col[y+64])||(col[y+64]>=146&&col[y+64]<=148))col[y+64]=0;
    col[a.y+64]=9;col[a.y+63]=q<.6?8:143;col[a.y+62]=143;
   }
  }else if(s.volcanic>.2&&s.height<280){
   int top=s.height;if(!undergroundRock(col[top+64])&&col[top+64]!=44&&col[top+64]!=45&&col[top+64]!=46)continue;
   // Broad coherent exposures, not per-block confetti.
   double blend=smooth(.15,.8,s.volcanic)*(0.6+.4*broad);
   int stone=(broad+.35*fine>blend)?98:(s.volcanicMountainRock?2:45);
   for(int d=0;d<3;++d)if(undergroundRock(col[top-d+64])||col[top-d+64]==44||col[top-d+64]==45||col[top-d+64]==46)col[top-d+64]=uint16_t(stone);
   double cut=0;bool cooled=false;
   for(auto const&v:detailVolcanoes){double dx=X-v[0],dz=Z-v[1],r=std::hypot(dx,dz);if(r<v[2]*1.6||r>v[3]*.92)continue;
    double angle=std::atan2(dz,dx);
    for(int n=0;n<3;++n){double axis=hash01(int(v[0]),int(v[1]),terrainSeed^(52201+n))*6.28318530718;
     double dist=std::abs(std::sin(angle-axis-.04*broad))*r;
     if(std::cos(angle-axis)>0&&dist<5+2*fine){cut=std::max(cut,2*(1-smooth(1.,6.,dist)));cooled=true;}
    }
   }
   for(auto const&a:sites)if(a.type==2){double dx=X-a.x,dz=Z-a.z,q=std::hypot(dx,dz)/a.radius,angle=std::atan2(dz,dx);
    // Open one side of the rim; erosion only, never raise a circular wall.
    if(q<1)cut=std::max(cut,(3+fine)*(1-smooth(.25,1.,q))*(.6+.4*std::cos(angle-a.angle)));
   }
   int depth=int(std::floor(cut));bool supported=true;for(int d=0;d<=depth+2;++d)supported&=col[top-d+64]!=0&&col[top-d+64]!=8&&col[top-d+64]!=43;
   if(supported){for(int d=0;d<depth;++d)col[top-d+64]=0;if(cooled)col[top-depth+64]=45;}
   // Scree in coarse patches stays supported on the existing slope.
   if(!cooled&&depth==0&&broad<-.23&&fine>.03&&s.height>85)col[top+64]=98;
  }
 }
}
J inspectLandscapeSites(int min,int max){J rows=J::array();
 for(int gz=int(std::floor(min/128.));gz<=int(std::floor(max/128.));++gz)
 for(int gx=int(std::floor(min/128.));gx<=int(std::floor(max/128.));++gx){auto a=detailSite(gx,gz);if(a.type)rows.push_back({{"x",a.x},{"z",a.z},{"y",a.y},{"type",a.type},{"radius",a.radius}});}
 return rows;
}
