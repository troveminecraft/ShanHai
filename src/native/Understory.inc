// Forest-floor pass. All anchors and probabilities use world coordinates.
// Runs after trees/rocks/logs: only air above real soil can be occupied.
void understory(int x0,int z0,std::vector<uint16_t>& raw,std::array<Surface,256>const& surfaces,
                std::vector<Structure>const& structures,std::vector<std::shared_ptr<Giant>>const& trees) {
 bool any=false;for(auto const&s:surfaces)any|=s.giant>.02;if(!any)return;
 auto soil=[](int id){return id==3||id==4||id==32||id==33;};
 auto patch=[&](int x,int z){return clamp(.5+.7*understoryNoise.at(x/39.,z/39.)+.23*understoryNoise.at(x/13.,z/13.),0,1);};
 auto excluded=[&](int x,int z){
  for(auto const&s:structures)if(std::abs(x-s.x)<48&&std::abs(z-s.z)<48)return true;
  for(auto const&t:trees)if(std::hypot(x-t->x,z-t->z)<t->radius+1)return true;
  return std::hypot(x-32768,z-32768)<12;
 };
 auto eligible=[&](Surface const&s){return s.giant>.02&&s.stone<.05&&!s.kind&&s.height>35&&s.height<Height-67&&s.cliffLow==-32768;};
 // A seven-block anchor grid is jittered across each full cell. The patch
 // field groups the shrubs, while sparse spaces remain walkable.
 for(int gz=int(std::floor((z0-3)/7.));gz<=int(std::floor((z0+18)/7.));gz++)for(int gx=int(std::floor((x0-3)/7.));gx<=int(std::floor((x0+18)/7.));gx++){
  auto rnd=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
  int x=gx*7+int(rnd(811)*7),z=gz*7+int(rnd(812)*7);auto s=field.surface(x,z,false);
  if(!eligible(s)||excluded(x,z)||rnd(813)>s.giant*(.22+.64*patch(x,z)))continue;
  double radius=1.35+rnd(814)*.85,phase=rnd(815)*6.283185307179586;int n=int(std::ceil(radius));
  int lo=s.height,hi=s.height;for(auto [dx,dz]:{std::pair{-n,0},{n,0},{0,-n},{0,n}}){int h=field.surface(x+dx,z+dz,false).height;lo=std::min(lo,h);hi=std::max(hi,h);}
  if(hi-lo>3)continue;
  for(int dz=-n;dz<=n;dz++)for(int dx=-n;dx<=n;dx++){
   double edge=radius*(1+.12*std::sin(std::atan2(double(dz),double(dx))*3+phase));if(dx*dx+dz*dz>edge*edge)continue;
   int X=x+dx-x0,Z=z+dz-z0;if(X<0||X>=16||Z<0||Z>=16)continue;
   auto const&local=surfaces[Z*16+X];if(!eligible(local)||excluded(x+dx,z+dz))continue;
   int h=local.height;auto col=raw.data()+(Z*16+X)*Height;
   if(!soil(col[h+64])||col[h+65]!=0||col[h+66]!=0)continue;
   // The broad bottom touches soil in every column. The one-block central
   // stem is hidden inside leaves; edge columns may be only one block tall.
   col[h+65]=(dx==0&&dz==0)?12:13;
   if(dx*dx+dz*dz<=radius*radius*.65)col[h+66]=13;
  }
 }
 for(int z=0;z<16;z++)for(int x=0;x<16;x++){
  auto const&s=surfaces[z*16+x];int X=x0+x,Z=z0+z;if(!eligible(s)||excluded(X,Z))continue;
  auto col=raw.data()+(z*16+x)*Height;int h=s.height;
  if(!soil(col[h+64])||col[h+65]!=0)continue;
  double cover=patch(X,Z);if(hash01(X,Z,terrainSeed^821)>s.giant*(.06+.48*cover))continue;
  double kind=hash01(X,Z,terrainSeed^823);
  if(kind<.31&&col[h+66]==0){int lower=hash01(X,Z,terrainSeed^827)<.55?38:40;col[h+65]=uint8_t(lower);col[h+66]=uint8_t(lower+1);}
  else col[h+65]=kind<.66?37:36;
 }
}
