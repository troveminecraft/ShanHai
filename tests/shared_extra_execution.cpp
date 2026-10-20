#include "rules/ExtraExecution.h"
#include "rules/StructureCandidates.h"
#include <iostream>
#include <stdexcept>
using namespace worldgen_rules;
void require(bool b){if(!b)throw std::runtime_error("Extra execution regression");}
int main(){
 size_t candidates=0,fragments=0;
 for(uint32_t seed:{0u,1u,0xfedcba98u})for(int z=-96;z<=96;z+=13)for(int x=-96;x<=96;x+=11){
  ChunkPosition owner{x,z};auto actual=nearbyExtraCandidates(owner,seed);std::vector<std::pair<int,ChunkPosition>>expected;
  // Retained original LLBDS cell traversal, independent of the shared helper.
  for(auto const&s:ExtraSpecs)for(int iz=spacingCell(z-s.radius,s.spacing);iz<=spacingCell(z+s.radius,s.spacing);++iz)for(int ix=spacingCell(x-s.radius,s.spacing);ix<=spacingCell(x+s.radius,s.spacing);++ix){
   auto c=candidateForCell(ix,iz,seed,s.spacing,s.separation,s.salt);if(std::abs(c.x-x)<=s.radius&&std::abs(c.z-z)<=s.radius)expected.emplace_back(s.type,c);
  }
  require(actual.size()==expected.size());for(size_t i=0;i<actual.size();++i)require(actual[i].first==expected[i].first&&actual[i].second.x==expected[i].second.x&&actual[i].second.z==expected[i].second.z);candidates+=actual.size();
 }
 StructureBox owner{{-32,-64,16},{-17,319,31}};
 std::array<StructureBox,4>pieces{{{{-30,10,17},{-28,12,19}},{{80,10,80},{90,15,90}},{{0,0,0},{-1,-1,-1}},{{-18,10,30},{-10,12,35}}}};
 for(int type:{0,4,5,6,9}){
  std::vector<size_t>visited;
  auto count=executeExtraPieces(type,{-7,3},owner,12345,pieces.size(),[&](size_t i){return pieces[i];},[&](size_t i,uint32_t randomSeed,auto const&clip){
   require(clip.min.x==-32&&clip.max.z==31);visited.push_back(i);
   auto cp=(type==4||type==5||type==9)?ChunkPosition{-7,3}:ChunkPosition{-2,1};
   require(randomSeed==placementSeed(cp.x,cp.z,extraSpec(type)->salt+int(i),12345));
  });
  require(count==2&&visited==std::vector<size_t>{0,3});fragments+=count;
 }
 auto ordered=orderedExtraCandidates(*extraSpec(11),-10,-10,17,-5,5,-5,5);
 for(size_t i=1;i<ordered.size();++i)require(std::tie(ordered[i-1].distance,ordered[i-1].anchor.z,ordered[i-1].anchor.x)<=std::tie(ordered[i].distance,ordered[i].anchor.z,ordered[i].anchor.x));
 std::cout<<"{\"candidateEntries\":"<<candidates<<",\"fragmentCalls\":"<<fragments<<",\"passed\":true}\n";
}
