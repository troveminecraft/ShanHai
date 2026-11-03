struct LargeBoulder {int x=0,z=0,ground=0;double rx=0,rz=0,height=0,phase=0;};
Cache<uint64_t,LargeBoulder> largeBoulders{256};
LargeBoulder largeBoulder(int gx,int gz){return largeBoulders.get(key2(gx,gz),[&]{
 LargeBoulder a;auto r=[&](int salt){return hash01(gx,gz,terrainSeed^(89701+salt));};
 if(r(0)>.18)return a;a.x=gx*64+16+int(r(1)*32);a.z=gz*64+16+int(r(2)*32);
 auto allowed=[](Surface const&s){return (s.biome==5||s.biome==7||s.biome==8||s.biome==12||s.biome==14||s.biome==20)
  &&s.water<s.height&&s.lava<s.height&&!s.kind&&!s.lake&&!s.river&&s.stone<.01&&s.volcanic<.01
  &&s.giant<.01&&s.glacier<.1&&s.superGlacier<.1&&s.cliffLow==-32768;};
 auto center=field.surface(a.x,a.z,false);if(!allowed(center)||nativeReserved(a.x,a.z))return LargeBoulder{};
 a.rx=4.5+4*r(3);a.rz=4+4*r(4);a.height=3.5+4.5*r(5);a.phase=r(6)*6.283185307;
 int low=center.height,high=center.height;
 for(int z=-10;z<=10;z+=2)for(int x=-10;x<=10;x+=2){
  if(x*x/(a.rx*a.rx)+z*z/(a.rz*a.rz)>1.4)continue;
  auto s=field.surface(a.x+x,a.z+z,false);
  if(!allowed(s)||nativeReserved(a.x+x,a.z+z))return LargeBoulder{};
  low=std::min(low,s.height);high=std::max(high,s.height);
 }
 if(high-low>4)return LargeBoulder{};a.ground=low;return a;
});}
void largeBoulderFeatures(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(C.value("groundFeatureRevision",0)<1)return;
 for(int gz=int(std::floor((z0-12)/64.));gz<=int(std::floor((z0+27)/64.));++gz)
 for(int gx=int(std::floor((x0-12)/64.));gx<=int(std::floor((x0+27)/64.));++gx){
  auto a=largeBoulder(gx,gz);if(!a.height)continue;
  for(int i=0;i<256;++i){int x=x0+i%16,z=z0+i/16;if(nativeReserved(x,z))continue;
   double dx=x-a.x,dz=z-a.z,angle=std::atan2(dz,dx),edge=1+.14*std::sin(angle*3+a.phase)+.07*std::cos(angle*5-a.phase);
   double q=(dx*dx/(a.rx*a.rx)+dz*dz/(a.rz*a.rz))/(edge*edge);if(q>=1)continue;
   int top=int(std::floor(a.ground+a.height*std::pow(1-q,.43)+.7*std::sin(dx*.6+dz*.3+a.phase)));
   for(int y=surfaces[i].height+1;y<=std::min(top,Height-65);++y){auto&b=raw[i*Height+y+64];
    if(b==0||b==36||b==37||b==7||b==146)b=hash01(x/3,z/3,terrainSeed^89731)<.28?20:2;
   }
  }
 }
}
