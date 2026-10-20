// World-space, bounded 2-D crevasses. No per-voxel noise or adjacent chunk reads.
bool snowRifts=false,thinSnowRifts=false,closedIceRifts=false,bedrockIceRifts=false;
struct SnowRift {double x,z,ux,uz,length,width,phase;bool valid;};
struct SnowRiftSample {double cut=0,lip=0,distance=INFINITY,width=0,end=0;};
Cache<uint64_t,SnowRift> snowRiftSites{256};
Perlin snowRiftNoise{seedHash(terrainSeed,51801)};
SnowRift snowRiftSite(int gx,int gz){return snowRiftSites.get(key2(gx,gz),[&]{
 auto r=[&](int salt){return hash01(gx,gz,terrainSeed^salt);};
 double a=r(51803)*6.28318530718;
 return SnowRift{(gx+.2+.6*r(51805))*512,(gz+.2+.6*r(51807))*512,std::cos(a),std::sin(a),210+130*r(51809),10+8*r(51811),r(51813)*6.28318530718,r(51815)<.84};
});}
std::array<SnowRift,9> riftNeighborhood(double x,double z){
 std::array<SnowRift,9> a{};int gx=int(std::floor(x/512)),gz=int(std::floor(z/512)),i=0;
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)a[i++]=snowRiftSite(gx+dx,gz+dz);
 return a;
}
SnowRiftSample riftSample(double x,double z,std::array<SnowRift,9>const&sites,bool narrow=true){
 SnowRiftSample out;
 for(auto const&r:sites){if(!r.valid)continue;
  double dx=x-r.x,dz=z-r.z,u=dx*r.ux+dz*r.uz,v=-dx*r.uz+dz*r.ux;
  if(std::abs(u)>=r.length||std::abs(v)>r.width+48)continue;
  double end=1-smooth(.64,1.,std::abs(u)/r.length);
  double bend=14*std::sin(u/91+r.phase)+7*std::sin(u/37-r.phase);
  if(std::abs(v-bend)>r.width+23)continue;
  double coarse=snowRiftNoise.at(x/23.,z/23.),fine=snowRiftNoise.at(x/8.+13,z/8.-7);
  double width=std::max(4.,r.width+6*coarse+3*fine),distance=std::abs(v-bend-4*coarse);
  if(closedIceRifts&&narrow){
   width=std::max(2.5,r.width*.25+1.2*coarse+.75*fine);
   double influence=(1-smooth(width+(bedrockIceRifts?6:3),width+(bedrockIceRifts?8:5),distance))*end;
   if(influence>out.cut)out={influence,0,distance,width,end};
   continue;
  }
  // Wide throat, steep but scalloped shoulders, and attached ice buttresses.
  double cut=(1-smooth(width*.55,width+7+3*coarse,distance))*end;
  double lip=smooth(width*.7,width+4,distance)*(1-smooth(width+4,width+21,distance))*end*(4+9*std::abs(coarse-fine));
  out.cut=std::max(out.cut,cut);if(!thinSnowRifts)out.lip=std::max(out.lip,lip);
 }
 return out;
}
SnowRiftSample riftAt(double x,double z){if(!snowRifts)return {};return riftSample(x,z,riftNeighborhood(x,z));}
double snowBodyLift(double n,double m,SnowRiftSample r){
 double thick=34+13*n+8*m;
 if(bedrockIceRifts)return clamp(5+2*n+1.5*m,3.,9.);
 if(closedIceRifts)return thick;
 if(thinSnowRifts)return thick*(1-r.cut)+3*r.cut-26*smooth(.55,1.,r.cut);
 return std::max(3.,thick*(1-r.cut)+3*r.cut+r.lip);
}
void snowCover(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(!snowRifts)return;
 bool any=false;for(auto const&s:surfaces)any|=s.glacier>.30;if(!any)return;
 auto sites=thinSnowRifts&&!closedIceRifts?riftNeighborhood(x0-32768,z0-32768):std::array<SnowRift,9>{};
 for(int i=0;i<256;++i){auto const&s=surfaces[i];
  if(s.glacier<(gradualIceEcotone?.62:.30)||s.kind||s.river||s.lake||s.water>=s.height||s.wetland>.01)continue;
  int X=x0+i%16-32768,Z=z0+i/16-32768;
  // The inward broken shoulders and floor stay bare. No elevated snow rim.
  if(closedIceRifts){auto const&p=closedRiftColumns[i];if(p.valid&&p.distance<=p.opening)continue;}
  else if(thinSnowRifts&&s.superGlacier>.02&&riftSample(X,Z,sites).cut>.35)continue;
  auto col=raw.data()+i*Height;
  for(int y=318;y>=std::max(-56,s.height-2);--y)
   if((col[y+64]==9||col[y+64]==143||col[y+64]==144)&&col[y+65]==0){
    double chance=hash01(X,Z,terrainSeed^51831);
    col[y+65]=thinSnowRifts?uint16_t(chance<.90?146:chance<.98?147:148):23;
   }
 }
}
