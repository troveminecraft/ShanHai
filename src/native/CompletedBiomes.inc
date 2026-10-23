// Opt-in immutable recipe: old worlds keep their original surfaces/palette.
bool completedBiomes=C.value("completedBiomes",0)==1;
Perlin islandNoise{seedHash(terrainSeed,72001)},mesaNoise{seedHash(terrainSeed,72003)};
struct MushroomIsland {bool valid=false;int x=0,z=0,r=0;};
Cache<uint64_t,MushroomIsland> islandSites{512};
MushroomIsland mushroomIsland(int gx,int gz){return islandSites.get(key2(gx,gz),[&]{
 MushroomIsland a;auto rnd=[&](int k){return hash01(gx,gz,terrainSeed^(72100+k));};
 if(!completedBiomes||!oceanEnabled||rnd(0)>.38)return a;
 a.x=gx*1536+480+int(rnd(1)*576);a.z=gz*1536+480+int(rnd(2)*576);a.r=128+int(rnd(3)*49);
 // Evaluate the unmodified terrain, never surface(), to avoid recursive
 // island/shore searches. Reject frozen seas, lakes and special landforms.
 auto wet=[&](int x,int z){auto s=compute(x,z,false);return s.height<25&&s.water==31&&!s.lake&&s.T>.26&&s.stone==0&&s.volcanic==0;};
 if(!wet(a.x,a.z))return a;
 int reach=a.r+112;
 for(int z=-reach;z<=reach;z+=48)for(int x=-reach;x<=reach;x+=48)
  if(!wet(a.x+x,a.z+z))return a;
 a.valid=true;return a;
});}
bool mushroomSurface(Surface&s,int x,int z){
 if(!completedBiomes)return false;
 int gx=int(std::floor(x/1536.)),gz=int(std::floor(z/1536.));
 // Sites stay >=480 blocks from their cell edge; the entire influence is
 // <=288 blocks. A single cell lookup is sufficient, also at negative X/Z.
 auto a=mushroomIsland(gx,gz);if(!a.valid)return false;
 double dx=x-a.x,dz=z-a.z,d=std::hypot(dx,dz);
 if(d>=a.r+96)return false;
 double shore=a.r+10*islandNoise.at(x/65.,z/65.);
 double inland=1-smooth(shore-48,shore,d);
 // The outer 64+ blocks are ALWAYS sea. This explicit collar, applied last
 // to biome/height, proves no mushroom column can touch a non-ocean biome,
 // even if coarse candidate sampling missed a small natural outcrop.
 int bed=std::min(s.height,23);
 int h=int(std::floor(bed+(36+12*islandNoise.at(x/110.,z/110.)-bed)*inland));
 s=Surface{};s.height=h;s.preRiverHeight=h;s.T=.65;s.H=.9;s.rockCap=0;
 s.continuous=s.e=(h-31)/123.;s.mushroomZone=true;
 if(h>=31){s.biome=25;s.mushroom=true;s.surf=9;}
 else{s.water=31;s.kind=3;s.surf=6;s.ocean=1;s.biome=1;}
 return true;
}

// Vanilla mesa semantics: a world-seeded repeating clay band sequence,
// smoothly offset per column; red sand on low tops, bare terracotta cliffs,
// wooded plateau caps, and a separate eroded (Bryce) pillar variant.
// Height/noise are adapted to this generator's sea level 31. This does not
// claim bit-identical Mojang Xoroshiro noise or vanilla biome placement.
std::array<uint8_t,192> mesaBands{};
bool mesaBandsReady=false;
void prepareMesaBands(){
 if(mesaBandsReady)return;mesaBandsReady=true;mesaBands.fill(170);
 uint32_t state=seedHash(terrainSeed,72201);
 auto next=[&](int bound){state=1664525u*state+1013904223u;return int((state>>8)%uint32_t(bound));};
 for(int y=next(5);y<192;y+=1+next(5))mesaBands[y]=171;
 for(int color:{172,173,174})for(int n=6+next(10);n>0;--n){int start=next(192),width=1+next(3);for(int k=0;k<width&&start+k<192;++k)mesaBands[start+k]=uint8_t(color);}
 for(int y=next(12);y<192;y+=6+next(16)){mesaBands[y]=175;if(y>0&&next(2))mesaBands[y-1]=176;if(y<191&&next(2))mesaBands[y+1]=176;}
}
void badlandsSurface(Surface&s,int x,int z){
 if(!completedBiomes||s.biome!=15||s.water>=s.height||s.lake||s.river||s.stone>0||s.volcanic>0||s.iceHabitat>0||s.scarp>0||s.aridType)return;
 s.badlands=1;s.T=.95;s.H=.05;s.treeD=0;s.giant=0;s.rockCap=0;s.rock=0;s.soilSand=0;s.surf=10;
 double variant=mesaNoise.at(x/850.,z/850.);
 if(variant>.12){
  s.badlands=2;s.biome=26;
  double pillar=smooth(.12,.3,variant)*smooth(.05,.40,mesaNoise.at(x/45.+19,z/45.-7));
  double roof=18+18*clamp(.5+mesaNoise.at(x/120.-31,z/120.+23),0,1);
  s.height=std::min(240,s.height+int(std::floor(roof*pillar)));
 }else if(s.height>=76){
  s.badlands=3;s.biome=27;
  if(variant<-.12){s.badlands=4;s.biome=28;s.treeK=1;s.treeD=.0014;}
 }
 s.continuous=s.e=(s.height-31)/123.;
}
int badlandsMaterial(Surface const&s,int x,int y,int z){
 if(!s.badlands||y<25)return -1;
 if(s.badlands==4&&y>=s.height-3){
  if(y<s.height)return 3;
  return mesaNoise.at(x/28.,z/28.)>0?4:33;
 }
 // Sand needs a supporting clay/sandstone bed. Tall exposed mesa tops and
 // walls keep their bands instead of painting every cliff with red sand.
 if(s.height<67&&y>=s.height-2)return y==s.height?169:177;
 prepareMesaBands();int offset=roundJS(3*mesaNoise.at(x/512.+7,z/512.-9));
 int i=(y+offset)%192;if(i<0)i+=192;return mesaBands[i];
}
