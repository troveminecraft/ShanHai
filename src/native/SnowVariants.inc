void applySnowVariant(Surface&s,int wx,int wz){
 if(!elevationVariants)return;
 s.parentBiome=s.biome;s.altitudeBand=elevation::band(s.height);
 if(s.volcanic*s.volcanicDeposit>.12&&s.height>=seaLevelValue-3&&s.height<=seaLevelValue+1&&!s.closedBasin&&!s.lake){
  s.lavaSeaContact=s.water==seaLevelValue&&s.height<seaLevelValue;
  if(!s.lavaSeaContact)for(auto [dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){
   auto n=base(wx+dx,wz+dz);if(n.water==seaLevelValue&&n.height<seaLevelValue&&!n.closedBasin&&!n.lake)s.lavaSeaContact=true;
  }
 }
 double x=wx-32768,z=wz-32768;
 double climate=surfaceTemperature(s,wx,wz);
 double regionalCold=1-smooth(.20,.34,climate);
 double altitudeCold=smooth(335+65*climate,410+65*climate,s.height);
 double halo=0,vegetation=1;
 // The collar is measured OUTSIDE the ellipse, with 512 blocks of complete
 // snow and another 512 of gradual retreat. Owners are prepared before this.
 for(auto const&r:iceAtlas){
  double angle=std::atan2(z-r.z,x-r.x);
  double radius=1/std::hypot(std::cos(angle)/r.rx,std::sin(angle)/r.rz);
  double distance=std::hypot(x-r.x,z-r.z)-radius*1.08;
  halo=std::max(halo,1-smooth(512,1024,distance));
  vegetation=std::min(vegetation,smooth(-256,1024,distance));
 }
 // The snow collar is fully white for 512 blocks; vegetation still thins
 // continuously through that collar and into the glacier interior.
 if(C.value("glacierEcologyRevision",0)>=1)s.glacierVegetation=vegetation;
 s.glacierSnow=halo;s.snowCover=std::max({regionalCold,altitudeCold,halo});
 // Exposed hot ground cannot retain snow; cold volcanic outskirts can.
 s.snowCover*=1-smooth(.05,.35,s.volcanoHeat);
 if(s.lava>=s.height)s.snowCover=0;
 auto weights=elevation::bands(s.height);
 s.treeD*=1-.22*weights[1]-.55*weights[2];
 s.treeD*=1-.75*smooth(.6,1.,s.snowCover);
 if(s.riftWeight>.1)s.rockCap=std::max(s.rockCap,s.riftWeight);
 // Snow is a skin, so the parent biome, sand, terracotta, mycelium and
 // underwater materials remain identifiable beneath it.
}
