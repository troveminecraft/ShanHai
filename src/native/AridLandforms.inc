// Sparse dry-land sediment spans, selected against the final water/terrain
// package. No runtime searches, flow simulation or per-voxel noise.
struct AridSpan {int x0,x1,height,type,top;};
std::unordered_map<int,std::vector<AridSpan>> aridRows;
void loadAridLandforms(){
 if(!data.contains("aridLandforms"))return;
 auto const&p=data.at("aridLandforms");
 if(p.at("algorithm")!="arid-dry-landforms-v1"||p.at("seed")!=terrainSeed||p.at("features").size()>8)
  throw std::runtime_error("Invalid arid landforms package");
 size_t columns=0,spans=0;
 for(auto const&f:p.at("features"))for(auto const&r:f.at("rows")){
  if(r.size()!=6)throw std::runtime_error("Invalid arid span size");
  int z=r.at(0);AridSpan a{r.at(1),r.at(2),r.at(3),r.at(4),r.at(5)};
  if(z<terrain_bounds::MinBlock+144||z>terrain_bounds::MaxBlock-144||a.x0<terrain_bounds::MinBlock+144||a.x1>terrain_bounds::MaxBlock-144||a.x0>a.x1||a.height<38||a.height>210
   ||a.type<1||a.type>3||(a.top!=5&&a.top!=48&&a.top!=94&&!(a.type==1&&(a.top==2||a.top==3||a.top==6||a.top==33))))throw std::runtime_error("Invalid arid span");
  columns+=a.x1-a.x0+1;++spans;
  if(columns>180000||spans>100000)throw std::runtime_error("Arid footprint budget exceeded");
  aridRows[z].push_back(a);
 }
 for(auto&[z,row]:aridRows){
  std::sort(row.begin(),row.end(),[](auto const&a,auto const&b){return a.x0<b.x0;});
  for(size_t i=1;i<row.size();++i)if(row[i-1].x1>=row[i].x0)throw std::runtime_error("Overlapping arid spans");
 }
}
void aridSurface(Surface&s,int wx,int wz){
 if(aridRows.empty())return;
 auto row=aridRows.find(wz-32768);if(row==aridRows.end())return;
 int x=wx-32768;auto const&v=row->second;
 auto it=std::lower_bound(v.begin(),v.end(),x,[](auto const&a,int x){return a.x1<x;});
 if(it==v.end()||it->x0>x)return;
 auto const&a=*it;
 // Reject incompatible water/special surfaces even if a stale input escaped
 // the source hash checks. These features must never overwrite live water.
 if(s.water>=s.height||s.lake||s.river||s.fluvialType||s.stone>.01||s.volcanic>0||s.giant>.01||s.scarp>.01
    ||s.wetland>0||s.cliffLow!=-32768||s.riverCutLow!=-32768||s.desert<=.4
    ||a.height-s.height>3||s.height-a.height>2)throw std::runtime_error("Arid footprint does not match dry ground");
 s.height=a.height;s.aridType=a.type;s.aridTop=a.top;
 s.continuous=s.e=(s.height-31)/123.;s.treeD=0;s.rockCap=0;s.rock=0;
 s.surf=7;s.soilSand=a.top==5?1:0;if(a.type!=1||s.sandCover>.5)s.biome=11;s.kind=0;s.oasis=0;
}
