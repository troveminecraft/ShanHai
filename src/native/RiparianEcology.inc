// Dry-bank vegetation, placed after trees and other accepted ecology.
void riparianEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,std::vector<Structure>const&ss){
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(!glacierPlantKept(s,x0+x,z0+z))continue;int h=s.height;
  if(s.kind||s.water>h||s.lava>h||s.giant>.01||s.stone>.01||s.barren>.01||s.cliffLow!=-32768||s.rock||field.ecologyHeight(s)>230||field.ecologyTemperature(s)<.22||coniferHabitat(s))continue;
  int X=x0+x,Z=z0+z;auto col=raw.data()+(z*16+x)*Height;
  if(col[h+64]!=4||col[h+65])continue;
  bool blocked=false;for(auto const&a:ss)if(std::abs(X-a.x)<48&&std::abs(Z-a.z)<48){blocked=true;break;}if(blocked)continue;
  double patch=clamp(.5+understoryNoise.at(X/31.,Z/31.),0,1);
  int gx=int(std::floor(X/12.)),gz=int(std::floor(Z/12.));
  int ax=gx*12+1+int(hash01(gx,gz,terrainSeed^2301)*10),az=gz*12+1+int(hash01(gx,gz,terrainSeed^2302)*10);
  bool shrub=std::abs(X-ax)<=1&&std::abs(Z-az)<=1&&hash01(gx,gz,terrainSeed^2303)<.40&&patch>.45;
  double pick=hash01(X,Z,terrainSeed^2311);if(!shrub&&pick>.06+.32*patch)continue;
  // Nearby water must be freshwater at approximately the bank height.
  // This rejects high cliffs above rivers and does not dress ocean beaches.
  int distance=99;
  for(int d:{1,2,4}){
   for(auto [dx,dz]:{std::pair{d,0},{-d,0},{0,d},{0,-d}}){
    auto b=field.surface(X+dx,Z+dz,false);
    if((b.river>0||b.lake)&&b.water>b.height&&b.lava<=b.height&&!b.lakeFrozen&&h>=b.water&&h-b.water<=2){distance=d;break;}
   }
   if(distance!=99)break;
  }
  if(distance==99)continue;
  int relief=0;for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=field.surface(X+dx,Z+dz,false);if(b.water<=b.height)relief=std::max(relief,std::abs(b.height-h));}
  if(relief>2)continue;
  // Shrubs stay back from the immediate shoreline; all foliage rests on land.
  if(shrub&&distance>=2){col[h+65]=11;if(X==ax&&Z==az&&!col[h+66])col[h+66]=11;}
  else if(patch>.55&&pick<.10&&col[h+66]==0){col[h+65]=40;col[h+66]=41;}
  else col[h+65]=s.H>.40?37:36;
 }
}
