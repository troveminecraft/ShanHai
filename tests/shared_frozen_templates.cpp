#include "rules/FrozenTemplates.h"
#include <fstream>
#include <iostream>
#include <random>
using namespace worldgen_rules;
using J=nlohmann::json;
void check(bool value,char const*why){if(!value)throw std::runtime_error(why);}
struct FixedRandom{int rotation;bool basement;int count;int nextInt(int n){return n==4?rotation:count;}int nextInt(){return 1234567;}double nextDouble(){return basement?.75:.25;}};
J catalog;
J const*model(IglooTemplateRole role){if(catalog.is_null())return nullptr;for(auto const&m:catalog.at("models"))if(m.at("name").get_ref<std::string const&>()==iglooTemplateName(role))return &m;throw std::runtime_error("native asset fixture missing role");}
StructurePoint size(IglooTemplateRole r){if(auto m=model(r)){auto const&s=m->at("size");return {s[0],s[1],s[2]};}return r==IglooTemplateRole::Ladder?StructurePoint{3,3,3}:r==IglooTemplateRole::Laboratory?StructurePoint{9,6,9}:StructurePoint{7,5,8};}
struct Surface{int height=77,water=31,biome=5;double lake=0,river=0,volcanic=0,stone=0;};
std::vector<LegacyDataMarker>markers(IglooTemplateRole r){if(auto m=model(r)){std::vector<LegacyDataMarker>out;for(auto const&marker:m->at("rawMarkers")){auto p=marker.at("local");out.push_back({{p[0],p[1],p[2]},marker.at("label")});}return out;}if(r==IglooTemplateRole::Laboratory)return {{{1,2,1},"Villager"},{{2,2,1},"Zombie Villager"},{{3,3,1},"chest"}};return {};}
int main(int argc,char**argv){try{
 if(argc!=2&&argc!=3)throw std::runtime_error("report.json [asset-catalog.json]");if(argc==3){std::ifstream f(argv[2]);f>>catalog;}J geometry=J::array(),random=J::array();
 for(uint32_t seed:{0u,1u,42u,0xffffffffu,0x8ab341abu}){StructureRandom actual(seed);std::mt19937 expected(seed);for(int i=0;i<32;++i){int n=actual.nextInt(20);check(n==int(expected()%20),"bounded RNG differs");int full=actual.nextInt();check(full==int(expected()>>1),"unbounded native RNG must be 31-bit");double d=actual.nextDouble();check(d==double(expected())/4294967296.,"native double must consume one 32-bit word");if(i<2)random.push_back({seed,n,full,d});}}
 int cases=0;size_t fragmentCount=0;
 for(int rotation=0;rotation<4;++rotation)for(bool basement:{false,true})for(int ladders:{0,7}){
  FixedRandom rng{rotation,basement,ladders};auto plan=planFrozenIgloo({-3,-2},80,rng,size);auto fitted=fitFrozenIglooFloor(plan,5,[](int,int){return Surface{};});check(fitted.accepted&&fitted.floor==78,"shared whole-footprint fit failed");auto program=compileFrozenIgloo(plan,markers);
  check(program.geometry.parts.front().origin.y==77,"frozen top changed its fitted floor");check(program.parts.size()==size_t(basement?ladders+6:1),"part count differs from shaft plan");
  using Key=std::array<int,3>;using World=std::map<Key,std::string>;World forward,reverse;J partRows=J::array();
  std::vector<VillageXZ>owners;for(auto const&[owner,ops]:program.owners)owners.push_back(owner);
  auto generate=[&](std::vector<VillageXZ>const&order,World&world){for(auto owner:order){++fragmentCount;StructureBox clip{{owner.first*16,-64,owner.second*16},{owner.first*16+15,319,owner.second*16+15}};
   executeFrozenIgloo(program,clip,[&](size_t i,auto const&p,int r,uint32_t seed,uint32_t rngSeed,auto const&){check(r==rotation&&seed==1234567u&&rngSeed==1234567u+uint32_t(i)*0x9e3779b9u,"part state depends on fragment order");for(auto const&m:program.parts[i].markers)if(inStructureOwner(m.position,clip))world[{m.position.x,m.position.y,m.position.z}]="structure_marker";},[&](StructurePoint p){check(inStructureOwner(p,clip),"foreign marker cleared");world[{p.x,p.y,p.z}]="air";},[&](StructureLootAssignment const&a){check(inStructureOwner(a.position,clip)&&a.table=="loot_tables/chests/igloo_chest.json"&&a.seed==1234567,"loot escaped owning fragment or lost seed");world[{a.position.x,a.position.y,a.position.z}]=a.table;});
  }};
  generate(owners,forward);std::reverse(owners.begin(),owners.end());generate(owners,reverse);check(forward==reverse,"frozen marker/loot results depend on generation order");
  if(basement){check(program.population.size()>0&&program.lootWitnesses.size()==1,"basement markers lost");int reads=0;
   bool missingRejected=false;try{compileFrozenIgloo(plan,[](auto){return std::vector<LegacyDataMarker>{};});}catch(...){missingRejected=true;}check(missingRejected,"missing processed laboratory markers were silently accepted");
   auto name=[&](StructurePoint p)->std::string_view{++reads;if(p==program.lootWitnesses.front())return "minecraft:chest";for(auto const&[key,actors]:program.population)for(auto const&m:actors)if(p==StructurePoint{m.position.x,m.position.y-1,m.position.z})return "minecraft:stone_bricks";return "minecraft:air";};
   for(auto const&[owner,actors]:program.population){reads=0;check(!prepareIglooPopulation(program,{owner.first,owner.second},[](ChunkPosition){return false;},name)&&reads==0,"unloaded basement queried blocks");auto ready=prepareIglooPopulation(program,{owner.first,owner.second},[](ChunkPosition){return true;},name);check(ready&&ready->size()==actors.size(),"basement residents not ready together");auto missingChest=prepareIglooPopulation(program,{owner.first,owner.second},[](ChunkPosition){return true;},[](StructurePoint)->std::string_view{return "minecraft:air";});check(!missingChest,"blueprint-only basement scheduled actors");}
  }else check(program.population.empty()&&program.lootWitnesses.empty(),"plain top created basement population");
  for(size_t i=0;i<plan.parts.size();++i){auto const&p=plan.parts[i];J data=J::array();for(auto const&m:program.parts[i].markers)data.push_back({m.position.x,m.position.y,m.position.z,m.definition});partRows.push_back({{"index",i},{"template",std::string(iglooTemplateName(p.role))},{"origin",{p.origin.x,p.origin.y,p.origin.z}},{"bounds",structureBoxJson(p.bounds)},{"seed",program.parts[i].seed},{"markers",data}});}geometry.push_back({{"rotation",rotation},{"basement",basement},{"ladderChoice",ladders},{"parts",partRows},{"ownerCount",program.owners.size()}});++cases;
 }
 FixedRandom deepRng{0,true,7};auto tooDeep=planFrozenIgloo({0,0},0,deepRng,size);moveFrozenIgloo(tooDeep,-60);int probes=0;check(!fitFrozenIglooFloor(tooDeep,5,[&](int,int){++probes;return Surface{};}).accepted&&probes==0,"bedrock-overlapping laboratory sampled/accepted");
 J report={{"passed",true},{"planCases",cases},{"fragments",fragmentCount},{"geometry",geometry},{"random",random},{"orderIndependentMarkersAndLoot",true},{"sharedWholeFootprintFit",true},{"incompleteLaboratoryRejected",true},{"bedrockOverlapRejectedBeforeSampling",true},{"unloadedBasementNoBlockReads",true},{"actualChestWitnessRequired",true},{"rawGameAssetSizesAndMarkersUsed",!catalog.is_null()},{"fixtureModels",catalog.is_null()?J{}:catalog.at("models")},{"processedRuntimeTemplatesVerified",false},{"gameplayTestPerformed",false},{"nativeTemplatePlacementExecuted",false},{"nativeTemplateParityVerified",false}};std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)throw std::runtime_error("report failed");std::cout<<"Shared frozen-template plan/executor contract passed\n";return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
