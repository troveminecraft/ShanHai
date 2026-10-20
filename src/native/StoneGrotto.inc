// Sparse, seed-selected special structure geometry. Only affected chunks have
// runs; this is not an aquifer rule for ordinary coastal caves.
struct GrottoRun {int column,lo,hi,block;};
std::unordered_map<uint64_t,std::vector<GrottoRun>> grottoRuns;
bool stoneForestFalls=false;
void loadStoneGrotto(J const&data){
 stoneForestFalls=C.at("cave").value("stoneForestFalls",0)==1;
 if(!data.contains("stoneForestGrotto"))return;
 auto const&p=data.at("stoneForestGrotto");
 if(p.at("seed")!=terrainSeed||p.at("algorithm")!="stone-forest-falls-v1")throw std::runtime_error("Stone grotto seed mismatch");
 auto load=[&](J const&features){for(auto const&f:features)for(auto const&r:f.at("runs")){
  int x=r[0],z=r[1],lo=r[2],hi=r[3],id=r[4];
  if(x<terrain_bounds::MinBlock||x>terrain_bounds::MaxBlock||z<terrain_bounds::MinBlock||z>terrain_bounds::MaxBlock||lo<-55||hi>319||lo>hi||!(id==0||id==2||id==8||id==94||id==126||id==6||id==48||(id>=127&&id<=142)))throw std::runtime_error("Invalid grotto run");
  int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
  grottoRuns[key2(cx,cz)].push_back({(z-cz*16)*16+x-cx*16,lo,hi,id});
 }};
 load(p.at("features"));
 if(p.contains("lushPools"))load(p.at("lushPools"));
 if(p.contains("entrances"))load(p.at("entrances"));
}
KARMA_NOINLINE void stoneGrotto(int x0,int z0,std::vector<uint16_t>&raw){
 if(grottoRuns.empty())return;
 auto it=grottoRuns.find(key2((x0-32768)/16,(z0-32768)/16));if(it==grottoRuns.end())return;
 for(auto const&r:it->second)std::fill(raw.data()+r.column*Height+r.lo+64,raw.data()+r.column*Height+r.hi+65,uint16_t(r.block));
}
