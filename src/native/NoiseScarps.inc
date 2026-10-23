// Compact, offline noise-domain deformation. Runtime only reads column spans.
// Heights come from the existing terrain noise, never an independent cliff loft.
struct NoiseScarp {
 int minX,minZ,width,depth;
 std::vector<std::array<int16_t,4>> columns;
};
std::vector<NoiseScarp> noiseScarps;
std::unordered_map<uint64_t,std::vector<size_t>> noiseScarpBins;
void loadNoiseScarps(){
 auto const&q=data.at("overhangCliffs");
 if(q.at("seed").get<uint32_t>()!=terrainSeed||q.at("sites").size()>32)throw std::runtime_error("Invalid noise scarp seed/count");
 size_t budget=0;
 for(auto const&a:q.at("sites")){
  auto const&b=a.at("bounds");NoiseScarp s{b.at(0),b.at(1),b.at(2),b.at(3),{}};
  if(s.width<1||s.depth<1||s.width>320||s.depth>320||s.minX<terrain_bounds::MinBlock||s.minZ<terrain_bounds::MinBlock||s.minX+s.width>terrain_bounds::MaxBlock+1||s.minZ+s.depth>terrain_bounds::MaxBlock+1)throw std::runtime_error("Invalid noise scarp bounds");
  budget+=size_t(s.width)*s.depth;if(budget>2000000)throw std::runtime_error("Noise scarp budget exceeded");
  s.columns.assign(size_t(s.width)*s.depth,{-32768,-32768,-32768,0});
  for(auto const&r:a.at("rows")){
   if(r.size()<6||(r.size()-2)%4)throw std::runtime_error("Invalid scarp row");
   int z=r.at(0),x=r.at(1),count=int((r.size()-2)/4);
   if(z<s.minZ||z>=s.minZ+s.depth||x<s.minX||x+count>s.minX+s.width)throw std::runtime_error("Scarp row outside bounds");
   for(int i=0;i<count;++i){
    int top=r.at(2+i*4),lo=r.at(3+i*4),hi=r.at(4+i*4),rock=r.at(5+i*4);
    if(top!=-32768&&(top<40||top>290||(lo!=-32768&&(lo<34||hi<lo||hi>=top))||rock<0||rock>1))throw std::runtime_error("Invalid scarp column");
    auto&v=s.columns[size_t(z-s.minZ)*s.width+x-s.minX+i];
    if(v[0]!=-32768)throw std::runtime_error("Overlapping scarp rows");
    v={int16_t(top),int16_t(lo),int16_t(hi),int16_t(rock)};
   }
  }
  size_t id=noiseScarps.size();noiseScarps.push_back(std::move(s));
  auto const&v=noiseScarps.back();
  for(int z=int(std::floor(v.minZ/64.));z<=int(std::floor((v.minZ+v.depth-1)/64.));++z)
   for(int x=int(std::floor(v.minX/64.));x<=int(std::floor((v.minX+v.width-1)/64.));++x)noiseScarpBins[key2(x,z)].push_back(id);
 }
}
void noiseScarpRelief(Surface&s,int wx,int wz){
 if(noiseScarps.empty())return;
 int x=wx-32768,z=wz-32768;
 auto it=noiseScarpBins.find(key2(int(std::floor(x/64.)),int(std::floor(z/64.))));if(it==noiseScarpBins.end())return;
 for(auto id:it->second){auto const&a=noiseScarps[id];
  if(x<a.minX||x>=a.minX+a.width||z<a.minZ||z>=a.minZ+a.depth)continue;
  auto const&v=a.columns[size_t(z-a.minZ)*a.width+x-a.minX];if(v[0]==-32768)continue;
  int original=s.height;s.height=v[0];s.cliffLow=v[1];s.cliffHigh=v[2];s.scarp=v[3];s.scarpWarped=true;
  int floor=v[1]>-32768?v[1]-1:s.height;
  s.scarpCaveCeiling=int(std::floor(std::min(original,floor)-caveRoof));
  s.continuous=(s.height-seaLevelValue)/123.;s.e=s.continuous;
 }
}
