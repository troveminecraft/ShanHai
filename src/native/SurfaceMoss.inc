// Sparse surface patches; only existing, supported soil is replaced. This pass
// never carves terrain or loads neighbours. Underground moss is sampled before
// surface placement, so an azalea indicates an actual generated lush cavity.
KARMA_NOINLINE void surfaceMoss(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!C.value("surfaceMoss",0))return;
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){
  int i=z*16+x;auto const&s=surfaces[i];int h=s.height;
  if(h<-55||h>=Height-66||s.water>=h||s.volcanic>.01||s.volcanoHeat>.01||s.desert>.1||s.badlands||s.mushroom||s.iceHabitat>.01||s.glacier>.01||s.T<.24||s.H<.38)continue;
  int X=x0+x,Z=z0+z;if(nativeReserved(X,Z))continue;
  auto col=raw.data()+i*Height;int ground=col[h+64],above=col[h+65];
  if((ground!=3&&ground!=4&&ground!=33&&ground!=85)||above!=0)continue;
  // Do not turn a cave roof or unsupported ledge into a planting site.
  if(col[h+63]==0||col[h+63]==8||col[h+63]==43)continue;
  double p=hash01(X,Z,terrainSeed^88103);
  bool lush=false;
  if(p<.004&&s.H>.43){
   for(int y=-39;y<h-8;++y)if(col[y+64]==80&&col[y+65]==0){
    // Lush caves have moss ceilings as well as floors; damp-only floor moss
    // must not advertise a lush cave at the surface.
    for(int roof=y+7;roof<std::min(h-8,y+64);++roof){
     int b=col[roof+64];if(b==80){lush=true;break;}
     if(undergroundRock(b))break;
    }
    if(lush)break;
   }
  }
  if(lush){col[h+65]=p<.0012?122:121;continue;}
  double patch=caveHabitatNoise.at((X-32768)/19.,(Z-32768)/19.);
  double chance=.035*smooth(.38,.72,s.H)*smooth(-.02,.25,patch);
  if(p>=chance)continue;
  col[h+64]=80;
  if(hash01(X,Z,terrainSeed^88109)<.55)col[h+65]=120;
 }
}
