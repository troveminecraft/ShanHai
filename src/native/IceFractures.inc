// Original column-based glacier body. References: BYG/BWG extend terrain at
// surface generation and use ice bands; no upstream implementation is copied.
// Cells are world-seeded, not chunk-seeded. No 3-D noise or neighbour loads.
bool fracturedGlaciers=false,refinedGlaciers=false,slantedGlaciers=false,smallIceEnabled=false,continuousGlacierBodies=false;
Perlin fractureNoise{seedHash(terrainSeed,51201)};
struct IcePlatePoint {double x,z,lift,tiltX,tiltZ;};
struct IceRiftColumn {int bodyTop=-32768;double edge=INFINITY,strength=0,band=0;};
Cache<uint64_t,IcePlatePoint> icePlatePoints{512};
IcePlatePoint icePlatePoint(int gx,int gz){return icePlatePoints.get(key2(gx,gz),[&]{
 auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 return IcePlatePoint{(gx+.22+.56*r(51203))*64,(gz+.22+.56*r(51205))*64,
     18+16*r(51207),(r(51209)-.5)*.13,(r(51211)-.5)*.13};
});}
KARMA_NOINLINE std::array<IceRiftColumn,256> glacierBody(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 std::array<IceRiftColumn,256> out{};if(!fracturedGlaciers)return out;
 bool any=false;for(auto const&s:surfaces)any|=s.superGlacier>(continuousGlacierBodies?.02:.35);if(!any)return out;
 // Fetch each cell once per chunk. Per-column nearest-cell searches then
 // use this small array instead of thousands of LRU/hash-table operations.
 int minX=int(std::floor((x0-32768-10)/64.))-1,minZ=int(std::floor((z0-32768-10)/64.))-1;
 int maxX=int(std::floor((x0-32768+25)/64.))+1,maxZ=int(std::floor((z0-32768+25)/64.))+1;
 int width=maxX-minX+1;std::array<IcePlatePoint,64> points{};
 if(!snowRifts)for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x)points[(z-minZ)*width+x-minX]=icePlatePoint(x,z);
 auto snowSites=snowRifts?riftNeighborhood(x0-32768,z0-32768):std::array<SnowRift,9>{};
 for(int i=0;i<256;++i){auto const&s=surfaces[i];
  // Keep all pre-existing water routes, wet ground and protected landforms.
  if((gradualIceEcotone&&s.iceGround!=143)||s.superGlacier<=(continuousGlacierBodies?.02:.35)||s.glacier<(continuousGlacierBodies?.02:.45)||s.kind||s.river||s.lake||s.fluvialType||s.wetland>.01||s.water>=s.height||s.scarp>.01||s.cliffLow!=-32768)continue;
  double X=x0+i%16-32768,Z=z0+i/16-32768;
  double n=fractureNoise.at(X/110.,Z/110.),m=fractureNoise.at(X/110.+37.,Z/110.-61.);
  double x=X+9*n,z=Z+9*m;
  int gx=int(std::floor(x/64)),gz=int(std::floor(z/64));
  double first=INFINITY,second=INFINITY;IcePlatePoint nearest{};
  // The .22..78 inset guarantees both nearest points are within 3x3:
  // at every query position two inner candidates have smaller maximum
  // distance than the closest possible outer-ring point. Keep the inset.
  if(!snowRifts)for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto const&p=points[(gz+dz-minZ)*width+gx+dx-minX];double d=(x-p.x)*(x-p.x)+(z-p.z)*(z-p.z);
   if(d<first){second=first;first=d;nearest=p;}else second=std::min(second,d);
  }
  double edge=snowRifts?100.:.5*(std::sqrt(second)-std::sqrt(first));
  double fade=continuousGlacierBodies?s.superGlacier:smooth(.35,.75,s.superGlacier);
  if(glacierEcologyEnabled)fade*=s.glacier>=.65?1.:std::max(0.,(.65/.47)*(s.glacier-.18)/std::max(.001,s.glacier));
  double width=1.3+.5*(n+1);
  double split=1-smooth(width,width+5.,edge);
  double lift=refinedGlaciers ? 11+9*n+6*m+7*std::abs(n-m) : nearest.lift+nearest.tiltX*(x-nearest.x)+nearest.tiltZ*(z-nearest.z)+3*n;
  // Displaced slabs with a broad broken lip and a deep narrow throat. The
  // lowest ice floor stays above original ground and cannot open old caves.
  auto snowRift=snowRifts?riftSample(X,Z,snowSites):SnowRiftSample{};
  int top=s.height+int(std::floor(fade*(snowRifts?snowBodyLift(n,m,snowRift):std::max(3.,lift-(refinedGlaciers?12+4*m:24+5*m)*split))));
  auto col=raw.data()+i*Height;
  if(closedIceRifts)planClosedRift(i,int(X),int(Z),top,s,snowRift,n,m);
  if(thinSnowRifts&&top<s.height){
   // Downcut into solid ground, keeping three blocks above any pre-existing
   // cave, aquifer, lava or bedrock. This is a short bounded column scan.
   top=std::max(-48,top);
   for(int y=s.height;y>=top-3;--y){int b=col[y+64];if(b==0||b==1||b==8||b==43||b==126||b==145){top=std::max(top,y+3);break;}}
   for(int y=s.height;y>top;--y)col[y+64]=0;
   for(int y=top-2;y<=top;++y)col[y+64]=uint16_t(y==top?9:144);
  }
  for(int y=s.height+1;y<=top;++y){int band=(y+int(6*n)+384)%18;
   col[y+64]=uint16_t(refinedGlaciers ? (y>=top-1?9:band<3?144:143) : y==top&&edge>8&&s.iceSnow?7:band<3?144:143);
  }
  out[i]={top,edge,fade*split,n};
 }
 return out;
}
KARMA_NOINLINE void fractureIceTips(std::vector<uint16_t>&raw,std::array<IceRiftColumn,256>const&rifts,std::array<int,256>const&iceTops){
 if(!fracturedGlaciers||refinedGlaciers)return;
 for(int i=0;i<256;++i){auto const&r=rifts[i];if(r.bodyTop==-32768||r.strength<.15)continue;
  auto col=raw.data()+i*Height;int top=iceTops[i];
  if(top<=r.bodyTop+12)continue;
  // Extend some fissures into the surrounding ice towers as uneven open
  // notches, not holes through the whole mountain or floating prefab debris.
  int cut=int(std::floor((top-r.bodyTop)*.28*r.strength));
  for(int y=top;y>top-cut;--y)if(col[y+64]==143||col[y+64]==144||col[y+64]==7)col[y+64]=0;
 }
}
