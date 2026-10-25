 #include "VanillaOreBatches.inc"
bool vanillaOres=false;
// Surface tag adapters use the actual biomeNames mapping in this project.
// We have extreme_hills and mesa, but no mountains-only surface tag.
bool vanillaOreCondition(int condition,Surface const&s,int i)const{
 bool drip=caveBiomesEnabled&&s.glacier<=.3&&oreHabitat[i]<-.14&&oreRockField[i]>-.3;
 // The same local habitat predicate controls the existing dripstone ecology.
 switch(condition){case 1:return s.biome==14||(field.highMountainUndergroundEnabled()&&s.biome==5&&high_mountain_underground::weight(s)>0);case 2:return drip;case 3:return !drip;case 4:return (s.biome==15||(s.biome>=26&&s.biome<=28));case 5:return false;default:return true;}
}
KARMA_NOINLINE void vanillaOreProfiles(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 int gx0=(x0-32768)/16,gz0=(z0-32768)/16,top=-64;
 for(int i=0;i<256;++i){auto col=raw.data()+i*Height;for(int y=std::min(Height-65,surfaces[i].height);y>top;--y)if(col[y+64]==2||col[y+64]==95){top=y;break;}}
 if(top<-60)return;
 std::array<uint8_t,256> eligibility{};uint8_t anyEligibility=0;
 for(int i=0;i<256;++i){auto const&s=surfaces[i];bool drip=vanillaOreCondition(2,s,i);
  eligibility[i]=uint8_t(1|(vanillaOreCondition(1,s,i)?2:0)|(drip?4:8)|((s.biome==15||(s.biome>=26&&s.biome<=28))?16:0));anyEligibility|=eligibility[i];
 }
 std::array<Surface,324> halo{};std::array<bool,324> known{};
 auto isAir=[&](int x,int y,int z){
  if(y<-64||y>Height-65)return false;
  if(x0+x-32768<terrain_bounds::MinBlock||x0+x-32768>terrain_bounds::MaxBlock||z0+z-32768<terrain_bounds::MinBlock||z0+z-32768>terrain_bounds::MaxBlock)return true;
  if(x>=0&&x<16&&z>=0&&z<16)return raw[(z*16+x)*Height+y+64]==0;
  int i=(z+1)*18+x+1;if(!known[i]){halo[i]=field.surface(x0+x,z0+z,false);known[i]=true;}
  return finalOreAir(halo[i],x0+x,y,z0+z);
 };
 // Only neighbouring anchors whose finite cluster intersects this chunk are
 // replayed. There are no neighbour loads or order-dependent cross writes.
 for(int gz=gz0-1;gz<=gz0+1;++gz)for(int gx=gx0-1;gx<=gx0+1;++gx){int batch=0;
  for(auto const&b:VanillaOreBatches){uint32_t salt=76000+batch++*8192;
   if(!(anyEligibility&(1<<b.condition)))continue;
   // scatter_chance gates this entire batch, before its iterations.
   if(hash01(gx,gz,seedHash(terrainSeed,salt))>=b.chance)continue;
   for(int attempt=0;attempt<b.attempts;++attempt){
    auto r=[&](int k){return hash01(gx,gz,seedHash(terrainSeed,salt+1+attempt*257+k));};
    // Bedrock scatter extents are inclusive. Do not discard a candidate
    // outside world height and redistribute it into an in-range layer.
    double t=b.triangle?(r(0)+r(1))*.5:r(0);int Y=b.low+int(std::floor((b.high-b.low+1)*t));
    if(Y<-64-5||Y>top+5)continue;
    int X=gx*16+int(r(2)*17)-(x0-32768),Z=gz*16+int(r(3)*17)-(z0-32768);
    if(X<-5||X>20||Z<-5||Z>20)continue;
    OreCluster cluster(X,Y,Z,b.size,r);
    int xl=std::max(0,cluster.xlo),xh=std::min(15,cluster.xhi),zl=std::max(0,cluster.zlo),zh=std::min(15,cluster.zhi),yl=std::max(-60,cluster.ylo),yh=std::min(top,cluster.yhi);
    if(xl>xh||zl>zh||yl>yh)continue;
    for(int z=zl;z<=zh;++z)for(int x=xl;x<=xh;++x){int i=z*16+x;if(!(eligibility[i]&(1<<b.condition)))continue;
     auto col=raw.data()+i*Height;
     for(int y=yl;y<=yh;++y){auto&block=col[y+64];
      // Exact shipped replace_rules: other custom geological layers are not
      // silently treated as stone. Air, ice, fluids and bedrock cannot host ore.
      if((block!=2&&block!=95)||!cluster.contains(x,y,z))continue;
      bool reject=false;
      if(b.airDiscard>0&&hash01(x0+x,z0+z,seedHash(terrainSeed,salt+attempt*257+uint32_t(y)*53))<b.airDiscard)
       for(auto d:{std::array<int,3>{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})if(isAir(x+d[0],y+d[1],z+d[2])){reject=true;break;}
      if(!reject)block=uint16_t((block==95?108:100)+b.type);
     }
    }
   }
  }
 }
}
