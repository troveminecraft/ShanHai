#include "rules/CityExecution.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>
#include <tuple>
using namespace worldgen_rules;using J=nlohmann::json;
void check(bool p,char const*why){if(!p)throw std::runtime_error(why);}
using Key=std::tuple<int,int,int>;
struct State {std::string_view name="minecraft:air";int bits=0;bool valid=true;};
struct World {
 std::map<Key,State>cells;bool rockDefault=false;std::optional<StructureBox>owner;
 std::set<std::pair<int,int>>foreignLiveReads;
 State get(StructurePoint p){if(owner&&(p.x<owner->min.x||p.x>owner->max.x||p.z<owner->min.z||p.z>owner->max.z))foreignLiveReads.emplace(p.x,p.z);auto i=cells.find({p.x,p.y,p.z});return i==cells.end()?State{rockDefault?"minecraft:deepslate":"minecraft:air"}:i->second;}
 std::string_view name(State s){return s.name;}
 bool isAir(State s){return s.name=="minecraft:air";}
 bool solid(State s){return s.name=="minecraft:deepslate"||s.name=="minecraft:stone"||s.name=="minecraft:sculk"||s.name=="minecraft:moss_block"||s.name=="minecraft:bedrock"||s.name=="minecraft:polished_deepslate"||s.name=="minecraft:chest";}
 bool natural(State s){return cityNatural(s.name);}
 bool survives(State s,StructurePoint){return s.valid;}
 int veinBits(State s){return s.bits;}
 int faceMask(size_t i){return 1<<i;}
 void write(StructurePoint p,State s){if(owner)check(p.x>=owner->min.x&&p.x<=owner->max.x&&p.z>=owner->min.z&&p.z<=owner->max.z,"city write escaped owner");cells[{p.x,p.y,p.z}]=s;}
 void air(StructurePoint p){write(p,{});}
 void sculk(StructurePoint p){write(p,{"minecraft:sculk"});}
 void vein(StructurePoint p,State s,int bits){s.bits=bits;write(p,s);}
 void fringe(StructurePoint p,int bits){write(p,{"minecraft:sculk_vein",bits});}
 J rows()const{J j=J::array();for(auto const&[k,s]:cells){auto[x,y,z]=k;j.push_back({x,y,z,s.name,s.bits,s.valid});}return j;}
};
std::vector<StructureBlueprintPiece> rooms(){StructureBlueprintPiece p;p.box={{-9,-43,-7},{26,-31,10}};p.kind=StructurePieceKind::Template;return {p};}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("report.json");auto pieces=rooms();std::vector<CityLink>links{{{-8,-38,0},{-20,-27,0}},{{25,-38,1},{44,-30,1}}};std::vector<StructureBox>neighbours{{{40,-13,-8},{49,-7,9}}};
 World forward;forward.rockDefault=true;forward.cells[{-2,-38,0}]={"minecraft:chest"};forward.cells[{-1,-38,0}]={"minecraft:polished_deepslate"};forward.cells[{0,-38,0}]={"minecraft:bedrock"};forward.cells[{1,-38,0}]={"minecraft:sculk_sensor"};forward.cells[{2,-38,0}]={"minecraft:flowing_lava"};forward.cells[{3,-38,0}]={"minecraft:iron_ore"};auto reverse=forward;
 std::vector<StructureBox>owners;for(int z=-1;z<=0;++z)for(int x=-2;x<=2;++x)owners.push_back({{x*16,-64,z*16},{x*16+15,319,z*16+15}});
 unsigned carved=0;for(auto b:owners){forward.owner=b;carved+=carveCityEnvironment(pieces,links,neighbours,b,42,forward);}for(auto i=owners.rbegin();i!=owners.rend();++i){reverse.owner=*i;carveCityEnvironment(pieces,links,neighbours,*i,42,reverse);}check(forward.rows()==reverse.rows()&&carved>1000,"cavern changed with chunk generation order");forward.owner.reset();
 for(auto[x,name]:std::array<std::pair<int,std::string_view>,4>{{{-2,"minecraft:chest"},{-1,"minecraft:polished_deepslate"},{0,"minecraft:bedrock"},{1,"minecraft:sculk_sensor"}}})check(forward.get({x,-38,0}).name==name,"city carved protected native structure state");
 check(forward.isAir(forward.get({2,-38,0}))&&forward.isAir(forward.get({3,-38,0})),"city cavity left natural fluid or ore");
 auto feature=pieces;feature.front().kind=StructurePieceKind::Feature;World untouched;untouched.rockDefault=true;check(carveCityEnvironment(feature,{}, {},owners.front(),42,untouched)==0&&untouched.cells.empty(),"feature node manufactured a cavern room");
 // Live foreign rock must not attach a vein to the predicted neighbouring
 // cavity. These two worlds intentionally disagree about that foreign state.
 StructureBox city{{0,-43,0},{31,-31,15}},owner{{0,-64,0},{15,319,15}};
 World edgeA;edgeA.cells[{15,-38,4}]={"minecraft:sculk_vein",(1<<0)|(1<<5)};edgeA.cells[{15,-39,4}]={"minecraft:sculk"};edgeA.cells[{15,-40,4}]={"minecraft:deepslate"};edgeA.cells[{16,-38,4}]={"minecraft:stone"};edgeA.cells[{4,-40,4}]={"minecraft:sculk"};edgeA.cells[{6,-40,4}]={"minecraft:moss_carpet",0,false};edgeA.cells[{8,-40,4}]={"minecraft:cave_vines_body_with_berries"};auto edgeB=edgeA;edgeB.cells[{16,-38,4}]={};
 std::map<std::pair<int,int>,int>samples;auto sample=[&](int x,int z){++samples[{x,z}];return 7;};auto material=[](int,int,int,int){return 1;};edgeA.owner=edgeB.owner=owner;
 finishCityEnvironment(city,pieces,{}, {},owner,42,sample,material,edgeA);finishCityEnvironment(city,pieces,{}, {},owner,42,[](int,int){return 7;},material,edgeB);
 edgeA.owner.reset();edgeB.owner.reset();check(edgeA.get({15,-38,4}).bits==(1<<0)&&edgeB.get({15,-38,4}).bits==(1<<0),"vein used uncarved live neighbour support");
 check(edgeA.isAir(edgeA.get({4,-40,4}))&&edgeA.isAir(edgeA.get({6,-40,4}))&&edgeA.isAir(edgeA.get({8,-40,4})),"unsupported surviving ecology was not removed");
 check(edgeA.foreignLiveReads.empty()&&edgeB.foreignLiveReads.empty(),"ecology read live foreign chunk");for(auto const&[p,n]:samples)check(n==1,"halo surface resampled per voxel");
 auto support=cityEcologySupport({0,20,0},[](auto q){return q.y==14||q.y==21;},[](auto q){return q.y==13||q.y==20;});check(support&&support->y==14,"native ecology support priority changed");
 auto absent=cityEcologySupport({0,20,0},[](auto){return true;},[](auto){return false;});check(!absent,"ecology manufactured a support pedestal");
 auto nodes=pieces;StructureBlueprintPiece node;node.kind=StructurePieceKind::Feature;node.origin={15,-35,0};node.location="minecraft:sculk_patch_feature";nodes.push_back(node);node.origin.x=16;nodes.push_back(node);
 auto scheduled=cityEcologyNodes(nodes,{-7,-2},owner,42);check(scheduled.size()==1&&scheduled.front().piece==1&&scheduled.front().position==StructurePoint{15,-35,0}&&scheduled.front().feature=="minecraft:sculk_patch_feature"&&scheduled.front().seed==placementSeed(-7,-2,extraSpec(8)->salt+1,42),"native feature node ownership, source index or anchor seed changed");
 auto candidates=cityNeighbourCandidates(city,42);check(!candidates.empty()&&candidates.size()<256,"neighbour survey is absent or unbounded");for(auto c:candidates)check(c.type==6||c.type==11,"neighbour survey reserved mineshafts or unrelated families");check(cityNeighbourIncluded(city,{{32,-20,0},{40,-10,15}})&&!cityNeighbourIncluded(city,{{10000,-20,0},{10015,-10,15}}),"neighbour exclusion reach changed");
 World empty;StructureBox far{{10000,-64,10000},{10015,319,10015}};check(carveCityEnvironment(pieces,links,neighbours,far,42,empty)==0&&empty.cells.empty(),"non-overlap city issued writes");
 auto executionPieces=nodes;StructureBlueprintPiece later;later.box={{4,-43,4},{6,-40,6}};executionPieces.push_back(later);
 World execution;execution.rockDefault=true;execution.owner=owner;std::vector<std::string>events;std::optional<StructurePoint>sensor;
 auto result=executeCityFragment(executionPieces,{-7,-2},owner,42,[&]{events.push_back("carve");return carveCityEnvironment(executionPieces,{}, {},owner,42,execution);},[&](size_t i,uint32_t seed,auto const&){events.push_back("template"+std::to_string(i));check((i==0||i==3)&&seed==placementSeed(0,0,extraSpec(8)->salt+int(i),42),"city compressed piece indices or changed template owner seed");execution.write({int(i)+4,-43,4},{"minecraft:polished_deepslate"});},[&](auto const&n,auto const&){check(events==std::vector<std::string>{"carve","template0","template3"},"native feature ran before all template floors");events.push_back("feature");sensor=cityEcologySupport(n.position,[&](auto p){return execution.isAir(execution.get(p));},[&](auto p){return execution.solid(execution.get(p));});if(sensor)execution.write(*sensor,{"minecraft:sculk_sensor"});return bool(sensor);},[&]{events.push_back("finish");finishCityEnvironment(city,executionPieces,{}, {},owner,42,[](int,int){return 7;},material,execution);});
 check(result.carved>1000&&result.templates==2&&result.ecologyAttempts==1&&result.ecologyPlaced==1&&events.back()=="finish"&&sensor&&execution.get(*sensor).name=="minecraft:sculk_sensor","full city fragment lost ecology, order or protected native state");
 bool wideRejected=false,startedWide=false;try{executeCityFragment(executionPieces,{-7,-2},city,42,[&]{startedWide=true;return 0u;},[](auto...){},[](auto const&,auto const&){return false;},[]{});}catch(std::runtime_error const&){wideRejected=true;}check(wideRejected&&!startedWide,"multi-owner city started mutations before rejecting invalid ownership");
 J out={{"passed",true},{"carvedNaturalCells",carved},{"ownerFragments",owners.size()},{"reverseGenerationOrderEqual",true},{"nativeMasonryContainersBedrockAndSensorPreserved",true},{"naturalFluidAndOreCarved",true},{"featureNodesExcludedFromRoomGeometry",true},{"foreignLiveSupportIgnored",true},{"survivingEcologySupportValidated",true},{"haloSampledOncePerColumn",true},{"nativeEcologySupportSearchOrderPreserved",true},{"nativeEcologyNodeOwnerAndSeedPreserved",true},{"boundedNeighbourSurveyCandidates",candidates.size()},{"nativeFeaturePlacementExecuted",false},{"gameplayTestPerformed",false},{"veinOwnerState",edgeA.get({15,-38,4}).bits}};out["fullFragmentExecutionOrderAndOriginalPieceIndices"]=true;out["fragmentTemplateAndEcologyCounts"]={result.templates,result.ecologyAttempts,result.ecologyPlaced};out["fragmentCarvedNaturalCells"]=result.carved;out["invalidWideOwnerRejectedBeforeMutation"]=true;std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("report write failed");std::cout<<"Shared city environment owner, support and protected-state checks passed\n";return 0;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
