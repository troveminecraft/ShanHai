// World-coordinate column features: bounded cost, no deferred placement or seams.
Perlin seaPlants{seedHash(terrainSeed,1421)};
void oceanFeatures(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!C.contains("ocean")||!C.at("ocean").value("enabled",false))return;
 double kelpChance=num(C.at("ocean"),"kelpChance"),grassChance=num(C.at("ocean"),"grassChance");
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(s.river>0)continue;int depth=s.water-s.height;
  if(s.ocean<=0||s.lake||s.barren>=1||s.water!=31||depth<3)continue;
  auto col=raw.data()+(z*16+x)*Height;int ground=s.height+64;
  if(col[ground+1]!=8||col[ground+2]!=8)continue;
  int X=x0+x,Z=z0+z;double px=X-32768,pz=Z-32768;
  double patch=seaPlants.at(px/48.,pz/48.),pick=hash01(X,Z,terrainSeed^1423);
  double cover=smooth(.1,.45,s.ocean)*std::pow(1-s.barren,3)*smooth(.20,.30,s.T);
  // Broad, broken living reefs in warm shallow water; never near the surface.
  double reef=seaPlants.at(px/72.+71,pz/72.-31);
  double reefWeight=cover*smooth(.58,.70,s.T)*smooth(3.,8.,depth)*(1-smooth(22.,32.,depth))*smooth(-.03,.20,reef);
  if(reefWeight>0){
   double shape=seaPlants.at(px/7.,pz/7.);
   int n=std::clamp(int((1.3+reef*4+shape*3)*reefWeight+pick*.5),0,5);
   n=std::min(n,depth-2);
   int color=51+int(clamp(.5+.8*seaPlants.at(px/19.+9,pz/19.-7)+.25*seaPlants.at(px/6.,pz/6.),0,.999999)*5);
   for(int y=1;y<=n&&col[ground+y]==8;y++)col[ground+y]=uint8_t(color);
   if(n>0){if(pick<.30*reefWeight&&col[ground+n+1]==8&&n+1<depth)col[ground+n+1]=uint8_t(color+5);continue;}
  }
  double kelp=cover*(1-smooth(.60,.74,s.T))*smooth(7.,12.,depth)*(1-smooth(55.,70.,depth))*smooth(-.2,.15,patch);
  if(pick<kelpChance*(.6+patch)*kelp){
   int n=std::min(depth-2,3+int(hash01(X,Z,terrainSeed^1431)*14));
   bool clear=true;for(int y=1;y<=n;y++)if(col[ground+y]!=8)clear=false;
   if(clear)for(int y=1;y<=n;y++)col[ground+y]=50;
  }else if(pick<grassChance*(.65+patch)*cover*(1-smooth(35.,48.,depth))&&col[ground]==5)col[ground+1]=49;
 }
}
