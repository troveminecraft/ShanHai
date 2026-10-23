// Mountain ground cover, excluding accepted conifer stands and special terrain.
bool mountainHabitat(Surface const&s)const{
 return s.biome==14&&!coniferHabitat(s)&&!s.kind&&!s.lake&&!s.river&&s.water<=s.height&&s.lava<=s.height&&s.giant<.01&&s.stone<.01&&s.barren<.01&&s.cliffLow==-32768&&s.height<250;
}
void mountainEcology(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces,std::vector<Structure>const&ss){
 auto clearStructure=[&](int x,int z){for(auto const&a:ss)if(std::abs(x-a.x)<48&&std::abs(z-a.z)<48)return false;return true;};
 // Full shrub footprints are validated in world space before chunk clipping.
 for(int gz=int(std::floor((z0-3)/24.));gz<=int(std::floor((z0+18)/24.));gz++)for(int gx=int(std::floor((x0-3)/24.));gx<=int(std::floor((x0+18)/24.));gx++){
  int X=gx*24+3+int(hash01(gx,gz,terrainSeed^2201)*18),Z=gz*24+3+int(hash01(gx,gz,terrainSeed^2202)*18);
  auto s=field.surface(X,Z,false);if(!mountainHabitat(s)||s.height>190||!clearStructure(X,Z)||hash01(gx,gz,terrainSeed^2203)>.22)continue;
  int lo=Height-64,hi=-64;bool valid=true;std::vector<std::array<int,3>> cells;
  for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
   auto b=field.surface(X+dx,Z+dz,false);if(!mountainHabitat(b)||field.material(b,X+dx,b.height,Z+dz)!=4)valid=false;
   lo=std::min(lo,b.height);hi=std::max(hi,b.height);cells.push_back({X+dx,Z+dz,b.height});
  }
  if(!valid||hi-lo>2)continue;
  for(auto const&c:cells){int x=c[0]-x0,z=c[1]-z0;if(x<0||x>=16||z<0||z>=16)continue;auto col=raw.data()+(z*16+x)*Height;
   if(col[c[2]+64]!=4||col[c[2]+65])continue;
   col[c[2]+65]=11;if(c[0]==X&&c[1]==Z&&!col[c[2]+66])col[c[2]+66]=11;
  }
 }
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];if(!mountainHabitat(s))continue;
  int X=x0+x,Z=z0+z,h=s.height;if(!clearStructure(X,Z))continue;auto col=raw.data()+(z*16+x)*Height;if(col[h+65])continue;
  double patch=clamp(.5+understoryNoise.at(X/38.,Z/38.),0,1),pick=hash01(X,Z,terrainSeed^2211);
  double density=(.03+.23*patch)*clamp((235.-h)/100.,.10,1.);
  if(pick>std::max(density,.018))continue;
  int relief=0;for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}})relief=std::max(relief,std::abs(field.surface(X+dx,Z+dz,false).height-h));
  if(relief>2)continue;
  if(col[h+64]==4&&pick<density)col[h+65]=s.H>.43&&patch>.55?37:36;
  else if(col[h+64]==2&&s.rock&&patch>.5&&pick<.018)col[h+65]=20;
 }
}
