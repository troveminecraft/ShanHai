// Sparse, immutable seed-time floodplain footprints. Rows are sorted spans;
// normal terrain pays one empty-row lookup and no extra terrain/noise sample.
struct FluvialSpan {int x0,x1,height,water,type,top;};
std::unordered_map<int,std::vector<FluvialSpan>> fluvialRows;
void loadFluvialLandforms(){
 if(!data.contains("rivers")||!data.at("rivers").contains("landforms"))return;
 auto const&p=data.at("rivers").at("landforms");
 if(p.at("algorithm")!="floodplain-landforms-v1"||p.at("seed")!=terrainSeed)
  throw std::runtime_error("Invalid fluvial landform package");
 size_t columns=0,spans=0;
 if(p.at("features").size()>8)throw std::runtime_error("Fluvial feature budget exceeded");
 for(auto const&f:p.at("features"))for(auto const&r:f.at("rows")){
  if(r.size()!=7)throw std::runtime_error("Invalid fluvial span size");
  int z=r.at(0);FluvialSpan a{r.at(1),r.at(2),r.at(3),r.at(4),r.at(5),r.at(6)};
  if(z<terrain_bounds::MinBlock||z>terrain_bounds::MaxBlock||a.x0<terrain_bounds::MinBlock||a.x1>terrain_bounds::MaxBlock||a.x0>a.x1||a.height<28||a.height>110
    ||a.type<1||a.type>3||(a.water!=-32768&&(a.water<=a.height||a.water>a.height+16))
    ||(a.top!=3&&a.top!=4&&a.top!=5&&a.top!=33&&a.top!=42&&a.top!=48&&a.top!=85))
   throw std::runtime_error("Invalid fluvial span");
  columns+=a.x1-a.x0+1;++spans;
  if(columns>120000||spans>60000)throw std::runtime_error("Fluvial footprint budget exceeded");
  fluvialRows[z].push_back(a);
 }
 for(auto&[z,row]:fluvialRows){
  std::sort(row.begin(),row.end(),[](auto const&a,auto const&b){return a.x0<b.x0;});
  for(size_t i=1;i<row.size();++i)if(row[i-1].x1>=row[i].x0)throw std::runtime_error("Overlapping fluvial spans");
 }
}
void fluvialRelief(Surface&s,int wx,int wz){
 if(fluvialRows.empty())return;
 auto row=fluvialRows.find(wz-32768);if(row==fluvialRows.end())return;
 int x=wx-32768;auto const&v=row->second;
 auto it=std::lower_bound(v.begin(),v.end(),x,[](auto const&a,int x){return a.x1<x;});
 if(it==v.end()||it->x0>x)return;
 auto const&a=*it;s.height=a.height;s.water=a.water;s.fluvialType=a.type;s.fluvialTop=a.top;
 s.cliffLow=s.cliffHigh=s.riverCutLow=s.riverCutHigh=-32768;s.drip=0;s.riverDeposit=false;
 s.wetland=s.wetlandMoisture=0;s.scarp=0;s.scarpWarped=false;s.lakeFrozen=false;
 s.continuous=s.e=(s.height-31)/123.;s.kind=a.water>s.height?2:0;
 s.river=a.type==1&&s.kind?1:0;s.lake=a.type==2?(s.kind?1:2):0;
}
void fluvialSkin(Surface&s){
 if(!s.fluvialType)return;
 s.treeD=0;s.giant=0;s.rockCap=0;s.rock=0;
 s.surf=0;s.soilSand=s.fluvialTop==5?1:0;
 s.biome=s.water>s.height?18:8;
}
