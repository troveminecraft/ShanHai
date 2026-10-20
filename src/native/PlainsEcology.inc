// Open meadows: continuous density patches, world-coordinate randomness.
bool plainsHabitat(Surface const&s)const{
 return (s.biome==7||s.biome==8)&&!s.kind&&!s.lake&&!s.river&&s.water<=s.height&&s.lava<=s.height&&s.giant<.01&&s.stone<.01&&s.barren<.01&&!s.rock&&s.cliffLow==-32768;
}
void plainsEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,std::vector<Structure>const&ss){
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];int h=s.height;if(!plainsHabitat(s)||h>312)continue;
  int X=x0+x,Z=z0+z;bool blocked=false;
  for(auto const&a:ss)if(std::abs(X-a.x)<48&&std::abs(Z-a.z)<48){blocked=true;break;}
  if(blocked)continue;
  auto col=raw.data()+(z*16+x)*Height;if(col[h+64]!=4||col[h+65]!=0)continue;
  double patch=clamp(.5+1.1*understoryNoise.at(X/43.,Z/43.),0,1);
  double bloom=forestNoise.at(X/67.,Z/67.);
  double pick=hash01(X,Z,terrainSeed^2101);
  // Flower species belong to broad patches, not multicolour per-block confetti.
  if(bloom>.12&&pick<.06+.26*clamp((bloom-.12)*3,0,1)){
   double species=understoryNoise.at(X/113.,Z/113.);
   col[h+65]=species<-.12?62:species>.12?63:64;
  }else if(pick<.08+.40*patch){
   if(patch>.55&&hash01(X,Z,terrainSeed^2102)<.28&&col[h+66]==0){col[h+65]=38;col[h+66]=39;}
   else col[h+65]=36;
  }
 }
}
