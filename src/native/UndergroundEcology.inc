// A bounded post-pass over already generated blocks. It never carves rock,
// changes a fluid, or asks for another chunk. Ore exposure uses a bounded
// one-column surface halo only when a candidate touches a chunk edge.
bool undergroundEcologyEnabled=false;
bool caveBiomesEnabled=false;
bool caveHabitatRefinement=false;
Perlin geologyNoise{seedHash(terrainSeed,4101)},caveHabitatNoise{seedHash(terrainSeed,4103)};
static bool undergroundRock(int b){return b==2||(b>=95&&b<=99);}
 #include "OreProfiles.inc"
// Keep this optional pass out of the already large surface/feature chunk loop.
// Inlining it there can increase instruction-cache pressure for every biome.
KARMA_NOINLINE void undergroundEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!undergroundEcologyEnabled)return;
 std::array<int,256> limit{};
 std::array<double,256> rockField{},habitat{};
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){
  int i=z*16+x;auto const&s=surfaces[i];
  limit[i]=high_mountain_underground::scanTop(s,field.highMountainUndergroundEnabled(),Height-65,120,14);
  // Keep existing volcanic shafts, scarps and their supporting rock intact.
  if(s.volcanic>.01||s.vent||s.scarp>.01||limit[i]<-55){limit[i]=-57;continue;}
  int X=x0+x-32768,Z=z0+z-32768;
  double broad=geologyNoise.at(X/210.,Z/210.);
  double fold=geologyNoise.at(X/65.+17,Z/65.-9);
  rockField[i]=broad;habitat[i]=caveHabitatNoise.at(X/72.,Z/72.);
  int deep=int(std::floor(-3+9*broad+3*fold));
  int band=int(std::floor(-30+14*broad+7*fold));
  int thickness=5+int(4*(fold+.5));
  auto col=raw.data()+i*Height;
  // Correlated transition patches rather than a single horizontal or climate
  // threshold. One detail sample per column, not per block 3-D Perlin noise.
  double edge=caveBiomesEnabled?geologyNoise.at(X/13.+53,Z/13.-37):0;
  double upperMountain=field.highMountainUndergroundEnabled()?high_mountain_underground::weight(s):0;
  for(int y=finalTerrainOres?-59:-56;y<=limit[i];++y){
   if(col[y+64]!=2)continue;
   if(y>120){
    double grain=hash01(X,Z,seedHash(terrainSeed,4701+uint32_t(y)*17));
    col[y+64]=high_mountain_underground::upperRock(y,broad,fold,habitat[i],grain,upperMountain);continue;
   }
   if(caveBiomesEnabled){
    double grain=hash01(X,Z,seedHash(terrainSeed,4601+uint32_t(y)*17));
    double shift=edge*5+(grain-.5)*2;
    if(y<deep+shift)col[y+64]=95;
    double pick=clamp(.5+edge*.7+(grain-.5)*.35,0.,1.);
    double tuff=(1-smooth(thickness-2,thickness+3,std::abs(y-band-shift)))*smooth(-.20,-.04,broad);
    double granite=smooth(.17,.29,broad)*smooth(deep,deep+7,y-shift)*(1-smooth(34+12*fold,43+12*fold,y-shift));
    double andesite=(1-smooth(-.28,-.16,broad))*smooth(deep,deep+7,y-shift)*(1-smooth(52+16*fold,61+16*fold,y-shift));
    double diorite=(1-smooth(-.36,-.24,habitat[i]))*smooth(deep+4,deep+12,y-shift)*(1-smooth(68,77,y-shift));
    if(pick<tuff)col[y+64]=96;else if(pick<granite)col[y+64]=97;else if(pick<andesite)col[y+64]=98;else if(pick<diorite)col[y+64]=99;
    continue;
   }
   // Contiguous folded strata, not a random palette choice at every voxel.
   if(y<deep)col[y+64]=95;
   if(std::abs(y-band)<thickness&&broad>-.12)col[y+64]=96;
   else if(y>deep+3&&broad>.23&&y<38+int(12*fold))col[y+64]=97;
   else if(y>deep+3&&broad<-.22&&y<56+int(16*fold))col[y+64]=98;
   else if(y>deep+8&&habitat[i]<-.30&&y<72)col[y+64]=99;
  }
 }
 // Short seeded random walks form ore seams. Anchors are world cells; a
 // one-cell halo reproduces cross-chunk veins without neighbour loads.
 int gx0=(x0-32768)/16,gz0=(z0-32768)/16;
 if(finalTerrainOres){oreRockField=rockField;oreHabitat=habitat;}
 if(oreProfilesEnabled&&!finalTerrainOres)oreProfiles(x0,z0,raw,surfaces,rockField,habitat);
 else if(!oreProfilesEnabled&&!vanillaOres)for(int gz=gz0-1;gz<=gz0+1;++gz)for(int gx=gx0-1;gx<=gx0+1;++gx)for(int gy=-4;gy<=7;++gy){
  auto r=[&](int k){return hash01(gx,gz,seedHash(terrainSeed,4200+k+uint32_t(gy)*97));};
  if(r(0)>.68)continue;
  int type=int(r(1)*8); // coal, iron, copper, gold, lapis, redstone, diamond, emerald
  int X=gx*16+int(r(2)*16),Y=gy*16+int(r(3)*16),Z=gz*16+int(r(4)*16);
  constexpr int minY[8]={0,-54,-16,-54,-54,-54,-54,32},maxY[8]={120,112,64,30,30,0,-18,120};
  if(Y<minY[type]||Y>maxY[type]||(type==6&&r(5)>.24)||(type==7&&r(5)>.18))continue;
  int dx=0,dy=0,dz=0,steps=type>=6?3+int(r(6)*3):6+int(r(6)*7);
  for(int step=0;step<steps;++step){
   // A small connected cross, with a face-connected bounded centre walk.
   for(auto v:{std::array<int,3>{0,0,0},{1,0,0},{0,1,0},{0,0,1}}){
    int x=X+dx+v[0]-(x0-32768),y=Y+dy+v[1],z=Z+dz+v[2]-(z0-32768);
    if(x<0||x>=16||z<0||z>=16||y<minY[type]||y>maxY[type]||y<=-57)continue;
    int i=z*16+x;if(y>limit[i]||(type==7&&surfaces[i].height<110))continue;
    auto& b=raw[i*Height+y+64];if(!undergroundRock(b))continue;
    b=uint16_t((b==95?108:100)+type);
   }
   int axis=int(r(20+step*2)*3),sign=r(21+step*2)<.5?-1:1;
   int& delta=axis==0?dx:axis==1?dy:dz;delta=std::clamp(delta+sign,-5,5);
  }
 }
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){
  int i=z*16+x,top=limit[i];if(top<-50||surfaces[i].glacier>.3)continue;
  int X=x0+x-32768,Z=z0+z-32768;auto col=raw.data()+i*Height;
  auto const&s=surfaces[i];double patch=habitat[i];
  bool littoral=caveBiomesEnabled&&!stoneForestFalls&&s.stone>.002&&s.stone<.8&&s.water==field.seaLevel()&&s.height<90;
  bool lush=caveBiomesEnabled&&!littoral&&patch>-.04&&s.H>.43&&s.desert<.1;
  bool damp=patch>.12&&(s.H>.48||s.aquifer==2);
  bool drip=patch<-.14&&rockField[i]>-.3;
  if(!damp&&!drip&&!lush&&!littoral)continue;
  for(int y=-55;y<=top;){
   if(col[y+64]!=0){++y;continue;}
   int lo=y;while(y<=top&&col[y+64]==0)++y;int hi=y-1,n=hi-lo+1;
   if(n<4||y>top||!undergroundRock(col[lo+63])||!undergroundRock(col[hi+65]))continue;
   // Vertical support and clearance are known from this column. Do not
   // decorate water/ore or fill a low passage; preserve walking clearance.
   if(caveHabitatRefinement){
    bool wet=false;
    for(auto d:{std::pair{1,0},std::pair{-1,0},std::pair{0,1},std::pair{0,-1}}){
     int nx=x+d.first,nz=z+d.second;
     for(int yy:{lo,hi}){
      int b;
      if(nx>=0&&nx<16&&nz>=0&&nz<16)b=raw[(nz*16+nx)*Height+yy+64];
      else {auto ns=field.surface(x0+nx,z0+nz,false);b=field.material(ns,x0+nx,yy,z0+nz);}
      wet|=b==8||b==126||b==49||b==50;
     }
    }
    if(wet)continue;
   }
   double p=hash01(X,Z,seedHash(terrainSeed,4400+uint32_t(lo)*53));
   if(littoral){
    // Preserve the discovered sea/stone-forest cavity and its fluids. Give its
    // dry rock shelves a coherent mineral skin and hanging dripstone, instead
    // of replacing the whole cave with a uniform coastal material.
    double mineral=geologyNoise.at(X/19.+91,Z/19.-11);
    if(mineral>.08){col[lo+63]=mineral>.26?94:98;col[hi+65]=mineral>.26?94:98;}
    if(n>=10&&p<.12){int length=1+int(p*25);col[hi+65]=27;
     for(int k=0;k<length;++k)col[hi-k+64]=uint16_t(k==length-1?119:k==length-2?118:116);
    }
    continue;
   }
   if(lush&&lo>-40&&n>=7){
    double cover=smooth(-.04,.14,patch);
    if(p<.60+.35*cover){col[lo+63]=80;
     double plant=hash01(X,Z,terrainSeed^4617);
     if(plant<(caveHabitatRefinement?.006:.035))col[lo+64]=122;
     else if(plant<(caveHabitatRefinement?.018:.085))col[lo+64]=121;
     else if(plant<.24)col[lo+64]=37;
     else if(plant<.58)col[lo+64]=120;
    }
    if(p<.65)col[hi+65]=80;
    double hanging=hash01(X,Z,terrainSeed^4621);
    if(hanging<.11){int length=std::min(n-4,2+int(hash01(X,Z,terrainSeed^4623)*6));
     for(int k=0;k<length;++k)col[hi-k+64]=uint16_t(k==length-1?125:124);
    }else if(hanging<(caveHabitatRefinement?.112:.119))col[hi+64]=123;
   }else if(damp){
    if(p<std::min(.8,(patch-.08)*2.1))col[lo+63]=80;
    if(col[lo+63]==80&&p<.045)col[lo+64]=p<.032?67:68;
   }else if(drip&&n>=7&&p<.11){
    int length=1+int(hash01(X,Z,seedHash(terrainSeed,4411+uint32_t(lo)*61))*3);
    bool hanging=p<.075;int support=hanging?hi+1:lo-1;col[support+64]=27;
    for(int k=0;k<length;++k){
     int yy=hanging?hi-k:lo+k;
     int part=k==length-1?3:k==length-2?2:k==0?0:1;
     col[yy+64]=uint16_t((hanging?116:28)+part);
    }
   }
  }
 }
}
