#include "rules/ScatteredExecution.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace worldgen_rules;using J=nlohmann::json;
void check(bool v,char const*m){if(!v)throw std::runtime_error(m);}
struct Native {int id;bool chest=false;};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("report.json");J cases=J::array();
 for(int biome:{10,13})for(uint32_t seed:{0u,42u,0xffffffffu})for(int x:{-18,7}){
  StructureBox box{{x,60,-18},{x+24,77,9}};auto recipe=planScatteredFragments(biome,{spacingCell(x,16),-2},seed,box,60);auto initial=box.moved(20);check(scatteredFreshMove(recipe,initial)==-20,"fresh native floor changed");
  std::vector<ScatteredOwnerFragment>fragments;int created=0,placed=0;J rows=J::array();
  auto create=[&](auto const&r,auto const&f){check(r.floor==60&&f.seed==placementSeed(f.owner.x,f.owner.z,14357617,seed),"owner seed differs from upstream");return Native{++created};};
  auto place=[&](Native&native,auto const&,auto const&f){check(!native.chest,"state leaked between native fragments");native.chest=true;++placed;fragments.push_back(f);rows.push_back({f.owner.x,f.owner.z,f.seed,{{f.clip.min.x,f.clip.min.y,f.clip.min.z},{f.clip.max.x,f.clip.max.y,f.clip.max.z}}});return true;};
  check(executeScatteredFragments(recipe,{{INT_MIN,-64,INT_MIN},{INT_MAX,319,INT_MAX}},create,place),"whole clip placed nothing");check(created==placed&&placed>1,"native start not fresh per owner");
  auto whole=rows;rows=J::array();created=placed=0;auto reversed=fragments;std::reverse(reversed.begin(),reversed.end());for(auto const&f:reversed)executeScatteredFragments(recipe,f.clip,create,place);auto canonical=[](J rows){std::sort(rows.begin(),rows.end());return rows;};check(canonical(whole)==canonical(rows),"owner generation order changed results");
  int untouched=0;check(!executeScatteredFragments(recipe,{{10000,-64,10000},{10015,319,10015}},[&](auto const&,auto const&){++untouched;return Native{};},[&](auto&,auto const&,auto const&){++untouched;return true;})&&!untouched,"non-overlap calls native services");
  for(auto const&f:reversed){auto c=f.clip;check(scatteredFragmentWriteOwned(c,{c.min.x,-58,c.min.z})&&!scatteredFragmentWriteOwned(c,{c.min.x,-59,c.min.z})&&!scatteredFragmentWriteOwned(c,{c.max.x+1,60,c.min.z}),"native owner write boundary changed");check(!scatteredSeededRead(c,{c.min.x,-100,c.min.z})&&scatteredSeededRead(c,{c.min.x-1,60,c.min.z}),"seeded read differs from original XZ-only rule");}
  auto partial=reversed.front().clip;partial.min.x+=3;partial.max.z-=2;int calls=0;executeScatteredFragments(recipe,partial,create,[&](auto&,auto const&,auto const&f){++calls;check(f.clip.min==partial.min&&f.clip.max==partial.max,"partial caller clip widened");return true;});check(calls==1,"partial clip traversed other owners");
  cases.push_back({{"biome",biome},{"seed",seed},{"anchor",{recipe.anchor.x,recipe.anchor.z}},{"nativeBiome",scatteredNativeBiome(recipe)},{"fragments",whole}});
 }
 ScatteredSeededReadCache<int>cache;int queries=0;for(int i=0;i<100;++i)check(cache.get({-17,61,20},[&](auto){++queries;return 8;})==8,"seeded material changed");check(queries==1&&cache.size()==1,"seeded material resampled per read");
 auto reject=[](auto fn){bool failed=false;try{fn();}catch(std::exception const&){failed=true;}check(failed,"invalid scattered geometry accepted");};reject([]{planScatteredFragments(11,{0,0},42,{{0,60,0},{20,77,20}},60);});auto valid=planScatteredFragments(13,{0,0},42,{{0,60,0},{20,77,20}},60);reject([&]{scatteredFreshMove(valid,{{0,64,0},{21,81,20}});});reject([&]{scatteredFreshMove(valid,{{0,INT_MIN,0},{20,INT_MAX,20}});});
 J out={{"passed",true},{"cases",cases},{"freshPerOwner",true},{"upstreamOwnerSeeds",true},{"reversedOrderEqual",true},{"partialAndNonOverlapClips",true},{"ownerWriteAndSeededReadPolicy",true},{"oneQueryPerUniqueSeededPosition",true},{"nativePlacementExecuted",false},{"gameplayTestPerformed",false}};std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("report failed");std::cout<<"Shared fresh scattered fragment execution checks passed\n";return 0;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
