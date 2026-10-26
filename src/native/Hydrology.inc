// Coastal eligibility is sampled on a fixed world grid and interpolated.
// Lake masks are frozen basin analyses, stored as sorted row spans. Large
// lakes must not allocate one hash node per flooded column in every worker.
Cache<uint64_t,double> coastalNodes{4096};
Cache<uint64_t,double> coastalHeights{8192};
bool waterEnabled=false;
struct LakeCell{int level;bool flooded,frozen,sealAlcoves;};
struct LakeSpan{int x0,x1;LakeCell cell;};
std::vector<std::vector<LakeSpan>> lakeRows{terrain_bounds::Chunks*16};
LakeCell const* lakeAt(int x,int z)const{
 if(unboundedRegions){auto row=streamingLakeRows.find(z-32768);if(row==streamingLakeRows.end())return nullptr;auto const&spans=row->second;x-=32768;
  auto it=std::lower_bound(spans.begin(),spans.end(),x,[](LakeSpan const&a,int value){return a.x1<value;});return it!=spans.end()&&it->x0<=x?&it->cell:nullptr;
 }
 int row=z-32768-terrain_bounds::MinBlock;if(row<0||row>=int(lakeRows.size()))return nullptr;x-=32768;
 auto const& spans=lakeRows[row];
 auto it=std::lower_bound(spans.begin(),spans.end(),x,[](LakeSpan const&a,int value){return a.x1<value;});
 return it!=spans.end()&&it->x0<=x?&it->cell:nullptr;
}
double coastalNode(int x,int z){return coastalNodes.get(key2(x,z),[&]{
 double lo=INFINITY,hi=-INFINITY;
 for(auto [dx,dz]:{std::pair{-64,0},std::pair{64,0},std::pair{0,-64},std::pair{0,64}}){
  int sx=x+dx/16,sz=z+dz/16;
  double h=coastalHeights.get(key2(sx,sz),[&]{return uncliff(sx*16.,sz*16.);});lo=std::min(lo,h);hi=std::max(hi,h);
 }
 return (1-smooth(seaLevelValue-6,seaLevelValue+2,lo))*smooth(elevationVariants?126:90,elevationVariants?171:135,hi)*smooth(.45,1.1,(hi-lo)/128);
});}
double coastWeight(double x,double z,double h){
 if(!waterEnabled||h<seaLevelValue-6||h>(elevationVariants?141:105))return 0;
 int ix=int(std::floor(x/16)),iz=int(std::floor(z/16));double u=smooth(0,1,x/16-ix),v=smooth(0,1,z/16-iz);
 return (1-u)*(1-v)*coastalNode(ix,iz)+u*(1-v)*coastalNode(ix+1,iz)+(1-u)*v*coastalNode(ix,iz+1)+u*v*coastalNode(ix+1,iz+1);
}
void loadLakes(){
 waterEnabled=C.contains("waterUpdate")&&C.at("waterUpdate").value("enabled",false);
 if(!data.contains("naturalLakes"))return;
 size_t columns=0,spanCount=0;
 for(auto const&lake:data.at("naturalLakes")){
  int level=lake.at("level");if(level<seaLevelValue+3||level>generatedMaxYExclusive-30)throw std::runtime_error("Invalid lake level");
  double scale=num(C.at("epic"),"horizontalScale");
  double climate=.62+.45*temp(lake.at("center")[0].get<double>()*scale,0,lake.at("center")[1].get<double>()*scale);
  bool frozen=climate-std::clamp((level-160)*.0007,0.,.08)<.42;
  for(auto const&row:lake.at("rows")){
   int z=row.at(0),x0=row.at(1),x1=row.at(2),type=row.at(3);
   if(x0>x1||x0<terrain_bounds::MinBlock||x1>terrain_bounds::MaxBlock||z<terrain_bounds::MinBlock||z>terrain_bounds::MaxBlock||x1-x0>2048||(type!=0&&type!=1))throw std::runtime_error("Invalid lake span");
   columns+=size_t(x1-x0+1);++spanCount;
   if(columns>2000000||spanCount>100000)throw std::runtime_error("Excessive lake mask");
   lakeRows[z-terrain_bounds::MinBlock].push_back({x0,x1,{level,type==1,frozen,lake.value("large",false)}});
  }
 }
 for(auto&row:lakeRows){
  std::sort(row.begin(),row.end(),[](LakeSpan const&a,LakeSpan const&b){return a.x0<b.x0;});
  for(size_t i=1;i<row.size();++i)if(row[i-1].x1>=row[i].x0)throw std::runtime_error("Overlapping lake masks");
 }
}
void waterSurface(Surface& s,int x,int z){
 if(!waterEnabled)return;
 if(auto cell=lakeAt(x,z)){
  auto a=*cell;
  // A flooded basin and its narrow dry collar must not open through a
  // decorative hillside alcove. Heights and alcoves outside the mask stay intact.
  if(a.sealAlcoves&&s.cliffLow<=a.level)s.cliffLow=s.cliffHigh=-32768;
  if(a.flooded){if(s.height>=a.level){if(!streamingRegions)throw std::runtime_error("Frozen basin no longer matches terrain");s.lake=2;}else{s.water=a.level;s.kind=2;s.lake=1;s.lakeFrozen=a.frozen;}}
  else s.lake=2;
 }
 // Dry deep caves are the default. A stone collar encloses the few wet regions.
 if(boundedAquifers)caveWaterColumn(s,x,z);
 else {double wet=aquiferNoise.at((x-32768)/192.,(z-32768)/192.);s.aquifer=wet>.28?2:wet>.20?1:0;}
}
