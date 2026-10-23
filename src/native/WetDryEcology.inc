// Bounded native ecology for the two previously generic warm woodland types.
static constexpr uint16_t SwampMud=85,SwampLily=86,SwampReeds=87,DryBush=88,AcaciaWood=89,CocoaBase=90;
struct WetDryTree{int x=0,z=0,g=0,h=0;bool swamp=false;double phase=0,priority=0;};
Cache<uint64_t,WetDryTree> wetDrySites{1024};
bool wetDryHabitat(Surface const&s)const{
 return (s.biome==10||s.biome==12)&&!s.kind&&!s.lake&&!s.river&&s.giant<.01&&s.stone<.01&&s.barren<.01&&s.cliffLow==-32768&&s.height>=32&&s.height<180;
}
WetDryTree wetDrySite(int gx,int gz){return wetDrySites.get(key2(gx,gz),[&]{
 WetDryTree a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 a.x=gx*16+2+int(r(15101)*12);a.z=gz*16+2+int(r(15102)*12);a.priority=r(15103);
 auto s=field.surface(a.x,a.z,false);if(!wetDryHabitat(s))return a;
 a.swamp=s.biome==10;double patch=clamp(.5+forestNoise.at(a.x/93.,a.z/93.),0.,1.);
 double interior=forestInterior(a.x,a.z,a.swamp?5:6);
 double density=(a.swamp?.52+.24*patch:.18+.32*patch)*(.18+.82*smooth(.12,.85,interior));
 if(s.desert>0)density*=1-smooth(.05,.4,s.desert)*(1-smooth(.2,.7,s.oasis)*.8);
 if(r(15104)>density||blocked(a.x,a.z,false))return a;
 s=field.surface(a.x,a.z);if(s.rock||s.water>=s.height||s.lava>=s.height)return a;
 if(!treeGroundAllowed(s,a.x,a.z,false))return a;
 int material=field.material(s,a.x,s.height,a.z);if(material!=3&&material!=4&&material!=32&&material!=33&&!(a.swamp&&material==5))return a;
 for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=field.surface(a.x+dx,a.z+dz,false);
  if(b.water>=b.height||b.lava>=b.height||std::abs(b.height-s.height)>1)return a;
 }
 a.g=s.height;a.h=a.swamp?7+int(r(15105)*6*(.55+.45*interior)):9+int(r(15105)*8*(.55+.45*interior));a.phase=r(15106)*6.283185307;return a;
});}
bool wetDryAccepted(int gx,int gz,WetDryTree const&a){
 if(!a.h)return false;
 for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dz)continue;auto b=wetDrySite(gx+dx,gz+dz);
  if(b.h&&std::hypot(a.x-b.x,a.z-b.z)<(a.swamp?10.:14.)&&(b.priority<a.priority||(b.priority==a.priority&&std::pair{b.x,b.z}<std::pair{a.x,a.z})))return false;
 }return true;
}
void wetDryEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 auto put=[&](int X,int y,int Z,uint16_t id,bool wood=false){int x=X-x0,z=Z-z0;
  if(x<0||x>=16||z<0||z>=16||y<-60||y>Height-65)return;
  auto const&s=surfaces[z*16+x];if(y<=std::max({s.height,s.water,s.lava}))return;
  auto&v=raw[(z*16+x)*Height+y+64];if(v==0||(wood&&(v==11||v==17)))v=id;
 };
 std::vector<WetDryTree> trees;
 std::vector<std::array<int,5>> vines;
 for(int gz=int(std::floor((z0-12)/16.));gz<=int(std::floor((z0+27)/16.));gz++)for(int gx=int(std::floor((x0-12)/16.));gx<=int(std::floor((x0+27)/16.));gx++){
  auto a=wetDrySite(gx,gz);if(wetDryAccepted(gx,gz,a))trees.push_back(a);
 }
 // Skeletons first, then leaf crowns. No whole-world decoration pass.
 for(int pass=0;pass<2;pass++)for(auto const&a:trees){
  if(nativeReserved(a.x,a.z)||(a.swamp&&!forestTreeKept(a.x,a.z)))continue;
  uint16_t wood=a.swamp?69:AcaciaWood,leaf=a.swamp?11:17;
  int vineAttempts=0;
  auto line=[&](OakPoint p,OakPoint q){if(pass)return;put(p.x,p.y,p.z,wood,true);
   while(p.x!=q.x||p.y!=q.y||p.z!=q.z){
    if(p.x!=q.x){p.x+=p.x<q.x?1:-1;put(p.x,p.y,p.z,wood,true);}
    if(p.z!=q.z){p.z+=p.z<q.z?1:-1;put(p.x,p.y,p.z,wood,true);}
    if(p.y!=q.y){p.y+=p.y<q.y?1:-1;put(p.x,p.y,p.z,wood,true);}
   }
  };
  auto crown=[&](OakPoint p,double radius,int salt){if(!pass)return;
   for(int dz=-6;dz<=6;dz++)for(int dx=-6;dx<=6;dx++){
    double angle=std::atan2(double(dz),double(dx)),rim=radius*(1+.13*std::sin(3*angle+a.phase)+.07*std::cos(5*angle-salt));
    double q=(dx*dx+dz*dz)/(rim*rim);if(q>1)continue;
    int low=p.y-(q<.6?1:0),high=p.y+1+(q<.40);
    for(int y=low;y<=high;y++)put(p.x+dx,y,p.z+dz,leaf);
    if(a.swamp&&(!forestDesign||hash01(a.x,a.z,terrainSeed^52605)<.62)&&q>.65&&vineAttempts<5&&hash01(p.x+dx,p.z+dz,terrainSeed^(15201+salt))<.065){
     int side=std::abs(dx)>std::abs(dz)?(dx>0?1:3):(dz>0?2:0);
     auto d=std::array<std::array<int,2>,4>{{{0,-1},{1,0},{0,1},{-1,0}}}[side];
     vines.push_back({p.x+dx+d[0],low,p.z+dz+d[1],76+side,2+int(hash01(p.x+dx,p.z+dz,terrainSeed^15202)*3)});++vineAttempts;
    }
   }
  };
  OakPoint root{a.x,a.g+1,a.z},fork{a.x,a.g+int(a.h*.52),a.z};line(root,fork);
  int branches=a.swamp?3:2+int(hash01(a.x,a.z,terrainSeed^15110)<.4);
  for(int k=0;k<branches;k++){
   double angle=a.phase+k*(a.swamp?2.15:2.65),reach=(a.swamp?2.2:3.0)+hash01(a.x,a.z,terrainSeed^(15111+k))*2.0;
   OakPoint p{a.x+roundJS(std::cos(angle)*reach),a.g+a.h-2-k%2,a.z+roundJS(std::sin(angle)*reach)};
   line(fork,p);crown(p,(a.swamp?3.2:3.5)+hash01(a.x,a.z,terrainSeed^(15121+k)),k);
  }
  if(!pass)for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto s=field.surface(a.x+dx,a.z+dz,false);
   if(s.height<=a.g&&s.water<s.height)for(int y=s.height+1;y<=a.g+1;y++)put(a.x+dx,y,a.z+dz,wood,true);
  }
 }
 // The first vine has a side support; a leaf only above it is insufficient
 // in Bedrock. Candidate positions are derived before chunk clipping.
 for(auto v:vines){int x=v[0]-x0,z=v[2]-z0,y=v[1];if(x<0||x>=16||z<0||z>=16)continue;
  auto col=raw.data()+(z*16+x)*Height;auto const&s=surfaces[z*16+x];
  for(int n=0;n<v[4]&&y-n>std::max({s.height+3,s.water,s.lava})&&!col[y-n+64];n++)col[y-n+64]=uint16_t(v[3]);
 }
 auto soil=[](int b){return b==3||b==4||b==32||b==33||b==SwampMud;};
 bool wetMargin=forestInterior(x0+8,z0+8,5)>0,dryMargin=forestInterior(x0+8,z0+8,6)>0;
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];int X=x0+x,Z=z0+z,h=s.height;
  // Dry drainage deposits remain exposed, including their soil/rock reaches.
  // A neighbouring savanna must not repaint these as grassy ecotone columns.
  if(s.aridType)continue;
  double wet=(wetMargin||s.biome==10)?forestInterior(X,Z,5):0,dry=(dryMargin||s.biome==12)?forestInterior(X,Z,6):0;
  bool swamp=s.biome==10||wet>dry;double intensity=swamp?wet:dry;
  bool water=s.water>h&&s.water-h<=3&&!s.lakeFrozen&&s.lava<h&&(s.wetland>0||s.lake||s.river>0);
  bool fringe=false;
  if(s.biome==3&&h==field.seaLevel()&&s.water<h&&hash01(X,Z,terrainSeed^15211)<.13)
   for(int d:{8,16})for(auto [dx,dz]:{std::pair{d,0},{-d,0},{0,d},{0,-d}})if(field.surface(X+dx,Z+dz,false).biome==10)fringe=true;
  bool margin=intensity>0&&!s.kind&&!s.lake&&!s.river&&s.stone<.01&&s.giant<.01&&s.barren<.01&&s.height>=32&&s.height<180&&s.cliffLow==-32768;
  if(!wetDryHabitat(s)&&!water&&!fringe&&!margin)continue;
  auto col=raw.data()+(z*16+x)*Height;double pick=hash01(X,Z,terrainSeed^15211);
  if(water){
   if(pick>.065)continue;bool nearby=s.wetland>.45;
   for(auto [dx,dz]:{std::pair{8,0},{-8,0},{0,8},{0,-8}})if(field.surface(X+dx,Z+dz,false).biome==10){nearby=true;break;}
   if(nearby&&s.water<Height-66&&col[s.water+64]==8&&!col[s.water+65])col[s.water+65]=SwampLily;
   continue;
  }
  if(s.rock||h>300||s.water>=h||s.lava>=h||(!soil(col[h+64])&&!((swamp||fringe)&&col[h+64]==5))||col[h+65]||blocked(X,Z))continue;
  bool nearTrunk=false;for(auto const&t:trees)if(std::hypot(X-t.x,Z-t.z)<2.6){nearTrunk=true;break;}if(nearTrunk)continue;
  double patch=clamp(.5+understoryNoise.at(X/27.,Z/27.),0.,1.);
  if(swamp||fringe){
   if(swamp&&intensity*(.45+.55*patch)>.62)col[h+64]=SwampMud;
   bool bank=false;
   if(pick<.13)for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=field.surface(X+dx,Z+dz,false);if(b.water==h&&b.water>b.height&&b.lava<h){bank=true;break;}}
   if(bank){int n=2+int(hash01(X,Z,terrainSeed^15212)*2);bool free=true;for(int dy=1;dy<=n;dy++)free&=col[h+64+dy]==0;
    if(free){for(int dy=1;dy<=n;dy++)col[h+64+dy]=SwampReeds;continue;}
   }
   if(fringe)continue;
   if(patch>.55&&pick<.05*intensity)col[h+65]=pick<.037*intensity?67:68;
   else if(pick<(.14+.20*patch)*intensity){if(pick<.07*intensity&&col[h+66]==0){col[h+65]=40;col[h+66]=41;}else col[h+65]=37;}
  }else{
   bool drySoil=patch<.26*intensity;
   if(drySoil)col[h+64]=33;
   if(drySoil&&pick<.018*intensity)col[h+65]=DryBush;
   else if(pick<(.12+.38*patch)*intensity){if(patch>.6&&pick<.10*intensity&&col[h+66]==0){col[h+65]=38;col[h+66]=39;}else col[h+65]=36;}
  }
 }
}
