// Stable dry patches have occasional plants; active dune crests remain bare.
// Each cactus owns only its column. A cardinal clearance halo is checked
// against deterministic surfaces, including outside the current chunk.
void desertEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,std::vector<Structure>const&ss){
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];int h=s.height,X=x0+x,Z=z0+z;
  if(s.aridType||(!s.badlands&&s.desert<=.4)||s.oasis>.2||s.water>=h||s.lava>=h||s.kind||s.cliffLow!=-32768||s.scarp>.01||h>220)continue;
  auto col=raw.data()+(z*16+x)*Height;if(col[h+65]!=0)continue;
  if(!s.badlands&&s.desertSand>.55&&s.desertCrest>.5)continue;
  bool block=false;for(auto const&a:ss)if(std::abs(X-a.x)<80&&std::abs(Z-a.z)<80){block=true;break;}if(block)continue;
  double r=hash01(X,Z,terrainSeed^19071);
  double fertile=s.badlands?1.:smooth(-.28,.10,s.desertPatch)*(1-smooth(.18,.4,s.desertPatch));
  if(r>.009*fertile)continue;
  int ground=col[h+64];bool sand=ground==5||ground==169;
  if(!sand&&ground!=33&&ground!=3&&!(s.badlands&&ground>=170&&ground<=176))continue;
  if(!sand||r>.0022*fertile){col[h+65]=88;continue;}
  // Cacti on a jittered three-block lattice cannot touch one another.
  if(X%3||Z%3)continue;
  for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){
   auto b=field.surface(X+dx,Z+dz,false);
   if(b.height>h||b.water>h||(!b.badlands&&b.desert<=.4)||b.oasis>.05){block=true;break;}
   int nx=x+dx,nz=z+dz;if(nx>=0&&nx<16&&nz>=0&&nz<16&&raw[(nz*16+nx)*Height+h+65]){block=true;break;}
  }
  if(block)continue;
  int height=1+int(hash01(X,Z,terrainSeed^19073)*3);
  for(int y=h+1;y<=h+height;y++)if(col[y+64]){block=true;break;}
  if(!block)for(int y=h+1;y<=h+height;y++)col[y+64]=18;
 }
}
