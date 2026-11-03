// Command-only candidate enumeration. It does not prepare drainage or touch
// generated chunks. Coordinates are ordinary world coordinates, not +32768.
int biomeSearchGrid(int biome)const{
 if(biome==60)return 1024;
 if(biome==61||biome==-5)return basinGrid();
 if(biome==-1||biome==-2)return 4608;
 if(biome==-3)return int(volcanoGrid);
 return 0;
}
std::vector<std::array<int,2>> biomeSearchAnchors(int biome,int gx,int gz){
 std::vector<std::array<int,2>> out;
 auto add=[&](double x,double z){out.push_back({roundJS(x),roundJS(z)});};
 if(biome==60){auto a=cherryGroveSite(gx,gz);if(a.valid)add(a.x-32768,a.z-32768);}
 else if(biome==61||biome==-5){auto a=basinSite(gx,gz);if(a.valid&&(biome!=61||a.mangrove)){
  add(a.x,a.z);for(int k=0;k<8;++k){double t=k*.7853981634;add(a.x+std::cos(t)*a.rx*.25,a.z+std::sin(t)*a.rz*.25);}
 }}else if(biome==-1||biome==-2){auto a=iceRegion(gx,gz);if(a.valid){
  if(biome==-2)add(a.x,a.z);
  for(double q:biome==-1?std::array{.70,.85,.95}:std::array{.0,.30,.55})
   for(int k=0;k<8;++k){double t=k*.7853981634;add(a.x+std::cos(t)*a.rx*q,a.z+std::sin(t)*a.rz*q);}
 }}else if(biome==-3){auto a=volcanoRegion(gx,gz);if(a.valid){
  add(a.cone.x,a.cone.z);for(int k=0;k<8;++k){double t=k*.7853981634;add(a.cone.x+std::cos(t)*a.inner*.5,a.cone.z+std::sin(t)*a.inner*.5);}
 }}
 return out;
}
