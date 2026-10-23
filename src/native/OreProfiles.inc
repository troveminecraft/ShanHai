// Vanilla-style height batches. Distribution shapes and air-exposure semantics
// follow published Minecraft rules; these counts are explicit project tuning,
// not a claim that the closed Bedrock generator's RNG has been reproduced.
bool oreProfilesEnabled=false,oreBoundaryAir=false,finalTerrainOres=false;
std::array<double,256> oreRockField{},oreHabitat{};
struct OreBatch {int type,attempts,low,high,size;bool triangle;double airDiscard;};
static constexpr OreBatch OreBatches[]={
 {0,20,128,320,17,false,0},{0,30,0,192,17,true,.5},
 {1,90,80,384,9,true,0},{1,10,-24,56,9,true,0},{1,10,-64,72,4,false,0},
 {2,16,-16,112,16,true,0},
 {3,4,-64,32,9,true,.5},{3,2,-64,-48,9,false,.5},
 {4,2,-32,32,7,true,0},{4,4,-64,64,7,false,1},
 {5,4,-64,16,8,false,0},{5,8,-96,-32,8,true,0},
 {6,7,-144,16,4,true,.5},{6,4,-64,-4,8,false,1},
 {7,100,-16,480,3,true,0}
};
KARMA_NOINLINE void oreProfiles(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,
                                    std::array<double,256>const&rockField,std::array<double,256>const&habitat){
 int gx0=(x0-32768)/16,gz0=(z0-32768)/16,top=-57;
 for(auto const&s:surfaces)if(s.volcanic<=.01&&!s.vent&&s.scarp<=.01)top=std::max(top,s.height-5);
 std::array<Surface,324> halo{};std::array<bool,324> known{};
 auto isAir=[&](int x,int y,int z){
  if(y<-64||y>Height-65)return false;
  // Outside the supported map BDS builds void. Do not invent a solid halo
  // that permits buried-only ores to become exposed on the actual map edge.
  if(oreBoundaryAir&&(x0+x-32768<terrain_bounds::MinBlock||x0+x-32768>terrain_bounds::MaxBlock||
                      z0+z-32768<terrain_bounds::MinBlock||z0+z-32768>terrain_bounds::MaxBlock))return true;
  if(x>=0&&x<16&&z>=0&&z<16)return raw[(z*16+x)*Height+y+64]==0;
  int i=(z+1)*18+x+1;
  if(!known[i]){halo[i]=field.surface(x0+x,z0+z,false);known[i]=true;}
  return finalTerrainOres?finalOreAir(halo[i],x0+x,y,z0+z):field.material(halo[i],x0+x,y,z0+z)==0;
 };
 for(int gz=gz0-1;gz<=gz0+1;++gz)for(int gx=gx0-1;gx<=gx0+1;++gx){
  int batch=0;
  for(auto const&b:OreBatches){
   int salt=5000+batch++*4096;
   for(int attempt=0;attempt<(caveHabitatRefinement&&b.type==7?24:b.attempts);++attempt){
    auto r=[&](int k){return hash01(gx,gz,seedHash(terrainSeed,salt+attempt*31+k));};
    // Project rarity correction, independent of the vein walk and chunk order.
    // These are not advertised as closed-source BDS batch probabilities.
    if(finalTerrainOres){double keep=b.type==3?.65:b.type==4?.60:b.type==6?.45:b.type==7?.50:1.;if(r(9)>=keep)continue;}
    double t=b.triangle?(r(0)+r(1))*.5:r(0);int Y=int(std::floor(b.low+(b.high-b.low)*t));
    if(Y<-62||Y>top+6)continue;
    if(caveHabitatRefinement&&b.type==7&&r(8)>smooth(-16.,80.,Y))continue;
    int X=gx*16+int(r(2)*16)-(x0-32768),Z=gz*16+int(r(3)*16)-(z0-32768);
    if(X<-6||X>21||Z<-6||Z>21)continue;
    int dx=0,dy=0,dz=0;
    for(int step=0;step<(b.type==2?b.size*2:(caveHabitatRefinement&&b.type==7?1:b.size));++step){
     int x=X+dx,y=Y+dy,z=Z+dz;
     if(x>=0&&x<16&&z>=0&&z<16&&y>=(finalTerrainOres?-59:-56)&&y<Height-64){
      int i=z*16+x;auto const&s=surfaces[i];auto&block=raw[i*Height+y+64];
      bool copperExtra=b.type==2&&step>=b.size;
      bool permitted=y<=s.height-5&&s.volcanic<=.01&&!s.vent&&s.scarp<=.01;
      if(b.type==7)permitted=permitted&&(finalTerrainOres?s.biome==14:(s.height>=110&&s.mount>(caveHabitatRefinement?.35:.15)));
      if(copperExtra)permitted=permitted&&habitat[i]<-.14&&rockField[i]>-.3;
      if(permitted&&undergroundRock(block)){
       bool reject=false;
       double discard=finalTerrainOres&&b.type==4&&(y<-32||y>32)?1.:b.airDiscard;
       if(discard>0&&hash01(x0+x,z0+z,seedHash(terrainSeed,6001+uint32_t(y)*53+b.type))<discard)
        for(auto d:{std::array<int,3>{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})
         if(isAir(x+d[0],y+d[1],z+d[2])){reject=true;break;}
       if(!reject)block=uint16_t((block==95?108:100)+b.type);
      }
     }
     // Bounded world-anchor walk: every crossing chunk reconstructs the same
     // candidates, with no neighbouring chunk load or ore-order dependence.
     int axis=int(hash01(gx,gz,seedHash(terrainSeed,salt+attempt*257+step*2+20000))*3);
     int sign=hash01(gx,gz,seedHash(terrainSeed,salt+attempt*257+step*2+20001))<.5?-1:1;
     int&d=axis==0?dx:axis==1?dy:dz;d=std::clamp(d+sign,-6,6);
    }
   }
  }
 }
}
