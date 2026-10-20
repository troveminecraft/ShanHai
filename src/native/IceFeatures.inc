// Bounded native pass, no 3-D density noise or neighbour chunk loads.
KARMA_NOINLINE void iceFeatures(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!C.contains("snowUpdate")||!C.at("snowUpdate").value("enabled",false))return;
 bool any=false;for(auto const&s:surfaces)any|=s.glacier>.12;if(!any)return;
 // Snapshot the empty cave cells before decorating. Neighbour decisions must
 // not depend on whether an adjacent column already received an icicle.
 std::array<std::array<uint64_t,6>,256> air{};
 for(int i=0;i<256;++i)for(int y=48;y<193;++y)if(raw[i*Height+y]==0)air[i][y/64]|=1ULL<<(y%64);
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;auto const&s=surfaces[i];if(s.glacier<.12)continue;
  auto col=raw.data()+i*Height;int X=x0+x-32768,Z=z0+z-32768;
  if(refinedGlaciers&&!glacierEcologyEnabled)for(int y=std::max(-56,s.height-2);y<=std::min(319,s.height+2);++y)if(col[y+64]==7||col[y+64]==23)col[y+64]=143;
  if(s.water>s.height&&s.glacier>.35&&s.water<=s.height+80)col[s.water+64]=9;
  if(s.kind||s.river||s.lake||(finalTerrainOres?(s.iceGround!=143&&(gradualIceEcotone||s.superGlacier<.35)):s.glacier<.30))continue;
  int lo=std::max(-56,s.height-s.iceThickness+1);
  for(int y=lo;y<=s.height;++y){int b=col[y+64];if(b==0||b==8||b==43||b==1||(b>=100&&b<=115))continue;
   col[y+64]=uint16_t(!refinedGlaciers&&y==s.height&&s.iceSnow?7:y<s.height-6&&s.superGlacier>.4?144:143);
  }
  int cap=std::min(128,s.height-8);
  // Four halo columns are evaluated once. Do not repeatedly sample terrain
  // climate for each wall block at the chunk edge.
  std::array<Surface,4> haloSurface{};
  if(x==0)haloSurface[0]=field.surface(x0-1,z0+z,false);
  if(x==15)haloSurface[1]=field.surface(x0+16,z0+z,false);
  if(z==0)haloSurface[2]=field.surface(x0+x,z0-1,false);
  if(z==15)haloSurface[3]=field.surface(x0+x,z0+16,false);
  for(int y=-16;y<=cap;++y){if(!undergroundRock(col[y+64]))continue;bool exposed=false;int d=0;
   for(auto [dx,dz]:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}}){int nx=x+dx,nz=z+dz;
    if(nx>=0&&nx<16&&nz>=0&&nz<16)exposed|=(air[nz*16+nx][(y+64)/64]>>((y+64)%64))&1ULL;
    else exposed|=field.material(haloSurface[d],x0+nx,y,z0+nz)==0;
    ++d;
   }
   if(exposed)col[y+64]=143;
  }
  for(int y=-16;y<=cap;){if(col[y+64]!=0){++y;continue;}
   int floor=y;while(y<=cap&&col[y+64]==0)++y;int roof=y-1,n=roof-floor+1;
   auto rockOrIce=[](int b){return undergroundRock(b)||b==143||b==144||b==7;};
   if(n<6||y>cap||!rockOrIce(col[floor+63])||!rockOrIce(col[roof+65]))continue;
   for(int k=1;k<=2;++k){if(undergroundRock(col[floor+64-k]))col[floor+64-k]=143;if(undergroundRock(col[roof+64+k]))col[roof+64+k]=144;}
   if(!refinedGlaciers&&hash01(X,Z,terrainSeed^51021)<.35)col[floor+63]=7;
   // Never consume the floor's three-block walking clearance.
   if(hash01(X,Z,terrainSeed^51023)<.035){int length=std::min(n-3,2+int(hash01(X,Z,terrainSeed^51025)*5));for(int k=0;k<length;++k)col[roof-k+64]=143;}
  }
 }
 if(glacierEcologyEnabled)return;
 if(refinedGlaciers){refinedIceSpires(x0,z0,raw,surfaces);if(smallIceEnabled)smallIceSpikes(x0,z0,raw,surfaces);return;}
 auto rifts=glacierBody(x0,z0,raw,surfaces);
 std::array<int,256> iceTops{};for(int i=0;i<256;++i)iceTops[i]=std::max(surfaces[i].height,rifts[i].bodyTop);
 // A swept cone lies in the convex hull of its base ellipse and tip.
 // max(base radius, tip displacement) <=48; adding them made us evaluate
 // many distant anchors whose geometry could never reach this chunk.
 constexpr int grid=128,halo=50;
 for(int gz=int(std::floor((z0-32768-halo)/double(grid)));gz<=int(std::floor((z0-32768+15+halo)/double(grid)));++gz)
 for(int gx=int(std::floor((x0-32768-halo)/double(grid)));gx<=int(std::floor((x0-32768+15+halo)/double(grid)));++gx){
  auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
  int X=int(std::floor((gx+.15+.70*r(51031))*grid)),Z=int(std::floor((gz+.15+.70*r(51033))*grid));
  if(X+49<x0-32768||X-49>x0+15-32768||Z+49<z0-32768||Z-49>z0+15-32768)continue;
  auto s=field.surface(X+32768,Z+32768,false);bool large=s.superGlacier>.65;
  if(s.glacier<.60||s.river||s.lake||s.water>=s.height||s.scarp>.01||s.cliffLow!=-32768||r(51035)>(large?.78:.38))continue;
  double radius=large?22+16*r(51037):3+4*r(51037),rx=radius,rz=radius*(.72+.28*r(51039));
  int height=std::min(large?80+int(r(51041)*65):12+int(r(51041)*28),313-s.height);if(height<(large?55:10))continue;
  bool valid=true;
  for(auto [dx,dz]:{std::pair{-int(rx),0},std::pair{int(rx),0},std::pair{0,-int(rz)},std::pair{0,int(rz)}}){auto t=field.surface(X+dx+32768,Z+dz+32768,false);if(t.glacier<.4||t.river||t.lake||t.water>=t.height||t.volcanic>.01||std::abs(t.height-s.height)>18)valid=false;}
  if(!valid)continue;double phase=r(51043)*6.283185307179586;
  double lean=large?18+30*r(51045):3+5*r(51045),lx=std::cos(phase)*lean,lz=std::sin(phase)*lean;
  // A swept cone: the top moves sideways while its cross-section narrows.
  // Solve a quadratic interval once per column, rather than adding voxel noise.
  double A=lx*lx/(rx*rx)+lz*lz/(rz*rz)-1;
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;auto const&t=surfaces[i];if(t.glacier<.35||t.river||t.lake||t.water>=t.height)continue;
   double dx=x0+x-32768-X,dz=z0+z-32768-Z;
   if(dx<std::min(-rx,lx)-1||dx>std::max(rx,lx)+1||dz<std::min(-rz,lz)-1||dz>std::max(rz,lz)+1)continue;
   double B=2-2*(dx*lx/(rx*rx)+dz*lz/(rz*rz)),D=dx*dx/(rx*rx)+dz*dz/(rz*rz)-1;
   double discriminant=B*B-4*A*D;if(discriminant<0)continue;
   std::array<double,4> cuts{0,1,0,0};int n=2;
   if(std::abs(A)<1e-9){if(std::abs(B)>1e-9)cuts[n++]=clamp(-D/B,0.,1.);}
   else {cuts[n++]=clamp((-B-std::sqrt(discriminant))/(2*A),0.,1.);cuts[n++]=clamp((-B+std::sqrt(discriminant))/(2*A),0.,1.);}
   std::sort(cuts.begin(),cuts.begin()+n);auto col=raw.data()+i*Height;
   for(int k=1;k<n;++k){double mid=(cuts[k-1]+cuts[k])*.5;if((A*mid+B)*mid+D>1e-8)continue;
    int bottom=std::max(t.height+1,s.height+int(std::ceil(height*cuts[k-1]))),top=s.height+int(std::floor(height*cuts[k]));
    if(top>=bottom)iceTops[i]=std::max(iceTops[i],top);
    for(int y=bottom;y<=top;++y){double u=double(y-s.height)/height,qx=(dx-lx*u)/rx,qz=(dz-lz*u)/rz;
     col[y+64]=uint16_t(large&&qx*qx+qz*qz<.32*(1-u)*(1-u)&&u<.78?144:143);
    }
   }
  }
 }
 fractureIceTips(raw,rifts,iceTops);
}
