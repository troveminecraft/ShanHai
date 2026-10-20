struct LavaColumn {int x,z,bed,top,clear;bool source=false;};
struct LavaReservoir {double x,z,radius;int level;};
std::unordered_map<uint64_t,std::vector<LavaColumn>> lavaColumns;
std::unordered_map<uint64_t,std::vector<LavaReservoir>> lavaReservoirs;
void loadLavaFlows(J const&data){
 if(!data.contains("lavaFlows"))return;
 auto const&p=data.at("lavaFlows");
 bool natural=p.at("algorithm")=="volcanic-overflows-v4";
 bool vents=p.at("algorithm")=="volcanic-overflows-v3";
 bool shallow=natural||vents||p.at("algorithm")=="volcanic-overflows-v2";
 if((!shallow&&p.at("algorithm")!="volcanic-overflows-v1")||p.at("seed")!=terrainSeed||p.at("flows").size()>32)throw std::runtime_error("Invalid lava flow package");
 size_t count=0;
 for(auto const&f:p.at("flows")){
  if(shallow&&!vents&&!natural){
   auto const&r=f.at("reservoir");LavaReservoir v{r.at(0),r.at(1),r.at(2),r.at(3)};
   if(v.radius<1||v.radius>160||v.level<32||v.level>300)throw std::runtime_error("Invalid lava reservoir");
   for(int z=int(std::floor((v.z-v.radius)/16));z<=int(std::floor((v.z+v.radius)/16));++z)
   for(int x=int(std::floor((v.x-v.radius)/16));x<=int(std::floor((v.x+v.radius)/16));++x)lavaReservoirs[key2(x,z)].push_back(v);
  }
  if(natural){
   auto const&first=f.at("path").at(0);auto const&anchor=f.at("poolAnchor");
   if(std::abs(first.at(0).get<int>()-anchor.at(0).get<int>())+std::abs(first.at(1).get<int>()-anchor.at(1).get<int>())!=1)throw std::runtime_error("Overflow inlet does not touch pool");
   auto const&a=f.at("poolAnchor");auto s=field.surface(a.at(0).get<int>()+32768,a.at(1).get<int>()+32768,false);
   if(s.lava!=a.at(2).get<int>()||s.height>=s.lava||f.contains("source")||f.contains("reservoir"))throw std::runtime_error("Overflow has no natural pool source");
  }
  for(auto const&a:f.at("columns")){
   LavaColumn c{a.at(0),a.at(1),a.at(2),a.at(3),a.at(4)};
   if(++count>100000||c.bed<32||c.top<=c.bed||c.top>300||c.clear>310||c.clear<c.top||c.clear-c.bed>100)throw std::runtime_error("Invalid lava column");
   if(shallow&&(a.size()!=6||a.at(5).get<int>()-c.bed>2||a.at(5).get<int>()<c.bed))throw std::runtime_error("Deep lava trench rejected");
   if(natural&&c.top>f.at("poolAnchor").at(2).get<int>())throw std::runtime_error("Overflow rises above natural pool");
   if(vents)c.source=c.x==f.at("source").at(0).get<int>()&&c.z==f.at("source").at(1).get<int>();
   lavaColumns[key2(int(std::floor(c.x/16.)),int(std::floor(c.z/16.)))].push_back(c);
  }
 }
}
void lavaFlows(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 auto key=key2((x0-32768)/16,(z0-32768)/16);
 if(auto it=lavaReservoirs.find(key);it!=lavaReservoirs.end())for(auto const&r:it->second){
  for(int i=0;i<256;++i){auto const&s=surfaces[i];double x=x0+i%16-32768-r.x,z=z0+i/16-32768-r.z;
   if(x*x+z*z>r.radius*r.radius||s.volcanic<.99||s.lava==-32768||s.height>=r.level)continue;
   auto col=raw.data()+i*Height;
   // Raise the selected crater's magma head to its natural low rim. Fill
   // existing open crater space only; never excavate the rim or mountain.
   for(int y=std::max(s.height+1,s.lava+1);y<=r.level;++y)if(col[y+64]==0||col[y+64]==43||col[y+64]==145)col[y+64]=43;
  }
 }
 auto it=lavaColumns.find(key);if(it==lavaColumns.end())return;
 for(auto const&c:it->second){int x=c.x-(x0-32768),z=c.z-(z0-32768);auto col=raw.data()+(z*16+x)*Height;
  col[c.bed+64]=45;
  for(int y=c.bed+1;y<=c.clear;++y)col[y+64]=uint16_t(y<=c.top?145:0);
  if(c.source)col[c.bed+65]=43; // Native source at the shallow rim vent, not the deep crater lake.
 }
}
