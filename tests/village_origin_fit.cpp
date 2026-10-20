#include "rules/VillageFit.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
using J=nlohmann::json;
using namespace worldgen_rules;
J load(std::filesystem::path const&p){J v;std::ifstream(p)>>v;return v;}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("fixture-dir report.json");std::filesystem::path dir=argv[1];
 auto pieces=load(dir/"template-footing-fixture.json"),terrain=load(dir/"planned-vs-natural.json");
 std::map<VillageXZ,int> natural;for(auto const&r:terrain)natural[{r.at("x"),r.at("z")}]=r.at("natural");
 J report=J::array();size_t fitted=0,rejected=0;bool tanneryCorrected=false;
 for(auto const&p:pieces){int px=p.at("pos")[0],py=p.at("pos")[1],pz=p.at("pos")[2],rotation=p.at("rotation");VillageCells cells;std::map<VillageXZ,int> footing,doors;int entry=320;
  for(auto const&b:p.at("blocks")){std::string const&n=b.at(3).get_ref<std::string const&>();if(n.ends_with("_door"))entry=std::min(entry,py+b.at(1).get<int>()-1);}
  for(auto const&b:p.at("blocks")){auto const&n=b.at(3).get_ref<std::string const&>();if(n=="minecraft:structure_void"||n=="minecraft:jigsaw")continue;
   auto [rx,rz]=rotateVillageXZ(b.at(0),b.at(2),rotation);int x=px+rx,z=pz+rz,y=py+b.at(1).get<int>();cells[{x,y,z}]=n;
   if(y<py-1||y>py+1||!villageFoundationBlock(n))continue;VillageXZ key{x,z};auto [it,added]=footing.emplace(key,y-1);if(!added)it->second=std::min(it->second,y-1);if(entry<320)doors[key]=entry;
  }
  auto sample=[&](int x,int z){return natural.at({x,z});};auto dy=villageOriginOffset(cells,footing,doors,sample);int oldMax=0,newMax=0;
  for(auto const&[xz,h]:footing){int y=villageFinishedHeight(cells,xz,h,doors);oldMax=std::max(oldMax,std::abs(y-sample(xz.first,xz.second)));if(dy)newMax=std::max(newMax,std::abs(y+*dy-sample(xz.first,xz.second)));}
  if(dy){++fitted;if(newMax>2)throw std::runtime_error("Accepted piece still forces a tall terrace");
   VillageCells moved;std::map<VillageXZ,int> shiftedFooting,shiftedDoors;
   for(auto const&[q,n]:cells)moved[{q[0],q[1]+*dy,q[2]}]=n;
   for(auto const&[xz,h]:footing)shiftedFooting[xz]=h+*dy;
   for(auto const&[xz,h]:doors)shiftedDoors[xz]=h+*dy;
   if(villageOriginOffset(moved,shiftedFooting,shiftedDoors,sample)!=0)throw std::runtime_error("Saved village would move again on reload");
   if(p.at("location").get<std::string>().find("tannery")!=std::string::npos)tanneryCorrected=*dy<=-4&&newMax<=2;
  }else ++rejected;
  report.push_back({{"template",p.at("location")},{"footing",footing.size()},{"oldMaxHeightChange",oldMax},{"shift",dy?J(*dy):J(nullptr)},{"newMaxHeightChange",dy?J(newMax):J(nullptr)}});
 }
 if(!fitted||!tanneryCorrected)throw std::runtime_error("Reported high tannery pad not corrected");
 J out={{"passed",true},{"actualNativeTemplates",pieces.size()},{"acceptedPieces",fitted},{"unsuitablePiecesRejected",rejected},{"maximumAcceptedFootingChange",2},{"savedLayoutIdempotent",true},{"templates",report},{"gameplayAcceptancePerformed",false}};
 std::ofstream(argv[2])<<out.dump(2);std::cout<<out.dump(2)<<'\n';return 0;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
