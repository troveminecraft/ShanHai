// Ice skin follows the existing terrain and drainage, never a flat plateau.
bool snowUpdateEnabled=false;
bool uplandGlaciers=C.value("glacierAltitudeRevision",0)>=1;
bool gradualIceEcotone=terrainRevision(data)>=193;
bool blendedIceEdges=terrainRevision(data)>=177;
bool glacierWetlands=terrainRevision(data)>=168;
bool glacierEcology=glacierWetlands||data.value("version",std::string{})=="karma-terrain-2.167.0-native-agent-a";
bool continuousIceEdges=glacierEcology||data.value("version",std::string{})=="karma-terrain-2.166.0-native-agent-a";
Perlin glacierNoise{seedHash(terrainSeed,51001)};
struct IceRegion {bool valid=false;double x=0,z=0,rx=0,rz=0;int gx=0,gz=0;};
bool iceAtlasReady=false;
std::vector<IceRegion> iceAtlas;
void loadIceAtlas(){
 if(!data.contains("iceRegions"))return;
 auto const&p=data.at("iceRegions");
 if(p.at("algorithm")!="glacier-regions-v1"||p.at("seed")!=terrainSeed||p.at("regions").size()>64)throw std::runtime_error("Invalid glacier atlas");
 for(auto const&a:p.at("regions")){IceRegion r{true,a.at("x"),a.at("z"),a.at("radiusX"),a.at("radiusZ"),a.at("cellX"),a.at("cellZ")};
  if(!std::isfinite(r.x)||!std::isfinite(r.z)||r.rx<1500||r.rx>2000||r.rz<1500||r.rz>2000||std::abs(r.gx)>16||std::abs(r.gz)>16)throw std::runtime_error("Invalid glacier region");iceAtlas.push_back(r);
 }
 iceAtlasReady=true;
}
Cache<uint64_t,IceRegion> iceRegions{256};
IceRegion iceRegion(int gx,int gz){return iceRegions.get(key2(gx,gz),[&]{
 IceRegion r;r.gx=gx;r.gz=gz;if(hash01(gx,gz,terrainSeed^51003)>.72)return r;
 r.x=(gx+.22+.56*hash01(gx,gz,terrainSeed^51005))*4608;
 r.z=(gz+.22+.56*hash01(gx,gz,terrainSeed^51007))*4608;
 double best=INFINITY,bx=0,bz=0;bool found=false;
 // A coarse cell may contain ocean or a warm valley at its nominal centre.
 // Choose a real cold mainland anchor within it rather than forcing that
 // invalid centre into a glacier, using the same samples for every worker.
 for(int dz:{-1536,-1152,-768,-384,0,384,768,1152,1536})for(int dx:{-1536,-1152,-768,-384,0,384,768,1152,1536}){
  auto c=streamingRegions?regionalClimateSample(roundJS(r.x+dx)+32768,roundJS(r.z+dz)+32768):base(roundJS(r.x+dx)+32768,roundJS(r.z+dz)+32768);
  bool elevation=uplandGlaciers?(c.height>=seaLevelValue+3&&c.height<=360):
      (landscapeAltitude(c.height)>=36&&landscapeAltitude(c.height)<=80);
  if(c.T>.45||!elevation||c.water>=c.height||c.desert>=.05||c.stone>=.01||c.volcanic>=.01)continue;
  double sea=uplandGlaciers?0:INFINITY;
  if(!uplandGlaciers)for(int distance:{192,384,768,1280}){for(auto [sx,sz]:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1},std::pair{-1,-1},std::pair{1,1},std::pair{-1,1},std::pair{1,-1}}){
   auto shore=streamingRegions?regionalClimateSample(roundJS(r.x+dx+sx*distance)+32768,roundJS(r.z+dz+sz*distance)+32768):base(roundJS(r.x+dx+sx*distance)+32768,roundJS(r.z+dz+sz*distance)+32768);
   if(shore.height<seaLevelValue&&shore.water==seaLevelValue)sea=std::min(sea,double(distance));
  }if(std::isfinite(sea))break;}
  // The new altitude range follows cold mainland, including inland plateaus.
  // Its fixed 81-point survey needs no per-candidate search for an ocean.
  // Existing recipes retain the original coastal preference and anchors.
  double score=c.T+sea/6400.;
  if(score<best){best=score;bx=r.x+dx;bz=r.z+dz;found=true;}
 }
 if(!found)return r;r.x=bx;r.z=bz;
 int land=0;for(auto [dx,dz]:{std::pair{-640,0},std::pair{640,0},std::pair{0,-640},std::pair{0,640}}){
  auto s=streamingRegions?regionalClimateSample(roundJS(r.x+dx)+32768,roundJS(r.z+dz)+32768):base(roundJS(r.x+dx)+32768,roundJS(r.z+dz)+32768);
  if(s.T<.52&&landscapeAltitude(s.height)>38&&s.water<s.height&&s.desert<.1&&s.stone<.01&&s.volcanic<.01)++land;
 }
 if(land<1)return r;
 r.rx=1500+500*hash01(gx,gz,terrainSeed^51009);
 r.rz=1500+500*hash01(gx,gz,terrainSeed^51011);r.valid=true;return r;
});}
void glacierSkin(Surface&s,int wx,int wz){
 bool elevation=uplandGlaciers?(s.height>seaLevelValue-8&&s.height<470):
     (landscapeAltitude(s.height)>=32&&landscapeAltitude(s.height)<96);
 if(!snowUpdateEnabled||!iceAtlasReady||!elevation||s.stone>.01||s.giant>.01||s.desert>.08||s.volcanic>.01||s.barren>.1||s.T>.52)return;
 double x=wx-32768,z=wz-32768;int gx=int(std::floor(x/4608)),gz=int(std::floor(z/4608));double super=0;
 for(auto const&r:iceAtlas){if(std::abs(r.gx-gx)>1||std::abs(r.gz-gz)>1)continue;
  if(std::abs(x-r.x)>r.rx*1.15||std::abs(z-r.z)>r.rz*1.15)continue;
  double q=std::hypot((x-r.x)/r.rx,(z-r.z)/r.rz),warp=.09*glacierNoise.at(x/300,z/300);
  super=std::max(super,1-smooth(.64,1.08,q+warp));
 }
 super*=1-smooth(.36,.52,s.T);
 double cold=1-smooth(.20,.31,s.T),patch=smooth(-.12,.18,glacierNoise.at(x/520,z/520));
 double lowland=uplandGlaciers?smooth(seaLevelValue-8,seaLevelValue+6,s.height)*(1-smooth(400,470,s.height)):
     smooth(32,40,landscapeAltitude(s.height))*(1-smooth(72,96,landscapeAltitude(s.height)));
 if(continuousIceEdges)lowland*=(1-smooth(0.,.10,s.barren))*(1-smooth(0.,.01,s.stone))*(1-smooth(0.,.01,s.giant))*(1-smooth(0.,.08,s.desert))*(1-smooth(0.,.01,s.volcanic));
 s.superGlacier=super*lowland;
 // A cold/snowy parent biome is not an independent glacier. Keep the old
 // noise-driven ice patches only for saved recipes that explicitly used them.
 s.glacier=C.value("glacierBodyRevision",0)>=1?s.superGlacier:std::max(super,cold*patch)*lowland;
 if(glacierEcology&&s.glacier>0&&!s.kind&&!s.river&&!s.lake&&s.wetland<=.01&&s.water<s.height&&s.cliffLow==-32768){
  // One climate field controls the retreating forest, exposed mineral ground,
  // and ice body. Do this before ecology, not by burying existing vegetation.
  s.iceHabitat=s.glacier;
  double bare=gradualIceEcotone?smooth(.10,.62,s.iceHabitat):smooth(.015,blendedIceEdges?.30:.10,s.iceHabitat);
  double patch=clamp(.5+.9*glacierNoise.at(x/31.,z/31.),0.,1.);
  if(bare>patch){
   s.iceGround=s.iceHabitat<.065?33:s.iceHabitat<.16?(patch<.45?42:2):2;
   if(blendedIceEdges){double iceCover=gradualIceEcotone?smooth(.32,.72,s.iceHabitat):smooth(.16,.62,s.iceHabitat);if(iceCover>patch)s.iceGround=143;}else if(s.iceHabitat>=.30)s.iceGround=143;
  }
  s.treeD*=1-smooth(gradualIceEcotone?.06:.005,gradualIceEcotone?.60:.10,s.iceHabitat);
  if(s.iceHabitat>=(gradualIceEcotone?.62:.10)){s.treeD=0;s.giant=0;s.biome=s.superGlacier>.35?24:5;}
 }
 if(s.glacier<.02)return;
 s.iceThickness=2+int(s.glacier*(10+8*super));s.iceSnow=glacierNoise.at(x/27+7,z/27-13)>.01;
 if(!gradualIceEcotone)s.treeD*=1-smooth(.05,.3,s.glacier);s.rockCap=0;
 if(s.glacier>(gradualIceEcotone?.62:.3)){s.giant=0;s.treeD=0;s.biome=super>.35?24:5;}
 if(s.river>0||s.lake){s.biome=s.glacier>(gradualIceEcotone?.62:.3)?19:s.biome;s.iceSnow=false;}
}
