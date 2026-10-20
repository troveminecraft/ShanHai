// Ores are placed after the last landform carve. Cross-chunk exposure queries
// reconstruct sparse carve intervals; they never generate/load another chunk.
struct OreHaloCarves {ClosedRiftColumn rift;std::vector<DetailSite> details;std::vector<GrottoRun const*> grotto;};
Cache<uint64_t,std::shared_ptr<OreHaloCarves>> oreHaloCarves{1024};
std::shared_ptr<OreHaloCarves> oreCarves(Surface const&s,int X,int Z){return oreHaloCarves.get(key2(X,Z),[&]{
 auto a=std::make_shared<OreHaloCarves>();
 int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
 if(auto it=grottoRuns.find(key2(cx,cz));it!=grottoRuns.end())for(auto const&r:it->second)
  if(r.column==(Z-cz*16)*16+X-cx*16)a->grotto.push_back(&r);
 if(s.superGlacier>.02&&s.glacier>.02&&!s.kind&&!s.river&&!s.lake&&!s.fluvialType&&s.wetland<=.01&&s.water<s.height&&s.scarp<=.01&&s.cliffLow==-32768){
  auto r=riftAt(X,Z);
  if(r.cut>0){double n=fractureNoise.at(X/110.,Z/110.),m=fractureNoise.at(X/110.+37.,Z/110.-61.);
   double fade=s.superGlacier*(s.glacier>=.65?1.:std::max(0.,(.65/.47)*(s.glacier-.18)/std::max(.001,s.glacier)));
   int top=s.height+int(std::floor(fade*snowBodyLift(n,m,r)));
   a->rift=closedRiftGeometry(X,Z,top,r,n,m);
  }
 }
 if(s.glacier>.65&&!s.kind&&!s.river&&!s.lake&&s.water<s.height&&s.wetland<=.01){
  for(int gz=int(std::floor((Z-24)/128.));gz<=int(std::floor((Z+24)/128.));++gz)
  for(int gx=int(std::floor((X-24)/128.));gx<=int(std::floor((X+24)/128.));++gx){auto d=detailSite(gx,gz);
   if(d.type==3&&std::abs(X-d.x)<=24&&std::abs(Z-d.z)<=24)a->details.push_back(d);
  }
 }
 return a;
});}
bool finalOreAir(Surface const&s,int wx,int y,int wz){
 // Existing air is kept as an exclusion even if a decorative overlay later
 // covers it. This conservative halo can reject extra ore; it cannot approve
 // an originally exposed buried-only vein.
 if(field.material(s,wx,y,wz)==0)return true;
 int X=wx-32768,Z=wz-32768;
 // Most ore halos have no post-terrain carve at all. Avoid allocating a
 // descriptor and consulting ice-site caches for ordinary rock columns.
 if(s.glacier<=.65){
  int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));auto it=grottoRuns.find(key2(cx,cz));
  if(it==grottoRuns.end())return false;
  int column=(Z-cz*16)*16+X-cx*16;
  for(auto const&r:it->second)if(r.column==column&&r.block==0&&y>=r.lo&&y<=r.hi)return true;
  return false;
 }
 auto a=oreCarves(s,X,Z);
 for(auto r:a->grotto)if(r->block==0&&y>=r->lo&&y<=r->hi)return true;
 auto const&p=a->rift;
 if(p.valid){
  int floor=std::max(-59,p.floor),hi=p.top;
  if(p.distance>p.opening){double side=clamp((p.distance-p.opening)/std::max(.001,p.undercut),0.,1.);
   hi=std::min(p.top-3,floor+int(std::floor((p.top-floor)*(1-side))));
  }
  double outer=clamp((p.distance-p.opening-2)/std::max(.001,p.undercut),0.,1.);
  int liningTop=std::min(p.top,floor+int(std::floor((p.top-floor)*(1-outer))));
  if(liningTop>floor&&y>floor&&y<=hi)return true;
  if(s.superGlacier>.5&&p.distance>p.opening&&p.distance<p.opening+p.undercut+1.7){
   double broad=landscapeNoise.at(X/61.,Z/61.),fine=landscapeNoise.at(X/13.+17,Z/13.-11);
   int mid=p.top-12-int(4*fine),span=2+int(2*smooth(.20,.55,broad));
   if(broad>.20&&y>=std::max(p.floor+4,mid-span)&&y<=std::min(p.top-5,mid+span))return true;
  }
 }
 for(auto const&d:a->details){double dx=X-d.x,dz=Z-d.z,u=dx*std::cos(d.angle)+dz*std::sin(d.angle),v=-dx*std::sin(d.angle)+dz*std::cos(d.angle)-1.2*std::sin(u/7.);
  if(u<0||u>24||std::abs(v)>2.8)continue;
  int span=int(std::floor(2.8*std::sqrt(std::max(0.,1-v*v/7.84)))),mid=d.y+int(u*.9);
  if(y>=mid-span&&y<=mid+span)return true;
 }
 return false;
}
KARMA_NOINLINE void cleanupSnowSupports(std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 for(int i=0;i<256;++i){if(surfaces[i].glacier<.30)continue;auto col=raw.data()+i*Height;
  for(int y=1;y<Height;++y)if(col[y]>=146&&col[y]<=148){int b=col[y-1];
   if(b==0||b==8||b==43||b==126||b==145)col[y]=0;
  }
 }
}
