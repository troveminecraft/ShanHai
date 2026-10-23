// Opt-in province recipe. Old worlds keep the 0.1.5 surface selector unchanged.
bool regionalBadlands=C.value("badlandsRegions",0)==1;
struct BadlandsRegion {bool valid=false;int x=0,z=0;double rx=0,rz=0;};
Cache<uint64_t,BadlandsRegion> badlandsRegions{256};
Cache<uint64_t,Surface> preBadlandsCache{8192};
BadlandsRegion badlandsRegion(int gx,int gz){return badlandsRegions.get(key2(gx,gz),[&]{
 BadlandsRegion out;if(!regionalBadlands||!completedBiomes)return out;
 auto random=[&](int n){return hash01(gx,gz,seedHash(terrainSeed,72400+n));};
 if(random(0)>.82)return out;
 // All influence stays inside its 4096-block owner cell, including the
 // warped outline and 96-block desert collar. Negative cells use floor().
 for(int attempt=0;attempt<3;++attempt){
  int x=gx*4096+1536+int(random(1+attempt*2)*1024);
  int z=gz*4096+1536+int(random(2+attempt*2)*1024);
  auto center=compute(x,z,false);
  if(center.height<seaLevelValue+3||center.height>220||center.T<.45||center.H>.72||center.stone>.01||center.volcanic>.01)continue;
  double radius=704+random(8)*256;
  int dry=0,protectedLand=0;
  for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx){
   auto s=compute(x+int(dx*radius/2),z+int(dz*radius/2),false);
   dry+=s.height>=seaLevelValue;
   protectedLand+=s.stone>.01||s.volcanic>.01||s.T<.22;
  }
  if(dry<10||protectedLand>3)continue;
  out={true,x,z,radius,radius*(.85+.2*random(9))};break;
 }
 return out;
});}
double badlandsDistance(int x,int z){
 auto region=badlandsRegion(int(std::floor(x/4096.)),int(std::floor(z/4096.)));
 if(!region.valid)return INFINITY;
 double d=(std::hypot((x-region.x)/region.rx,(z-region.z)/region.rz)-1)*std::min(region.rx,region.rz);
 return d+48*mesaNoise.at(x/240.+91,z/240.-53)+16*mesaNoise.at(x/80.,z/80.);
}
static bool badlandsOcean(Surface const&s){
 return s.height<s.seaLevel&&s.water==s.seaLevel&&!s.lake&&
  (s.biome==0||s.biome==1||s.biome==2||s.biome==21||s.biome==22||s.ocean>0);
}
static bool badlandsProtected(Surface const&s){
 return s.mushroomZone||s.stone>.01||s.giant>.01||s.volcanic>.01||s.iceHabitat>.01||
  s.scarp>.05||s.wetland>.4||s.lava>=s.height;
}
Surface beforeBadlands(int x,int z){
 return preBadlandsCache.get(key2(x,z),[&]{return computeSurface(x,z,false);});
}
void regionalBadlandsSurface(Surface&s,int x,int z){
 double distance=badlandsDistance(x,z);
 if(distance>=96||s.mushroomZone)return;
 constexpr std::array<std::pair<int,int>,8> directions{{{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}}};
 if(badlandsOcean(s)){
  // Keep the ocean biome and water level. Only its adjacent coastal floor
  // assimilates; a distant/deep bay does not become one huge red-sand bed.
  for(int reach:{1,8,24})for(auto [dx,dz]:directions){
   int nx=x+dx*reach,nz=z+dz*reach;
   if(badlandsDistance(nx,nz)>=0)continue;
   auto land=beforeBadlands(nx,nz);
   if(!badlandsOcean(land)&&!badlandsProtected(land)&&land.height>=seaLevelValue){s.redSandCoast=true;return;}
  }
  return;
 }
 if(badlandsProtected(s))return;
 bool core=distance<0;
 // Preserve special landforms, with a desert separator instead of allowing
 // a badlands cell to touch a forest/glacier/stone-forest exclusion directly.
 if(core)for(auto [dx,dz]:directions){auto neighbour=beforeBadlands(x+dx,z+dz);
  if(!badlandsOcean(neighbour)&&badlandsProtected(neighbour)){core=false;break;}
 }
 if(!core){
  s.biome=11;s.badlandsDesert=true;s.treeK=5;s.treeD=0;s.soilSand=1;s.rockCap=0;s.rock=0;s.T=.95;s.H=.05;
  return;
 }
 bool wet=s.water>=s.height||s.lake||s.river>0||s.riverCutLow>-32768;
 applyBadlands(s,x,z,.35,!wet);
 // Low beach caps and higher coastal scarps both get a supported red-sand
 // rim; the exposed cliff below retains its terracotta bands.
 for(int reach:{1,8,24})for(auto [dx,dz]:directions)
  if(badlandsOcean(beforeBadlands(x+dx*reach,z+dz*reach))){s.redSandCoast=true;return;}
}
